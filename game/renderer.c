//
// NOTE(tbt): constants
//~

enum
{
 BATCH_SIZE = 1024,
 
 SHADER_INFO_LOG_MAX_LEN = 4096,
 
 SCREEN_W_IN_WORLD_UNITS = 1920,
 SCREEN_H_IN_WORLD_UNITS = 1080,
 
 BLUR_TEXTURE_W = SCREEN_W_IN_WORLD_UNITS / 2,
 BLUR_TEXTURE_H = SCREEN_H_IN_WORLD_UNITS / 2,
};

//
// NOTE(tbt): types
//~

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
 Rect mask;
 B32 in_use;
} RenderBatch;

typedef enum
{
 RENDER_MESSAGE_draw_rectangle,
 RENDER_MESSAGE_stroke_rectangle,
 RENDER_MESSAGE_draw_text,
 RENDER_MESSAGE_blur_screen_region,
 RENDER_MESSAGE_draw_gradient,
 RENDER_MESSAGE_do_post_processing,
} RenderMessageKind;

typedef enum
{
 POST_PROCESSING_KIND_world = LEVEL_KIND_world,
 POST_PROCESSING_KIND_memory = LEVEL_KIND_memory,
} PostProcessingKind;

typedef struct RenderMessage RenderMessage;
struct RenderMessage
{
 RenderMessage *next;
 RenderMessageKind kind;
 U8 sort;
 Rect mask;
 
 Rect rectangle;
 F32 *projection_matrix;
 Font *font;
 S8 string;
 TextureID texture;
 SubTexture sub_texture;
 F32 angle;
 F32 stroke_width;
 Colour colour;
 Gradient gradient;
 U32 strength;
 F32 exposure;
 PostProcessingKind post_processing_kind;
};

typedef struct
{
 U32 target;
 TextureID texture;
} Framebuffer;

//
// NOTE(tbt): state
//~

internal F32 global_projection_matrix[16];
internal F32 global_ui_projection_matrix[16];

typedef struct
{
 struct RcxMessageQueue
 {
  RenderMessage *start;
  RenderMessage *end;
 } message_queue;
 
 U32 vao;
 U32 ibo;
 U32 vbo;
 
 struct RcxShaders
 {
#define shader(_name, _vertex_shader) ShaderID _name;
#include "shader_list.h"
  
  struct RcxShadersLastModified
  {
#define shader(_name, _vertex_shader) U64 _name;
#include "shader_list.h"
  } last_modified;
  
  ShaderID current;
 } shaders;
 
 // NOTE(tbt): uniform cache
 struct RcxUniformLocations
 {
  // NOTE(tbt): uniforms for texture shader
  struct RcxTextureShaderUniformLocations
  {
   I32 projection_matrix;
  } texture;
  
  // NOTE(tbt): uniforms for text shader
  struct RcxTextShaderUniformLocations
  {
   I32 projection_matrix;
  } text;
  
  // NOTE(tbt): uniforms for blur shader
  struct RcxBlurShaderUniformLocations
  {
   I32 direction;
  } blur;
  
  // NOTE(tbt): uniforms for post processing and memory post processing shaders
  struct RcxPostProcessingUniformLocations
  {
   I32 time;
   I32 exposure;
   I32 screen_texture;
   I32 blur_texture;
  };
  struct RcxPostProcessingUniformLocations post_processing;
  struct RcxPostProcessingUniformLocations memory_post_processing;
 } uniform_locations;
 
 TextureID flat_colour_texture;
 
 struct RcxWindow
 {
  U32 w;
  U32 h;
 } window;
 
 struct RcxCamera
 {
  F32 x;
  F32 y;
 } camera;
 
 struct RcxFramebuffers
 {
  Framebuffer blur_a;
  Framebuffer blur_b;
  
  Framebuffer bloom_blur_a;
  Framebuffer bloom_blur_b;
  
  Framebuffer post_processing;
 } framebuffers;
 
 Rect mask_stack[64];
 U32 mask_stack_size;
} RenderContext;

internal RenderContext global_rcx = {{0}};

//
// NOTE(tbt): random utility functions
//~

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

void
gl_debug_message_callback(GLenum source​,
                          GLenum type​,
                          GLuint id​,
                          GLenum severity​,
                          GLsizei length​,
                          const GLchar *message,
                          const void *userParam​)
{
 debug_log("\n*** %s ***\n", message);
#if defined(_WIN32) && defined(LUCERNA_DEBUG)
 __debugbreak();
#endif
}


