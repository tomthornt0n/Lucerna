/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 04 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    ENTITY_FLAG_PLAYER_MOVEMENT,     // NOTE(tbt): movement with WASD and directional animation
    ENTITY_FLAG_RENDER_TEXTURE,      // NOTE(tbt): draws a texture, sorted by the y coordinate of the bottom of `bounds`
    ENTITY_FLAG_TRIGGER_SOUND,       // NOTE(tbt): plays a sounds while an entity with `ENTITY_FLAG_PLAYER_MOVEMENT`'s `bounds` intersects it's `bounds`
    ENTITY_FLAG_ANIMATED,            // NOTE(tbt): increments `frame` every `animation_speed` frames - constrained between `animation_start` and `animation_end`
    ENTITY_FLAG_DYNAMIC,             // NOTE(tbt): integrates velocity to the position of `bounds`
    ENTITY_FLAG_CAMERA_FOLLOW,       // NOTE(tbt): update the camera position to keep in the centre of the screen
    ENTITY_FLAG_DELETED,             // NOTE(tbt): will be removed next frame
    ENTITY_FLAG_TELEPORT_PLAYER,     // NOTE(tbt): loads the map referenced by `level_transport` when an entity with `ENTITY_FLAG_PLAYER_MOVEMENT`'s `bounds` intersects it's `bounds`
    ENTITY_FLAG_RENDER_GRADIENT,     // NOTE(tbt): draws a gradient
    ENTITY_FLAG_AUDIO_FALLOFF,       // NOTE(tbt): sets the volume of it's audio source based on the distance to the centre of `bounds` from the centre of the screen
    ENTITY_FLAG_AUDIO_3D_PAN,        // NOTE(tbt): sets the pan of it's audio source based on the distance to the centre of `bounds` from the centre of the screen
    ENTITY_FLAG_RENDER_RECTANGLE,    // NOTE(tbt): sets the pan of it's audio source based on the distance to the centre of `bounds` from the centre of the screen
    ENTITY_FLAG_DESTROY_ON_CONTACT,  // NOTE(tbt): sets the deleted flag if it comes into contact with a solid tile
    ENTITY_FLAG_LIMIT_RANGE,         // NOTE(tbt): sets the deleted flag once a specific range from it's initial position has been exceeded
    ENTITY_FLAG_CHECKPOINT,          // NOTE(tbt): saves the game when the player comes into contact with it
    ENTITY_FLAG_PROJECTILE,          // NOTE(tbt): set if it is allocated from the projectile pool

    ENTITY_FLAG_COUNT
};

#define ENTITY_FLAG_TO_STRING(_flag) ((_flag) == ENTITY_FLAG_PLAYER_MOVEMENT     ? "player movement"    : \
                                      (_flag) == ENTITY_FLAG_RENDER_TEXTURE      ? "render texture"     : \
                                      (_flag) == ENTITY_FLAG_TRIGGER_SOUND       ? "trigger sound"      : \
                                      (_flag) == ENTITY_FLAG_ANIMATED            ? "animated"           : \
                                      (_flag) == ENTITY_FLAG_DYNAMIC             ? "dynamic"            : \
                                      (_flag) == ENTITY_FLAG_CAMERA_FOLLOW       ? "camera follow"      : \
                                      (_flag) == ENTITY_FLAG_DELETED             ? "deleted"            : \
                                      (_flag) == ENTITY_FLAG_TELEPORT_PLAYER     ? "teleport player"    : \
                                      (_flag) == ENTITY_FLAG_RENDER_GRADIENT     ? "render gradient"    : \
                                      (_flag) == ENTITY_FLAG_AUDIO_FALLOFF       ? "audio falloff"      : \
                                      (_flag) == ENTITY_FLAG_AUDIO_3D_PAN        ? "audio 3d pan"       : \
                                      (_flag) == ENTITY_FLAG_RENDER_RECTANGLE    ? "render rectangle"   : \
                                      (_flag) == ENTITY_FLAG_DESTROY_ON_CONTACT  ? "destroy on contact" : \
                                      (_flag) == ENTITY_FLAG_LIMIT_RANGE         ? "limit range"        : \
                                      (_flag) == ENTITY_FLAG_CHECKPOINT          ? "checkpoint"         : NULL)

