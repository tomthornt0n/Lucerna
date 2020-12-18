/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 18 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#pragma pack (push, 0)
typedef struct
{
    F32 min_x, min_y;
    F32 max_x, max_y;
} SubTextureInFile;

typedef struct
{
    U64 flags;
    F32 x, y, w, h;
    F32 r, g, b, a;
    F32 speed;
    U32 frame, animation_length;
    U64 animation_speed;
    /* NOTE(tbt): animation_length * sizeof(SubTextureInFile) animation frames here */
    /* NOTE(tbt): null terminated string for texture path here */
} EntityInFile;

typedef struct
{
    SubTextureInFile sub_texture;
    B32 solid, visible;
    /* NOTE(tbt): null terminated string for texture path here */
} TileInFile;

typedef struct
{
    U64 entity_count;
    U32 tilemap_width, tilemap_height;
    /* NOTE(tbt): tilemap_width * tilemap_height * sizeof(TileInFile) tiles here */
    /* NOTE(tbt): entity_count * sizeof(EntityInFile) entities here */
} MapInFile;
#pragma pack (pop)

internal B32
serialise_entity(GameEntity entity,
                 FILE *file)
{
    B32 success = true;

    EntityInFile e;

    e.flags = entity.flags;

    e.x = entity.bounds.x;
    e.y = entity.bounds.y;
    e.w = entity.bounds.w;
    e.h = entity.bounds.h;

    e.r = entity.colour.r;
    e.g = entity.colour.g;
    e.b = entity.colour.b;
    e.a = entity.colour.a;

    e.speed = entity.speed;

    e.frame = entity.frame;
    e.animation_length = entity.animation_length;
    e.animation_speed = entity.animation_speed;

    success = success && (fwrite(&e, 1, sizeof(e), file) == sizeof(e));

    for (I32 frame_index = 0;
         frame_index < entity.animation_length;
         ++frame_index)
    {
        SubTextureInFile t;

        t.min_x = entity.sub_texture[frame_index].min_x;
        t.min_y = entity.sub_texture[frame_index].min_y;
        t.max_x = entity.sub_texture[frame_index].max_x;
        t.max_y = entity.sub_texture[frame_index].max_y;

        success = success &&
                  (fwrite(&t, 1, sizeof(t), file) == sizeof(t));
    }

    U32 texture_path_size = strlen(entity.texture->path) + 1;
    success = success &&
              (fwrite(entity.texture->path, 1, texture_path_size, file) ==
               texture_path_size);

    return success;
}

internal GameEntity *
deserialise_entity(MemoryArena *memory,
                   FILE *file)
{
    U32 err;

    GameEntity *result = arena_allocate(memory, sizeof(*result));

    /* NOTE(tbt): read main entity data */
    EntityInFile e;
    err = fread(&e, 1, sizeof(e), file);
    assert(err == sizeof(e));

    /* NOTE(tbt): read animation */
    result->sub_texture = arena_allocate(memory,
                                         sizeof(*result->sub_texture) *
                                         e.animation_length);
    temporary_memory_begin(memory);
    SubTextureInFile *ts = arena_allocate(memory,
                                          sizeof(*ts) * e.animation_length);

    err = fread(ts, sizeof(*ts), e.animation_length, file);

    for (I32 frame_index = 0;
         frame_index < e.animation_length;
         ++frame_index)
    {
        result->sub_texture[frame_index].min_x = ts[frame_index].min_x;
        result->sub_texture[frame_index].min_y = ts[frame_index].min_y;
        result->sub_texture[frame_index].max_x = ts[frame_index].max_x;
        result->sub_texture[frame_index].max_y = ts[frame_index].max_y;
    }
    temporary_memory_end(memory);

    /* NOTE(tbt): read asset path */
    U32 string_chunk_size = 32; /* NOTE(tbt): allocate for string in chunks of
                                              32 bytes
                                */
    I8 *string = arena_allocate(memory, string_chunk_size);
    I8 *path = string;
    U32 bytes_read = 0;
    while ((*string++ = fgetc(file)) != 0 &&
           !feof(file))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(memory, string_chunk_size, 1);
        }
    }

    /* NOTE(tbt): reconstruct entity */
    result->next = NULL;
    result->flags = e.flags;
    result->bounds = RECTANGLE(e.x, e.y, e.w, e.h);
    result->colour = COLOUR(e.r, e.g, e.b, e.a);
    result->speed = e.speed;
    result->frame = e.frame;
    result->animation_length = e.animation_length;
    result->animation_speed = e.animation_speed;
    if (!(result->texture = asset_from_path(path)))
    {
        result->texture = new_asset_from_path(&global_static_memory, path);
    }

    return result;
}