internal Rect
measure_s32(Font *font,
            F32 x, F32 y,
            U32 wrap_width,
            S32 string)
{
 Rect result = rect(x, y, 0, 0);
 
 F32 line_start = x;
 F32 curr_x = x, curr_y = y;
 
 for (I32 i = 0;
      i < string.len;
      ++i)
 {
  if (string.buffer[i] == '\n')
  {
   curr_x = line_start;
   curr_y += font->vertical_advance;
  }
  else if (string.buffer[i] >= font->bake_begin &&
           string.buffer[i] < font->bake_end)
  {
   stbtt_packedchar *b = font->char_data + (string.buffer[i] - font->bake_begin);
   
   if (wrap_width > 0.0f &&
       isspace(string.buffer[i]) &&
       curr_x > line_start + wrap_width)
   {
    curr_x = line_start;
    curr_y += font->vertical_advance;
   }
   else
   {
    result.w += b->xadvance;
    if (curr_x + b->xoff < result.x)
    {
     result.x = curr_x + b->xoff;
    }
    if (curr_x + b->xoff2 > (result.x + result.w))
    {
     result.w = (curr_x + b->xoff2) - result.x;
    }
    if (curr_y + b->yoff < result.y)
    {
     result.y = curr_y + b->yoff;
    }
    if (curr_y + b->yoff2 > (result.y + result.h))
    {
     result.h = (curr_y + b->yoff2) - result.y;
    }
    curr_x += b->xadvance;
   }
  }
 }
 
 return result;
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
 
 //
 // NOTE(tbt): we want to rotate about the quad's origin, not world's, so
 //            first a quad is generated at (0, 0), a rotation matrix is applied
 //            and it is then translated to be at the intended position.
 //
 
 Rect at_origin = rect(0.0f, 0.0f, rectangle.w, rectangle.h);
 
 result = generate_quad(at_origin, colour, sub_texture);
 
 
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
  vertices[i].x += rectangle.x;
  vertices[i].y += rectangle.y;
 }
 
 return result;
}

//
// NOTE(tbt): x-macro shader compilation (should probably have used LCDDL)
//~

