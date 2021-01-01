/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 01 Jan 2021
  License : N/A
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
    U32 frame, animation_length, animation_start, animation_end;
    U64 animation_speed;
    F32 gradient_tl_r;
    F32 gradient_tl_g;
    F32 gradient_tl_b;
    F32 gradient_tl_a;
    F32 gradient_tr_r;
    F32 gradient_tr_g;
    F32 gradient_tr_b;
    F32 gradient_tr_a;
    F32 gradient_bl_r;
    F32 gradient_bl_g;
    F32 gradient_bl_b;
    F32 gradient_bl_a;
    F32 gradient_br_r;
    F32 gradient_br_g;
    F32 gradient_br_b;
    F32 gradient_br_a;
    // NOTE(tbt): animation_length * sizeof(SubTextureInFile) animation frames here
    // NOTE(tbt): null terminated string for level transport here
    // NOTE(tbt): null terminated string for texture path here
} EntityInFileV0;

typedef struct
{
    U64 flags;
    F32 x, y, w, h;
    F32 r, g, b, a;
    F32 speed;
    U32 frame, animation_length, animation_start, animation_end;
    U64 animation_speed;
    F32 gradient_tl_r;
    F32 gradient_tl_g;
    F32 gradient_tl_b;
    F32 gradient_tl_a;
    F32 gradient_tr_r;
    F32 gradient_tr_g;
    F32 gradient_tr_b;
    F32 gradient_tr_a;
    F32 gradient_bl_r;
    F32 gradient_bl_g;
    F32 gradient_bl_b;
    F32 gradient_bl_a;
    F32 gradient_br_r;
    F32 gradient_br_g;
    F32 gradient_br_b;
    F32 gradient_br_a;
    // NOTE(tbt): animation_length * sizeof(SubTextureInFile) animation frames here
    // NOTE(tbt): null terminated string for level transport here
    // NOTE(tbt): null terminated string for texture path here
    // NOTE(tbt): null terminated string for audio path here
} EntityInFileV1;

// NOTE(tbt): `EntityInFile` typedef'd to most recent version
#define CURRENT_SAVE_FILE_VERSION 1
typedef EntityInFileV1 EntityInFile;

typedef struct
{
    SubTextureInFile sub_texture;
    B32 solid, visible;
    // NOTE(tbt): null terminated string for texture path here
} TileInFile;

typedef struct
{
    U32 version;
    U64 entity_count;
    U32 tilemap_width, tilemap_height;
    // NOTE(tbt): tilemap_width * tilemap_height * sizeof(TileInFile) tiles here
    // NOTE(tbt): entity_count * sizeof(EntityInFile) entities here
} MapInFile;
#pragma pack (pop)

