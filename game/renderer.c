/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 04 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define BATCH_SIZE 256

typedef U32 ShaderID;

typedef struct
{
    F32 x, y;
    F32 r, g, b, a;
    F32 u, v;
} Vertex;

typedef struct
{
    Vertex bl, br, tr, tl;
} Quad;

typedef struct
{
    Quad buffer[BATCH_SIZE];
    U32 quad_count;
    TextureID texture;
    ShaderID shader;
    F32 *projection_matrix;
    B32 in_use;
} RenderBatch;

enum
{
    RENDER_MESSAGE_draw_rectangle,
    RENDER_MESSAGE_draw_rotated_rectangle,
    RENDER_MESSAGE_stroke_rectangle,
    RENDER_MESSAGE_draw_text,
    RENDER_MESSAGE_blur_screen_region,
    RENDER_MESSAGE_mask_rectangle_begin,
    RENDER_MESSAGE_mask_rectangle_end,
    RENDER_MESSAGE_draw_gradient,
};

typedef struct RenderMessage RenderMessage;
struct RenderMessage
{
    RenderMessage *next;
    I32 kind;
    U32 sort;
    Rect rectangle;
    F32 *projection_matrix;
    
    union
    {
        struct
        {
            Font *font;
            S8 string;
        };
        
        struct
        {
            Asset *texture;
            SubTexture sub_texture;
            F32 angle;
            F32 stroke_width;
        };
        
    };
    
    union
    {
        Colour colour;
        Gradient gradient;
    };
};

typedef struct
{
    RenderMessage *start;
    RenderMessage *end;
} RenderQueue;

internal RenderQueue global_render_queue = {0};

internal U32 global_vao_id;
internal U32 global_index_buffer_id;
internal U32 global_vertex_buffer_id;

#define MAX_SHADERS 5

internal I32 global_projection_matrix_locations[MAX_SHADERS];

internal ShaderID global_default_shader;
internal ShaderID global_text_shader;
internal ShaderID global_blur_shader;

internal ShaderID global_currently_bound_shader = 0;

internal I32 global_blur_shader_direction_location;
internal I32 global_lights_uniform_location;

internal Texture global_flat_colour_texture;

internal U32 global_renderer_window_w = 0;
internal U32 global_renderer_window_h = 0;
internal F32 global_camera_x = 0.0f, global_camera_y = 0.0f;

internal F32 global_projection_matrix[16];
internal F32 global_ui_projection_matrix[16];

#define BLUR_TEXTURE_SCALE 4 // NOTE(tbt): bitshift window dimensions right by this much to get size of blur texture
internal U32 global_blur_target_a;
internal U32 global_blur_target_b;
internal TextureID global_blur_texture_a;
internal TextureID global_blur_texture_b;

internal void
generate_orthographic_projection_matrix(F32 *matrix,
                                        F32 left,
                                        F32 right,
                                        F32 top,
                                        F32 bottom)
{
    memset(matrix, 0, 16 * sizeof(F32));
    matrix[0]  = 2.0f / (right - left);
    matrix[5]  = 2.0f / (top - bottom);
    matrix[10] = -1.0f;
    matrix[12] = -(right + left) / (right - left);
    matrix[13] = -(top + bottom) / (top - bottom);
    matrix[14] = 0.0f;
    matrix[15] = 1.0f;
}

internal void
initialise_renderer(OpenGLFunctions *gl)
{
    I32 i;
    I32 offset;
    I32 indices[BATCH_SIZE * 6];
    void *render_queue_backing_memory;
    
    U32 flat_colour_texture_data = 0xffffffff;
    
    // NOTE(tbt): setup vao, vbo and ibo
    gl->GenVertexArrays(1, &global_vao_id);
    gl->BindVertexArray(global_vao_id);
    
    gl->GenBuffers(1, &global_vertex_buffer_id);
    gl->BindBuffer(GL_ARRAY_BUFFER, global_vertex_buffer_id);
    
    gl->EnableVertexAttribArray(0);
    gl->VertexAttribPointer(0,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            sizeof(Vertex),
                            NULL);
    
    gl->EnableVertexAttribArray(1);
    gl->VertexAttribPointer(1,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            sizeof(Vertex),
                            (const void *)(2 * sizeof(F32)));
    
    gl->EnableVertexAttribArray(2);
    gl->VertexAttribPointer(2,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            sizeof(Vertex),
                            (const void *)(6 * sizeof(F32)));
    
    gl->GenBuffers(1, &global_index_buffer_id);
    offset = 0;
    for (i = 0; i < BATCH_SIZE * 6; i += 6)
    {
        indices[i + 0] = 0 + offset;
        indices[i + 1] = 1 + offset;
        indices[i + 2] = 2 + offset;
        
        indices[i + 3] = 2 + offset;
        indices[i + 4] = 3 + offset;
        indices[i + 5] = 0 + offset;
        
        offset += 4;
    }
    
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_index_buffer_id);
    gl->BufferData(GL_ELEMENT_ARRAY_BUFFER,
                   BATCH_SIZE * 6 * sizeof(indices[0]),
                   indices,
                   GL_STATIC_DRAW);
    
    gl->BufferData(GL_ARRAY_BUFFER,
                   BATCH_SIZE * 4 * sizeof(Vertex),
                   NULL,
                   GL_DYNAMIC_DRAW);
    
    // NOTE(tbt): setup 1x1 white texture for rendering flat colours
    global_flat_colour_texture.width = 1;
    global_flat_colour_texture.height = 1;
    
    gl->GenTextures(1, &global_flat_colour_texture.id);
    gl->BindTexture(GL_TEXTURE_2D, global_flat_colour_texture.id);
    
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   1,
                   1,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   &flat_colour_texture_data);
    
    //
    // NOTE(tbt): compile shaders
    //
    I32 status;
