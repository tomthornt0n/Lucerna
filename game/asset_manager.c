/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 01 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define WAV_IMPLEMENTATION
#include "wav.h"

internal void *
malloc_for_stb(U64 size)
{
    return arena_allocate(&global_static_memory, size);
}

internal void *
realloc_for_stb(void *p,
                U64 old_size,
                U64 new_size)
{
    void *result = arena_allocate(&global_static_memory, new_size);
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

#define SHADER_PATH(_path) ("../assets/shaders/" _path)
#define SOUND_PATH(_path) ("../assets/audio/" _path)
#define TEXTURE_PATH(_path) ("../assets/textures/" _path)
#define FONT_PATH(_path) ("../assets/fonts/" _path)

typedef struct Asset Asset;

typedef U32 TextureID;

internal TextureID global_currently_bound_texture = 0;

typedef struct
{
    TextureID id;
    I32 width, height;
} Texture;

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

#define AUDIO_BUFFER_SIZE 512

enum
{
    ASSET_TYPE_NONE,

    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_AUDIO,
};

#define asset_hash(string) hash_string((string), ASSET_HASH_TABLE_SIZE);

struct Asset
{
    Asset *next_hash;
    Asset *next_loaded;
    I32 type;
    B32 loaded;
    I8 *path;

    union
    {
        Texture texture;
        AudioSource audio;
    };
};

#define ASSET_HASH_TABLE_SIZE (512)
internal Asset global_assets_dict[ASSET_HASH_TABLE_SIZE] = {{0}};

Asset *global_loaded_assets = NULL;

internal Asset *
asset_from_path(I8 *path)
{
    U64 index = asset_hash(path);
    Asset *result = global_assets_dict + index;

    while (result)
    {
        if (!result->path) { goto new_asset; }

        if (strcmp(result->path, path))
        {
            result = result->next_hash;
        }
        else
        {
            return result;
        }
    }

    return NULL;

new_asset:
    if (global_assets_dict[index].path &&
        strcmp(global_assets_dict[index].path, path))
    {
        // NOTE(tbt): hash collision
        Asset *tail;

        result = arena_allocate(&global_static_memory, sizeof(*result));

        while (tail->next_hash) { tail = tail->next_hash; }
        tail->next_hash = result;
    }
    else
    {
        result = global_assets_dict + index;
    }

    result->path = arena_allocate(&global_static_memory, strlen(path) + 1);
    strcpy(result->path, path);

    return result;
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
} Font;

internal void *
read_entire_file(MemoryArena *arena,
                 I8 *path)
{
    FILE *file;
    U64 file_size, bytes_read;
    I8 *result;

    file = fopen(path, "rb");

    assert(file);

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    result = arena_allocate_aligned(arena, file_size + 1, 1);
    bytes_read = fread(result, 1, file_size, file);
    assert(bytes_read == file_size);
    fclose(file);
    
    result[file_size] = 0;

    return result;
}

internal void
load_texture(OpenGLFunctions *gl,
             Asset *asset)
{
    U8 *pixels;

    if (!asset) { return; }
    if (!asset->path || asset->loaded) { return; }

    assert(asset->path);

    temporary_memory_begin(&global_static_memory);

    pixels = stbi_load(asset->path,
                       &(asset->texture.width),
                       &(asset->texture.height),
                       NULL, 4);

    if (!pixels)
    {
        temporary_memory_end(&global_static_memory);
        return;
    }

    gl->GenTextures(1, &(asset->texture.id));
    gl->BindTexture(GL_TEXTURE_2D, asset->texture.id);

    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

    temporary_memory_end(&global_static_memory);

    asset->type = ASSET_TYPE_TEXTURE;
    asset->loaded = true;
    asset->next_loaded = global_loaded_assets;
    global_loaded_assets = asset;
}

internal void
unload_texture(OpenGLFunctions *gl,
               Asset *asset)
{
    assert(asset->type == ASSET_TYPE_TEXTURE && asset->loaded);
    gl->DeleteTextures(1, &asset->texture.id);
    global_currently_bound_texture = 0;
    asset->texture.id = 0;
    asset->texture.width = 0;
    asset->texture.height = 0;
    asset->loaded = false;

    Asset **indirect = &global_loaded_assets;
    while (*indirect != asset) { indirect = &(*indirect)->next_loaded; }
    *indirect = (*indirect)->next_loaded;
}

internal SubTexture
create_sub_texture(Texture texture,
                   F32 x, F32 y,
                   F32 w, F32 h)
{
    SubTexture result;

    result.min_x = x / texture.width;
    result.min_y = y / texture.height;

    result.max_x = (x + w) / texture.width;
    result.max_y = (y + h) / texture.height;

    return result;
}

internal SubTexture *
slice_animation(Texture texture,
                F32 x, F32 y,
                F32 w, F32 h,
                U32 horizontal_count,
                U32 vertical_count)
{
    I32 x_index, y_index;
    I32 index = 0;

    SubTexture *result = arena_allocate(&global_static_memory,
                                        horizontal_count *
                                        vertical_count *
                                        sizeof(*result));

    for (y_index = 0;
         y_index < vertical_count;
         ++y_index)
    {
        for (x_index = 0;
             x_index < horizontal_count;
             ++x_index)
        {
            result[index++] = create_sub_texture(texture,
                                                 x + x_index * w,
                                                 y + y_index * h,
                                                 w, h);
        }
    }

    return result;
}

internal Font *
load_font(OpenGLFunctions *gl,
          I8 *path,
          U32 size)
{
    Font *result = arena_allocate(&global_static_memory, sizeof(*result));

    U8 *file_buffer;
    U8 pixels[1024 * 1024];

    temporary_memory_begin(&global_static_memory);

    file_buffer = read_entire_file(&global_static_memory, path);
    assert(file_buffer);

    stbtt_BakeFontBitmap(file_buffer, 0, size, pixels, 1024, 1024, 32, 96, result->char_data);

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
                   GL_ALPHA,
                   result->texture.width,
                   result->texture.height,
                   0,
                   GL_ALPHA,
                   GL_UNSIGNED_BYTE,
                   pixels);

    temporary_memory_end(&global_static_memory);

    return result;
}