internal B32
save_map(void)
{
    if (!global_map.path[0])
    {
        fprintf(stderr, "error saving map: empty path\n");
        return false;
    }

    // NOTE(tbt): append "_" to end of path
    I8 *temp_path = arena_allocate(&global_frame_memory,
                                   strlen(global_map.path) + 2);
    strcpy(temp_path, global_map.path);
    strcat(temp_path, "_");

    // NOTE(tbt): write to temp file to prevent loosing data if the write fails
    FILE *f = fopen(temp_path, "wb");
    if (!f)
    {
        fprintf(stderr,
                "error saving map: could not open path '%s' - %s\n",
                global_map.path,
                strerror(errno));
        return false;
    }

    // NOTE(tbt): write map header
    MapInFile m = {0};
    m.entity_count = global_map.entity_count;
    m.tilemap_width = global_map.tilemap_width;
    m.tilemap_height = global_map.tilemap_height;
    m.version = CURRENT_SAVE_FILE_VERSION;

    if (fwrite(&m, sizeof(m), 1, f) != 1)
    {
        fprintf(stderr,
                "error saving map '%s': failure while writing map header - %s\n",
                temp_path,
                strerror(errno));
        fclose(f);
        return false;
    }

    // NOTE(tbt): write tiles
    for (I32 tile_index = 0;
         tile_index < global_map.tilemap_width * global_map.tilemap_height;
         ++tile_index)
    {
        TileInFile t;

        t.sub_texture.min_x = global_map.tilemap[tile_index].sub_texture.min_x;
        t.sub_texture.min_y = global_map.tilemap[tile_index].sub_texture.min_y;
        t.sub_texture.max_x = global_map.tilemap[tile_index].sub_texture.max_x;
        t.sub_texture.max_y = global_map.tilemap[tile_index].sub_texture.max_y;

        t.solid = global_map.tilemap[tile_index].solid;
        t.visible = global_map.tilemap[tile_index].visible;

        if (fwrite(&t, sizeof(t), 1, f) != 1)
        {
            fprintf(stderr,
                    "error saving map '%s': failure while writing tile sub-texture - %s\n",
                    temp_path,
                    strerror(errno));
            fclose(f);
            return false;
        }

        if (global_map.tilemap[tile_index].texture)
        {
            U32 texture_path_size =
                strlen(global_map.tilemap[tile_index].texture->path) + 1;

            if (fwrite(global_map.tilemap[tile_index].texture->path,
                       1,
                       texture_path_size,
                       f) != texture_path_size)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing tile asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }
        else
        {
            if (fwrite("", 1, 1, f) != 1)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing tile asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }
    }

    // NOTE(tbt): write entities
    GameEntity *entity = global_map.entities;
    while (entity)
    {
        EntityInFile e;

        e.flags = entity->flags;

        e.x = entity->bounds.x;
        e.y = entity->bounds.y;
        e.w = entity->bounds.w;
        e.h = entity->bounds.h;

        e.r = entity->colour.r;
        e.g = entity->colour.g;
        e.b = entity->colour.b;
        e.a = entity->colour.a;

        e.gradient_tl_r = entity->gradient.tl.r;
        e.gradient_tl_g = entity->gradient.tl.g;
        e.gradient_tl_b = entity->gradient.tl.b;
        e.gradient_tl_a = entity->gradient.tl.a;
        e.gradient_tr_r = entity->gradient.tr.r;
        e.gradient_tr_g = entity->gradient.tr.g;
        e.gradient_tr_b = entity->gradient.tr.b;
        e.gradient_tr_a = entity->gradient.tr.a;
        e.gradient_bl_r = entity->gradient.bl.r;
        e.gradient_bl_g = entity->gradient.bl.g;
        e.gradient_bl_b = entity->gradient.bl.b;
        e.gradient_bl_a = entity->gradient.bl.a;
        e.gradient_br_r = entity->gradient.br.r;
        e.gradient_br_g = entity->gradient.br.g;
        e.gradient_br_b = entity->gradient.br.b;
        e.gradient_br_a = entity->gradient.br.a;

        e.speed = entity->speed;

        e.frame = entity->frame;
        e.animation_length = entity->animation_length;
        e.animation_start = entity->animation_start;
        e.animation_end = entity->animation_end;
        e.animation_speed = entity->animation_speed;

        if (fwrite(&e, sizeof(e), 1, f) != 1)
        {
            fprintf(stderr,
                    "error saving map '%s': failure while writing entity\n",
                    temp_path);
            fclose(f);
            return false;
        }

        if (entity->sub_texture)
        {
            for (I32 frame_index = 0;
                 frame_index < entity->animation_length;
                 ++frame_index)
            {
                SubTextureInFile t;

                t.min_x = entity->sub_texture[frame_index].min_x;
                t.min_y = entity->sub_texture[frame_index].min_y;
                t.max_x = entity->sub_texture[frame_index].max_x;
                t.max_y = entity->sub_texture[frame_index].max_y;

                if (fwrite(&t, sizeof(t), 1, f) != 1)
                {
                    fprintf(stderr,
                            "error saving map '%s': failure while writing entity sub-texture - %s\n",
                            temp_path,
                            strerror(errno));
                    fclose(f);
                    return false;
                }
            }
        }
        else
        {
            e.animation_length = 0;
        }

        if (entity->level_transport)
        {
            U32 map_path_size = strlen(entity->level_transport) + 1;
            if (fwrite(entity->level_transport, 1, map_path_size, f) !=
                map_path_size)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing entity level transport asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }
        else
        {
            if (fwrite("", 1, 1, f) != 1)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing entity level transport asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }

        if (entity->texture)
        {
            U32 texture_path_size = strlen(entity->texture->path) + 1;
            if (fwrite(entity->texture->path, 1, texture_path_size, f) !=
                texture_path_size)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing entity texture asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }
        else
        {
            if (fwrite("", 1, 1, f) != 1)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing entity texture asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }

        if (entity->sound)
        {
            U32 sound_path_size = strlen(entity->sound->path) + 1;
            if (fwrite(entity->sound->path, 1, sound_path_size, f) !=
                sound_path_size)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing entity texture sound - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }
        else
        {
            if (fwrite("", 1, 1, f) != 1)
            {
                fprintf(stderr,
                        "error saving map '%s': failure while writing entity sound asset - %s\n",
                        temp_path,
                        strerror(errno));
                fclose(f);
                return false;
            }
        }

        entity = entity->next;
    }

    fclose(f);

    // NOTE(tbt): rename temporary file over real file if successful
    if (0 == rename(temp_path, global_map.path))
    {
        fprintf(stderr,
                "successfully wrote %u entities, %u tiles to '%s'.\n",
                global_map.entity_count,
                global_map.tilemap_width * global_map.tilemap_height,
                global_map.path);

        return true;
    }
    else
    {
        fprintf(stderr,
                "error saving map '%s': failure while renaming temp file\n",
                temp_path);
        return false;
    }
}