internal B32
serialise_tile(Tile tile,
               FILE *file)
{
    B32 success = true;

    TileInFile t;

    t.sub_texture.min_x = tile.sub_texture.min_x;
    t.sub_texture.min_y = tile.sub_texture.min_y;
    t.sub_texture.max_x = tile.sub_texture.max_x;
    t.sub_texture.max_y = tile.sub_texture.max_y;

    t.solid = tile.solid;
    t.visible = tile.visible;

    success = success && (fwrite(&t, 1, sizeof(t), file) == sizeof(t));

    if (tile.texture)
    {
        U32 texture_path_size = strlen(tile.texture->path) + 1;
        success = success &&
                  (fwrite(tile.texture->path, 1, texture_path_size, file) ==
                   texture_path_size);
    }
    else
    {
        success = success &&
                  (fwrite("", 1, 1, file) == 1);
    }

    return success;
}

internal Tile
deserialise_tile(MemoryArena *memory,
                 FILE *file)
{
    U32 err;
    Tile result;

    /* NOTE(tbt): read main tile data */
    TileInFile t;
    err = fread(&t, 1, sizeof(t), file);
    assert(err == sizeof(t));

    /* NOTE(tbt): read asset path */
    temporary_memory_begin(memory);

    U32 string_chunk_size = 32; /* NOTE(tbt): allocate for string in chunks of
                                              32 bytes
                                */
    I8 *string = arena_allocate(memory, string_chunk_size);
    I8 *path = string;
    U32 bytes_read = 0;
    while ((*string++ = fgetc(file)) != 0 &&
           !feof(file))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(memory, string_chunk_size, 1);
        }
    }

    /* NOTE(tbt): reconstruct tile */
    result.sub_texture.min_x = t.sub_texture.min_x;
    result.sub_texture.min_y = t.sub_texture.min_y;
    result.sub_texture.max_x = t.sub_texture.max_x;
    result.sub_texture.max_y = t.sub_texture.max_y;
    result.solid = t.solid;
    result.visible = t.visible;
    if (strlen(path))
    {
        if (!(result.texture = asset_from_path(path)))
        {
            result.texture = new_asset_from_path(&global_static_memory, path);
        }
    }
    else
    {
        result.texture = NULL;
    }

    temporary_memory_end(memory);

    return result;
}

internal B32
write_map(GameMap *map,
          I8 *path,
          I8 *temp_path)
{
    B32 success = true;

    FILE *f = fopen(temp_path, "wb");

    /* NOTE(tbt): write map header */
    MapInFile m;
    m.entity_count = map->entity_count;
    m.tilemap_width = map->tilemap_width;
    m.tilemap_height = map->tilemap_height;

    success = success &&
              (fwrite(&m, 1, sizeof(m), f) == sizeof(m));

    /* NOTE(tbt): write tiles */
    for (I32 tile_index = 0;
         tile_index < map->tilemap_width * map->tilemap_height;
         ++tile_index)
    {
        success = success && serialise_tile(map->tilemap[tile_index], f);
    }

    /* NOTE(tbt): write entities */
    for (I32 entity_index = 0;
         entity_index < map->entity_count;
         ++entity_index)
    {
        success = success && serialise_entity(map->entities[entity_index], f);
    }

    fclose(f);

    /* NOTE(tbt): rename temporary file over real file if successful */
    if (success)
    {
        success = success && (0 == rename(temp_path, path));
    }

    return success;
}
#define write_map(_map, _path) write_map((_map), _path, _path "_")

internal GameMap
read_map(MemoryArena *memory,
         I8 *path)
{
    U32 err;

    GameMap result;

    result.arena = memory;

    FILE *f = fopen(path, "rb");

    /* NOTE(tbt): read map header */
    MapInFile header;
    err = fread(&header, 1, sizeof(header), f);
    assert(err == sizeof(header));

    /* NOTE(tbt): read tiles */
    result.tilemap_width = header.tilemap_width;
    result.tilemap_height = header.tilemap_height;
    result.tilemap = arena_allocate(memory,
                                    header.tilemap_height *
                                    header.tilemap_width *
                                    sizeof(*result.tilemap));

    for (I32 tile_index = 0;
         tile_index < header.tilemap_width * header.tilemap_height;
         ++tile_index)
    {
        result.tilemap[tile_index] = deserialise_tile(memory, f);
    }

    /* NOTE(tbt): read entities */
    result.entities = NULL;
    result.entity_count = header.entity_count;

    for (I32 entity_index = 0;
         entity_index < header.entity_count;
         ++entity_index)
    {
        GameEntity *e = deserialise_entity(memory, f);
        e->next = result.entities;
        result.entities = e;
    }

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