AudioSource *global_playing_sources;

// TODO(tbt): audio streaming
internal void
load_audio(Asset *asset)
{
    I32 data_rate, bits_per_sample, channels, data_size;

    if (!asset) { return; }
    if (asset->loaded || !asset->path) { return; }

    wav_read(asset->path,
             &data_rate,
             &bits_per_sample,
             &channels,
             &data_size,
             NULL);

    assert(data_rate == 44100);
    assert(bits_per_sample == 16);
    assert(channels == 2);

    asset->audio.buffer = arena_allocate(&global_level_memory, data_size);
    wav_read(asset->path, NULL, NULL, NULL, NULL, asset->audio.buffer);

    asset->audio.buffer_size = data_size;
    asset->audio.l_gain = 0.5f;
    asset->audio.r_gain = 0.5f;
    asset->audio.level = 1.0f;
    asset->audio.pan = 0.5f;

    asset->type = ASSET_TYPE_AUDIO;
    asset->loaded = true;
    asset->next_loaded = global_loaded_assets;
    global_loaded_assets = asset;
}

internal void
unload_audio(Asset *asset)
{
    // NOTE(tbt): remove source from list of playing sources
    if (asset->audio.flags & SOURCE_FLAG_ACTIVE)
    {
        AudioSource **indirect_source = &global_playing_sources;
        while (*indirect_source != &asset->audio) { indirect_source = &(*indirect_source)->next; }
        *indirect_source = (*indirect_source)->next;

        asset->audio.flags &= ~SOURCE_FLAG_ACTIVE;
        asset->audio.flags |= SOURCE_FLAG_REWIND;
    }

    asset->loaded = false;
    // NOTE(tbt): audio data is in memory with level duration so will be freed when the
    //            map is unloaded
    asset->audio.buffer = NULL;

    Asset **indirect_asset = &global_loaded_assets;
    while (*indirect_asset != asset) { indirect_asset = &(*indirect_asset)->next_loaded; }
    *indirect_asset = (*indirect_asset)->next_loaded;
}