internal GameEntity *
deserialise_entity_v0(OpenGLFunctions *gl,
                      FILE *f)
{
    U32 err;
    GameEntity *entity = arena_allocate(&global_level_memory,
                                        sizeof(*entity));

    // NOTE(tbt): read main entity data
    EntityInFileV0 e;
    err = fread(&e, 1, sizeof(e), f);
    if (err != sizeof(e))
    {
        return NULL;
    }

    // NOTE(tbt): read animation
    entity->sub_texture = arena_allocate(&global_level_memory,
                                         sizeof(*entity->sub_texture) *
                                         e.animation_length);
    temporary_memory_begin(&global_level_memory);
    SubTextureInFile *ts = arena_allocate(&global_level_memory,
                                          sizeof(*ts) * e.animation_length);

    err = fread(ts, sizeof(*ts), e.animation_length, f);
    if (err != e.animation_length)
    {
        temporary_memory_end(&global_level_memory);
        return NULL;
    }

    for (I32 frame_index = 0;
         frame_index < e.animation_length;
         ++frame_index)
    {
        entity->sub_texture[frame_index].min_x = ts[frame_index].min_x;
        entity->sub_texture[frame_index].min_y = ts[frame_index].min_y;
        entity->sub_texture[frame_index].max_x = ts[frame_index].max_x;
        entity->sub_texture[frame_index].max_y = ts[frame_index].max_y;
    }
    temporary_memory_end(&global_level_memory);

    I8 *string;
    U32 bytes_read;
    U32 string_chunk_size = 32; // NOTE(tbt): allocate for string in chunks of 32 bytes

    // NOTE(tbt): read level transport path
    string = arena_allocate(&global_level_memory, string_chunk_size);
    I8 *map_path = string;
    bytes_read = 0;
    while ((*string++ = fgetc(f)) != 0 &&
           !feof(f))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(&global_level_memory,
                                   string_chunk_size,
                                   1);
        }
    }

    // NOTE(tbt): read texture asset path
    string = arena_allocate(&global_frame_memory, string_chunk_size);
    I8 *texture_path = string;
    bytes_read = 0;
    while ((*string++ = fgetc(f)) != 0 &&
           !feof(f))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(&global_frame_memory,
                                   string_chunk_size,
                                   1);
        }
    }

    // NOTE(tbt): reconstruct entity
    entity->next = NULL;
    entity->flags = e.flags;
    entity->bounds = RECTANGLE(e.x, e.y, e.w, e.h);
    entity->colour = COLOUR(e.r, e.g, e.b, e.a);
    entity->speed = e.speed;
    entity->frame = e.frame;
    entity->animation_length = e.animation_length;
    entity->animation_speed = e.animation_speed;
    entity->animation_start = e.animation_start;
    entity->animation_end = e.animation_end;
    entity->gradient.tl.r = e.gradient_tl_r;
    entity->gradient.tl.g = e.gradient_tl_g;
    entity->gradient.tl.b = e.gradient_tl_b;
    entity->gradient.tl.a = e.gradient_tl_a;
    entity->gradient.tr.r = e.gradient_tr_r;
    entity->gradient.tr.g = e.gradient_tr_g;
    entity->gradient.tr.b = e.gradient_tr_b;
    entity->gradient.tr.a = e.gradient_tr_a;
    entity->gradient.bl.r = e.gradient_bl_r;
    entity->gradient.bl.g = e.gradient_bl_g;
    entity->gradient.bl.b = e.gradient_bl_b;
    entity->gradient.bl.a = e.gradient_bl_a;
    entity->gradient.br.r = e.gradient_br_r;
    entity->gradient.br.g = e.gradient_br_g;
    entity->gradient.br.b = e.gradient_br_b;
    entity->gradient.br.a = e.gradient_br_a;
    if (strlen(texture_path))
    {
        entity->texture = asset_from_path(texture_path);
    }
    else
    {
        entity->texture = NULL;
    }

    if (strlen(map_path))
    {
        entity->level_transport = map_path;
    }
    else
    {
        entity->level_transport = NULL;
    }

    return entity;
}