struct GameEntity
{
    GameEntity *next;
    U64 flags;

    Rectangle bounds;         // NOTE(tbt): used for collision checks and rendering
    Asset *texture;
    Asset *sound;
    SubTexture *sub_texture;  // NOTE(tbt): an array of SubTextures. ENTITY_FLAG_RENDER_TEXTURE uses *sub_texture by default, or sub_texture[frame] if ENTITY_FLAG_ANIMATED is set
    Colour colour;            // NOTE(tbt): used by ENTITY_FLAG_RENDER_RECTANGLE, ignored by ENTITY_FLAG_RENDER_TEXTURE
    F32 speed;
    F32 x_vel, y_vel;         // NOTE(tbt): added to bounds.x and bounds.y respectively if ENTITY_FLAG_DYNAMIC is set
    F32 rotation;             // NOTE(tbt): angle in radians to render at. ignored by collision checks
    U32 frame;
    U32 animation_length;     // NOTE(tbt): number of elements in the sub_texture array
    U32 animation_start, animation_end; // NOTE(tbt): loop within this range
    F64 animation_speed, animation_clock;
    I8 *level_transport;      // NOTE(tbt): map with this path is loaded if bounds intersects an entity with ENTITY_FLAG_PLAYER_MOVEMENT
    Gradient gradient;
    F32 cooldown;             // NOTE(tbt): cooldown for weapons

    // NOTE(tbt): remove entity when the distance between `bounds.xy` and the initial position exceeds `range`
    F32 range; // NOTE(tbt): range does not need to be serialised as it is only used on projectiles
    F32 initial_x, initial_y;

    B32 colliding_with_player;
};

internal GameEntity *global_editor_selected_entity;

#define PROJECTILE_POOL_SIZE 4
internal GameEntity global_projectile_pool[PROJECTILE_POOL_SIZE];
internal GameEntity *global_projectile_free_list = NULL;

#define TILE_SIZE 64
internal F32 one_over_tile_size = 1.0f / TILE_SIZE;

struct Tile
{
    Asset *texture;
    SubTexture sub_texture;
    B32 solid;
    B32 visible;
};

#define PLAYER_FIRE_RATE 0.5f
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
    result->speed = 300.0f;
    result->animation_speed = 0.18;
    result->animation_length = 16;
    result->colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);

    result->texture = asset_from_path(TEXTURE_PATH("spritesheet.png"));
    load_texture(gl, result->texture);
    assert(result->texture->loaded);
    result->sub_texture = slice_animation(result->texture->texture,
                                          48.0f, 0.0f, 16.0f, 16.0f,
                                          4, 4);

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
    result->colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);
    result->texture = asset_from_path(texture);
    result->sub_texture = arena_allocate(&global_static_memory,
                                         sizeof(*result->sub_texture));
    *result->sub_texture = sub_texture;

    result->next = global_map.entities;
    global_map.entities = result;

    return result;
}

