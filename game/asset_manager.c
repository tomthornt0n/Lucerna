internal void *
malloc_for_stb(U64 size)
{
 return arena_allocate(&global_level_memory, size);
}

internal void *
realloc_for_stb(void *p,
                U64 old_size,
                U64 new_size)
{
 void *result = arena_allocate(&global_level_memory, new_size);
 memcpy(result, p, old_size);
 return result;
}

internal void
free_for_stb(void *p)
{
 return;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz) malloc_for_stb(sz)
#define STBI_REALLOC_SIZED(p, old_size, new_size) realloc_for_stb(p, old_size, new_size)
#define STBI_FREE(p) free_for_stb(p)
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void)(u),malloc_for_stb(x))
#define STBTT_free(x, u) ((void)(u),free_for_stb(x))
#include "stb_truetype.h"

typedef U32 TextureID;

#define DUMMY_TEXTURE ((Texture){ global_flat_colour_texture, 1, 1 })

typedef struct
{
 TextureID id;
 I32 width, height;
} Texture;

internal TextureID global_currently_bound_texture = 0;

#define ENTIRE_TEXTURE ((SubTexture){ 0.0f, 0.0f, 1.0f, 1.0f })
typedef struct
{
 F32 min_x, min_y;
 F32 max_x, max_y;
} SubTexture;

#define FONT_BAKE_BEGIN 32
#define FONT_BAKE_END 255

typedef struct
{
 Texture texture;
 stbtt_bakedchar char_data[FONT_BAKE_END - FONT_BAKE_BEGIN];
 U32 size;
 F32 estimate_char_width;
} Font;

internal B32
load_texture(OpenGLFunctions *gl,
             S8 path,
             Texture *result)
{
 U8 *pixels;
 
 I8 path_cstr[1024] = {0};
 snprintf(path_cstr, 1024, "%.*s", (I32)path.len, path.buffer);
 
 TextureID texture_id;
 I32 width, height;
 
 arena_temporary_memory(&global_level_memory)
 {
  pixels = stbi_load(path_cstr,
                     &width,
                     &height,
                     NULL, 4);
  
  if (!pixels)
  {
   debug_log("Error loading texture '%s'\n", path_cstr);
   _temporary_memory_end(&global_level_memory);
   memset(result, 0, sizeof(*result));
   return false;
  }
  
  gl->GenTextures(1, &texture_id);
  gl->BindTexture(GL_TEXTURE_2D, texture_id);
  
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  
  gl->TexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels);
 }
 
 result->id = texture_id;
 result->width = width;
 result->height = height;
 
 debug_log("successfully loaded texture: '%s'\n", path_cstr);
 return true;
}

internal void process_render_queue(OpenGLFunctions *gl);

internal void
unload_texture(OpenGLFunctions *gl,
               Texture *texture)
{
 // NOTE(tbt): early process all currently queued render messages in case any of them depend on the texture about to be unloaded
 process_render_queue(gl);
 
 if (global_currently_bound_texture == texture->id)
 {
  global_currently_bound_texture = 0;
 }
 gl->DeleteTextures(1, &texture->id);
 
 texture->id = 0;
 texture->width = 0;
 texture->height = 0;
 
 debug_log("unloaded texture\n");
}

internal SubTexture
sub_texture_from_texture(Texture *texture,
                         F32 x, F32 y,
                         F32 w, F32 h)
{
 SubTexture result;
 
 result.min_x = x / texture->width;
 result.min_y = y / texture->height;
 
 result.max_x = (x + w) / texture->width;
 result.max_y = (y + h) / texture->height;
 
 return result;
}

/*
internal void
slice_animation(OpenGLFunctions *gl,
                SubTexture *result,   // NOTE(tbt): must be an array with `horizontal_count * vertical_count` elements
                Asset *texture,       // NOTE(tbt): the texture to slice from
                F32 x, F32 y,         // NOTE(tbt): the coordinate in pixels of the top left corner of the top left frame
                F32 w, F32 h,         // NOTE(tbt): the size in pixels of each frame
                U32 horizontal_count, // NOTE(tbt): the number of frames horizontally
                U32 vertical_count)   // NOTE(tbt): the number of frames vertically
{
 I32 x_index, y_index;
 I32 index = 0;
 
 for (y_index = 0;
      y_index < vertical_count;
      ++y_index)
 {
  for (x_index = 0;
       x_index < horizontal_count;
       ++x_index)
  {
   result[index++] = sub_texture_from_texture(gl,
                                              texture,
                                              x + x_index * w,
                                              y + y_index * h,
                                              w, h);
  }
 }
}
*/

internal Font *
load_font(OpenGLFunctions *gl,
          S8 path,
          U32 size)
{
 Font *result = arena_allocate(&global_static_memory, sizeof(*result));
 
 U8 pixels[1024 * 1024];
 
 arena_temporary_memory(&global_level_memory)
 {
  S8 file = platform_read_entire_file_p(&global_level_memory, path);
  
  assert(file.buffer);
  
  stbtt_BakeFontBitmap(file.buffer,
                       0,
                       size,
                       pixels,
                       1024, 1024,
                       FONT_BAKE_BEGIN, FONT_BAKE_END - FONT_BAKE_BEGIN,
                       result->char_data);
  
  result->texture.width = 1024;
  result->texture.height = 1024;
  result->size = size;
  
  gl->GenTextures(1, &result->texture.id);
  gl->BindTexture(GL_TEXTURE_2D, result->texture.id);
  
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  
  gl->TexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RED,
                 result->texture.width,
                 result->texture.height,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 pixels);
  
  F32 x = 0.0f, y = 0.0f;
  stbtt_aligned_quad q;
  stbtt_GetBakedQuad(result->char_data,
                     result->texture.width,
                     result->texture.height,
                     'e' - 32,
                     &x,
                     &y,
                     &q);
  
  result->estimate_char_width = x;
 }
 
 debug_log("successfully loaded font: '%.*s'\n", (I32)path.len, path.buffer);
 return result;
}

internal U8 *
_find_wav_sub_chunk(U8 *data,
                    U32 data_size,
                    U8 *name,
                    I32 *size)
{
 U8 *p = data + 12;
 
 next:
 *size = *((U32 *)(p + 4));
 if (memcmp(p, name, 4))
 {
  p += 8 + *size;
  if (p > data + data_size)
  {
   *size = 0;
   return NULL;
  }
  else
  {
   goto next;
  }
 }
 
 return p + 8;
}

internal S8List *
load_dialogue(MemoryArena *memory,
              S8 path)
{
 S8List *result = NULL;
 
 S8 file = platform_read_entire_file_p(memory, path);
 if (file.buffer)
 {
  S8 line = {0};
  line.buffer = file.buffer;
  
  for (U64 i = 0;
       i <= file.len;
       ++i)
  {
   if (file.buffer[i] == '\n')
   {
    result= append_s8_to_list(memory, result, line);
    i += 1;
    line.len = 0;
    line.buffer = file.buffer + i;
   }
   else
   {
    line.len += 1;
   }
  }
  result = append_s8_to_list(memory, result, line);
 }
 
 return result;
}