internal GameEntity *
deserialise_entity_v1(OpenGLFunctions *gl,
                      FILE *f)
{
    U32 err;
    GameEntity *entity = arena_allocate(&global_level_memory,
                                        sizeof(*entity));

    // NOTE(tbt): read main entity data
    EntityInFileV1 e;
    err = fread(&e, 1, sizeof(e), f);
    if (err != sizeof(e))
    {
        return NULL;
    }

    // NOTE(tbt): read animation
    entity->sub_texture = arena_allocate(&global_level_memory,
                                         sizeof(*entity->sub_texture) *
                                         e.animation_length);
    temporary_memory_begin(&global_level_memory);
    SubTextureInFile *ts = arena_allocate(&global_level_memory,
                                          sizeof(*ts) * e.animation_length);

    err = fread(ts, sizeof(*ts), e.animation_length, f);
    if (err != e.animation_length)
    {
        temporary_memory_end(&global_level_memory);
        return NULL;
    }

    for (I32 frame_index = 0;
         frame_index < e.animation_length;
         ++frame_index)
    {
        entity->sub_texture[frame_index].min_x = ts[frame_index].min_x;
        entity->sub_texture[frame_index].min_y = ts[frame_index].min_y;
        entity->sub_texture[frame_index].max_x = ts[frame_index].max_x;
        entity->sub_texture[frame_index].max_y = ts[frame_index].max_y;
    }
    temporary_memory_end(&global_level_memory);

    I8 *string;
    U32 bytes_read;
    U32 string_chunk_size = 32; // NOTE(tbt): allocate for string in chunks of 32 bytes

    // NOTE(tbt): read level transport path
    string = arena_allocate(&global_level_memory, string_chunk_size);
    I8 *map_path = string;
    bytes_read = 0;
    while ((*string++ = fgetc(f)) != 0 &&
           !feof(f))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(&global_level_memory,
                                   string_chunk_size,
                                   1);
        }
    }

    // NOTE(tbt): read texture asset path
    string = arena_allocate(&global_frame_memory, string_chunk_size);
    I8 *texture_path = string;
    bytes_read = 0;
    while ((*string++ = fgetc(f)) != 0 &&
           !feof(f))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(&global_frame_memory,
                                   string_chunk_size,
                                   1);
        }
    }

    // NOTE(tbt): read sound asset path
    string = arena_allocate(&global_frame_memory, string_chunk_size);
    I8 *sound_path = string;
    bytes_read = 0;
    while ((*string++ = fgetc(f)) != 0 &&
           !feof(f))
    {
        if (!(++bytes_read % string_chunk_size))
        {
            arena_allocate_aligned(&global_frame_memory,
                                   string_chunk_size,
                                   1);
        }
    }

    // NOTE(tbt): reconstruct entity
    entity->next = NULL;
    entity->flags = e.flags;
    entity->bounds = RECTANGLE(e.x, e.y, e.w, e.h);
    entity->colour = COLOUR(e.r, e.g, e.b, e.a);
    entity->speed = e.speed;
    entity->frame = e.frame;
    entity->animation_length = e.animation_length;
    entity->animation_speed = e.animation_speed;
    entity->animation_start = e.animation_start;
    entity->animation_end = e.animation_end;
    entity->gradient.tl.r = e.gradient_tl_r;
    entity->gradient.tl.g = e.gradient_tl_g;
    entity->gradient.tl.b = e.gradient_tl_b;
    entity->gradient.tl.a = e.gradient_tl_a;
    entity->gradient.tr.r = e.gradient_tr_r;
    entity->gradient.tr.g = e.gradient_tr_g;
    entity->gradient.tr.b = e.gradient_tr_b;
    entity->gradient.tr.a = e.gradient_tr_a;
    entity->gradient.bl.r = e.gradient_bl_r;
    entity->gradient.bl.g = e.gradient_bl_g;
    entity->gradient.bl.b = e.gradient_bl_b;
    entity->gradient.bl.a = e.gradient_bl_a;
    entity->gradient.br.r = e.gradient_br_r;
    entity->gradient.br.g = e.gradient_br_g;
    entity->gradient.br.b = e.gradient_br_b;
    entity->gradient.br.a = e.gradient_br_a;
    if (texture_path[0])
    {
        entity->texture = asset_from_path(texture_path);
    }
    else
    {
        entity->texture = NULL;
    }

    if (sound_path[0])
    {
        entity->sound = asset_from_path(sound_path);
        load_audio(entity->sound);
    }
    else
    {
        entity->sound = NULL;
    }

    if (map_path[0])
    {
        entity->level_transport = map_path;
    }
    else
    {
        entity->level_transport = NULL;
    }

    return entity;
}

