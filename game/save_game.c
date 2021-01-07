/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 02 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    LOAD_SAVE_GAME_STATUS_SUCCESS =  1,
    LOAD_SAVE_GAME_STATUS_FAILURE =  0,
    LOAD_SAVE_GAME_STATUS_NO_MAP  = -1,
    LOAD_SAVE_GAME_STATUS_NO_SAVE = -2,
};

internal I32
load_map_and_get_most_recent_save(OpenGLFunctions *gl,
                                  I8 *path)
{
    FILE *f, *save_file;
    U32 err;

    // NOTE(tbt): 'validate' path
    if (!path) { return 0; }
    if (!(*path))
    {
        fprintf(stderr, "loading save game: could not load map - empty path.\n");
        return LOAD_SAVE_GAME_STATUS_NO_MAP;
    }

    // NOTE(tbt): generate path for save game
    I8 *save_path = arena_allocate(&global_frame_memory,
                                   strlen(path) + 6);
    strcpy(save_path, path);
    strcat(save_path, ".save");

    // NOTE(tbt): open the save file and ensure it exists
    save_file = fopen(save_path, "rb");
    if (!save_file)
    {
        fprintf(stderr, "could not find a save for '%s'.\n", path);
        return LOAD_SAVE_GAME_STATUS_NO_SAVE;
    }

    // NOTE(tbt): open the map file
    f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "could not load save game - map at path '%s' does not exist.\n", path);
        return LOAD_SAVE_GAME_STATUS_NO_MAP;
    }

    // NOTE(tbt): clear state left from previous loaded map
    arena_free_all(&global_level_memory);
    global_map.entities = NULL;
    unload_all_assets(gl);
    
    // NOTE(tbt): read map header
    MapInFile header;
    err = fread(&header, 1, sizeof(header), f);
    if(err != sizeof(header))
    {
        fprintf(stderr, "error loading save game - failure reading map header.\n");
        fclose(f);
        return LOAD_SAVE_GAME_STATUS_FAILURE;
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
            fprintf(stderr, "error loading save game - failure reading tiles.\n");
            fclose(f);
            return LOAD_SAVE_GAME_STATUS_FAILURE;
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

    // NOTE(tbt): read entities from save file, not original map file
    GameEntity *last_entity = NULL;

    assert(fread(&global_map.entity_count, sizeof(global_map.entity_count), 1, save_file) == 1);

    for (I32 entity_index = 0;
         entity_index < global_map.entity_count;
         ++entity_index)
    {
        GameEntity *entity;
        
        switch (header.version)
        {
            case 0:
            {
                entity = deserialise_entity_v0(gl, save_file);
                break;
            }
            case 1:
            {
                entity = deserialise_entity_v1(gl, save_file);
                break;
            }
            default:
            {
                fprintf(stderr, "error loading save game - unrecognised file version.\n");
                fclose(f);
                return LOAD_SAVE_GAME_STATUS_FAILURE;
            }
        }

        if (!entity)
        {
            fprintf(stderr,
                    "error loading save game - "
                    "failure while loading entities. read %d of %u\n",
                    entity_index,
                    global_map.entity_count);
            fclose(f);
            return LOAD_SAVE_GAME_STATUS_FAILURE;
        }

        if (last_entity) { last_entity->next = entity; }
        else { global_map.entities = entity; }
        last_entity = entity;
    }

    fprintf(stderr,
            "loading save game: successfully read %u entities, %u tiles from map '%s'.\n",
            header.entity_count,
            header.tilemap_width * header.tilemap_height,
            path);

    fclose(f);
    fclose(save_file);

    return LOAD_SAVE_GAME_STATUS_SUCCESS;
}

internal I32
load_most_recent_save_for_current_map(OpenGLFunctions *gl)
{
    load_map_and_get_most_recent_save(gl, global_map.path);
}

internal B32
save_game(void)
{
    // NOTE(tbt): append "_" to end of path for temporary file
    I8 *temp_path = arena_allocate(&global_frame_memory,
                                   strlen(global_map.path) + 2);
    strcpy(temp_path, global_map.path);
    strcat(temp_path, "_");

    // NOTE(tbt): generate path for save game
    I8 *save_path = arena_allocate(&global_frame_memory,
                                   strlen(global_map.path) + 6);
    strcpy(save_path, global_map.path);
    strcat(save_path, ".save");

    // NOTE(tbt): write to temp file to prevent loosing data if the write fails
    FILE *f = fopen(temp_path, "wb");
    if (!f)
    {
        fprintf(stderr,
                "error saving game: could not open path '%s' - %s\n",
                temp_path,
                strerror(errno));
        return false;
    }

    // NOTE(tbt): write the number of entities in the file
    fwrite(&global_map.entity_count, sizeof(&global_map.entity_count), 1, f);

    // NOTE(tbt): actually write the entities
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
    if (0 == rename(temp_path, save_path))
    {
        fprintf(stderr,
                "successfully saved %u entities\n",
                global_map.entity_count);

        return true;
    }
    else
    {
        fprintf(stderr,
                "error saving game: failure while renaming temp file\n",
                temp_path);
        return false;
    }
}