#define SHADER_INFO_LOG_MAX_LEN 128
    
    global_default_shader = gl->CreateProgram();
    global_blur_shader = gl->CreateProgram();
    global_text_shader = gl->CreateProgram();
    S8 shader_src;
    ShaderID vertex_shader;
    ShaderID fragment_shader;
    
    temporary_memory_begin(&global_static_memory);
    
    shader_src = platform_read_entire_file(&global_static_memory,
                                           s8_literal("../assets/shaders/default.vert"));
    assert(shader_src.buffer);
    
    vertex_shader = gl->CreateShader(GL_VERTEX_SHADER);
    gl->ShaderSource(vertex_shader, 1, (const I8 * const *)&shader_src.buffer, NULL);
    gl->CompileShader(vertex_shader);
    
    // NOTE(tbt): check for vertex shader compilation errors
    gl->GetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(vertex_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteShader(vertex_shader);
        
        fprintf(stderr, "vertex shader compilation failure. '%s'\n", msg);
        exit(-1);
    }
    
    // NOTE(tbt): attach vertex shader to all shader programs
    gl->AttachShader(global_default_shader,  vertex_shader);
    gl->AttachShader(global_blur_shader,     vertex_shader);
    gl->AttachShader(global_text_shader,     vertex_shader);
    
    // NOTE(tbt): compile default fragment shader
    shader_src = platform_read_entire_file(&global_static_memory,
                                           s8_literal("../assets/shaders/default.frag"));
    assert(shader_src.buffer);
    fragment_shader = gl->CreateShader(GL_FRAGMENT_SHADER);
    gl->ShaderSource(fragment_shader, 1, (const I8 * const *)&shader_src.buffer, NULL);
    gl->CompileShader(fragment_shader);
    
    gl->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(fragment_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteShader(fragment_shader);
        
        fprintf(stderr, "fragment shader compilation failure. '%s'\n", msg);
        exit(-1);
    }
    gl->AttachShader(global_default_shader, fragment_shader);
    
    // NOTE(tbt): link default shader
    gl->LinkProgram(global_default_shader);
    
    gl->GetProgramiv(global_default_shader, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(global_default_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteProgram(global_default_shader);
        gl->DeleteShader(vertex_shader);
        gl->DeleteShader(fragment_shader);
        
        fprintf(stderr, "shader link failure. '%s'\n", msg);
        exit(-1);
    }
    
    gl->DetachShader(global_default_shader, vertex_shader);
    gl->DetachShader(global_default_shader, fragment_shader);
    gl->DeleteShader(fragment_shader);
    
    // NOTE(tbt): compile blur fragment shader
    shader_src = platform_read_entire_file(&global_static_memory,
                                           s8_literal("../assets/shaders/blur.frag"));
    assert(shader_src.buffer);
    fragment_shader = gl->CreateShader(GL_FRAGMENT_SHADER);
    gl->ShaderSource(fragment_shader, 1, (const I8 * const *)&shader_src.buffer, NULL);
    gl->CompileShader(fragment_shader);
    
    gl->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(fragment_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteShader(fragment_shader);
        
        fprintf(stderr, "fragment shader compilation failure. '%s'\n", msg);
        exit(-1);
    }
    gl->AttachShader(global_blur_shader, fragment_shader);
    
    // NOTE(tbt): link blur shader
    gl->LinkProgram(global_blur_shader);
    
    gl->GetProgramiv(global_blur_shader, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(global_blur_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteProgram(global_blur_shader);
        gl->DeleteShader(vertex_shader);
        gl->DeleteShader(fragment_shader);
        
        fprintf(stderr, "shader link failure. '%s'\n", msg);
        exit(-1);
    }
    
    gl->DetachShader(global_blur_shader, vertex_shader);
    gl->DetachShader(global_blur_shader, fragment_shader);
    gl->DeleteShader(fragment_shader);
    
    // NOTE(tbt): compile text fragment shader
    shader_src = platform_read_entire_file(&global_static_memory,
                                           s8_literal("../assets/shaders/text.frag"));
    assert(shader_src.buffer);
    fragment_shader = gl->CreateShader(GL_FRAGMENT_SHADER);
    gl->ShaderSource(fragment_shader, 1, (const I8 * const *)&shader_src.buffer, NULL);
    gl->CompileShader(fragment_shader);
    
    gl->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(fragment_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteShader(fragment_shader);
        
        fprintf(stderr, "fragment shader compilation failure. '%s'\n", msg);
        exit(-1);
    }
    gl->AttachShader(global_text_shader, fragment_shader);
    
    // NOTE(tbt): link text shader
    gl->LinkProgram(global_text_shader);
    
    gl->GetProgramiv(global_text_shader, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        I8 msg[SHADER_INFO_LOG_MAX_LEN];
        
        gl->GetShaderInfoLog(global_text_shader,
                             SHADER_INFO_LOG_MAX_LEN,
                             NULL,
                             msg);
        gl->DeleteProgram(global_text_shader);
        gl->DeleteShader(vertex_shader);
        gl->DeleteShader(fragment_shader);
        
        fprintf(stderr, "shader link failure. '%s'\n", msg);
        exit(-1);
    }
    
    gl->DetachShader(global_text_shader, vertex_shader);
    gl->DetachShader(global_text_shader, fragment_shader);
    gl->DeleteShader(fragment_shader);
    
    // NOTE(tbt): cleanup vertex shader
    gl->DeleteShader(vertex_shader);
    temporary_memory_end(&global_static_memory);
    
    // NOTE(tbt): cache projection matrix locations
    global_projection_matrix_locations[global_default_shader] =
        gl->GetUniformLocation(global_default_shader,
                               "u_projection_matrix");
    assert(global_projection_matrix_locations[global_default_shader] != -1);
    
    global_projection_matrix_locations[global_text_shader] =
        gl->GetUniformLocation(global_text_shader,
                               "u_projection_matrix");
    assert(global_projection_matrix_locations[global_text_shader] != -1);
    
    global_projection_matrix_locations[global_blur_shader] =
        gl->GetUniformLocation(global_blur_shader,
                               "u_projection_matrix");
    assert(global_projection_matrix_locations[global_blur_shader] != -1);
    
    // NOTE(tbt): special uniforms
    global_blur_shader_direction_location = 
        gl->GetUniformLocation(global_blur_shader,
                               "u_direction");
    assert(global_blur_shader_direction_location != -1);
    
    // NOTE(tbt): reset currently bounds shader
    gl->UseProgram(global_currently_bound_shader);
    
    // NOTE(tbt): setup some OpenGL state
    gl->ClearColor(0.92f, 0.88f, 0.9f, 1.0f);
    
    gl->Enable(GL_BLEND);
    gl->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // NOTE(tbt): generate framebuffer for first blur pass
    gl->GenFramebuffers(1, &global_blur_target_a);
    gl->BindFramebuffer(GL_FRAMEBUFFER, global_blur_target_a);
    
    gl->GenTextures(1, &global_blur_texture_a);
    gl->BindTexture(GL_TEXTURE_2D, global_blur_texture_a);
    
    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   1920,
                   1080,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   NULL);
    
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    gl->FramebufferTexture2D(GL_FRAMEBUFFER,
                             GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D,
                             global_blur_texture_a,
                             0);
    
    // NOTE(tbt): generate framebuffer for second blur pass
    gl->GenFramebuffers(1, &global_blur_target_b);
    gl->BindFramebuffer(GL_FRAMEBUFFER, global_blur_target_b);
    
    gl->GenTextures(1, &global_blur_texture_b);
    gl->BindTexture(GL_TEXTURE_2D, global_blur_texture_b);
    
    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   1920,
                   1080,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   NULL);
    
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    gl->FramebufferTexture2D(GL_FRAMEBUFFER,
                             GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D,
                             global_blur_texture_b,
                             0);
    
    gl->BindTexture(GL_TEXTURE_2D, 0);
    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

