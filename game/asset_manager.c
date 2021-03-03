#define WAV_IMPLEMENTATION
#include "wav.h"

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

typedef struct Asset Asset;

typedef U32 TextureID;

typedef struct
{
 TextureID id;
 I32 width, height;
} Texture;

internal TextureID global_currently_bound_texture = 0;

#define AUDIO_BUFFER_SIZE 512

enum
{
 SOURCE_FLAG_NO_FLAGS = 0,
 
 SOURCE_FLAG_ACTIVE  = 1 << 0,
 SOURCE_FLAG_REWIND  = 1 << 1,
 SOURCE_FLAG_LOOPING = 1 << 2
};

typedef struct AudioSource AudioSource;
struct AudioSource
{
 AudioSource *next; // NOTE(tbt): keep a list of sources to be processed
 I16 *buffer;       // NOTE(tbt): actual PCM data - 16 bit signed integer
 U32 buffer_size;   // NOTE(tbt): size of `buffer` in bytes
 U32 playhead;      // NOTE(tbt): index into `buffer` where data is read from to fill the master buffer
 U8  flags;         // NOTE(tbt): bit field of source properties - see the enum above
 F32 l_gain;        // NOTE(tbt): left channel is multiplied by this before mixing
 F32 r_gain;        // NOTE(tbt): right channel is multiplied by this before mixing
 
 F32 level;         // NOTE(tbt): as set by `set_audio_source_level()`
 F32 pan;           // NOTE(tbt): as set by `set_audio source_pan`
};

typedef struct Entity Entity;

enum
{
 ASSET_KIND_none,
 
 ASSET_KIND_texture,
 ASSET_KIND_audio,
 ASSET_KIND_level,
};

struct Asset
{
 B32 touched;
 Asset *next_hash;
 Asset *next_loaded;
 I32 kind;
 B32 loaded;
 S8 path;
 
 union
 {
  Texture texture;
  AudioSource audio;
  LevelDescriptor level_descriptor;
 };
};

#define ASSET_HASH_TABLE_SIZE (512)
internal Asset global_assets_dict[ASSET_HASH_TABLE_SIZE] = {0};
#define asset_hash(string) hash_string((string), ASSET_HASH_TABLE_SIZE);

Asset *global_loaded_assets = NULL;

internal Asset *
asset_from_path(S8 path)
{
 U64 index = asset_hash(path);
 Asset *result = &global_assets_dict[index];
 
 if (result->touched)
 {
  Asset *prev = NULL;
  for (; NULL != result; prev = result, result = result->next_hash)
  {
   if (string_match(path, result->path))
   {
    return result;
   }
  }
  
  assert(prev);
  
  prev->next_hash = arena_allocate(&global_static_memory,
                                   sizeof(*prev->next_hash));
  result = prev->next_hash;
  result->touched = true;
  result->path = copy_string(&global_static_memory, path);
  
  return result;
 }
 else
 {
  result->touched = true;
  result->path = copy_string(&global_static_memory, path);
  return result;
 }
}

#define ENTIRE_TEXTURE ((SubTexture){ 0.0f, 0.0f, 1.0f, 1.0f })
typedef struct
{
 F32 min_x, min_y;
 F32 max_x, max_y;
} SubTexture;

typedef struct
{
 Texture texture;
 stbtt_bakedchar char_data[96];
 U32 size;
 F32 estimate_char_width;
} Font;

internal void
load_texture(OpenGLFunctions *gl,
             Asset *asset)
{
 U8 *pixels;
 
 if (!asset) { return; }
 if (!asset->touched || asset->loaded) { return; }
 
 
 I8 path_cstr[1024] = {0};
 snprintf(path_cstr, 1024, "%.*s", (I32)asset->path.len, asset->path.buffer);
 
 temporary_memory_begin(&global_level_memory);
 
 pixels = stbi_load(path_cstr,
                    &(asset->texture.width),
                    &(asset->texture.height),
                    NULL, 4);
 
 if (!pixels)
 {
  fprintf(stderr, "Error loading texture '%s'\n", path_cstr);
  return;
 }
 
 gl->GenTextures(1, &(asset->texture.id));
 gl->BindTexture(GL_TEXTURE_2D, asset->texture.id);
 
 gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
 gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
 
 gl->TexImage2D(GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                asset->texture.width,
                asset->texture.height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels);
 
 temporary_memory_end(&global_level_memory);
 
 asset->kind = ASSET_KIND_texture;
 asset->loaded = true;
 asset->next_loaded = global_loaded_assets;
 global_loaded_assets = asset;
 
 fprintf(stderr, "Successfully loaded texture: '%s'\n", path_cstr);
}

