/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 15 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define WAV_IMPLEMENTATION
#include "wav.h"

MemoryArena global_asset_memory;

internal void *
malloc_for_stb(U64 size)
{
    return arena_allocate(&global_asset_memory, size);
}

internal void *
realloc_for_stb(void *p,
                U64 old_size,
                U64 new_size)
{
    void *result = arena_allocate(&global_asset_memory, new_size);
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

typedef U32 TextureID;

typedef struct Texture Texture;
struct Texture
{
    TextureID id;
    I32 width, height;
    F32 min_x, min_y;
    F32 max_x, max_y;
};

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

internal Texture
load_texture(OpenGLFunctions *gl,
             I8 *path)
{
    Texture result = {0};
    U8 *pixels;

    temporary_memory_begin(&global_asset_memory);

    pixels = stbi_load(path, &result.width, &result.height, NULL, 4);

    assert(pixels);

    result.min_x = 0.0f;
    result.min_y = 0.0f;
    result.max_x = 1.0f;
    result.max_y = 1.0f;

    gl->GenTextures(1, &result.id);
    gl->BindTexture(GL_TEXTURE_2D, result.id);

    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    gl->TexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   result.width,
                   result.height,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   pixels);

    temporary_memory_end(&global_asset_memory);

    return result;
}

internal Texture
create_sub_texture(Texture texture,
                   F32 x, F32 y,
                   F32 w, F32 h)
{
    texture.min_x = x / texture.width;
    texture.min_y = y / texture.height;

    texture.max_x = (x + w) / texture.width;
    texture.max_y = (y + h) / texture.height;

    return texture;
}

internal Texture *
slice_animation(Texture texture,
                F32 x, F32 y,
                F32 w, F32 h,
                U32 horizontal_count,
                U32 vertical_count)
{
    I32 x_index, y_index;
    I32 index = 0;

    Texture *result = arena_allocate(&global_asset_memory,
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
    Font *result = arena_allocate(&global_asset_memory, sizeof(*result));

    U8 *file_buffer;
    U8 pixels[1024 * 1024];

    temporary_memory_begin(&global_asset_memory);

    file_buffer = read_entire_file(&global_asset_memory, path);
    assert(file_buffer);

    stbtt_BakeFontBitmap(file_buffer, 0, size, pixels, 1024, 1024, 32, 96, result->char_data);

    result->texture.width = 1024;
    result->texture.height = 1024;
    result->texture.min_x = 0.0f;
    result->texture.min_y = 0.0f;
    result->texture.max_x = 1.0f;
    result->texture.max_y = 1.0f;
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

    temporary_memory_end(&global_asset_memory);

    return result;
}

internal AudioSource
load_wav(I8 *path)
{
    AudioSource result;

    I32 data_rate, bits_per_sample, channels, data_size;
    U8 *data;

    wav_read(path,
             &data_rate,
             &bits_per_sample,
             &channels,
             &data_size,
             NULL);

    assert(data_rate == 44100);
    assert(bits_per_sample == 16);
    assert(channels == 2);

    data = arena_allocate(&global_asset_memory, data_size);

    wav_read(path, NULL, NULL, NULL, NULL, data);

    result.stream = data;
    result.stream_size = data_size;
    result.l_gain = 0.5f;
    result.r_gain = 0.5f;
    result.level = 1.0f;
    result.pan = 0.5f;

    return result;
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

