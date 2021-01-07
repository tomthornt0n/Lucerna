/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 02 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
                temp_path,
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
        if (!serialise_entity(entity, f))
        {
            fclose(f);
            return false;
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

    unload_all_assets(gl);
    
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

    unload_all_assets(gl);
    
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