internal GameEntity *
create_laser_projectile(F32 x, F32 y,
                        F32 target_x, F32 target_y,
                        F32 speed)
{
    if (!global_projectile_free_list) { return NULL; }
    GameEntity *result = global_projectile_free_list;
    global_projectile_free_list = global_projectile_free_list->next;
    ++(global_map.entity_count);

    result->flags = BIT(ENTITY_FLAG_RENDER_RECTANGLE)   |
                    BIT(ENTITY_FLAG_TRIGGER_SOUND)      |
                    BIT(ENTITY_FLAG_DYNAMIC)            |
                    BIT(ENTITY_FLAG_DESTROY_ON_CONTACT) |
                    BIT(ENTITY_FLAG_LIMIT_RANGE)        |
                    BIT(ENTITY_FLAG_PROJECTILE);
    result->bounds = RECTANGLE(x, y, 4.0f, 32.0f);
    result->colour = COLOUR(1.0f, 0.0f, 0.0f, 0.5f);
    result->sound = asset_from_path(AUDIO_PATH("laser.wav"));
    result->initial_x = x;
    result->initial_y = y;
    result->range = 256.0f;

    F32 x_offset = target_x - x;
    F32 y_offset = target_y - y;

    F32 length_squared = x_offset * x_offset + y_offset * y_offset;

    result->x_vel = x_offset * reciprocal_sqrt_f(length_squared) * speed;
    result->y_vel = y_offset * reciprocal_sqrt_f(length_squared) * speed;

    result->rotation = atan(y_offset / x_offset) + 1.5707963268; // NOTE(tbt): offset by 90°

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

    I32 index = floor(y * one_over_tile_size) * global_map.tilemap_width + floor(x * one_over_tile_size);

    return global_map.tilemap + index;
}

enum
{
    ORIENT_N,
    ORIENT_E,
    ORIENT_S,
    ORIENT_W,
};

Gradient global_teleport_gradients[] = {
    { .tl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f}, 
      .tr = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f}, 
      .bl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f}, 
      .br = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f} },

    { .tl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f}, 
      .tr = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f}, 
      .bl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f}, 
      .br = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f} },

    { .tl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f}, 
      .tr = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f}, 
      .bl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f}, 
      .br = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f} },

    { .tl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f}, 
      .tr = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f}, 
      .bl = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 1.0f}, 
      .br = { .r = 0.92f, .g = 0.88f, .b = 0.90f, .a = 0.0f} }
};