internal void
unload_currently_loaded_assets(OpenGLFunctions *gl)
{
    Asset *loaded_asset = global_loaded_assets;
    while (loaded_asset)
    {
        switch (loaded_asset->type)
        {
            case ASSET_TYPE_TEXTURE:
            {
                unload_texture(gl, loaded_asset);
                break;
            }
            case ASSET_TYPE_AUDIO:
            {
                unload_audio(loaded_asset);
                break;
            }
            default:
            {
                fprintf(stderr,
                        "skipping unloading asset '%s'...\n",
                        loaded_asset->path);
                break;
            }
        }
        loaded_asset = loaded_asset->next_loaded;
    }
}

// NOTE(tbt): returns 1 on success, 0 on failure, -1 if the file could not be opened
internal I32
load_map(OpenGLFunctions *gl,
         I8 *path)
{
    FILE *f;
    U32 err;

    if (!path) { return 0; }
    if (!(*path))
    {
        fprintf(stderr, "could not load map - empty path.\n");
        return -1;
    }

    // NOTE(tbt): open file to read from
    f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "could not load map - '%s' does not exist.\n", path);
        return -1;
    }

    // NOTE(tbt): clear state left from previous loaded map
    arena_free_all(&global_level_memory);
    global_map.entities = NULL;

    // NOTE(tbt): save path
    if (path != global_map.path)
    {
        strncpy(global_map.path, path, sizeof(global_map.path));
    }

    unload_currently_loaded_assets(gl);
    
    // NOTE(tbt): read map header
    MapInFile header;
    err = fread(&header, 1, sizeof(header), f);
    if(err != sizeof(header))
    {
        fprintf(stderr, "error loading map - failure reading map header.\n");
        fclose(f);
        return 0;
    }

    // NOTE(tbt): read tiles
    global_map.tilemap_width = header.tilemap_width;
    global_map.tilemap_height = header.tilemap_height;
    global_map.tilemap = arena_allocate(&global_level_memory,
                                        header.tilemap_height *
                                        header.tilemap_width *
                                        sizeof(*global_map.tilemap));

    for (I32 tile_index = 0;
         tile_index < header.tilemap_width * header.tilemap_height;
         ++tile_index)
    {
        Tile tile;

        // NOTE(tbt): read main tile data
        TileInFile t;
        err = fread(&t, 1, sizeof(t), f);
        if (err != sizeof(t))
        {
            fprintf(stderr, "error loading map - failure reading tiles.\n");
            fclose(f);
            return 0;
        }

        // NOTE(tbt): read asset path
        U32 string_chunk_size = 32; // NOTE(tbt): allocate for string in chunks of
                                    //            32 bytes
        I8 *string = arena_allocate(&global_frame_memory, string_chunk_size);
        I8 *path = string;
        U32 bytes_read = 0;
        while ((*string++ = fgetc(f)) != 0 &&
               !feof(f))
        {
            if (!(++bytes_read % string_chunk_size))
            {
                arena_allocate_aligned(&global_frame_memory,
                                       string_chunk_size, 1);
            }
        }

        // NOTE(tbt): reconstruct tile
        tile.sub_texture.min_x = t.sub_texture.min_x;
        tile.sub_texture.min_y = t.sub_texture.min_y;
        tile.sub_texture.max_x = t.sub_texture.max_x;
        tile.sub_texture.max_y = t.sub_texture.max_y;
        tile.solid = t.solid;
        tile.visible = t.visible;
        if (strlen(path))
        {
            tile.texture = asset_from_path(path);
        }
        else
        {
            tile.texture = NULL;
        }

        global_map.tilemap[tile_index] = tile;
    }

    // NOTE(tbt): read entities
    GameEntity *last_entity = NULL;
    global_map.entity_count = header.entity_count;

    for (I32 entity_index = 0;
         entity_index < header.entity_count;
         ++entity_index)
    {
        GameEntity *entity;
        
        switch (header.version)
        {
            case 0:
            {
                entity = deserialise_entity_v0(gl, f);
                break;
            }
            case 1:
            {
                entity = deserialise_entity_v1(gl, f);
                break;
            }
            default:
            {
                fprintf(stderr, "error loading map - unrecognised file version.\n");
                fclose(f);
                return 0;
            }
        }

        if (!entity)
        {
            fprintf(stderr,
                    "error loading map - "
                    "failure while loading entities. read %d of %u\n",
                    entity_index,
                    header.entity_count);
            fclose(f);
            return 0;
        }

        if (last_entity) { last_entity->next = entity; }
        else { global_map.entities = entity; }
        last_entity = entity;
    }

    fprintf(stderr,
            "successfully read %u entities, %u tiles from map '%s'.\n",
            header.entity_count,
            header.tilemap_width * header.tilemap_height,
            global_map.path);

    fclose(f);

    return 1;
}

internal void
create_new_map(OpenGLFunctions *gl,
               U32 tilemap_width,
               U32 tilemap_height)
{
    global_map.path[0] = 0;

    unload_currently_loaded_assets(gl);
    
    // NOTE(tbt): clear state left from previous loaded map
    arena_free_all(&global_level_memory);
    memset(&global_map, 0, sizeof(global_map));

    // NOTE(tbt): setup new map
    global_map.tilemap_width = tilemap_width;
    global_map.tilemap_height = tilemap_height;
    global_map.tilemap = arena_allocate(&global_level_memory,
                                        tilemap_width * tilemap_height *
                                        sizeof(*global_map.tilemap));
}


