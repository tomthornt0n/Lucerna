/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 23 Dec 2020
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
save_map(Asset *asset)
{
    if (!asset ||
        !asset->path)
    {
        fprintf(stderr, "error saving map: asset was NULL\n");
        return false;
    }

    /* NOTE(tbt): append "_" to end of path */
    I8 *temp_path = arena_allocate(&global_frame_memory,
                                   strlen(asset->path) + 2);
    strcpy(temp_path, asset->path);
    strcat(temp_path, "_");

    /* NOTE(tbt): write to temp file to prevent loosing data if the write fails */
    FILE *f = fopen(temp_path, "wb");
    if (!f)
    {
        fprintf(stderr,
                "error saving map: could not open path '%s' - %s\n",
                strerror(errno));
        return false;
    }

    /* NOTE(tbt): write map header */
    MapInFile m = {0};
    m.entity_count = asset->map.entity_count;
    m.tilemap_width = asset->map.tilemap_width;
    m.tilemap_height = asset->map.tilemap_height;

    if (fwrite(&m, sizeof(m), 1, f) != 1)
    {
        fprintf(stderr,
                "error saving map '%s': failure while writing map header - %s\n",
                temp_path,
                strerror(errno));
        fclose(f);
        return false;
    }

    /* NOTE(tbt): write tiles */
    for (I32 tile_index = 0;
         tile_index < asset->map.tilemap_width * asset->map.tilemap_height;
         ++tile_index)
    {
        TileInFile t;

        t.sub_texture.min_x = asset->map.tilemap[tile_index].sub_texture.min_x;
        t.sub_texture.min_y = asset->map.tilemap[tile_index].sub_texture.min_y;
        t.sub_texture.max_x = asset->map.tilemap[tile_index].sub_texture.max_x;
        t.sub_texture.max_y = asset->map.tilemap[tile_index].sub_texture.max_y;

        t.solid = asset->map.tilemap[tile_index].solid;
        t.visible = asset->map.tilemap[tile_index].visible;

        if (fwrite(&t, sizeof(t), 1, f) != 1)
        {
            fprintf(stderr,
                    "error saving map '%s': failure while writing tile sub-texture - %s\n",
                    temp_path,
                    strerror(errno));
            fclose(f);
            return false;
        }

        if (asset->map.tilemap[tile_index].texture)
        {
            U32 texture_path_size =
                strlen(asset->map.tilemap[tile_index].texture->path) + 1;

            if (fwrite(asset->map.tilemap[tile_index].texture->path,
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

    /* NOTE(tbt): write entities */
    GameEntity *entity = asset->map.entities;
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

        e.speed = entity->speed;

        e.frame = entity->frame;
        e.animation_length = entity->animation_length;
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

        entity = entity->next;
    }

    fclose(f);

    /* NOTE(tbt): rename temporary file over real file if successful */
    if (0 == rename(temp_path, asset->path))
    {
        fprintf(stderr,
                "successfully wrote %u entities, %u tiles.\n",
                asset->map.entity_count,
                asset->map.tilemap_width * asset->map.tilemap_height);

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

/* NOTE(tbt): returns 1 on success, 0 on failure, -1 if the file could not be
              opened
*/
internal I32
load_map(OpenGLFunctions *gl,
         Asset *asset)
{
    FILE *f;
    U32 err;

    if (!asset || !asset->path) { return 0; }
    if (asset->loaded)          { return 1;  }
    
    arena_free_all(&global_level_memory);
    asset->map.arena = &global_level_memory;
    asset->map.entities = NULL;
    asset->map.assets = NULL;

    f = fopen(asset->path, "rb");
    if (!f)
    {
        return -1;
    }

    /* NOTE(tbt): read map header */
    MapInFile header;
    err = fread(&header, 1, sizeof(header), f);
    if(err != sizeof(header))
    {
        fclose(f);
        return 0;
    }

    /* NOTE(tbt): read tiles */
    asset->map.tilemap_width = header.tilemap_width;
    asset->map.tilemap_height = header.tilemap_height;
    asset->map.tilemap = arena_allocate(&global_level_memory,
                                        header.tilemap_height *
                                        header.tilemap_width *
                                        sizeof(*asset->map.tilemap));

    for (I32 tile_index = 0;
         tile_index < header.tilemap_width * header.tilemap_height;
         ++tile_index)
    {
        Tile tile;

        /* NOTE(tbt): read main tile data */
        TileInFile t;
        err = fread(&t, 1, sizeof(t), f);
        if (err != sizeof(t))
        {
            return 0;
        }

        /* NOTE(tbt): read asset path */
        U32 string_chunk_size = 32; /* NOTE(tbt): allocate for string in chunks of
                                                  32 bytes
                                    */
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

        /* NOTE(tbt): reconstruct tile */
        tile.sub_texture.min_x = t.sub_texture.min_x;
        tile.sub_texture.min_y = t.sub_texture.min_y;
        tile.sub_texture.max_x = t.sub_texture.max_x;
        tile.sub_texture.max_y = t.sub_texture.max_y;
        tile.solid = t.solid;
        tile.visible = t.visible;
        if (strlen(path))
        {
            tile.texture = asset_from_path(path);
            if (!tile.texture)
            {
                tile.texture = new_asset_from_path(&global_static_memory, path);
            }

            /* NOTE(tbt): assumes that all assets from the previous level will
                          have been unloaded
            */
            if (!tile.texture->loaded)
            {
                load_texture(gl, tile.texture);
                if (!tile.texture->loaded)
                {
                    fclose(f);
                    return 0;
                }

                tile.texture->next_in_level = asset->map.assets;
                asset->map.assets = tile.texture;
            }
        }
        else
        {
            tile.texture = NULL;
        }

        asset->map.tilemap[tile_index] = tile;
    }

    /* NOTE(tbt): read entities */
    GameEntity *last_entity = NULL;
    asset->map.entity_count = header.entity_count;

    for (I32 entity_index = 0;
         entity_index < header.entity_count;
         ++entity_index)
    {
        GameEntity *entity = arena_allocate(&global_level_memory,
                                            sizeof(*entity));

        /* NOTE(tbt): read main entity data */
        EntityInFile e;
        err = fread(&e, 1, sizeof(e), f);
        if (err != sizeof(e))
        {
            fclose(f);
            return 0;
        }

        /* NOTE(tbt): read animation */
        entity->sub_texture = arena_allocate(&global_level_memory,
                                             sizeof(*entity->sub_texture) *
                                             e.animation_length);
        temporary_memory_begin(&global_level_memory);
        SubTextureInFile *ts = arena_allocate(&global_level_memory,
                                              sizeof(*ts) * e.animation_length);

        err = fread(ts, sizeof(*ts), e.animation_length, f);
        if (err != e.animation_length)
        {
            fclose(f);
            temporary_memory_end(&global_level_memory);
            return 0;
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

        /* NOTE(tbt): read asset path */
        U32 string_chunk_size = 32; /* NOTE(tbt): allocate for string in chunks of
                                                  32 bytes
                                    */
        I8 *string = arena_allocate(&global_frame_memory, string_chunk_size);
        I8 *path = string;
        U32 bytes_read = 0;
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

        /* NOTE(tbt): reconstruct entity */
        entity->next = NULL;
        entity->flags = e.flags;
        entity->bounds = RECTANGLE(e.x, e.y, e.w, e.h);
        entity->colour = COLOUR(e.r, e.g, e.b, e.a);
        entity->speed = e.speed;
        entity->frame = e.frame;
        entity->animation_length = e.animation_length;
        entity->animation_speed = e.animation_speed;
        if (strlen(path))
        {
            entity->texture = asset_from_path(path);
            if (!entity->texture)
            {
                entity->texture = new_asset_from_path(&global_static_memory, path);
            }

            /* NOTE(tbt): assumes that all assets from the previous level will
                          have been unloaded
            */
            if (!entity->texture->loaded)
            {
                load_texture(gl, entity->texture);
                if (!entity->texture->loaded)
                {
                    fclose(f);
                    return 0;
                }

                entity->texture->next_in_level = asset->map.assets;
                asset->map.assets = entity->texture;
            }
        }
        else
        {
            entity->texture = NULL;
        }

        if (last_entity) { last_entity->next = entity; }
        else { asset->map.entities = entity; }
        last_entity = entity;
    }

    fprintf(stderr,
            "successfully read %u entities, %u tiles from map '%s'.\n",
            header.entity_count,
            header.tilemap_width * header.tilemap_height,
            asset->path);

    fclose(f);
    asset->loaded = true;        
    asset->type = ASSET_TYPE_MAP;

    return 1;
}

internal void
unload_map(OpenGLFunctions *gl,
           Asset *asset)
{
    Asset *asset_in_map = asset->map.assets;
    while (asset_in_map)
    {
        switch (asset_in_map->type)
        {
            case ASSET_TYPE_TEXTURE:
            {
                unload_texture(gl, asset_in_map);
                break;
            }
            default:
            {
                fprintf(stderr,
                        "skipping unloading asset '%s'...\n",
                        asset_in_map->path);
                break;
            }
        }
        asset_in_map = asset_in_map->next_in_level;
    }
    asset->loaded = false;
}