#define renderer_compile_and_link_fragment_shader(_name, _vertex_shader)                                     \
shader_src = cstring_from_s8(&global_static_memory,                                                         \
platform_read_entire_file_p(&global_static_memory,                             \
s8("../assets/shaders/" #_name ".frag"))); \
fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);                                                       \
glShaderSource(fragment_shader, 1, &shader_src, NULL);                                                      \
glCompileShader(fragment_shader);                                                                         \
glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);                                               \
if (status == GL_FALSE)                                                                                     \
{                                                                                                           \
I8 msg[SHADER_INFO_LOG_MAX_LEN];                                                                           \
glGetShaderInfoLog(fragment_shader,                                                                      \
SHADER_INFO_LOG_MAX_LEN,                                                              \
NULL,                                                                                 \
msg);                                                                                 \
debug_log(#_name " fragment shader compilation failure. '%s'\n", msg);                                     \
exit(-1);                                                                                                  \
}                                                                                                           \
glAttachShader(global_rcx.shaders. ## _name, fragment_shader);                                            \
glLinkProgram(global_rcx.shaders. ## _name);                                                              \
glGetProgramiv(global_rcx.shaders. ## _name, GL_LINK_STATUS, &status);                                    \
if (status == GL_FALSE)                                                                                     \
{                                                                                                           \
I8 msg[SHADER_INFO_LOG_MAX_LEN];                                                                           \
glGetShaderInfoLog(global_rcx.shaders. ## _name,                                                         \
SHADER_INFO_LOG_MAX_LEN,                                                              \
NULL,                                                                                 \
msg);                                                                                 \
glDeleteProgram(global_rcx.shaders. ## _name);                                                           \
glDeleteShader(_vertex_shader);                                                                          \
glDeleteShader(fragment_shader);                                                                         \
debug_log(#_name " shader link failure. '%s'\n", msg);                                                     \
exit(-1);                                                                                                  \
}                                                                                                           \
glDetachShader(global_rcx.shaders. ## _name, _vertex_shader);                                             \
glDetachShader(global_rcx.shaders. ## _name, fragment_shader);                                            \
glDeleteShader(fragment_shader);                                                                          \
global_rcx.shaders.last_modified. ## _name = platform_get_file_modified_time_p(s8("../assets/shaders/" #_name ".frag"))

//
// NOTE(tbt): get the location of all uniforms and save them in a global variable
//~

internal void
cache_uniform_locations(void)
{
 global_rcx.uniform_locations.texture.projection_matrix = glGetUniformLocation(global_rcx.shaders.texture, "u_projection_matrix");
 
 global_rcx.uniform_locations.text.projection_matrix = glGetUniformLocation(global_rcx.shaders.text, "u_projection_matrix");
 
 global_rcx.uniform_locations.blur.direction = glGetUniformLocation(global_rcx.shaders.blur, "u_direction");
 
 global_rcx.uniform_locations.post_processing.time = glGetUniformLocation(global_rcx.shaders.post_processing, "u_time");
 global_rcx.uniform_locations.post_processing.exposure = glGetUniformLocation(global_rcx.shaders.post_processing, "u_exposure");
 global_rcx.uniform_locations.post_processing.screen_texture = glGetUniformLocation(global_rcx.shaders.post_processing, "u_screen_texture");
 global_rcx.uniform_locations.post_processing.blur_texture = glGetUniformLocation(global_rcx.shaders.post_processing, "u_blur_texture");
 
 global_rcx.uniform_locations.memory_post_processing.time = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_time");
 global_rcx.uniform_locations.memory_post_processing.exposure = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_exposure");
 global_rcx.uniform_locations.memory_post_processing.screen_texture = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_screen_texture");
 global_rcx.uniform_locations.memory_post_processing.blur_texture = glGetUniformLocation(global_rcx.shaders.memory_post_processing, "u_blur_texture");
}

internal void renderer_flush_message_queue(void);

// NOTE(tbt): I have no idea why I chose this preprocessor monstrosity over a simple LCDDL metaprogram, but it works
internal void
hot_reload_shaders(F64 frametime_in_s)
{
 persist F64 time = 0.0;
 time += frametime_in_s;
 
 I32 status;
 U32 fragment_shader;
 const GLchar *shader_src;
 
 F64 refresh_time = 2.0;
 if (time > refresh_time)
 {
#define shader(_name, _vertex_shader_name) \
{\
U64 last_modified = platform_get_file_modified_time_p(s8("../assets/shaders/" #_name ".frag"));\
if (last_modified > global_rcx.shaders.last_modified. ## _name) \
{\
renderer_flush_message_queue();\
global_rcx.shaders.last_modified. ## _name = last_modified;\
debug_log("hot reloading " #_name " shader\n");\
shader_src = cstring_from_s8(&global_temp_memory, platform_read_entire_file_p(&global_temp_memory, s8("../assets/shaders/" #_vertex_shader_name ".vert")));\
U32 _vertex_shader_name ## _vertex_shader = glCreateShader(GL_VERTEX_SHADER);\
glShaderSource(_vertex_shader_name ## _vertex_shader, 1, &shader_src, NULL);\
glCompileShader(_vertex_shader_name ## _vertex_shader);\
glGetShaderiv(_vertex_shader_name ## _vertex_shader, GL_COMPILE_STATUS, &status);\
if (status == GL_FALSE)\
{\
I8 msg[SHADER_INFO_LOG_MAX_LEN];\
glGetShaderInfoLog(_vertex_shader_name ## _vertex_shader, SHADER_INFO_LOG_MAX_LEN, NULL, msg);\
glDeleteShader(_vertex_shader_name ## _vertex_shader);\
debug_log("vertex shader compilation failure. '%s'\n", msg);\
exit(-1);\
}\
glAttachShader(global_rcx.shaders. ## _name, _vertex_shader_name ## _vertex_shader);\
renderer_compile_and_link_fragment_shader(_name, _vertex_shader_name ## _vertex_shader);\
glDeleteShader(_vertex_shader_name ## _vertex_shader);\
cache_uniform_locations();\
}\
}
#include "shader_list.h"
  
 }
}

internal void
initialise_renderer(void)
{
 I32 i;
 I32 offset;
 I32 indices[BATCH_SIZE * 6];
 void *render_queue_backing_memory;
 
 //
 // NOTE(tbt): general OpenGL setup
 //~
 
#ifdef LUCERNA_DEBUG
 glDebugMessageCallback(gl_debug_message_callback, NULL);
#endif
 
 glGenVertexArrays(1, &global_rcx.vao);
 glBindVertexArray(global_rcx.vao);
 
 glGenBuffers(1, &global_rcx.vbo);
 glBindBuffer(GL_ARRAY_BUFFER, global_rcx.vbo);
 
 glEnableVertexAttribArray(0);
 glVertexAttribPointer(0,
                       2,
                       GL_FLOAT,
                       GL_FALSE,
                       sizeof(Vertex),
                       NULL);
 
 glEnableVertexAttribArray(1);
 glVertexAttribPointer(1,
                       4,
                       GL_FLOAT,
                       GL_FALSE,
                       sizeof(Vertex),
                       (const void *)(2 * sizeof(F32)));
 
 glEnableVertexAttribArray(2);
 glVertexAttribPointer(2,
                       2,
                       GL_FLOAT,
                       GL_FALSE,
                       sizeof(Vertex),
                       (const void *)(6 * sizeof(F32)));
 
 glGenBuffers(1, &global_rcx.ibo);
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
 
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_rcx.ibo);
 glBufferData(GL_ELEMENT_ARRAY_BUFFER,
              BATCH_SIZE * 6 * sizeof(indices[0]),
              indices,
              GL_STATIC_DRAW);
 
 glBufferData(GL_ARRAY_BUFFER,
              BATCH_SIZE * 4 * sizeof(Vertex),
              NULL,
              GL_DYNAMIC_DRAW);
 
 glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
 
 glEnable(GL_BLEND);
 glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
 
 glEnable(GL_SCISSOR_TEST);
 
 //
 // NOTE(tbt): setup 1x1 white texture for rendering flat colours
 //~
 
 U32 flat_colour_texture_data = 0xffffffff;
 
 glGenTextures(1, &global_rcx.flat_colour_texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.flat_colour_texture);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              1,
              1,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              &flat_colour_texture_data);
 
 //
 // NOTE(tbt): shader compilation
 //~
 
 I32 status;
 global_rcx.shaders.texture = glCreateProgram();
 global_rcx.shaders.blur = glCreateProgram();
 global_rcx.shaders.text = glCreateProgram();
 global_rcx.shaders.post_processing = glCreateProgram();
 global_rcx.shaders.memory_post_processing = glCreateProgram();
 const GLchar *shader_src;
 ShaderID default_vertex_shader, fullscreen_vertex_shader;
 ShaderID fragment_shader;
 
 arena_temporary_memory(&global_temp_memory)
 {
  // NOTE(tbt): compile the default vertex shader
  shader_src = cstring_from_s8(&global_temp_memory,
                               platform_read_entire_file_p(&global_temp_memory,
                                                           s8("../assets/shaders/default.vert")));
  
  default_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(default_vertex_shader, 1, &shader_src, NULL);
  glCompileShader(default_vertex_shader);
  
  // NOTE(tbt): check for default vertex shader compilation errors
  glGetShaderiv(default_vertex_shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
  {
   I8 msg[SHADER_INFO_LOG_MAX_LEN];
   
   glGetShaderInfoLog(default_vertex_shader,
                      SHADER_INFO_LOG_MAX_LEN,
                      NULL,
                      msg);
   glDeleteShader(default_vertex_shader);
   
   debug_log("default vertex shader compilation failure. '%s'\n", msg);
   exit(-1);
  }
  
  debug_log("successfully compiled default vertex shader\n");
  
  // NOTE(tbt): compile the fullscreen vertex shader
  shader_src = cstring_from_s8(&global_temp_memory,
                               platform_read_entire_file_p(&global_temp_memory,
                                                           s8("../assets/shaders/fullscreen.vert")));
  
  fullscreen_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(fullscreen_vertex_shader, 1, &shader_src, NULL);
  glCompileShader(fullscreen_vertex_shader);
  
  // NOTE(tbt): check for fullscreen vertex shader compilation errors
  glGetShaderiv(fullscreen_vertex_shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
  {
   I8 msg[SHADER_INFO_LOG_MAX_LEN];
   
   glGetShaderInfoLog(fullscreen_vertex_shader,
                      SHADER_INFO_LOG_MAX_LEN,
                      NULL,
                      msg);
   glDeleteShader(fullscreen_vertex_shader);
   
   debug_log("fullscreen vertex shader compilation failure. '%s'\n", msg);
   exit(-1);
  }
  
  debug_log("successfully compiled fullscreen vertex shader\n");
  
  // NOTE(tbt): attach vertex shaders to shader programs
#define shader(_name, _vertex_shader_name) glAttachShader(global_rcx.shaders. ## _name, _vertex_shader_name ## _vertex_shader);
#include "shader_list.h"
  
  // NOTE(tbt): compile and line fragment shaders
#define shader(_name, _vertex_shader_name) renderer_compile_and_link_fragment_shader(_name, _vertex_shader_name ## _vertex_shader);
#include "shader_list.h"
  
  // NOTE(tbt): cleanup shader stuff
  glDeleteShader(default_vertex_shader);
  glDeleteShader(fullscreen_vertex_shader);
 }
 
 cache_uniform_locations();
 
 // NOTE(tbt): reset currently bound shader
 glUseProgram(global_rcx.shaders.current);
 
 
 //
 // NOTE(tbt): setup framebuffers
 //~
 
 // NOTE(tbt): framebuffer for first blur pass
 glGenFramebuffers(1, &global_rcx.framebuffers.blur_a.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
 
 glGenTextures(1, &global_rcx.framebuffers.blur_a.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_a.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.blur_a.texture,
                        0);
 
 // NOTE(tbt): framebuffer for second blur pass
 glGenFramebuffers(1, &global_rcx.framebuffers.blur_b.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_b.target);
 
 glGenTextures(1, &global_rcx.framebuffers.blur_b.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_b.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.blur_b.texture,
                        0);
 
 // NOTE(tbt): framebuffer for first blur pass for bloom
 glGenFramebuffers(1, &global_rcx.framebuffers.bloom_blur_a.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.bloom_blur_a.target);
 
 glGenTextures(1, &global_rcx.framebuffers.bloom_blur_a.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.bloom_blur_a.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.bloom_blur_a.texture,
                        0);
 
 // NOTE(tbt): framebuffer for second blur pass for bloom
 glGenFramebuffers(1, &global_rcx.framebuffers.bloom_blur_b.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.bloom_blur_b.target);
 
 glGenTextures(1, &global_rcx.framebuffers.bloom_blur_b.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.bloom_blur_b.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              BLUR_TEXTURE_W,
              BLUR_TEXTURE_H,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.bloom_blur_b.texture,
                        0);
 
 // NOTE(tbt): framebuffer for post processing
 glGenFramebuffers(1, &global_rcx.framebuffers.post_processing.target);
 glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.post_processing.target);
 
 glGenTextures(1, &global_rcx.framebuffers.post_processing.texture);
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.post_processing.texture);
 
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              DEFAULT_WINDOW_WIDTH,
              DEFAULT_WINDOW_HEIGHT,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 
 glFramebufferTexture2D(GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        global_rcx.framebuffers.post_processing.texture,
                        0);
 
 glBindTexture(GL_TEXTURE_2D, 0);
 glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//
// NOTE(tbt): controls for mask stack
//~

#define mask_rectangle(_mask) defer_loop(renderer_push_mask(_mask), renderer_pop_mask())

internal void
renderer_push_mask(Rect mask)
{
 if (global_rcx.mask_stack_size < array_count(global_rcx.mask_stack))
 {
  global_rcx.mask_stack_size += 1;
  global_rcx.mask_stack[global_rcx.mask_stack_size] = mask;
 }
}

internal void
renderer_pop_mask(void)
{
 if (global_rcx.mask_stack_size)
 {
  global_rcx.mask_stack_size -= 1;
 }
}

internal Rect
renderer_current_mask(void)
{
 return global_rcx.mask_stack[global_rcx.mask_stack_size];
}

//
// NOTE(tbt): controls for render message queue
//~

internal void
renderer_enqueue_message(RenderMessage message)
{
 RenderMessage *queued_message =
  arena_push(&global_frame_memory,
             sizeof(*queued_message));
 
 *queued_message = message;
 
 // NOTE(tbt): copy mask from stack
 {
  queued_message->mask = global_rcx.mask_stack[global_rcx.mask_stack_size];
  
  if (queued_message->mask.x < 0.0f)
  {
   queued_message->mask.w -= queued_message->mask.x;
   queued_message->mask.x = 0.0f;
  }
  if (queued_message->mask.y < 0.0f)
  {
   queued_message->mask.h -= queued_message->mask.y;
   queued_message->mask.y = 0.0f;
  }
  queued_message->mask.w = max_f(queued_message->mask.w, 0.0f);
  queued_message->mask.h = max_f(queued_message->mask.h, 0.0f);
 }
 
 if (global_rcx.message_queue.end)
 {
  global_rcx.message_queue.end->next = queued_message;
 }
 else
 {
  global_rcx.message_queue.start = queued_message;
 }
 
 global_rcx.message_queue.end = queued_message;
}

internal B32
renderer_dequeue_message(RenderMessage *result)
{
 
 if (!global_rcx.message_queue.start) { return false; }
 
 *result = *global_rcx.message_queue.start;
 global_rcx.message_queue.start = global_rcx.message_queue.start->next;
 
 return true;
}

//
// NOTE(tbt): 'public' API for corresponding render messages
//~

internal void
draw_rotated_sub_texture(Rect rectangle,
                         F32 angle,
                         Colour colour,
                         Texture *texture,
                         SubTexture sub_texture,
                         U8 sort,
                         F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_rectangle;
 message.rectangle = rectangle;
 message.angle = angle;
 message.colour = colour;
 message.texture = texture->id;
 message.sub_texture = sub_texture;
 message.projection_matrix = projection_matrix;
 message.sort = sort;
 
 renderer_enqueue_message(message);
}

internal void
draw_sub_texture(Rect rectangle,
                 Colour colour,
                 Texture *texture,
                 SubTexture sub_texture,
                 U8 sort,
                 F32 *projection_matrix)
{
 draw_rotated_sub_texture(rectangle, 0.0f, colour, texture, sub_texture, sort, projection_matrix);
}

internal void
fill_rotated_rectangle(Rect rectangle,
                       F32 angle,
                       Colour colour,
                       U8 sort,
                       F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_rectangle;
 message.rectangle = rectangle;
 message.angle = angle;
 message.colour = colour;
 message.texture = global_rcx.flat_colour_texture;
 message.sub_texture = ENTIRE_TEXTURE;
 message.projection_matrix = projection_matrix;
 message.sort = sort;
 
 renderer_enqueue_message(message);
}

internal void
fill_rectangle(Rect rectangle,
               Colour colour,
               U8 sort,
               F32 *projection_matrix)
{
 fill_rotated_rectangle(rectangle, 0.0f, colour, sort, projection_matrix);
}

internal void
stroke_rectangle(Rect rectangle,
                 Colour colour,
                 F32 stroke_width,
                 U8 sort,
                 F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_stroke_rectangle;
 message.rectangle = rectangle;
 message.colour = colour;
 message.stroke_width = stroke_width;
 message.sort = sort;
 message.projection_matrix = projection_matrix;
 
 renderer_enqueue_message(message);
}

internal void
draw_s8(Font *font,
        F32 x, F32 y,
        U32 wrap_width,
        Colour colour,
        S8 string,
        U8 sort,
        F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_text;
 message.font = font;
 message.colour = colour;
 message.rectangle.x = x;
 message.rectangle.y = y;
 message.rectangle.w = wrap_width;
 message.string = string;
 message.sort = sort;
 message.projection_matrix = projection_matrix;
 
 renderer_enqueue_message(message);
}

internal void
draw_gradient(Rect rectangle,
              Gradient gradient,
              U8 sort,
              F32 *projection_matrix)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_draw_gradient;
 message.rectangle = rectangle;
 message.projection_matrix = projection_matrix;
 message.sort = sort;
 message.gradient = gradient;
 
 renderer_enqueue_message(message);
}

internal void
blur_screen_region(Rect region,
                   U32 strength,
                   U8 sort)
{
 RenderMessage message = {0};
 
 message.kind = RENDER_MESSAGE_blur_screen_region;
 message.rectangle = region;
 message.strength = strength;
 message.sort = sort;
 
 renderer_enqueue_message(message);
}

internal void
do_post_processing(F32 exposure,
                   PostProcessingKind kind,
                   U8 sort)
{
 RenderMessage message = {0};
 
 message.sort = sort;
 message.kind = RENDER_MESSAGE_do_post_processing;
 message.exposure = exposure;
 message.post_processing_kind = kind;
 
 renderer_enqueue_message(message);
}

//
// NOTE(tbt): more 'public' API that does not necessarily map to a render message
//~

#define MOUSE_WORLD_X ((((F32)input->mouse_x / (F32)global_renderer_window_w) - 0.5f) * SCREEN_W_IN_WORLD_UNITS + global_camera_x)
#define MOUSE_WORLD_Y ((((F32)input->mouse_y / (F32)global_renderer_window_h) - 0.5f) * (SCREEN_W_IN_WORLD_UNITS * ((F32)global_renderer_window_h / (F32)global_renderer_window_w)) + global_camera_y)

internal void
renderer_recalculate_world_projection_matrix(void)
{
 persist F32 half_screen_width_in_world_units = (F32)(SCREEN_W_IN_WORLD_UNITS >> 1);
 
 F32 aspect = (F32)global_rcx.window.h / (F32)global_rcx.window.w;
 
 generate_orthographic_projection_matrix(global_projection_matrix,
                                         global_rcx.camera.x - half_screen_width_in_world_units,
                                         global_rcx.camera.x + half_screen_width_in_world_units,
                                         global_rcx.camera.y - half_screen_width_in_world_units * aspect,
                                         global_rcx.camera.y + half_screen_width_in_world_units * aspect);
}

internal void
set_renderer_window_size(U32 w, U32 h)
{
 global_rcx.window.w = w;
 global_rcx.window.h = h;
 
 global_rcx.mask_stack[0] = rect(0.0f, 0.0f, w, h);
 
 glViewport(0, 0, w, h);
 
 glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.post_processing.texture);
 glTexImage2D(GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              w,
              h,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              NULL);
 
 glBindTexture(GL_TEXTURE_2D, global_currently_bound_texture);
 
 generate_orthographic_projection_matrix(global_ui_projection_matrix,
                                         0, w,
                                         0, h);
 
 renderer_recalculate_world_projection_matrix();
 
 glUseProgram(global_rcx.shaders.current);
}

internal void
set_camera_position(F32 x,
                    F32 y)
{
 global_rcx.camera.x = x;
 global_rcx.camera.y = y;
 
 renderer_recalculate_world_projection_matrix();
}

//
// NOTE(tbt): actually drawing stuff
//~

internal void
renderer_flush_batch(RenderBatch *batch)
{
 if (!batch->in_use) return;
 
 glScissor(batch->mask.x,
           global_rcx.window.h - batch->mask.y - batch->mask.h,
           batch->mask.w,
           batch->mask.h);
 
 if (global_rcx.shaders.current != batch->shader)
 {
  glUseProgram(batch->shader);
  global_rcx.shaders.current = batch->shader;
 }
 
 if (global_currently_bound_texture != batch->texture)
 {
  glBindTexture(GL_TEXTURE_2D, batch->texture);
  global_currently_bound_texture = batch->texture;
 }
 
 if (batch->shader == global_rcx.shaders.texture)
 {
  glUniformMatrix4fv(global_rcx.uniform_locations.texture.projection_matrix,
                     1,
                     GL_FALSE,
                     batch->projection_matrix);
 }
 else if (batch->shader == global_rcx.shaders.text)
 {
  glUniformMatrix4fv(global_rcx.uniform_locations.text.projection_matrix,
                     1,
                     GL_FALSE,
                     batch->projection_matrix);
 }
 
 glBufferData(GL_ARRAY_BUFFER,
              batch->quad_count * sizeof(Quad),
              batch->buffer,
              GL_DYNAMIC_DRAW);
 
 glDrawElements(GL_TRIANGLES,
                batch->quad_count * 6,
                GL_UNSIGNED_INT,
                NULL);
 
 batch->quad_count = 0;
 batch->texture = 0;
 batch->shader = 0;
 batch->projection_matrix = NULL;
 batch->in_use = false;
}

internal void
renderer_flush_message_queue()
{
 RenderMessage message;
 
 RenderBatch batch;
 batch.quad_count = 0;
 batch.texture = 0;
 batch.shader = 0;
 batch.in_use = false;
 
 glEnable(GL_SCISSOR_TEST);
 //
 // NOTE(tbt): sort message queue
 //~
 {
  // NOTE(tbt): form a bucket for each depth
  //-
  RenderMessage *heads[256] = {0};
  RenderMessage *tails[256] = {0};
  
  RenderMessage *next = NULL;
  
  for (RenderMessage *node = global_rcx.message_queue.start;
       node;
       node = next)
  {
   if (tails[node->sort])
   {
    tails[node->sort]->next = node;
   }
   else
   {
    heads[node->sort] = node;
   }
   tails[node->sort] = node;
   next = node->next;
   node->next = NULL;
  }
  
  // NOTE(tbt): reconstruct queue in correct order
  //-
  global_rcx.message_queue.start = NULL;
  global_rcx.message_queue.end = NULL;
  
  for (I32 i = 0;
       i < 256;
       ++i)
  {
   if (!heads[i]) { continue; }
   
   if (global_rcx.message_queue.end)
   {
    global_rcx.message_queue.end->next = heads[i];
   }
   else
   {
    global_rcx.message_queue.start = heads[i];
   }
   global_rcx.message_queue.end = tails[i];
  }
 }
 
 //
 // NOTE(tbt): main render message processing loop
 //~
 
 while (renderer_dequeue_message(&message))
 {
  switch (message.kind)
  {
   //~
   case RENDER_MESSAGE_draw_rectangle:
   {
    if (batch.texture != message.texture ||
        batch.shader != global_rcx.shaders.texture ||
        batch.quad_count >= BATCH_SIZE ||
        batch.projection_matrix != message.projection_matrix ||
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.texture;
     batch.texture = message.texture;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    batch.buffer[batch.quad_count] = generate_rotated_quad(message.rectangle,
                                                           message.angle,
                                                           message.colour,
                                                           message.sub_texture);
    batch.quad_count += 1;
    
    break;
   }
   
   //~
   case RENDER_MESSAGE_stroke_rectangle:
   {
    Rect top, bottom, left, right;
    F32 stroke_width;
    
    if (batch.texture != global_rcx.flat_colour_texture ||
        batch.shader != global_rcx.shaders.texture||
        batch.projection_matrix != message.projection_matrix ||
        batch.quad_count >= BATCH_SIZE - 4 || // NOTE(tbt): 4 quads to stroke a rectangle
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.texture;
     batch.texture = global_rcx.flat_colour_texture;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    stroke_width = message.stroke_width;
    
    top = rect(message.rectangle.x,
               message.rectangle.y,
               message.rectangle.w,
               stroke_width);
    
    bottom = rect(message.rectangle.x,
                  message.rectangle.y +
                  message.rectangle.h -
                  stroke_width,
                  message.rectangle.w,
                  stroke_width);
    
    left = rect(message.rectangle.x,
                message.rectangle.y +
                stroke_width,
                stroke_width,
                message.rectangle.h -
                stroke_width * 2);
    
    right = rect(message.rectangle.x +
                 message.rectangle.w -
                 stroke_width,
                 message.rectangle.y +
                 stroke_width,
                 stroke_width,
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
   
   //~
   case RENDER_MESSAGE_draw_text:
   {
    F32 line_start = message.rectangle.x;
    F32 x = message.rectangle.x;
    F32 y = message.rectangle.y;
    U32 wrap_width = message.rectangle.w;
    
    if (batch.texture != message.font->texture.id ||
        batch.shader != global_rcx.shaders.text ||
        batch.projection_matrix != message.projection_matrix ||
        batch.quad_count >= BATCH_SIZE ||
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.text;
     batch.texture = message.font->texture.id;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    U32 font_bake_begin = message.font->bake_begin;
    U32 font_bake_end = message.font->bake_end;
    
    U32 i = 0;
    for (UTF8Consume consume = consume_utf8_from_string(message.string, i);
         i < message.string.size;
         i += consume.advance, consume = consume_utf8_from_string(message.string, i))
    {
     if (consume.codepoint == '\n')
     {
      x = line_start;
      y += message.font->vertical_advance;
     }
     else if (consume.codepoint >= font_bake_begin &&
              consume.codepoint < font_bake_end)
     {
      stbtt_aligned_quad q;
      Rect rectangle;
      SubTexture sub_texture;
      
      stbtt_GetPackedQuad(message.font->char_data,
                          message.font->texture.width,
                          message.font->texture.height,
                          consume.codepoint - font_bake_begin,
                          &x, &y,
                          &q,
                          false);
      
      sub_texture.min_x = q.s0;
      sub_texture.min_y = q.t0;
      sub_texture.max_x = q.s1;
      sub_texture.max_y = q.t1;
      
      rectangle = rect(q.x0, q.y0,
                       q.x1 - q.x0,
                       q.y1 - q.y0);
      
      if (batch.quad_count >= BATCH_SIZE)
      {
       renderer_flush_batch(&batch);
       
       batch.shader = global_rcx.shaders.text;
       batch.texture = message.font->texture.id;
       batch.projection_matrix = message.projection_matrix;
       batch.mask = message.mask;
      }
      else
      {
       batch.buffer[batch.quad_count++] =
        generate_quad(rectangle,
                      message.colour,
                      sub_texture);
      }
      if (wrap_width > 0.0f &&
          isspace(consume.codepoint))
      {
       if (x > line_start + wrap_width)
       {
        x = line_start;
        y += message.font->vertical_advance;
       }
      }
     }
    }
    
    break;
   }
   
   //~
   case RENDER_MESSAGE_blur_screen_region:
   {
    renderer_flush_batch(&batch);
    
    glDisable(GL_SCISSOR_TEST);
    
    // NOTE(tbt): blit screen to framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
    
    glViewport(0, 0, BLUR_TEXTURE_W, BLUR_TEXTURE_H);
    
    glBlitFramebuffer(0,
                      0,
                      global_rcx.window.w,
                      global_rcx.window.h,
                      0,
                      0,
                      BLUR_TEXTURE_W,
                      BLUR_TEXTURE_H,
                      GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    
    glUseProgram(global_rcx.shaders.blur);
    global_rcx.shaders.current = global_rcx.shaders.blur;
    
    for (I32 i = 0;
         i < message.strength;
         ++i)
    {
     // NOTE(tbt): apply first (horizontal) blur pass
     glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_b.target);
     glUniform2f(global_rcx.uniform_locations.blur.direction, 1.0f, 0.0f);
     glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_a.texture);
     glDrawArrays(GL_TRIANGLES, 0, 6);
     
     // NOTE(tbt): apply second (vertical) blur pass
     glBindFramebuffer(GL_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
     glUniform2f(global_rcx.uniform_locations.blur.direction, 0.0f, 1.0f);
     glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.blur_b.texture);
     glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    // NOTE(tbt): blit desired region back to screen
    glBindFramebuffer(GL_READ_FRAMEBUFFER, global_rcx.framebuffers.blur_a.target);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    
    glViewport(0,
               0,
               global_rcx.window.w,
               global_rcx.window.h);
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(message.mask.x,
              global_rcx.window.h - message.mask.y - message.mask.h,
              message.mask.w,
              message.mask.h);
    
    F32 x0 = message.rectangle.x;
    F32 y0 = global_rcx.window.h - message.rectangle.y;
    F32 x1 = message.rectangle.x + message.rectangle.w;
    F32 y1 = global_rcx.window.h - message.rectangle.y - message.rectangle.h;
    
    F32 x_scale = (F32)BLUR_TEXTURE_W / (F32)global_rcx.window.w;
    F32 y_scale = (F32)BLUR_TEXTURE_H / (F32)global_rcx.window.h;
    glBlitFramebuffer(x0 * x_scale,
                      y0 * y_scale,
                      x1 * x_scale,
                      y1 * y_scale,
                      x0, y0, x1, y1,
                      GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    
    // NOTE(tbt): reset current texture
    glBindTexture(GL_TEXTURE_2D, global_currently_bound_texture);
    
    break;
   }
   
   //~
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
    
    if (batch.texture != global_rcx.flat_colour_texture ||
        batch.shader != global_rcx.shaders.texture ||
        batch.quad_count >= BATCH_SIZE ||
        batch.projection_matrix != message.projection_matrix ||
        !rect_match(batch.mask, message.mask) ||
        !(batch.in_use))
    {
     renderer_flush_batch(&batch);
     
     batch.shader = global_rcx.shaders.texture;
     batch.texture = global_rcx.flat_colour_texture;
     batch.projection_matrix = message.projection_matrix;
     batch.mask = message.mask;
    }
    
    batch.in_use = true;
    
    batch.buffer[batch.quad_count++] = quad;
    
    break;
   }
   
   //~
   case RENDER_MESSAGE_do_post_processing:
   {
    renderer_flush_batch(&batch);
    
    glDisable(GL_SCISSOR_TEST);
    
    //-NOTE(tbt): setup for relevant post processing kind
    
    U32 post_shader;
    Framebuffer *blur_framebuffer_1;
    Framebuffer *blur_framebuffer_2;
    struct RcxPostProcessingUniformLocations *uniforms;
    
    if (message.post_processing_kind == POST_PROCESSING_KIND_memory)
    {
     post_shader = global_rcx.shaders.memory_post_processing;
     uniforms = &global_rcx.uniform_locations.memory_post_processing;
     blur_framebuffer_1 = &global_rcx.framebuffers.bloom_blur_a;
     blur_framebuffer_2 = &global_rcx.framebuffers.bloom_blur_b;
    }
    else if (message.post_processing_kind == POST_PROCESSING_KIND_world)
    {
     post_shader = global_rcx.shaders.post_processing;
     uniforms = &global_rcx.uniform_locations.post_processing;
     blur_framebuffer_1 = &global_rcx.framebuffers.blur_a;
     blur_framebuffer_2 = &global_rcx.framebuffers.blur_b;
    }
    else
    {
     break;
    }
    
    //-NOTE(tbt): blit screen to framebuffers
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, global_rcx.framebuffers.post_processing.target);
    {
     glBlitFramebuffer(0, 0, global_rcx.window.w, global_rcx.window.h,
                       0, 0, global_rcx.window.w, global_rcx.window.h,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blur_framebuffer_1->target);
    {
     glViewport(0, 0, BLUR_TEXTURE_W, BLUR_TEXTURE_H);
     glBlitFramebuffer(0, 0, global_rcx.window.w, global_rcx.window.h,
                       0, 0, BLUR_TEXTURE_W, BLUR_TEXTURE_H,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    
    //-NOTE(tbt): blur for bloom
    glUseProgram(global_rcx.shaders.blur);
    
    // NOTE(tbt): apply first (horizontal) blur pass
    glBindFramebuffer(GL_FRAMEBUFFER, blur_framebuffer_2->target);
    glUniform2f(global_rcx.uniform_locations.blur.direction, 1.0f, 0.0f);
    glBindTexture(GL_TEXTURE_2D, blur_framebuffer_1->texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // NOTE(tbt): apply second (vertical) blur pass
    glBindFramebuffer(GL_FRAMEBUFFER, blur_framebuffer_1->target);
    glUniform2f(global_rcx.uniform_locations.blur.direction, 0.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, blur_framebuffer_2->texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    //-NOTE(tbt): blend back to screen
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    
    glUseProgram(post_shader);
    glUniform1f(uniforms->time, (F32)global_time);
    glUniform1f(uniforms->exposure, message.exposure);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blur_framebuffer_1->texture);
    glUniform1i(uniforms->blur_texture, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, global_rcx.framebuffers.post_processing.texture);
    glUniform1i(uniforms->screen_texture, 1);
    
    glViewport(0, 0, global_rcx.window.w, global_rcx.window.h);
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(message.mask.x,
              global_rcx.window.h - message.mask.y - message.mask.h,
              message.mask.w,
              message.mask.h);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, global_currently_bound_texture);
    glUseProgram(global_rcx.shaders.current);
    
    break;
   }
  }
 }
 
 renderer_flush_batch(&batch);
 
 global_rcx.message_queue.start = NULL;
 global_rcx.message_queue.end = NULL;
 glDisable(GL_SCISSOR_TEST);
}