internal void
unload_texture(OpenGLFunctions *gl,
               Asset *asset)
{
 assert(asset->kind == ASSET_KIND_texture && asset->loaded);
 gl->DeleteTextures(1, &asset->texture.id);
 global_currently_bound_texture = 0;
 asset->texture.id = 0;
 asset->texture.width = 0;
 asset->texture.height = 0;
 asset->loaded = false;
 
 Asset **indirect = &global_loaded_assets;
 while (*indirect != asset) { indirect = &(*indirect)->next_loaded; }
 *indirect = (*indirect)->next_loaded;
 
 fprintf(stderr, "Unloaded texture: '%.*s'\n", (I32)asset->path.len, asset->path.buffer);
}

internal SubTexture
sub_texture_from_texture(OpenGLFunctions *gl,
                         Asset *texture,
                         F32 x, F32 y,
                         F32 w, F32 h)
{
 SubTexture result;
 
 if (texture->kind != ASSET_KIND_texture && texture->loaded) { return ENTIRE_TEXTURE; }
 
 load_texture(gl, texture);
 
 result.min_x = x / texture->texture.width;
 result.min_y = y / texture->texture.height;
 
 result.max_x = (x + w) / texture->texture.width;
 result.max_y = (y + h) / texture->texture.height;
 
 return result;
}

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

internal Font *
load_font(OpenGLFunctions *gl,
          S8 path,
          U32 size)
{
 Font *result = arena_allocate(&global_static_memory, sizeof(*result));
 
 U8 pixels[1024 * 1024];
 
 temporary_memory_begin(&global_level_memory);
 
 S8 file = platform_read_entire_file(&global_level_memory, path);
 
 assert(file.buffer);
 
 stbtt_BakeFontBitmap(file.buffer, 0, size, pixels, 1024, 1024, 32, 96, result->char_data);
 
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
 
 temporary_memory_end(&global_level_memory);
 
 fprintf(stderr, "Sucessfully loaded font: '%.*s'\n", (I32)path.len, path.buffer);
 return result;
}

AudioSource *global_playing_sources;

// TODO(tbt): audio streaming
internal void
load_audio(Asset *asset)
{
 I32 data_rate, bits_per_sample, channels, data_size;
 
 if (!asset) { return; }
 if (asset->loaded || !asset->touched) { return; }
 
 I8 path_cstr[512] = {0};
 snprintf(path_cstr, 512, "%.*s", (I32)asset->path.len, asset->path.buffer);
 
 wav_read(path_cstr,
          &data_rate,
          &bits_per_sample,
          &channels,
          &data_size,
          NULL);
 
 assert(data_rate == 44100);
 assert(bits_per_sample == 16);
 assert(channels == 2);
 
 asset->audio.buffer = arena_allocate(&global_level_memory, data_size);
 wav_read(path_cstr, NULL, NULL, NULL, NULL, asset->audio.buffer);
 
 asset->audio.buffer_size = data_size;
 asset->audio.l_gain = 0.5f;
 asset->audio.r_gain = 0.5f;
 asset->audio.level = 1.0f;
 asset->audio.pan = 0.5f;
 
 asset->kind = ASSET_KIND_audio;
 asset->loaded = true;
 asset->next_loaded = global_loaded_assets;
 global_loaded_assets = asset;
 
 fprintf(stderr, "Sucessfully loaded audio: '%s'\n", path_cstr);
}

