/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 17 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    ENTITY_FLAG_PLAYER_MOVEMENT = 1 << 0,
    ENTITY_FLAG_RENDER_TEXTURE  = 1 << 1,
    ENTITY_FLAG_COLOURED        = 1 << 2,
    ENTITY_FLAG_ANIMATED        = 1 << 3,
    ENTITY_FLAG_DYNAMIC         = 1 << 4,
    ENTITY_FLAG_CAMERA_FOLLOW   = 1 << 5,
};

typedef struct GameEntity GameEntity;
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

typedef struct
{
    Asset *texture;
    SubTexture sub_texture;
    B32 solid;
    B32 visible;
} Tile;

typedef struct
{
    MemoryArena *arena;
    U64 entity_count;
    GameEntity *entities;
    U32 tilemap_width, tilemap_height;
    Tile *tilemap;
} GameMap;

internal GameMap
create_map(MemoryArena *memory,
           U32 width, U32 height)
{
    GameMap result;

    result.arena = memory;
    result.entity_count = 0;
    result.entities = NULL;
    result.tilemap_width = width;
    result.tilemap_height = height;
    result.tilemap = arena_allocate(memory,
                                    width *
                                    height *
                                    sizeof(*result.tilemap));

    return result;
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

    result->next = map->entities;
    map->entities = result;

    return result;
}

internal GameEntity *
create_static_object(GameMap *map,
                     Rectangle rectangle,
                     I8 *texture,
                     SubTexture *sub_texture)
{
    GameEntity *result = arena_allocate(map->arena, sizeof(*result));
    ++(map->entity_count);

    result->flags = ENTITY_FLAG_RENDER_TEXTURE;
    result->bounds = rectangle;
    if (!(result->texture = asset_from_path(texture)))
    {
        result->texture = new_asset_from_path(&global_static_memory, texture);
    }
    result->sub_texture = sub_texture;

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
    GameEntity *entity;
    for (entity = map->entities;
         entity;
         entity = entity->next)
    {
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

            if (!moving)
            {
                entity->x_vel = 0.0f;
                entity->y_vel = 0.0f;
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
                                  global_renderer_window_w / 2;
            F32 camera_centre_y = global_camera_y +
                                  global_renderer_window_h / 2;

            F32 camera_x_vel = (entity->bounds.x - camera_centre_x) / 2;
            F32 camera_y_vel = (entity->bounds.y - camera_centre_y) / 2;

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
            if (!entity->texture->loaded)
            {
                load_texture(gl, entity->texture);
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
                                   entity->texture->texture,
                                   entity->sub_texture[entity->frame]);
        }
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
    I32 max_x = (viewport.x + viewport.w) / TILE_SIZE + 2;
    I32 max_y = (viewport.y + viewport.h) / TILE_SIZE + 2;

    for (x = min_x;
         x < max_x;
         ++x)
    {
        if (x > map.tilemap_width) { continue; }

        for (y = min_y;
             y < max_y;
             ++y)
        {
            if (y > map.tilemap_width) { continue; }

            Tile tile = map.tilemap[y * map.tilemap_width + x];

            if (!tile.visible) { continue; }

            if (!tile.texture->loaded)
            {
                load_texture(gl, tile.texture);
            }
            
            Colour colour;
            if (editor_mode &&
                tile.solid)
            {
                colour = COLOUR(1.0f, 0.5f, 0.5f, 1.0f);
            }
            else
            {
                colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);
            }

            world_draw_sub_texture(RECTANGLE((F32)x * TILE_SIZE,
                                             (F32)y * TILE_SIZE,
                                             (F32)TILE_SIZE,
                                             (F32)TILE_SIZE),
                                    colour,
                                    tile.texture->texture,
                                    tile.sub_texture);
        }
    }
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

