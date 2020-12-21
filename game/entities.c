/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 21 Dec 2020
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

/* NOTE(tbt): returns a pointer to the selected entity in editor mode, NULL
              otherwise
*/
internal GameEntity *
process_entities(OpenGLFunctions *gl,
                 PlatformState *input,
                 GameMap *map,
                 I32 game_state)
{
    /* NOTE(tbt): only used when editor mode is enabled */
    static GameEntity *selected = NULL, *active = NULL, *dragging = NULL;

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

        if (game_state == GAME_STATE_PLAYING)
        {
            selected = NULL;

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
        }
        else
        {

            F32 editor_camera_speed = 8.0f;

            if (!global_keyboard_focus)
            {
                set_camera_position(global_camera_x +
                                    input->is_key_pressed[KEY_D] * editor_camera_speed +
                                    input->is_key_pressed[KEY_A] * -editor_camera_speed,
                                    global_camera_y +
                                    input->is_key_pressed[KEY_S] * editor_camera_speed +
                                    input->is_key_pressed[KEY_W] * -editor_camera_speed);
            }

            if (point_is_in_region(global_camera_x + input->mouse_x,
                                   global_camera_y + input->mouse_y,
                                   entity->bounds) &&
                game_state == GAME_STATE_EDITOR &&
                !global_is_mouse_over_ui)
            {
                stroke_rectangle(entity->bounds,
                                 COLOUR(1.0f, 1.0f, 1.0f, 0.8f),
                                 2.0f,
                                 3,
                                 global_projection_matrix);

                if (input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
                {
                    if (selected == entity)
                    {
                        dragging = entity;
                    }
                    else
                    {
                        active = entity;
                    }
                }
            }

            if (input->is_key_pressed[KEY_ESCAPE])
            {
                selected = NULL;
            }

            if (active == entity &&
                !input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                    selected = entity;
            }

            if (selected == entity)
            {
                fill_rectangle(entity->bounds,
                               COLOUR(0.0f, 0.0f, 1.0f, 0.3f),
                               3,
                               global_projection_matrix);

                if (input->is_key_pressed[KEY_DEL])
                {
                    fprintf(stderr, "deleting entity\n");
                    entity->flags |= ENTITY_FLAG_DELETED;
                    selected = NULL;
                }
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

            if (game_state == GAME_STATE_TILE_EDITOR)
            {
                colour.a = 0.4f;
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

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
    {
        active = NULL;
    }

    if (dragging)
    {
        F32 drag_x = input->mouse_x + global_camera_x;
        F32 drag_y = input->mouse_y + global_camera_y;

        if (input->is_key_pressed[KEY_LEFT_ALT])
        {
            dragging->bounds.x = drag_x;
            dragging->bounds.y = drag_y;
        }
        else
        {
#define GRID_SNAP 4.0f
            dragging->bounds.x = floor(drag_x / GRID_SNAP) * GRID_SNAP;
            dragging->bounds.y = floor(drag_y / GRID_SNAP) * GRID_SNAP;
        }

        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT] ||
            global_is_mouse_over_ui)
        {
            dragging = NULL;
        }
    }

    if (game_state == GAME_STATE_EDITOR)
    {
        return selected;
    }
    else
    {
        return NULL;
    }
}

internal void
render_tiles(OpenGLFunctions *gl,
             GameMap map,
             I32 game_state)
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
            
            Colour colour;
            if (game_state == GAME_STATE_TILE_EDITOR &&
                tile.solid)
            {
                colour = COLOUR(1.0f, 0.7f, 0.7f, 1.0f);
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
                                    texture,
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

