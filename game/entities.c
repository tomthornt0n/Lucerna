/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 01 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    ENTITY_FLAG_PLAYER_MOVEMENT, // NOTE(tbt): movement with WASD and directional animation
    ENTITY_FLAG_RENDER_TEXTURE,  // NOTE(tbt): draws a texture, sorted by the y coordinate of the bottom of `bounds`
    ENTITY_FLAG_TRIGGER_SOUND,   // NOTE(tbt): plays a sounds while an entity with `ENTITY_FLAG_PLAYER_MOVEMENT`'s `bounds` intersects it's `bounds`
    ENTITY_FLAG_ANIMATED,        // NOTE(tbt): increments `frame` every `animation_speed` frames - constrained between `animation_start` and `animation_end`
    ENTITY_FLAG_DYNAMIC,         // NOTE(tbt): integrates velocity to the position of `bounds`
    ENTITY_FLAG_CAMERA_FOLLOW,   // NOTE(tbt): update the camera position to keep in the centre of the screen
    ENTITY_FLAG_DELETED,         // NOTE(tbt): will be removed next frame
    ENTITY_FLAG_TELEPORT_PLAYER, // NOTE(tbt): loads the map referenced by `level_transport` when an entity with `ENTITY_FLAG_PLAYER_MOVEMENT`'s `bounds` intersects it's `bounds`
    ENTITY_FLAG_RENDER_GRADIENT, // NOTE(tbt): draws a gradient
    ENTITY_FLAG_AUDIO_FALLOFF,   // NOTE(tbt): sets the volume of it's audio source based on the distance to the centre of `bounds` from the centre of the screen
    ENTITY_FLAG_AUDIO_3D_PAN,    // NOTE(tbt): sets the pan of it's audio source based on the distance to the centre of `bounds` from the centre of the screen

    ENTITY_FLAG_COUNT
};

#define ENTITY_FLAG_TO_STRING(_flag) ((_flag) == ENTITY_FLAG_PLAYER_MOVEMENT  ? "player movement"  : \
                                      (_flag) == ENTITY_FLAG_RENDER_TEXTURE   ? "render texture"   : \
                                      (_flag) == ENTITY_FLAG_TRIGGER_SOUND    ? "trigger sound"    : \
                                      (_flag) == ENTITY_FLAG_ANIMATED         ? "animated"         : \
                                      (_flag) == ENTITY_FLAG_DYNAMIC          ? "dynamic"          : \
                                      (_flag) == ENTITY_FLAG_CAMERA_FOLLOW    ? "camera follow"    : \
                                      (_flag) == ENTITY_FLAG_DELETED          ? "deleted"          : \
                                      (_flag) == ENTITY_FLAG_TELEPORT_PLAYER  ? "teleport player"  : \
                                      (_flag) == ENTITY_FLAG_RENDER_GRADIENT  ? "render gradient"  : \
                                      (_flag) == ENTITY_FLAG_AUDIO_FALLOFF    ? "audio falloff"    : \
                                      (_flag) == ENTITY_FLAG_AUDIO_3D_PAN     ? "audio 3d pan"    : NULL)

struct GameEntity
{
    GameEntity *next;
    U64 flags;
    Rectangle bounds;
    Asset *texture;
    Asset *sound;
    SubTexture *sub_texture;
    Colour colour;
    F32 speed;
    F32 x_vel, y_vel;
    U32 frame, animation_length, animation_start, animation_end;
    U64 animation_speed, animation_clock;
    I8 *level_transport;
    Gradient gradient;
};

internal GameEntity *global_editor_selected_entity;

#define TILE_SIZE 64

struct Tile
{
    Asset *texture;
    SubTexture sub_texture;
    B32 solid;
    B32 visible;
};