internal void
enqueue_render_message(RenderQueue *queue,
                       RenderMessage message)
{
    RenderMessage *queued_message =
        arena_allocate(&global_frame_memory,
                       sizeof(*queued_message));
    
    *queued_message = message;
    
    if (queue->end)
    {
        queue->end->next = queued_message;
    }
    else
    {
        queue->start = queued_message;
    }
    
    queue->end = queued_message;
}

internal B32
dequeue_render_message(RenderQueue *queue,
                       RenderMessage *result)
{
    if (!queue->start) { return false; }
    
    *result = *queue->start;
    queue->start = queue->start->next;
    
    return true;
}

#define UI_SORT_DEPTH    1080
#define WORLD_SORT_DEPTH 0

#define ui_draw_sub_texture(_rectangle, _colour, _texture, _sub_texture) draw_sub_texture((_rectangle), (_colour), (_texture), (_sub_texture), UI_SORT_DEPTH, global_ui_projection_matrix)
#define world_draw_sub_texture(_rectangle, _colour, _texture, _sub_texture) draw_sub_texture((_rectangle), (_colour), (_texture), (_sub_texture), WORLD_SORT_DEPTH, global_projection_matrix)
internal void
draw_sub_texture(Rect rectangle,
                 Colour colour,
                 Asset *texture,
                 SubTexture sub_texture,
                 U32 sort,
                 F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    
    message.kind = RENDER_MESSAGE_draw_rectangle;
    message.rectangle = rectangle;
    message.colour = colour;
    message.texture = texture;
    message.sub_texture = sub_texture;
    message.projection_matrix = projection_matrix;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

#define ui_draw_rotated_sub_texture(_gl, _rectangle, _angle, _colour, _texture, _sub_texture) draw_rotated_sub_texture((_gl), (_rectangle), (_angle), (_colour), (_texture), (_sub_texture), UI_SORT_DEPTH, global_ui_projection_matrix)
#define world_draw_rotated_sub_texture(_gl, _rectangle, _angle, _colour, _texture, _sub_texture) draw_rotated_sub_texture((_gl), (_rectangle), (_angle), (_colour), (_texture), (_sub_texture), WORLD_SORT_DEPTH, global_projection_matrix)
internal void
draw_rotated_sub_texture(OpenGLFunctions *gl,
                         Rect rectangle,
                         F32 angle,
                         Colour colour,
                         Asset *texture,
                         SubTexture sub_texture,
                         U32 sort,
                         F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_draw_rotated_rectangle;
    message.rectangle = rectangle;
    message.angle = angle;
    message.colour = colour;
    message.texture = texture;
    message.sub_texture = sub_texture;
    message.projection_matrix = projection_matrix;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

#define ui_fill_rectangle(_rectangle, _colour) fill_rectangle((_rectangle), (_colour), UI_SORT_DEPTH, global_ui_projection_matrix)
#define world_fill_rectangle(_rectangle, _colour) fill_rectangle((_rectangle), (_colour), WORLD_SORT_DEPTH, global_projection_matrix)
internal void
fill_rectangle(Rect rectangle,
               Colour colour,
               U32 sort,
               F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_draw_rectangle;
    message.rectangle = rectangle;
    message.colour = colour;
    message.texture = NULL;
    message.sub_texture = ENTIRE_TEXTURE;
    message.projection_matrix = projection_matrix;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

#define ui_fill_rotated_rectangle(_rectangle, _angle, _colour) fill_rotated_rectangle((_rectangle), (_angle), (_colour), UI_SORT_DEPTH, global_ui_projection_matrix)
#define world_fill_rotated_rectangle(_rectangle, _angle, _colour) fill_rotated_rectangle((_rectangle), (_angle), (_colour), WORLD_SORT_DEPTH, global_projection_matrix)
internal void
fill_rotated_rectangle(Rect rectangle,
                       F32 angle,
                       Colour colour,
                       U32 sort,
                       F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_draw_rotated_rectangle;
    message.rectangle = rectangle;
    message.angle = angle;
    message.colour = colour;
    message.texture = NULL;
    message.sub_texture = ENTIRE_TEXTURE;
    message.projection_matrix = projection_matrix;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

#define ui_stroke_rectangle(_rectangle, _colour, _stroke_width) stroke_rectangle((_rectangle), (_colour), (_stroke_width), UI_SORT_DEPTH, global_ui_projection_matrix)
#define world_stroke_rectangle(_rectangle, _colour, _stroke_width) stroke_rectangle((_rectangle), (_colour), (_stroke_width), WORLD_SORT_DEPTH, global_projection_matrix)
internal void
stroke_rectangle(Rect rectangle,
                 Colour colour,
                 F32 stroke_width,
                 U32 sort,
                 F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_stroke_rectangle;
    message.rectangle = rectangle;
    message.colour = colour;
    message.stroke_width = stroke_width;
    message.sort = sort;
    message.projection_matrix = projection_matrix;
    
    enqueue_render_message(&global_render_queue, message);
}

#define ui_draw_text(_font, _x, _y, _wrap_width, _colour, _string) draw_text((_font), (_x), (_y), (_wrap_width), (_colour), (_string), UI_SORT_DEPTH, global_ui_projection_matrix) 
#define world_draw_text(_font, _x, _y, _wrap_width, _colour, _string) draw_text((_font), (_x), (_y), (_wrap_width), (_colour), (_string), WORLD_SORT_DEPTH, global_projection_matrix) 
internal void
draw_text(Font *font,
          F32 x, F32 y,
          U32 wrap_width,
          Colour colour,
          S8 string,
          U32 sort,
          F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_draw_text;
    message.font = font;
    message.colour = colour;
    message.rectangle.x = x;
    message.rectangle.y = y;
    message.rectangle.w = wrap_width;
    message.string = copy_string(&global_frame_memory, string);
    message.sort = sort;
    message.projection_matrix = projection_matrix;
    
    enqueue_render_message(&global_render_queue, message);
}

#define ui_draw_gradient(_rectangle, _gradient) draw_gradient((_rectangle), (_gradient), UI_SORT_DEPTH, global_ui_projection_matrix)
#define world_draw_gradient(_rectangle, _gradient) draw_gradient((_rectangle), (_gradient), WORLD_SORT_DEPTH, global_projection_matrix)
internal void
draw_gradient(Rect rectangle,
              Gradient gradient,
              U32 sort,
              F32 *projection_matrix)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_draw_gradient;
    message.rectangle = rectangle;
    message.projection_matrix = projection_matrix;
    message.sort = sort;
    message.gradient = gradient;
    
    enqueue_render_message(&global_render_queue, message);
}

internal void
begin_rectangle_mask(Rect region,
                     U32 sort)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_mask_rectangle_begin;
    message.rectangle = region;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

internal void
end_rectangle_mask(U32 sort)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_mask_rectangle_end;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

// NOTE(tbt): be careful - _sort is evaluated twice!
#define mask_rectangular_region(_region, _sort) for (I32 i = (begin_rectangle_mask((_region), (_sort)), 0); !i; (++i, end_rectangle_mask((_sort))))

internal void
blur_screen_region(Rect region,
                   U32 sort)
{
    RenderMessage message = {0};
    
    message.kind = RENDER_MESSAGE_blur_screen_region;
    message.rectangle = region;
    message.sort = sort;
    
    enqueue_render_message(&global_render_queue, message);
}

internal void
set_renderer_window_size(OpenGLFunctions *gl,
                         U32 width,
                         U32 height)
{
    global_renderer_window_w = width;
    global_renderer_window_h = height;
    
    gl->Viewport(0,
                 0,
                 width,
                 height);
    
    gl->BindTexture(GL_TEXTURE_2D, global_blur_texture_a);
    
    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   width >> BLUR_TEXTURE_SCALE,
                   height >> BLUR_TEXTURE_SCALE,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   NULL);
    
    gl->BindTexture(GL_TEXTURE_2D, global_blur_texture_b);
    
    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   width >> BLUR_TEXTURE_SCALE,
                   height >> BLUR_TEXTURE_SCALE,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   NULL);
    
    gl->BindTexture(GL_TEXTURE_2D, global_currently_bound_texture);
    
    generate_orthographic_projection_matrix(global_projection_matrix,
                                            global_camera_x,
                                            global_camera_x + width,
                                            global_camera_y,
                                            global_camera_y + height);
    
    generate_orthographic_projection_matrix(global_ui_projection_matrix,
                                            0, width,
                                            0, height);
    
    gl->UseProgram(global_blur_shader);
    gl->UniformMatrix4fv(global_projection_matrix_locations[global_blur_shader],
                         1,
                         GL_FALSE,
                         global_ui_projection_matrix);
    
    gl->UseProgram(global_currently_bound_shader);
}

internal void
set_camera_position(F32 x,
                    F32 y)
{
    global_camera_x = x;
    global_camera_y = y;
    
    generate_orthographic_projection_matrix(global_projection_matrix,
                                            x, x + global_renderer_window_w,
                                            y, y + global_renderer_window_h);
}

internal void
flush_batch(OpenGLFunctions *gl,
            RenderBatch *batch)
{
    if (!batch->in_use) return;
    
    if (global_currently_bound_shader != batch->shader)
    {
        gl->UseProgram(batch->shader);
        global_currently_bound_shader = batch->shader;
    }
    
    if (global_currently_bound_texture != batch->texture)
    {
        gl->BindTexture(GL_TEXTURE_2D, batch->texture);
        global_currently_bound_texture = batch->texture;
    }
    
    gl->UniformMatrix4fv(global_projection_matrix_locations[batch->shader],
                         1,
                         GL_FALSE,
                         batch->projection_matrix);
    
    
    gl->BufferData(GL_ARRAY_BUFFER,
                   batch->quad_count * sizeof(Quad),
                   batch->buffer,
                   GL_DYNAMIC_DRAW);
    
    gl->DrawElements(GL_TRIANGLES,
                     batch->quad_count * 6,
                     GL_UNSIGNED_INT,
                     NULL);
    
    batch->quad_count = 0;
    batch->texture = 0;
    batch->shader = 0;
    batch->in_use = false;
}

Quad
generate_quad(Rect rectangle,
              Colour colour,
              SubTexture sub_texture)
{
    Quad result;
    
    result.bl.x = rectangle.x;
    result.bl.y = rectangle.y + rectangle.h;
    result.bl.r = colour.r;
    result.bl.g = colour.g;
    result.bl.b = colour.b;
    result.bl.a = colour.a;
    result.bl.u = sub_texture.min_x;
    result.bl.v = sub_texture.max_y;
    
    result.br.x = rectangle.x + rectangle.w;
    result.br.y = rectangle.y + rectangle.h;
    result.br.r = colour.r;
    result.br.g = colour.g;
    result.br.b = colour.b;
    result.br.a = colour.a;
    result.br.u = sub_texture.max_x;
    result.br.v = sub_texture.max_y;
    
    result.tr.x = rectangle.x + rectangle.w;
    result.tr.y = rectangle.y;
    result.tr.r = colour.r;
    result.tr.g = colour.g;
    result.tr.b = colour.b;
    result.tr.a = colour.a;
    result.tr.u = sub_texture.max_x;
    result.tr.v = sub_texture.min_y;
    
    result.tl.x = rectangle.x;
    result.tl.y = rectangle.y;
    result.tl.r = colour.r;
    result.tl.g = colour.g;
    result.tl.b = colour.b;
    result.tl.a = colour.a;
    result.tl.u = sub_texture.min_x;
    result.tl.v = sub_texture.min_y;
    
    return result;
}

Quad
generate_rotated_quad(Rect rectangle,
                      F32 angle,
                      Colour colour,
                      SubTexture sub_texture)
{
    Quad result;
    
    // NOTE(tbt): undecided as to whether it should be converted or not
    // angle *= 3.1415926535f / 180.0f; // NOTE(tbt): convert degress to radians
    
    //
    // NOTE(tbt): we want to rotate about the centre of the quad, not the
    //            coordinates (0, 0) in the world, so first a quad is generated
    //            with it's centre at the origin, a rotation matrix is applied
    //            and it is then translated to be at the intended position.
    //
    
    F32 x = -(rectangle.w * 0.5f), y = -(rectangle.h * 0.5f);
    F32 x_offset = rectangle.x - x, y_offset = rectangle.y - y;
    
    Rect centre_at_origin = rectangle_literal(x, y, rectangle.w, rectangle.h);
    
    result = generate_quad(centre_at_origin, colour, sub_texture);
    
    
    // NOTE(tbt): cast the quad to a vertex pointer, so we can iterate through
    //            each vertex
    Vertex *vertices = (Vertex *)(&result);
    
    for (I32 i = 0; i < 4; ++i)
    {
        // NOTE(tbt): matrix generation and multiplication are combined into one step
        Vertex vertex = vertices[i];
        vertices[i].x = vertex.x * cos(angle) - vertex.y * sin(angle);
        vertices[i].y = vertex.x * sin(angle) + vertex.y * cos(angle);
        
        // NOTE(tbt): once a vertex has been rotated it can be translated to the
        //            intended position
        vertices[i].x += x_offset;
        vertices[i].y += y_offset;
    }
    
    return result;
}

internal RenderMessage *
render_queue_sorted_merge(RenderMessage *a, RenderMessage *b)
{
    RenderMessage *result;
    
    if (!a) { return b; }
    if (!b) { return a; }
    
    if (a->sort <= b->sort)
    {
        result = a;
        result->next = render_queue_sorted_merge(a->next, b);
    }
    else
    {
        result = b;
        result->next = render_queue_sorted_merge(a, b->next);
    }
    
    return result;
}

internal void
render_queue_split(RenderMessage *source,
                   RenderMessage **a,
                   RenderMessage **b)
{
    RenderMessage *fast, *slow;
    
    slow = source;
    fast = source->next;
    
    while (fast)
    {
        fast = fast->next;
        if (fast)
        {
            slow = slow->next;
            fast = fast->next;
        }
    }
    
    *a = source;
    *b = slow->next;
    slow->next = NULL;
}

internal void
render_queue_merge_sort(RenderMessage **head_reference)
{
    RenderMessage *head = *head_reference;
    RenderMessage *a, *b;
    
    if (!head || !(head->next)) { return; }
    
    render_queue_split(head, &a, &b);
    
    render_queue_merge_sort(&a);
    render_queue_merge_sort(&b);
    
    *head_reference = render_queue_sorted_merge(a, b);
}

internal void
sort_render_queue(RenderQueue *queue)
{
    render_queue_merge_sort(&(queue->start));
    // NOTE(tbt): does not update the end pointer of the queue
    // NOTE(tbt): fine for now because only sorted at the end of every frame
    queue->end = NULL;
}

internal void
process_render_queue(OpenGLFunctions *gl)
{
    RenderMessage message;
    
    RenderBatch batch;
    batch.quad_count = 0;
    batch.texture = 0;
    batch.shader = 0;
    batch.in_use = false;
    
    print_opengl_errors(gl);
    
    sort_render_queue(&global_render_queue);
    
    gl->Clear(GL_COLOR_BUFFER_BIT);
    
    while (dequeue_render_message(&global_render_queue, &message))
    {
        switch (message.kind)
        {
            case RENDER_MESSAGE_draw_rectangle:
            case RENDER_MESSAGE_draw_rotated_rectangle:
            {
                TextureID texture_id = 0;
                
                texture_id = global_flat_colour_texture.id;
                if (message.texture)
                {
                    load_texture(gl, message.texture);
                    
                    if (message.texture->loaded)
                    {
                        texture_id = message.texture->texture.id;
                    }
                }
                
                if (batch.texture != texture_id                          ||
                    batch.shader != global_default_shader                ||
                    batch.quad_count >= BATCH_SIZE                       ||
                    batch.projection_matrix != message.projection_matrix ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
                    
                    batch.shader = global_default_shader;
                    batch.texture = texture_id;
                    batch.projection_matrix = message.projection_matrix;
                }
                
                batch.in_use = true;
                
                if (message.kind == RENDER_MESSAGE_draw_rotated_rectangle)
                {
                    batch.buffer[batch.quad_count++] =
                        generate_rotated_quad(message.rectangle,
                                              message.angle,
                                              message.colour,
                                              message.sub_texture);
                }
                else
                {
                    batch.buffer[batch.quad_count++] =
                        generate_quad(message.rectangle,
                                      message.colour,
                                      message.sub_texture);
                }
                
                break;
            }
            case RENDER_MESSAGE_stroke_rectangle:
            {
                Rect top, bottom, left, right;
                F32 stroke_width;
                
                if (batch.texture != global_flat_colour_texture.id       ||
                    batch.shader != global_default_shader                ||
                    batch.projection_matrix != message.projection_matrix ||
                    batch.quad_count >= BATCH_SIZE                       ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
                    
                    batch.shader = global_default_shader;
                    batch.texture = global_flat_colour_texture.id;
                    batch.projection_matrix = message.projection_matrix;
                }
                
                batch.in_use = true;
                
                stroke_width = message.stroke_width;
                
                top = rectangle_literal(message.rectangle.x,
                                        message.rectangle.y,
                                        message.rectangle.w,
                                        stroke_width);
                
                bottom = rectangle_literal(message.rectangle.x,
                                           message.rectangle.y +
                                           message.rectangle.h,
                                           message.rectangle.w,
                                           -stroke_width);
                
                left = rectangle_literal(message.rectangle.x,
                                         message.rectangle.y +
                                         stroke_width,
                                         stroke_width,
                                         message.rectangle.h -
                                         stroke_width * 2);
                
                right = rectangle_literal(message.rectangle.x +
                                          message.rectangle.w,
                                          message.rectangle.y +
                                          stroke_width,
                                          -stroke_width,
                                          message.rectangle.h -
                                          stroke_width * 2);
                
                batch.buffer[batch.quad_count++] =
                    generate_quad(top,
                                  message.colour,
                                  message.sub_texture);
                
                batch.buffer[batch.quad_count++] =
                    generate_quad(bottom,
                                  message.colour,
                                  message.sub_texture);
                
                batch.buffer[batch.quad_count++] =
                    generate_quad(left,
                                  message.colour,
                                  message.sub_texture);
                
                batch.buffer[batch.quad_count++] =
                    generate_quad(right,
                                  message.colour,
                                  message.sub_texture);
                
                break;
            }
            case RENDER_MESSAGE_draw_text:
            {
                F32 line_start = message.rectangle.x;
                F32 x = message.rectangle.x;
                F32 y = message.rectangle.y;
                U32 wrap_width = message.rectangle.w;
                
                if (batch.texture != message.font->texture.id            ||
                    batch.shader != global_text_shader                   ||
                    batch.projection_matrix != message.projection_matrix ||
                    batch.quad_count >= BATCH_SIZE                       ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
                    
                    batch.shader = global_text_shader;
                    batch.texture = message.font->texture.id;
                    batch.projection_matrix = message.projection_matrix;
                }
                
                batch.in_use = true;
                
                for (I32 i = 0;
                     i < message.string.len;
                     ++i)
                {
                    if (message.string.buffer[i] >=32 &&
                        message.string.buffer[i] < 128)
                    {
                        stbtt_aligned_quad q;
                        Rect rectangle;
                        SubTexture sub_texture;
                        
                        stbtt_GetBakedQuad(message.font->char_data,
                                           message.font->texture.width,
                                           message.font->texture.height,
                                           message.string.buffer[i] - 32,
                                           &x,
                                           &y,
                                           &q);
                        
                        sub_texture.min_x = q.s0;
                        sub_texture.min_y = q.t0;
                        sub_texture.max_x = q.s1;
                        sub_texture.max_y = q.t1;
                        
                        rectangle = rectangle_literal(q.x0, q.y0,
                                                      q.x1 - q.x0,
                                                      q.y1 - q.y0);
                        
                        batch.buffer[batch.quad_count++] =
                            generate_quad(rectangle,
                                          message.colour,
                                          sub_texture);
                        
                        if (wrap_width && isspace(message.string.buffer[i]))
                        {
                            if (x + rectangle.w * 2 >
                                line_start + wrap_width)
                            {
                                y += message.font->size;
                                x = line_start;
                            }
                        }
                    }
                    else if (message.string.buffer[i] == '\n')
                    {
                        y += message.font->size;
                        x = line_start;
                    }
                }
                
                break;
            }
            case RENDER_MESSAGE_blur_screen_region:
            {
                flush_batch(gl, &batch);
                
                // NOTE(tbt): blit screen to framebuffer
                gl->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, global_blur_target_a);
                
                gl->Viewport(0,
                             0,
                             global_renderer_window_w >> BLUR_TEXTURE_SCALE,
                             global_renderer_window_h >> BLUR_TEXTURE_SCALE);
                
                gl->Clear(GL_COLOR_BUFFER_BIT);
                
                gl->BlitFramebuffer(0,
                                    0,
                                    global_renderer_window_w,
                                    global_renderer_window_h,
                                    0,
                                    0,
                                    global_renderer_window_w >> BLUR_TEXTURE_SCALE,
                                    global_renderer_window_h >> BLUR_TEXTURE_SCALE,
                                    GL_COLOR_BUFFER_BIT,
                                    GL_LINEAR);
                
                
                // NOTE(tbt): apply first (horizontal) blur pass
                gl->BindFramebuffer(GL_FRAMEBUFFER, global_blur_target_b);
                
                gl->Viewport(0,
                             0,
                             global_renderer_window_w >> BLUR_TEXTURE_SCALE,
                             global_renderer_window_h >> BLUR_TEXTURE_SCALE);
                
                gl->Clear(GL_COLOR_BUFFER_BIT);
                
                batch.shader = global_blur_shader;
                batch.texture = global_blur_texture_a;
                batch.projection_matrix = global_ui_projection_matrix;
                batch.in_use = true;
                batch.quad_count = 1;
                batch.buffer[0] = generate_quad(rectangle_literal(0.0f,
                                                                  0.0f,
                                                                  (F32)global_renderer_window_w,
                                                                  (F32)global_renderer_window_h),
                                                colour_literal(1.0f, 1.0f, 1.0f, 1.0f),
                                                ENTIRE_TEXTURE);
                
                gl->UseProgram(global_blur_shader);
                global_currently_bound_shader = global_blur_shader;
                gl->Uniform2f(global_blur_shader_direction_location, 1.0f, 0.0f);
                flush_batch(gl, &batch);
                
                gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
                gl->Viewport(0,
                             0,
                             global_renderer_window_w,
                             global_renderer_window_h);
                
                // NOTE(tbt): render back to screen, applying second blur pass
                gl->Uniform2f(global_blur_shader_direction_location, 0.0f, 1.0f);
                
                batch.texture = global_blur_texture_b;
                batch.shader = global_blur_shader;
                batch.projection_matrix = global_ui_projection_matrix;
                batch.in_use = true;
                
                SubTexture blur_tex_coords;
                blur_tex_coords.min_x = message.rectangle.x /
                    (F32)global_renderer_window_w;
                
                blur_tex_coords.max_x = (message.rectangle.x +
                                         message.rectangle.w) /
                    (F32)global_renderer_window_w;
                
                blur_tex_coords.min_y = message.rectangle.y /
                    (F32)global_renderer_window_h;
                
                blur_tex_coords.max_y = (message.rectangle.y +
                                         message.rectangle.h) /
                    (F32)global_renderer_window_h;
                
                batch.buffer[batch.quad_count++] =
                    generate_quad(message.rectangle,
                                  colour_literal(1.0f, 1.0f, 1.0f, 1.0f),
                                  blur_tex_coords);
                
                batch.in_use = true;
                break;
            }
            case RENDER_MESSAGE_mask_rectangle_begin:
            {
                flush_batch(gl, &batch);
                gl->Enable(GL_SCISSOR_TEST);
                gl->Scissor(message.rectangle.x, global_renderer_window_h - message.rectangle.y - message.rectangle.h,
                            message.rectangle.w, message.rectangle.h);
                break;
            }
            case RENDER_MESSAGE_mask_rectangle_end:
            {
                flush_batch(gl, &batch);
                gl->Disable(GL_SCISSOR_TEST);
                break;
            }
            case RENDER_MESSAGE_draw_gradient:
            {
                Quad quad;
                
                quad.bl.x = message.rectangle.x;
                quad.bl.y = message.rectangle.y + message.rectangle.h;
                quad.bl.r = message.gradient.bl.r;
                quad.bl.g = message.gradient.bl.g;
                quad.bl.b = message.gradient.bl.b;
                quad.bl.a = message.gradient.bl.a;
                quad.bl.u = 0.0f;
                quad.bl.v = 1.0f;
                
                quad.br.x = message.rectangle.x + message.rectangle.w;
                quad.br.y = message.rectangle.y + message.rectangle.h;
                quad.br.r = message.gradient.br.r;
                quad.br.g = message.gradient.br.g;
                quad.br.b = message.gradient.br.b;
                quad.br.a = message.gradient.br.a;
                quad.br.u = 1.0f;
                quad.br.v = 1.0f;
                
                quad.tr.x = message.rectangle.x + message.rectangle.w;
                quad.tr.y = message.rectangle.y;
                quad.tr.r = message.gradient.tr.r;
                quad.tr.g = message.gradient.tr.g;
                quad.tr.b = message.gradient.tr.b;
                quad.tr.a = message.gradient.tr.a;
                quad.tr.u = 1.0f;
                quad.tr.v = 0.0f;
                
                quad.tl.x = message.rectangle.x;
                quad.tl.y = message.rectangle.y;
                quad.tl.r = message.gradient.tl.r;
                quad.tl.g = message.gradient.tl.g;
                quad.tl.b = message.gradient.tl.b;
                quad.tl.a = message.gradient.tl.a;
                quad.tl.u = 0.0f;
                quad.tl.v = 0.0f;
                
                if (batch.texture != global_flat_colour_texture.id       ||
                    batch.shader != global_default_shader                ||
                    batch.quad_count >= BATCH_SIZE                       ||
                    batch.projection_matrix != message.projection_matrix ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
                    
                    batch.shader = global_default_shader;
                    batch.texture = global_flat_colour_texture.id;
                    batch.projection_matrix = message.projection_matrix;
                }
                
                batch.in_use = true;
                
                batch.buffer[batch.quad_count++] = quad;
                
                break;
            }
        }
    }
    
    flush_batch(gl, &batch);
    
    global_render_queue.start = NULL;
    global_render_queue.end = NULL;
    
    arena_free_all(&global_frame_memory);
}