internal void
unload_audio(Asset *asset)
{
 // NOTE(tbt): remove source from list of playing sources
 if (asset->audio.flags & SOURCE_FLAG_ACTIVE)
 {
  platform_get_audio_lock();
  AudioSource **indirect_source = &global_playing_sources;
  while (*indirect_source != &asset->audio) { indirect_source = &(*indirect_source)->next; }
  *indirect_source = (*indirect_source)->next;
  
  asset->audio.flags &= ~SOURCE_FLAG_ACTIVE;
  asset->audio.flags |= SOURCE_FLAG_REWIND;
  platform_release_audio_lock();
 }
 
 asset->loaded = false;
 // NOTE(tbt): audio data is in memory with level duration so will be freed when the
 //            map is unloaded
 asset->audio.buffer = NULL;
 
 Asset **indirect_asset = &global_loaded_assets;
 while (*indirect_asset != asset) { indirect_asset = &(*indirect_asset)->next_loaded; }
 *indirect_asset = (*indirect_asset)->next_loaded;
 
 fprintf(stderr, "Unloaded audio: '%.*s'\n", (I32)asset->path.len, asset->path.buffer);
}

internal void unload_all_assets(OpenGLFunctions *gl);

internal void
load_level_descriptor(Asset *asset)
{
 
 if (!asset) { return; }
 if (!asset->touched || asset->loaded) { return; }
 
 fprintf(stderr, "Parsing level descriptor: '%.*s'\n", (I32)asset->path.len, asset->path.buffer);
 
 S8 file = platform_read_entire_file(&global_static_memory, asset->path);
 asset->level_descriptor = parse_level_descriptor(file);
 
 asset->loaded = true;
 asset->kind = ASSET_KIND_level;
 asset->next_loaded = global_loaded_assets;
 global_loaded_assets = asset;
}

internal void
unload_level_descriptor(Asset *asset)
{
 asset->loaded = false;
 
 Asset **indirect_asset = &global_loaded_assets;
 while (*indirect_asset != asset) { indirect_asset = &(*indirect_asset)->next_loaded; }
 *indirect_asset = (*indirect_asset)->next_loaded;
}

internal void
serialise_level_descriptor(Asset *asset)
{
 fprintf(stderr, "Serialising level descriptor %.*s\n", (I32)asset->path.len, asset->path.buffer);
 
 U8 buffer[1024];
 U64 length = snprintf(buffer,
                       1024,
                       "player_spawn: %f, %f\n"
                       "bg: \"%.*s\"\n"
                       "fg: \"%.*s\"\n"
                       "music: \"%.*s\"\n"
                       "entities: \"%.*s\"\n"
                       "exposure: %f\n"
                       "kind: %s",
                       asset->level_descriptor.player_spawn_x, asset->level_descriptor.player_spawn_y,
                       (I32)asset->level_descriptor.bg_path.len, asset->level_descriptor.bg_path.buffer,
                       (I32)asset->level_descriptor.fg_path.len, asset->level_descriptor.fg_path.buffer,
                       (I32)asset->level_descriptor.music_path.len, asset->level_descriptor.music_path.buffer,
                       (I32)asset->level_descriptor.entities_path.len, asset->level_descriptor.entities_path.buffer,
                       asset->level_descriptor.exposure,
                       asset->level_descriptor.is_memory ? "memory" : "world");
 
 if (length < 1024)
 {
  platform_write_entire_file(asset->path, buffer, length);
 }
 else
 {
  fprintf(stderr, "Error serialising level descriptor - buffer too small");
 }
}

internal void
unload_all_assets(OpenGLFunctions *gl)
{
 fprintf(stderr, "Unloading all assets...\n");
 
 Asset *loaded_asset = global_loaded_assets;
 while (loaded_asset)
 {
  switch (loaded_asset->kind)
  {
   case ASSET_KIND_texture:
   {
    unload_texture(gl, loaded_asset);
    break;
   }
   case ASSET_KIND_audio:
   {
    unload_audio(loaded_asset);
    break;
   }
   case ASSET_KIND_level:
   {
    unload_level_descriptor(loaded_asset);
    break;
   }
   default:
   {
    fprintf(stderr,
            "skipping unloading asset '%.*s'...\n",
            (I32)loaded_asset->path.len,
            loaded_asset->path.buffer);
    break;
   }
  }
  loaded_asset = loaded_asset->next_loaded;
 }
}

