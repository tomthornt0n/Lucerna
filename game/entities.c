/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 23 Dec 2020
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    ENTITY_FLAG_PLAYER_MOVEMENT = 1 << 0,
    ENTITY_FLAG_RENDER_TEXTURE  = 1 << 1,
    ENTITY_FLAG_COLOURED        = 1 << 2,
    ENTITY_FLAG_ANIMATED        = 1 << 3,
    ENTITY_FLAG_DYNAMIC         = 1 << 4,
    ENTITY_FLAG_CAMERA_FOLLOW   = 1 << 5,
    ENTITY_FLAG_DELETED         = 1 << 6,
};

struct GameEntity
{
    GameEntity *next;
    U64 flags;
    Rectangle bounds;
    Asset *texture;
    SubTexture *sub_texture;
    Colour colour;
    F32 speed;
    F32 x_vel, y_vel;
    U32 frame, animation_length;
    U64 animation_speed, animation_clock;
};

#define TILE_SIZE 64

struct Tile
{
    Asset *texture;
    SubTexture sub_texture;
    B32 solid;
    B32 visible;
};

internal void
create_new_map(U32 tilemap_width,
               U32 tilemap_height,
               GameMap *map)
{
    memset(map, sizeof(*map), 0);
    map->arena = &global_level_memory;
    map->tilemap_width = tilemap_width;
    map->tilemap_height = tilemap_height;
    map->tilemap = arena_allocate(map->arena,
                                  tilemap_width * tilemap_height *
                                  sizeof(*map->tilemap));
}

internal SubTexture *global_player_left_texture;
internal SubTexture *global_player_right_texture;
internal SubTexture *global_player_up_texture;
internal SubTexture *global_player_down_texture;

internal GameEntity *
create_player(GameMap *map,
              F32 x, F32 y)
{
    GameEntity *result = arena_allocate(map->arena, sizeof(*result));
    ++(map->entity_count);

    result->flags = ENTITY_FLAG_PLAYER_MOVEMENT |
                    ENTITY_FLAG_RENDER_TEXTURE  |
                    ENTITY_FLAG_DYNAMIC         |
                    ENTITY_FLAG_ANIMATED        |
                    ENTITY_FLAG_CAMERA_FOLLOW;
    result->bounds = RECTANGLE(x, y, TILE_SIZE, TILE_SIZE);
    if (!(result->texture = asset_from_path(TEXTURE_PATH("spritesheet.png"))))
    {
        result->texture = new_asset_from_path(&global_static_memory,
                                              TEXTURE_PATH("spritesheet.png"));
    }
    result->sub_texture = global_player_down_texture;
    result->speed = 8.0f;
    result->animation_speed = 10;
    result->animation_length = 4;
    result->colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);

    result->next = map->entities;
    map->entities = result;

    return result;
}

internal GameEntity *
create_static_object(GameMap *map,
                     Rectangle rectangle,
                     I8 *texture,
                     SubTexture sub_texture)
{
    GameEntity *result = arena_allocate(map->arena, sizeof(*result));
    ++(map->entity_count);

    result->flags = ENTITY_FLAG_RENDER_TEXTURE;
    result->bounds = rectangle;
    if (!(result->texture = asset_from_path(texture)))
    {
        result->texture = new_asset_from_path(&global_static_memory, texture);
    }
    result->sub_texture = arena_allocate(&global_static_memory,
                                         sizeof(*result->sub_texture));
    *result->sub_texture = sub_texture;
    result->animation_length = 1;
    result->colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);

    result->next = map->entities;
    map->entities = result;

    return result;
}

internal Tile *
get_tile_at_position(GameMap map,
                     F32 x, F32 y)
{
    I32 tile_x = x / TILE_SIZE;
    I32 tile_y = y / TILE_SIZE;
    I32 index;

    if (tile_x > map.tilemap_width  ||
        tile_x < 0                  ||
        tile_y > map.tilemap_height ||
        tile_y < 0)
    {
        return NULL;
    }

    index = tile_y * map.tilemap_width + tile_x;
    return map.tilemap + (index);
}

internal Tile *
get_tile_under_cursor(PlatformState *input,
                      GameMap map)
{
    return get_tile_at_position(map,
                                input->mouse_x + global_camera_x,
                                input->mouse_y + global_camera_y);
}

