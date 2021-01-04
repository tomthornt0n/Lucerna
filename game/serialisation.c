/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 02 Jan 2021
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
serialise_entity(GameEntity *entity,
                 FILE *f)
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
        fprintf(stderr, "failure while writing entity\n");
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
                        "failure while writing entity sub-texture - %s\n",
                        strerror(errno));
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
                    "failure while writing entity level transport asset - %s\n",
                    strerror(errno));
            return false;
        }
    }
    else
    {
        if (fwrite("", 1, 1, f) != 1)
        {
            fprintf(stderr,
                    "failure while writing entity level transport asset - %s\n",
                    strerror(errno));
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
                    "failure while writing entity texture asset - %s\n",
                    strerror(errno));
            return false;
        }
    }
    else
    {
        if (fwrite("", 1, 1, f) != 1)
        {
            fprintf(stderr,
                    "failure while writing entity texture asset - %s\n",
                    strerror(errno));
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
                    "failure while writing entity texture sound - %s\n",
                    strerror(errno));
            return false;
        }
    }
    else
    {
        if (fwrite("", 1, 1, f) != 1)
        {
            fprintf(stderr,
                    "failure while writing entity sound asset - %s\n",
                    strerror(errno));
            return false;
        }
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