internal GameEntity *
create_teleport(Rectangle rectangle,
                I32 orient,
                I8 *level)
{
    GameEntity *result = arena_allocate(&global_level_memory, sizeof(*result));
    ++(global_map.entity_count);

    result->flags = BIT(ENTITY_FLAG_RENDER_GRADIENT) |
                    BIT(ENTITY_FLAG_TELEPORT_PLAYER);
    result->bounds = rectangle;
    result->level_transport = level;
    result->gradient = global_teleport_gradients[orient];
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
                 PlatformState *input,
                 F64 frametime_in_s)
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

            // NOTE(tbt): return deleted projectiles to the free list
            if (entity->flags & BIT(ENTITY_FLAG_PROJECTILE))
            {
                entity->next = global_projectile_free_list;
                global_projectile_free_list = entity;
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_AUDIO_FALLOFF) ||
            entity->flags & BIT(ENTITY_FLAG_AUDIO_3D_PAN) &&
            entity->sound)
        {
            I32 screen_centre_x = global_camera_x + (global_renderer_window_w >> 1);
            I32 screen_centre_y = global_camera_y + (global_renderer_window_h >> 1);
            I32 bounds_centre_x = entity->bounds.x + entity->bounds.w * 0.5f;
            I32 bounds_centre_y = entity->bounds.y + entity->bounds.h * 0.5f;

            I32 x_offset = screen_centre_x - bounds_centre_x;
            I32 y_offset = screen_centre_y - bounds_centre_y;

            if (entity->flags & BIT(ENTITY_FLAG_AUDIO_FALLOFF))
            {
                F32 level = 1.0f / (x_offset * x_offset + y_offset * y_offset) * entity->bounds.w;
                set_audio_source_level(entity->sound, clamp_f(level, 0.0f, 1.0f));
            }

            if (entity->flags & BIT(ENTITY_FLAG_AUDIO_3D_PAN))
            {
                F32 pan = ((F32)x_offset / entity->bounds.w * 0.5f);
                pan /= 4.0f;
                pan += 0.5f;
                set_audio_source_pan(entity->sound, 1.0f - pan);
            }
        }

        B32 colliding_with_player = false;

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
                    load_most_recent_save_for_current_map(gl);

                    return;
                }

                if (entity->flags & BIT(ENTITY_FLAG_CHECKPOINT) &&
                    !entity->colliding_with_player)
                {
                    save_game();
                }

                colliding_with_player = true;
                break;
            }
        }
        entity->colliding_with_player = colliding_with_player;

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

            // HACK(tbt): entities other than the player will be able to shoot
            // TODO(tbt): give shooting it's own flag
            if (entity->cooldown < 0.0f)
            {
                if (input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
                {
                    create_laser_projectile(entity->bounds.x + entity->bounds.w * 0.5f + entity->x_vel * frametime_in_s,
                                            entity->bounds.y + entity->bounds.w * 0.5f + entity->y_vel * frametime_in_s,
                                            input->mouse_x + global_camera_x,
                                            input->mouse_y + global_camera_y,
                                            512.0f);
                    entity->cooldown = PLAYER_FIRE_RATE;
                }
            }
            else
            {
                entity->cooldown -= frametime_in_s;
            }
        }

        // BUG(tbt): only checks each corner, so doesn't work for entities
        //           larger than `TILE_SIZE`
        // TODO(tbt): step through each edge in intervals of TILE_SIZE and
        //            check for collisions

        B32 colliding;
        Tile *tile; 
        Rectangle new_bounds;

        // NOTE(tbt): x-axis collision check
        colliding = false;
        new_bounds = entity->bounds;
        new_bounds.x += entity->x_vel * frametime_in_s;

        tile = get_tile_at_position(new_bounds.x, new_bounds.y);
        if (!tile || tile->solid) { colliding = true; }
        tile = get_tile_at_position(new_bounds.x + new_bounds.w, new_bounds.y);
        if (!tile || tile->solid) { colliding = true; }
        tile = get_tile_at_position(new_bounds.x, new_bounds.y + new_bounds.h);
        if (!tile || tile->solid) { colliding = true; }
        tile = get_tile_at_position(new_bounds.x + new_bounds.w, new_bounds.y + new_bounds.h);
        if (!tile || tile->solid) { colliding = true; }

        if (!colliding)
        {
            if (entity->flags & BIT(ENTITY_FLAG_DYNAMIC)) { entity->bounds.x = new_bounds.x; }
        }
        else if (entity->flags & BIT(ENTITY_FLAG_DESTROY_ON_CONTACT))
        {
            entity->flags |= BIT(ENTITY_FLAG_DELETED);
        }

        // NOTE(tbt): y-axis collision check
        colliding = false;
        new_bounds = entity->bounds;
        new_bounds.y += entity->y_vel * frametime_in_s;

        tile = get_tile_at_position(new_bounds.x, new_bounds.y);
        if (!tile || tile->solid) { colliding = true; }
        tile = get_tile_at_position(new_bounds.x + new_bounds.w, new_bounds.y);
        if (!tile || tile->solid) { colliding = true; }
        tile = get_tile_at_position(new_bounds.x, new_bounds.y + new_bounds.h);
        if (!tile || tile->solid) { colliding = true; }
        tile = get_tile_at_position(new_bounds.x + new_bounds.w, new_bounds.y + new_bounds.h);
        if (!tile || tile->solid) { colliding = true; }

        if (!colliding)
        {
            if (entity->flags & BIT(ENTITY_FLAG_DYNAMIC)) { entity->bounds.y = new_bounds.y; }
        }
        else if (entity->flags & BIT(ENTITY_FLAG_DESTROY_ON_CONTACT))
        {
            entity->flags |= BIT(ENTITY_FLAG_DELETED);
        }
        

        if (entity->flags & BIT(ENTITY_FLAG_CAMERA_FOLLOW))
        {
            F32 camera_centre_x = global_camera_x +
                                  global_renderer_window_w * 0.5f;
            F32 camera_centre_y = global_camera_y +
                                  global_renderer_window_h * 0.5f;

            F32 camera_to_entity_x_vel = (entity->bounds.x + entity->bounds.w * 0.5f - camera_centre_x) * 0.5f;
            F32 camera_to_entity_y_vel = (entity->bounds.y + entity->bounds.h * 0.5f - camera_centre_y) * 0.5f;

            F32 camera_to_mouse_x_vel = ((input->mouse_x + global_camera_x) - camera_centre_x) * 0.5f;
            F32 camera_to_mouse_y_vel = ((input->mouse_y + global_camera_y) - camera_centre_y) * 0.5f;

            F32 camera_x_vel = (camera_to_entity_x_vel * 1.8f +
                                camera_to_mouse_x_vel * 0.2f) /
                               2.0f;
            F32 camera_y_vel = (camera_to_entity_y_vel * 1.8f +
                                camera_to_mouse_y_vel * 0.2f) /
                               2.0f;

            set_camera_position(global_camera_x + camera_x_vel,
                                global_camera_y + camera_y_vel);
        }

        if (entity->flags & BIT(ENTITY_FLAG_LIMIT_RANGE))
        {
            F32 x_offset = entity->bounds.x - entity->initial_x;
            F32 y_offset = entity->bounds.y - entity->initial_y;

            F32 range_squared = x_offset * x_offset + y_offset * y_offset;
            if (range_squared > entity->range * entity->range)
            {
                entity->flags |= BIT(ENTITY_FLAG_DELETED);
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_ANIMATED))
        {
            entity->animation_clock += frametime_in_s;

            if (entity->animation_clock >= entity->animation_speed)
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

        if (entity->flags & BIT(ENTITY_FLAG_RENDER_RECTANGLE))
        {
            if (entity->rotation > -0.003f &&
                entity->rotation <  0.003f)
            {
                world_fill_rectangle(entity->bounds, entity->colour);
            }
            else
            {
                world_fill_rotated_rectangle(entity->bounds,
                                             entity->rotation,
                                             entity->colour);
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_RENDER_TEXTURE))
        {
            if (!entity->texture) { continue; }

            U32 sort_depth = (I32)(entity->bounds.y + entity->bounds.h) % // NOTE(tbt): sort by the y-coordinate of the bottom of the entity
                             (UI_SORT_DEPTH - 1);                         // NOTE(tbt): keep below the UI render layer
            sort_depth += 1;                                              // NOTE(tbt): allow a a layer to draw the tilemap

            if (entity->rotation > -0.003f &&
                entity->rotation <  0.003f)
            {
                draw_sub_texture(gl,
                                 entity->bounds,
                                 COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                                 entity->texture,
                                 entity->sub_texture ?
                                 entity->sub_texture[entity->flags &
                                                     BIT(ENTITY_FLAG_ANIMATED) ?
                                                     entity->frame : 0] :
                                 ENTIRE_TEXTURE,
                                 sort_depth,
                                 global_projection_matrix);
            }
            else
            {
                draw_rotated_sub_texture(gl,
                                         entity->bounds,
                                         entity->rotation,
                                         COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                                         entity->texture,
                                         entity->sub_texture ?
                                         entity->sub_texture[entity->flags &
                                                             BIT(ENTITY_FLAG_ANIMATED) ?
                                                             entity->frame : 0] :
                                         ENTIRE_TEXTURE,
                                         sort_depth,
                                         global_projection_matrix);
            }
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

    I32 min_x = viewport.x * one_over_tile_size - 2;
    I32 min_y = viewport.y * one_over_tile_size - 2;
    I32 max_x = min_i((viewport.x + viewport.w) * one_over_tile_size + 2, global_map.tilemap_width);
    I32 max_y = min_i((viewport.y + viewport.h) * one_over_tile_size + 2, global_map.tilemap_height);

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