internal GameEntity *
create_player(OpenGLFunctions *gl,
              F32 x, F32 y)
{
    GameEntity *result = arena_allocate(&global_level_memory, sizeof(*result));
    ++(global_map.entity_count);

    result->flags = BIT(ENTITY_FLAG_PLAYER_MOVEMENT) |
                    BIT(ENTITY_FLAG_RENDER_TEXTURE)  |
                    BIT(ENTITY_FLAG_DYNAMIC)         |
                    BIT(ENTITY_FLAG_ANIMATED)        |
                    BIT(ENTITY_FLAG_CAMERA_FOLLOW);
    result->bounds = RECTANGLE(x, y, TILE_SIZE, TILE_SIZE);

    result->texture = asset_from_path(TEXTURE_PATH("spritesheet.png"));
    load_texture(gl, result->texture);
    assert(result->texture->loaded);
    result->sub_texture = slice_animation(result->texture->texture,
                                          48.0f, 0.0f, 16.0f, 16.0f,
                                          4, 4);

    result->speed = 8.0f;
    result->animation_speed = 10;
    result->animation_length = 16;
    result->colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);

    result->next = global_map.entities;
    global_map.entities = result;

    return result;
}

internal GameEntity *
create_empty_entity(Rectangle rectangle)
{
    GameEntity *result = arena_allocate(&global_level_memory, sizeof(*result));
    ++(global_map.entity_count);

    result->bounds = rectangle;
    result->next = global_map.entities;
    global_map.entities = result;

    return result;
}

internal GameEntity *
create_static_object(Rectangle rectangle,
                     I8 *texture,
                     SubTexture sub_texture)
{
    GameEntity *result = arena_allocate(&global_level_memory, sizeof(*result));
    ++(global_map.entity_count);

    result->flags = BIT(ENTITY_FLAG_RENDER_TEXTURE);
    result->bounds = rectangle;
    result->texture = asset_from_path(texture);
    result->sub_texture = arena_allocate(&global_static_memory,
                                         sizeof(*result->sub_texture));
    *result->sub_texture = sub_texture;
    result->animation_length = 1;
    result->colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);

    result->next = global_map.entities;
    global_map.entities = result;

    return result;
}

internal Tile *
get_tile_at_position(F32 x, F32 y)
{
    if (x >= global_map.tilemap_width * TILE_SIZE  ||
        x < 0.0f                                   ||
        y >= global_map.tilemap_height * TILE_SIZE ||
        y < 0.0f)
    {
        return NULL;
    }

    I32 index = floor(y / TILE_SIZE) * global_map.tilemap_width + floor(x / TILE_SIZE);

    return global_map.tilemap + index;
}

enum
{
    ORIENT_N,
    ORIENT_E,
    ORIENT_S,
    ORIENT_W,
};

internal GameEntity *
create_teleport(Rectangle rectangle,
                I32 orient,
                I8 *level)
{
    Gradient teleport_gradients[] = {
        GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f),
                 COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f)),
    
        GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f),
                 COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f)),
    
        GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f),
                 COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f)),
    
        GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f),
                 COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f))
    };

    GameEntity *result = arena_allocate(&global_level_memory, sizeof(*result));
    ++(global_map.entity_count);

    result->flags = BIT(ENTITY_FLAG_RENDER_GRADIENT) |
                    BIT(ENTITY_FLAG_TELEPORT_PLAYER);
    result->bounds = rectangle;
    result->level_transport = level;
    result->gradient = teleport_gradients[orient];
    result->next = global_map.entities;
    global_map.entities = result;

    return result;
}

internal Tile *
get_tile_under_cursor(PlatformState *input)
{
    return get_tile_at_position(input->mouse_x + global_camera_x,
                                input->mouse_y + global_camera_y);
}

internal B32 save_map(void);
internal I32 load_map(OpenGLFunctions *gl, I8 *path);

