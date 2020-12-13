/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 13 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define BATCH_SIZE 256

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
    B32 in_use;
} RenderBatch;

enum
{
    RENDER_MESSAGE_DRAW_RECTANGLE,
    RENDER_MESSAGE_STROKE_RECTANGLE,
    RENDER_MESSAGE_DRAW_TEXT,
    RENDER_MESSAGE_BLUR_SCREEN_REGION,
};

typedef struct RenderMessage RenderMessage;
struct RenderMessage
{
    RenderMessage *next;
    I32 type;
    U32 sort;
    Font *font;
    Texture texture;
    Colour colour;
    Rectangle rectangle;
    F32 *projection_matrix;
    void *data;
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

internal I32 global_default_shader_projection_matrix_location;
internal ShaderID global_default_shader;

internal I32 global_text_shader_projection_matrix_location;
internal ShaderID global_text_shader;

internal I32 global_blur_shader_projection_matrix_location;
internal I32 global_blur_shader_direction_location;
internal ShaderID global_blur_shader;

internal Texture global_flat_colour_texture;

internal ShaderID global_currently_bound_shader = 0;
internal TextureID global_currently_bound_texture = 0;

internal U32 global_renderer_window_w = 0;
internal U32 global_renderer_window_h = 0;
internal F32 global_camera_x = 0.0f, global_camera_y = 0.0f;

internal F32 global_projection_matrix[16];
internal F32 global_ui_projection_matrix[16];

#define BLUR_TEXTURE_SCALE 4 /* NOTE(tbt): divide window dimensions by this to
                                           get size of blur texture */
internal U32 global_blur_target_a;
internal U32 global_blur_target_b;
internal TextureID global_blur_texture_a;
internal TextureID global_blur_texture_b;

internal void
initialise_renderer(OpenGLFunctions *gl)
{
    I32 i;
    I32 offset;
    I32 indices[BATCH_SIZE * 6];
    void *render_queue_backing_memory;

    U32 flat_colour_texture_data = 0xffffffff;

    /* NOTE(tbt): setup vao, vbo and ibo */
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

    /* NOTE(tbt): setup 1x1 white texture for rendering flat colours */
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

    /* NOTE(tbt): load shaders and get uniform locations */
    global_default_shader = load_shader(gl,
                                        SHADER_PATH("default.vert"),
                                        SHADER_PATH("default.frag"));
    global_default_shader_projection_matrix_location =
        gl->GetUniformLocation(global_default_shader,
                               "u_projection_matrix");
    assert(global_default_shader_projection_matrix_location != -1);

    global_text_shader = load_shader(gl,
                                     SHADER_PATH("text.vert"),
                                     SHADER_PATH("text.frag"));
    global_text_shader_projection_matrix_location =
        gl->GetUniformLocation(global_text_shader,
                               "u_projection_matrix");
    assert(global_text_shader_projection_matrix_location != -1);

    global_blur_shader = load_shader(gl,
                                     SHADER_PATH("blur.vert"),
                                     SHADER_PATH("blur.frag"));
    global_blur_shader_projection_matrix_location =
        gl->GetUniformLocation(global_blur_shader,
                               "u_projection_matrix");
    assert(global_blur_shader_projection_matrix_location != -1);
    global_blur_shader_direction_location = 
        gl->GetUniformLocation(global_blur_shader,
                               "u_direction");
    assert(global_blur_shader_direction_location != -1);

    gl->UseProgram(global_currently_bound_shader);

    /* NOTE(tbt): setup some OpenGL state */
    gl->ClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    gl->Enable(GL_BLEND);
    gl->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* NOTE(tbt): generate framebuffer for first blur pass */
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

    /* NOTE(tbt): generate framebuffer for second blur pass */
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
upload_projection_matrix(OpenGLFunctions *gl,
                         F32 *matrix)
{
            gl->UseProgram(global_default_shader);
            gl->UniformMatrix4fv(global_default_shader_projection_matrix_location,
                                 1,
                                 GL_FALSE,
                                 matrix);

            gl->UseProgram(global_text_shader);
            gl->UniformMatrix4fv(global_text_shader_projection_matrix_location,
                                 1,
                                 GL_FALSE,
                                 matrix);

            gl->UseProgram(global_currently_bound_shader);
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

#define ui_draw_texture(_rectangle, _colour, _texture) draw_texture((_rectangle), (_colour), (_texture), 5, global_ui_projection_matrix)
#define world_draw_texture(_rectangle, _colour, _texture) draw_texture((_rectangle), (_colour), (_texture), 2, global_projection_matrix)
internal void
draw_texture(Rectangle rectangle,
             Colour colour,
             Texture texture,
             U32 sort,
             F32 *projection_matrix)
{
    RenderMessage message = {0};

    message.type = RENDER_MESSAGE_DRAW_RECTANGLE;
    message.rectangle = rectangle;
    message.colour = colour;
    message.texture = texture;
    message.projection_matrix = projection_matrix;
    message.sort = sort;

    enqueue_render_message(&global_render_queue, message);
}

#define ui_fill_rectangle(_rectangle, _colour) fill_rectangle((_rectangle), (_colour), 5, global_ui_projection_matrix)
#define world_fill_rectangle(_rectangle, _colour) fill_rectangle((_rectangle), (_colour), 2, global_projection_matrix)
internal void
fill_rectangle(Rectangle rectangle,
               Colour colour,
               U32 sort,
               F32 *projection_matrix)
{
    draw_texture(rectangle,
                 colour,
                 global_flat_colour_texture,
                 sort,
                 projection_matrix);
}

#define ui_stroke_rectangle(_rectangle, _colour, _stroke_width) stroke_rectangle((_rectangle), (_colour), (_stroke_width), 5, global_ui_projection_matrix)
#define world_stroke_rectangle(_rectangle, _colour, _stroke_width) stroke_rectangle((_rectangle), (_colour), (_stroke_width), 2, global_projection_matrix)
internal void
stroke_rectangle(Rectangle rectangle,
                 Colour colour,
                 F32 stroke_width,
                 U32 sort,
                 F32 *projection_matrix)
{
    RenderMessage message = {0};

    message.type = RENDER_MESSAGE_STROKE_RECTANGLE;
    message.rectangle = rectangle;
    message.colour = colour;
    message.data = arena_allocate(&global_frame_memory, sizeof(F32));
    *((F32 *)message.data) = stroke_width;
    message.sort = sort;
    message.projection_matrix = projection_matrix;

    enqueue_render_message(&global_render_queue, message);
}

#define ui_draw_text(_font, _x, _y, _wrap_width, _colour, _string) draw_text((_font), (_x), (_y), (_wrap_width), (_colour), (_string), 5, global_ui_projection_matrix) 
#define world_draw_text(_font, _x, _y, _wrap_width, _colour, _string) draw_text((_font), (_x), (_y), (_wrap_width), (_colour), (_string), 2, global_projection_matrix) 
internal void
draw_text(Font *font,
          F32 x, F32 y,
          U32 wrap_width,
          Colour colour,
          I8 *string,
          U32 sort,
          F32 *projection_matrix)
{
    RenderMessage message = {0};

    message.type = RENDER_MESSAGE_DRAW_TEXT;
    message.font = font;
    message.colour = colour;
    message.rectangle.x = x;
    message.rectangle.y = y;
    message.rectangle.w = wrap_width;
    message.data = arena_allocate(&global_frame_memory, strlen(string) + 1);
    memcpy(message.data, string, strlen(string));
    message.sort = sort;
    message.projection_matrix = projection_matrix;

    enqueue_render_message(&global_render_queue, message);
}

internal void
blur_screen_region(Rectangle region,
                   U32 sort)
{
    RenderMessage message = {0};

    message.type = RENDER_MESSAGE_BLUR_SCREEN_REGION;
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
                   width / BLUR_TEXTURE_SCALE,
                   height / BLUR_TEXTURE_SCALE,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   NULL);

    gl->BindTexture(GL_TEXTURE_2D, global_blur_texture_b);

    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   width / BLUR_TEXTURE_SCALE,
                   height / BLUR_TEXTURE_SCALE,
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
    gl->UniformMatrix4fv(global_blur_shader_projection_matrix_location,
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
print_opengl_errors(OpenGLFunctions *gl)
{
#ifdef LUCERNA_DEBUG
    GLenum error;
    while ((error = gl->GetError()) != GL_NO_ERROR)
    {
        fprintf(stderr, "OpenGL error: %s\n", error == GL_INVALID_ENUM ? "GL_INVALID_ENUM" :
                                              error == GL_INVALID_VALUE ? "GL_INVALID_VALUE":
                                              error == GL_INVALID_OPERATION ? "GL_INVALID_OPERATION":
                                              error == GL_INVALID_FRAMEBUFFER_OPERATION ? "GL_INVALID_FRAMEBUFFER_OPERATION":
                                              error == GL_OUT_OF_MEMORY ? "GL_OUT_OF_MEMORY":
                                              error == GL_STACK_UNDERFLOW ? "GL_STACK_UNDERFLOW":
                                              error == GL_STACK_OVERFLOW ? "GL_STACK_OVERFLOW":
                                              NULL);
    }
#endif
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
generate_quad(Rectangle rectangle,
              Colour colour,
              Texture texture)
{
    Quad result;

    result.bl.x = rectangle.x;
    result.bl.y = rectangle.y + rectangle.h;
    result.bl.r = colour.r;
    result.bl.g = colour.g;
    result.bl.b = colour.b;
    result.bl.a = colour.a;
    result.bl.u = texture.min_x;
    result.bl.v = texture.max_y;

    result.br.x = rectangle.x + rectangle.w;
    result.br.y = rectangle.y + rectangle.h;
    result.br.r = colour.r;
    result.br.g = colour.g;
    result.br.b = colour.b;
    result.br.a = colour.a;
    result.br.u = texture.max_x;
    result.br.v = texture.max_y;

    result.tr.x = rectangle.x + rectangle.w;
    result.tr.y = rectangle.y;
    result.tr.r = colour.r;
    result.tr.g = colour.g;
    result.tr.b = colour.b;
    result.tr.a = colour.a;
    result.tr.u = texture.max_x;
    result.tr.v = texture.min_y;

    result.tl.x = rectangle.x;
    result.tl.y = rectangle.y;
    result.tl.r = colour.r;
    result.tl.g = colour.g;
    result.tl.b = colour.b;
    result.tl.a = colour.a;
    result.tl.u = texture.min_x;
    result.tl.v = texture.min_y;

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
    /* NOTE(tbt): does not update the end pointer of the queue */
    /* NOTE(tbt): fine for now because only sorted at the end of every frame*/
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
        switch (message.type)
        {
            case RENDER_MESSAGE_DRAW_RECTANGLE:
            {
                if (batch.texture != message.texture.id   ||
                    batch.shader != global_default_shader ||
                    batch.quad_count >= BATCH_SIZE        ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
        
                    batch.shader = global_default_shader;
                    batch.texture = message.texture.id;
                }

                upload_projection_matrix(gl, message.projection_matrix);
        
                batch.in_use = true;
        
                batch.buffer[batch.quad_count++] =
                    generate_quad(message.rectangle,
                                  message.colour,
                                  message.texture);
        
                break;
            }
            case RENDER_MESSAGE_STROKE_RECTANGLE:
            {
                Rectangle top, bottom, left, right;
                F32 stroke_width;

                if (batch.texture != global_flat_colour_texture.id ||
                    batch.shader  != global_default_shader         ||
                    batch.quad_count >= BATCH_SIZE                 ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
        
                    batch.shader = global_default_shader;
                    batch.texture = global_flat_colour_texture.id;
                }
        
                upload_projection_matrix(gl, message.projection_matrix);

                batch.in_use = true;

                stroke_width = *((F32 *)message.data);

                top = RECTANGLE(message.rectangle.x,
                                message.rectangle.y,
                                message.rectangle.w,
                                stroke_width);
            
                bottom = RECTANGLE(message.rectangle.x,
                                   message.rectangle.y +
                                   message.rectangle.h,
                                   message.rectangle.w,
                                   -stroke_width);

                left = RECTANGLE(message.rectangle.x,
                                 message.rectangle.y +
                                 stroke_width,
                                 stroke_width,
                                 message.rectangle.h -
                                 stroke_width * 2);
            
                right = RECTANGLE(message.rectangle.x +
                                  message.rectangle.w,
                                  message.rectangle.y +
                                  stroke_width,
                                  -stroke_width,
                                  message.rectangle.h -
                                  stroke_width * 2);

                batch.buffer[batch.quad_count++] =
                    generate_quad(top,
                                  message.colour,
                                  message.texture);

                batch.buffer[batch.quad_count++] =
                    generate_quad(bottom,
                                  message.colour,
                                  message.texture);

                batch.buffer[batch.quad_count++] =
                    generate_quad(left,
                                  message.colour,
                                  message.texture);

                batch.buffer[batch.quad_count++] =
                    generate_quad(right,
                                  message.colour,
                                  message.texture);

                break;
            }
            case RENDER_MESSAGE_DRAW_TEXT:
            {
                F32 line_start = message.rectangle.x;
                I8 *string = message.data;
                F32 x = message.rectangle.x;
                F32 y = message.rectangle.y;
                U32 wrap_width = message.rectangle.w;

                if (message.font->texture.id != batch.texture ||
                    batch.shader != global_text_shader        ||
                    batch.quad_count >= BATCH_SIZE            ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
        
                    batch.shader = global_text_shader;
                    batch.texture = message.font->texture.id;
                }

                batch.in_use = true;

                upload_projection_matrix(gl, message.projection_matrix);
        
                while (*string)
                {
                    if (*string >=32 &&
                        *string < 128)
                    {
                        stbtt_aligned_quad q;
                        Rectangle rectangle;
                        Texture texture; /* NOTE(tbt): temporary texture to
                                                       store tex coords for
                                                       glyph so it can be passed
                                                       to `generate_quad`
                                         */
        
                        stbtt_GetBakedQuad(message.font->char_data,
                                           message.font->texture.width,
                                           message.font->texture.height,
                                           *string - 32,
                                           &x,
                                           &y,
                                           &q,
                                           1);
        
                        texture.min_x = q.s0;
                        texture.min_y = q.t0;
                        texture.max_x = q.s1;
                        texture.max_y = q.t1;
        
                        rectangle = RECTANGLE(q.x0, q.y0,
                                              q.x1 - q.x0,
                                              q.y1 - q.y0);
        
                        batch.buffer[batch.quad_count++] =
                            generate_quad(rectangle,
                                          message.colour,
                                          texture);
        
                        if (wrap_width && isspace(*string))
                        {
                            if (x + rectangle.w * 2 >
                                line_start + wrap_width)
                            {
                                y += message.font->size;
                                x = line_start;
                            }
                        }
                    }
                    else if (*string == '\n')
                    {
                        y += message.font->size;
                        x = line_start;
                    }
                    ++string;
                }
        
                break;
            }
            case RENDER_MESSAGE_BLUR_SCREEN_REGION:
            {
                Texture texture;

                flush_batch(gl, &batch);

                upload_projection_matrix(gl, global_ui_projection_matrix);

                /* NOTE(tbt): blit screen to framebuffer */
                gl->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                gl->BindFramebuffer(GL_DRAW_FRAMEBUFFER, global_blur_target_a);

                gl->Viewport(0,
                             0,
                             global_renderer_window_w / BLUR_TEXTURE_SCALE,
                             global_renderer_window_h / BLUR_TEXTURE_SCALE);

                gl->Clear(GL_COLOR_BUFFER_BIT);

                gl->BlitFramebuffer(0,
                                    0,
                                    global_renderer_window_w,
                                    global_renderer_window_h,
                                    0,
                                    0,
                                    global_renderer_window_w / BLUR_TEXTURE_SCALE,
                                    global_renderer_window_h / BLUR_TEXTURE_SCALE,
                                    GL_COLOR_BUFFER_BIT,
                                    GL_LINEAR);


                /* NOTE(tbt): apply first (horizontal) blur pass */
                gl->BindFramebuffer(GL_FRAMEBUFFER, global_blur_target_b);

                gl->Viewport(0,
                             0,
                             global_renderer_window_w / BLUR_TEXTURE_SCALE,
                             global_renderer_window_h / BLUR_TEXTURE_SCALE);

                gl->Clear(GL_COLOR_BUFFER_BIT);

                Texture blur_tex_coords =
                {
                    .min_x = 0.0f,
                    .min_y = 0.0f,
                    .max_x = 1.0f,
                    .max_y = 1.0f
                };

                batch.shader = global_blur_shader;
                batch.texture = global_blur_texture_a;
                batch.in_use = true;
                batch.quad_count = 1;
                batch.buffer[0] = generate_quad(RECTANGLE(0.0f,
                                                          0.0f,
                                                          (F32)global_renderer_window_w,
                                                          (F32)global_renderer_window_h),
                                                COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                                                blur_tex_coords);

                gl->UseProgram(global_blur_shader);
                global_currently_bound_shader = global_blur_shader;
                gl->Uniform2f(global_blur_shader_direction_location, 1.0f, 0.0f);
                flush_batch(gl, &batch);

                gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
                gl->Viewport(0,
                             0,
                             global_renderer_window_w,
                             global_renderer_window_h);

                /* NOTE(tbt): render back to screen, applying second blur pass */
                gl->Uniform2f(global_blur_shader_direction_location, 0.0f, 1.0f);

                if (batch.texture != global_blur_texture_b ||
                    batch.shader != global_blur_shader     ||
                    batch.quad_count >= BATCH_SIZE         ||
                    !(batch.in_use))
                {
                    flush_batch(gl, &batch);
                    batch.texture = global_blur_texture_b;
                    batch.shader = global_blur_shader;
                }

                texture.min_x = message.rectangle.x /
                                (F32)global_renderer_window_w;

                texture.max_x = (message.rectangle.x + message.rectangle.w) /
                                (F32)global_renderer_window_w;

                texture.min_y = message.rectangle.y /
                                (F32)global_renderer_window_h;

                texture.max_y = (message.rectangle.y + message.rectangle.h) /
                                (F32)global_renderer_window_h;

                batch.buffer[batch.quad_count++] =
                    generate_quad(message.rectangle,
                                  COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                                  texture);

                batch.in_use = true;
                break;
            }
        }
    }

    flush_batch(gl, &batch);

    global_render_queue.start = NULL;
    global_render_queue.end = NULL;

    arena_free_all(&global_frame_memory);
}

/*
MIT License

Copyright (c) 2020 Tom Thornton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