internal void
process_entities(OpenGLFunctions *gl,
                 PlatformState *input,
                 GameMap *map)
{
    GameEntity *entity, *previous = NULL;
    for (entity = map->entities;
         entity;
         entity = entity->next)
    {
        if (entity->flags & ENTITY_FLAG_DELETED)
        {
            --map->entity_count;

            if (previous)
            {
                previous->next = entity->next;
            }
            else
            {
                map->entities = entity->next;
            }
        }

        if (entity->flags & ENTITY_FLAG_PLAYER_MOVEMENT)
        {
            B32 moving = false;

            if (input->is_key_pressed[KEY_A])
            {
                entity->x_vel = -entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->sub_texture = global_player_left_texture;
                moving = true;
            }
            else if (input->is_key_pressed[KEY_D])
            {
                entity->x_vel = entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->sub_texture = global_player_right_texture;
                moving = true;
            }
            else
            {
                entity->x_vel = 0.0f;
            }

            if (input->is_key_pressed[KEY_W])
            {
                entity->y_vel = -entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->sub_texture = global_player_up_texture;
                moving = true;
            }
            else if (input->is_key_pressed[KEY_S])
            {
                entity->y_vel = entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->sub_texture = global_player_down_texture;
                moving = true;
            }
            else
            {
                entity->y_vel = 0.0f;
            }

            if (!moving)
            {
                entity->flags &= ~ENTITY_FLAG_ANIMATED;
                entity->frame = 0;
            }
        }

        if (entity->flags & ENTITY_FLAG_DYNAMIC)
        {
            B32 colliding;
            Tile *tile; 
            Rectangle r;

            /* NOTE(tbt): x-axis collision check */
            colliding = false;
            r = entity->bounds;
            r.x += entity->x_vel;

            tile = get_tile_at_position(*map, r.x, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(*map, r.x + r.w, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(*map, r.x, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(*map, r.x + r.w, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }

            if (!colliding) { entity->bounds.x += entity->x_vel; }

            /* NOTE(tbt): y-axis collision check */
            colliding = false;
            r = entity->bounds;
            r.y += entity->y_vel;

            tile = get_tile_at_position(*map, r.x, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(*map, r.x + r.w, r.y);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(*map, r.x, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }
            tile = get_tile_at_position(*map, r.x + r.w, r.y + r.h);
            if (!tile || tile->solid) { colliding = true; }

            if (!colliding) { entity->bounds.y += entity->y_vel; }
        }

        if (entity->flags & ENTITY_FLAG_CAMERA_FOLLOW)
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

        if (entity->flags & ENTITY_FLAG_ANIMATED)
        {
            ++entity->animation_clock;
            if (entity->animation_clock == entity->animation_speed)
            {
                entity->frame = (entity->frame + 1) % entity->animation_length;
                entity->animation_clock = 0;
            }
        }

        if (entity->flags & ENTITY_FLAG_RENDER_TEXTURE)
        {
            Texture texture;

            if (entity->texture)
            {
                if (!entity->texture->loaded)
                {
                    load_texture(gl, entity->texture);
                }
                texture = entity->texture->texture;
            }
            else
            {
                texture = global_flat_colour_texture;
            }

            Colour colour;
            if (entity->flags & ENTITY_FLAG_COLOURED)
            {
                colour = entity->colour;
            }
            else
            {
                colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);
            }

            world_draw_sub_texture(entity->bounds,
                                   colour,
                                   texture,
                                   entity->sub_texture ?
                                   entity->sub_texture[entity->frame] :
                                   ENTIRE_TEXTURE);
        }

        previous = entity;
    }
}

internal void
render_tiles(OpenGLFunctions *gl,
             GameMap map,
             B32 editor_mode)
{
    I32 x, y;

    Rectangle viewport = RECTANGLE(global_camera_x, global_camera_y,
                                   global_renderer_window_w,
                                   global_renderer_window_h);

    I32 min_x = viewport.x / TILE_SIZE - 2;
    I32 min_y = viewport.y / TILE_SIZE - 2;
    I32 max_x = min_i((viewport.x + viewport.w) / TILE_SIZE + 2, map.tilemap_width);
    I32 max_y = min_i((viewport.y + viewport.h) / TILE_SIZE + 2, map.tilemap_height);

    for (x = min_x;
         x < max_x;
         ++x)
    {
        if (x > map.tilemap_width) { continue; }

        for (y = min_y;
             y < max_y;
             ++y)
        {
            Texture texture;

            if (y > map.tilemap_width) { continue; }

            Tile tile = map.tilemap[y * map.tilemap_width + x];

            if (!tile.visible) { continue; }

            if (tile.texture && tile.texture->path)
            {
                if (!tile.texture->loaded)
                {
                    load_texture(gl, tile.texture);
                }
                texture = tile.texture->texture;
            }
            else
            {
                texture = global_flat_colour_texture;
            }
            
            if (editor_mode && tile.solid)
            {
                world_draw_sub_texture(RECTANGLE((F32)x * TILE_SIZE,
                                                 (F32)y * TILE_SIZE,
                                                 (F32)TILE_SIZE,
                                                 (F32)TILE_SIZE),
                                        COLOUR(1.0f, 0.7f, 0.7f, 1.0f),
                                        texture,
                                        tile.sub_texture);
            }
            else
            {
                world_draw_sub_texture(RECTANGLE((F32)x * TILE_SIZE,
                                                 (F32)y * TILE_SIZE,
                                                 (F32)TILE_SIZE,
                                                 (F32)TILE_SIZE),
                                        COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                                        texture,
                                        tile.sub_texture);
            }
        }
    }
}