internal void
process_entities(OpenGLFunctions *gl,
                 PlatformState *input)
{
    GameEntity *entity, *previous = NULL;
    for (entity = global_map.entities;
         entity;
         entity = entity->next)
    {
        if (entity->flags & BIT(ENTITY_FLAG_DELETED))
        {
            --global_map.entity_count;

            if (previous)
            {
                previous->next = entity->next;
            }
            else
            {
                global_map.entities = entity->next;
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_AUDIO_FALLOFF) ||
            entity->flags & BIT(ENTITY_FLAG_AUDIO_3D_PAN) &&
            entity->sound)
        {
            I32 screen_centre_x = global_camera_x + (global_renderer_window_w >> 1);
            I32 screen_centre_y = global_camera_y + (global_renderer_window_h >> 1);
            I32 bounds_centre_x = entity->bounds.x + entity->bounds.w / 2.0f;
            I32 bounds_centre_y = entity->bounds.y + entity->bounds.h / 2.0f;

            I32 x_offset = screen_centre_x - bounds_centre_x;
            I32 y_offset = screen_centre_y - bounds_centre_y;

            if (entity->flags & BIT(ENTITY_FLAG_AUDIO_FALLOFF))
            {
                F32 level = 1.0f / (x_offset * x_offset + y_offset * y_offset) * global_renderer_window_w;
                set_audio_source_level(entity->sound, clamp_f(level, 0.0f, 1.0f));
            }

            if (entity->flags & BIT(ENTITY_FLAG_AUDIO_3D_PAN))
            {
                F32 pan = ((F32)x_offset / (F32)(global_renderer_window_w >> 1));
                pan /= 4.0f;
                pan += 0.5f;
                set_audio_source_pan(entity->sound, 1.0f - pan);
            }
        }

        for (GameEntity *b = global_map.entities;
             b;
             b = b->next)
        {
            if (b->flags & BIT(ENTITY_FLAG_PLAYER_MOVEMENT) &&
                rectangles_are_intersecting(b->bounds, entity->bounds))
            {
                if (entity->flags & BIT(ENTITY_FLAG_TRIGGER_SOUND))
                {
                    play_audio_source(entity->sound);
                }
                if (entity->flags & BIT(ENTITY_FLAG_TELEPORT_PLAYER))
                {
                    // NOTE(tbt): set editor selected entity to NULL to prevent it
                    //            pointing to an entity which no longer exists
                    global_editor_selected_entity = NULL;

                    load_map(gl, entity->level_transport);

                    return;
                }
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_PLAYER_MOVEMENT))
        {
            B32 moving = false;

            if (input->is_key_pressed[KEY_A])
            {
                entity->x_vel = -entity->speed;
                entity->flags |= BIT(ENTITY_FLAG_ANIMATED);
                entity->animation_start = 4;
                entity->animation_end = 8;
                moving = true;
            }
            else if (input->is_key_pressed[KEY_D])
            {
                entity->x_vel = entity->speed;
                entity->flags |= BIT(ENTITY_FLAG_ANIMATED);
                entity->animation_start = 8;
                entity->animation_end = 12;
                moving = true;
            }
            else
            {
                entity->x_vel = 0.0f;
            }

            if (input->is_key_pressed[KEY_W])
            {
                entity->y_vel = -entity->speed;
                entity->flags |= BIT(ENTITY_FLAG_ANIMATED);
                entity->animation_start = 12;
                entity->animation_end = 16;
                moving = true;
            }
            else if (input->is_key_pressed[KEY_S])
            {
                entity->y_vel = entity->speed;
                entity->flags |= BIT(ENTITY_FLAG_ANIMATED);
                entity->animation_start = 0;
                entity->animation_end = 4;
                moving = true;
            }
            else
            {
                entity->y_vel = 0.0f;
            }

            if (!moving)
            {
                entity->flags &= ~BIT(ENTITY_FLAG_ANIMATED);
                entity->frame = 0;
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_DYNAMIC))
        {
            B32 colliding;
            Tile *tile; 
            Rectangle r;
            
            // BUG(tbt): only checks each corner, so doesn't work for entities
            //           larger than `TILE_SIZE`
            // TODO(tbt): step through each edge in intervals of TILE_SIZE and
            //            check for collisions

            // NOTE(tbt): x-axis collision check
            colliding = false;
            r = entity->bounds;
            r.x += entity->x_vel;

            tile = get_tile_at_position(r.x, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(r.x + r.w, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(r.x, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(r.x + r.w, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }

            if (!colliding) { entity->bounds.x += entity->x_vel; }

            // NOTE(tbt): y-axis collision check
            colliding = false;
            r = entity->bounds;
            r.y += entity->y_vel;

            tile = get_tile_at_position(r.x, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(r.x + r.w, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(r.x, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(r.x + r.w, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }

            if (!colliding) { entity->bounds.y += entity->y_vel; }
        }

        if (entity->flags & BIT(ENTITY_FLAG_CAMERA_FOLLOW))
        {
            F32 camera_centre_x = global_camera_x +
                                  global_renderer_window_w / 2.0f;
            F32 camera_centre_y = global_camera_y +
                                  global_renderer_window_h / 2.0f;

            F32 camera_to_entity_x_vel = (entity->bounds.x + entity->bounds.w / 2.0f - camera_centre_x) / 2.0f;
            F32 camera_to_entity_y_vel = (entity->bounds.y + entity->bounds.h / 2.0f - camera_centre_y) / 2.0f;

            F32 camera_to_mouse_x_vel = ((input->mouse_x + global_camera_x) - camera_centre_x) / 2;
            F32 camera_to_mouse_y_vel = ((input->mouse_y + global_camera_y) - camera_centre_y) / 2;

            F32 camera_x_vel = (camera_to_entity_x_vel * 1.8f +
                                camera_to_mouse_x_vel * 0.2f) /
                               2.0f;
            F32 camera_y_vel = (camera_to_entity_y_vel * 1.8f +
                                camera_to_mouse_y_vel * 0.2f) /
                               2.0f;

            set_camera_position(global_camera_x + camera_x_vel,
                                global_camera_y + camera_y_vel);
        }

        if (entity->flags & BIT(ENTITY_FLAG_ANIMATED))
        {
            ++entity->animation_clock;

            if (entity->animation_clock == entity->animation_speed)
            {
                I32 animation_length = entity->animation_end -
                                       entity->animation_start;
                if (animation_length > 0)
                {
                    entity->frame = (entity->frame + 1) %
                                    (entity->animation_end -
                                     entity->animation_start) +
                                    entity->animation_start;
                }
                entity->animation_clock = 0;
            }

            if (entity->frame > entity->animation_end)
            {
                entity->frame = entity->animation_end;
            }
            else if (entity->frame < entity->animation_start)
            {
                entity->frame = entity->animation_start;
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_RENDER_GRADIENT))
        {
            world_draw_gradient(entity->bounds, entity->gradient);
        }

        if (entity->flags & BIT(ENTITY_FLAG_RENDER_TEXTURE))
        {
            if (!entity->texture) { continue; }

            U32 sort_depth = (I32)(entity->bounds.y + entity->bounds.h) % // NOTE(tbt): sort by the y-coordinate of the bottom of the entity
                             (UI_SORT_DEPTH - 1);                         // NOTE(tbt): keep below the UI render layer
            sort_depth += 1;                                              // NOTE(tbt): allow a a layer to draw the tilemap

            draw_sub_texture(gl,
                             entity->bounds,
                             COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                             entity->texture,
                             entity->sub_texture ?
                             entity->sub_texture[entity->frame] :
                             ENTIRE_TEXTURE,
                             sort_depth,
                             global_projection_matrix);
        }

        previous = entity;
    }
}

internal void
render_tiles(OpenGLFunctions *gl,
             B32 editor_mode)
{
    I32 x, y;

    Rectangle viewport = RECTANGLE(global_camera_x, global_camera_y,
                                   global_renderer_window_w,
                                   global_renderer_window_h);

    I32 min_x = viewport.x / TILE_SIZE - 2;
    I32 min_y = viewport.y / TILE_SIZE - 2;
    I32 max_x = min_i((viewport.x + viewport.w) / TILE_SIZE + 2, global_map.tilemap_width);
    I32 max_y = min_i((viewport.y + viewport.h) / TILE_SIZE + 2, global_map.tilemap_height);

    for (x = min_x;
         x < max_x;
         ++x)
    {
        if (x > global_map.tilemap_width) { continue; }

        for (y = min_y;
             y < max_y;
             ++y)
        {
            Texture texture;

            if (y > global_map.tilemap_width) { continue; }

            Tile tile = global_map.tilemap[y * global_map.tilemap_width + x];

            if (!tile.visible || !tile.texture) { continue; }

            if (editor_mode && tile.solid)
            {
                world_draw_sub_texture(gl,
                                       RECTANGLE((F32)x * TILE_SIZE,
                                                 (F32)y * TILE_SIZE,
                                                 (F32)TILE_SIZE,
                                                 (F32)TILE_SIZE),
                                       COLOUR(1.0f, 0.7f, 0.7f, 1.0f),
                                       tile.texture,
                                       tile.sub_texture);
            }
            else
            {
                world_draw_sub_texture(gl,
                                       RECTANGLE((F32)x * TILE_SIZE,
                                                 (F32)y * TILE_SIZE,
                                                 (F32)TILE_SIZE,
                                                 (F32)TILE_SIZE),
                                       COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                                       tile.texture,
                                       tile.sub_texture);
            }
        }
    }
}

