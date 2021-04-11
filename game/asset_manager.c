internal void *
malloc_for_stb(U64 size)
{
 return arena_push(&global_temp_memory, size);
}

internal void *
realloc_for_stb(void *p,
                U64 old_size,
                U64 new_size)
{
 void *result = arena_push(&global_temp_memory, new_size);
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
#define STB_RECT_PACK_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void)(u),malloc_for_stb(x))
#define STBTT_free(x, u) ((void)(u),free_for_stb(x))
#include "stb_rect_pack.h"
#include "stb_truetype.h"

typedef U32 TextureID;

#define DUMMY_TEXTURE ((Texture){ global_rcx.flat_colour_texture, 1, 1 })

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

struct Font
{
 Texture texture;
 U32 bake_begin, bake_end;
 stbtt_packedchar *char_data;
 U32 size;
 F32 vertical_advance;
 F32 estimate_char_width; // NOTE(tbt): hacky thing for estimating the correct width for text entries
};

internal S8
path_from_texture_path(MemoryArena *memory,
                       S8 path)
{
 S8List *list = NULL;
 list = push_s8_to_list(&global_frame_memory, list, path);
 list = push_s8_to_list(&global_frame_memory, list, s8("../assets/textures/"));
 return join_s8_list(memory, list);
}

internal S8
path_from_audio_path(MemoryArena *memory,
                     S8 path)
{
 S8List *list = NULL;
 list = push_s8_to_list(&global_frame_memory, list, path);
 list = push_s8_to_list(&global_frame_memory, list, s8("../assets/audio/"));
 return join_s8_list(memory, list);
}

internal S8
path_from_dialogue_path(MemoryArena *memory,
                        S8 path)
{
 S8List *list = NULL;
 list = push_s8_to_list(&global_frame_memory, list, path);
 list = push_s8_to_list(&global_frame_memory, list, s8("/"));
 list = push_s8_to_list(&global_frame_memory, list, s8_from_locale(&global_frame_memory, global_current_locale_config.locale));
 list = push_s8_to_list(&global_frame_memory, list, s8("../assets/dialogue/"));
 return join_s8_list(memory, list);
}

internal B32
load_texture(S8 path,
             Texture *result)
{
 B32 success;
 
 TextureID texture_id;
 I32 width, height;
 
 arena_temporary_memory(&global_temp_memory)
 {
  U8 *pixels = stbi_load(cstring_from_s8(&global_temp_memory, path),
                         &width,
                         &height,
                         NULL, 4);
  
  if (pixels)
  {
   glGenTextures(1, &texture_id);
   glBindTexture(GL_TEXTURE_2D, texture_id);
   
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   
   glTexImage2D(GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                width,
                height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels);
   
   result->id = texture_id;
   result->width = width;
   result->height = height;
   
   debug_log("successfully loaded texture: '%.*s'\n", unravel_s8(path));
   success = true;
  }
  else
  {
   debug_log("error loading texture: '%.*s'\n", unravel_s8(path));
   success = false;
  }
 }
 
 return success;
}

internal void renderer_flush_message_queue(void);

internal void
unload_texture(Texture *texture)
{
 // NOTE(tbt): early process all currently queued render messages in case any of them depend on the texture about to be unloaded
 renderer_flush_message_queue();
 
 if (global_currently_bound_texture == texture->id)
 {
  global_currently_bound_texture = 0;
 }
 glDeleteTextures(1, &texture->id);
 
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

internal Font *
load_font(MemoryArena *memory,
          S8 path,
          U32 size,
          U32 font_bake_begin,
          U32 font_bake_count)
{
 Font *result = NULL;
 
 I32 font_texture_w = 8192;
 I32 font_texture_h = 8192;
 
 arena_temporary_memory(&global_temp_memory)
 {
  S8 file = platform_read_entire_file_p(&global_temp_memory, path);
  if (file.buffer)
  {
   U8 *bitmap = arena_push(&global_temp_memory, font_texture_w * font_texture_h);
   
   stbtt_pack_context packing_context;
   if (stbtt_PackBegin(&packing_context,
                       bitmap,
                       font_texture_w, font_texture_h,
                       0, 1, NULL))
    
   {
    result = arena_push(memory, sizeof(*result));
    
    result->char_data = arena_push(memory, sizeof(result->char_data[0]) * font_bake_count);
    
    stbtt_PackFontRange(&packing_context,
                        file.buffer,
                        0,
                        size,
                        font_bake_begin,
                        font_bake_count,
                        result->char_data);
    
    result->texture.width = font_texture_w;
    result->texture.height = font_texture_h;
    result->bake_begin = font_bake_begin;
    result->bake_end = font_bake_begin + font_bake_count;
    result->size = size;
    
    stbtt_fontinfo font_info = {0};
    if (stbtt_InitFont(&font_info,
                       file.buffer,
                       0))
    {
     F32 scale = stbtt_ScaleForMappingEmToPixels(&font_info, size);
     
     I32 ascent, descent, line_gap;
     stbtt_GetFontVMetrics(&font_info,
                           &ascent,
                           &descent,
                           &line_gap);
     result->vertical_advance = scale * (ascent - descent + line_gap);
     
     I32 advance_width, left_side_bearing;
     stbtt_GetCodepointHMetrics(&font_info,
                                'e',
                                &advance_width,
                                &left_side_bearing);
     result->estimate_char_width = scale * (advance_width - left_side_bearing);
    }
    else
    {
     debug_log("warning loading font - could not get accurate metrics\n");
     result->vertical_advance = size;
     result->estimate_char_width = size;
    }
    
    glGenTextures(1, &result->texture.id);
    glBindTexture(GL_TEXTURE_2D, result->texture.id);
    global_currently_bound_texture = result->texture.id;
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RED,
                 result->texture.width,
                 result->texture.height,
                 0,
                 GL_RED,
                 GL_UNSIGNED_BYTE,
                 bitmap);
   }
   else
   {
    debug_log("error loading font - could not initialise packing context\n");
    result = NULL;
   }
   
   stbtt_PackEnd(&packing_context);
  }
  else
  {
   debug_log("error loading font - could not read file\n");
   result = NULL;
  }
 }
 
 return result;
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
       i <= file.size;
       ++i)
  {
   if (file.buffer[i] == '\n')
   {
    result= append_s8_to_list(memory, result, line);
    i += 1;
    line.size = 0;
    line.buffer = file.buffer + i;
   }
   else
   {
    line.size += 1;
   }
  }
  result = append_s8_to_list(memory, result, line);
 }
 
 return result;
}
