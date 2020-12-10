/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 10 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <immintrin.h>

#include "lucerna.h"

#include "arena.c"
#include "audio.c"
#include "asset_manager.c"
#include "math.c"

#define GRAVITY 4.0f

MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;

Font *global_ui_font;

internal B32
rectangles_are_intersecting(Rectangle a,
                            Rectangle b)
{
    if (a.x + a.w < b.x || a.x > b.x + b.w) { return false; }
    if (a.y + a.h < b.y || a.y > b.y + b.h) { return false; }
    return true;
}

internal B32
point_is_in_region(F32 x, F32 y,
                   Rectangle region)
{
    if (x < region.x              ||
        y < region.y              ||
        x > (region.x + region.w) ||
        y > (region.y + region.h))
    {
        return false;
    }
    return true;
}

#include "renderer.c"
#include "dev_ui.c"

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
    Texture *texture;
    Colour colour;
    F32 speed;
    F32 x_vel, y_vel;
    U32 frame, animation_length;
    U64 animation_speed, animation_clock;
};


#define TILE_SIZE 64

typedef struct
{
    Texture *texture;
    B32 solid;
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

Texture *global_player_left_texture;
Texture *global_player_right_texture;
Texture *global_player_up_texture;
Texture *global_player_down_texture;

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
    result->texture = global_player_down_texture;
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
                     Texture *texture)
{
    GameEntity *result = arena_allocate(map->arena, sizeof(*result));
    ++(map->entity_count);

    result->flags = ENTITY_FLAG_RENDER_TEXTURE;
    result->bounds = rectangle;
    result->texture = texture;

    result->next = map->entities;
    map->entities = result;

    return result;
}

internal void
process_entities(PlatformState *input,
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
                entity->texture = global_player_left_texture;
                moving = true;
            }
            else if (input->is_key_pressed[KEY_D])
            {
                entity->x_vel = entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->texture = global_player_right_texture;
                moving = true;
            }

            if (input->is_key_pressed[KEY_W])
            {
                entity->y_vel = -entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->texture = global_player_up_texture;
                moving = true;
            }
            else if (input->is_key_pressed[KEY_S])
            {
                entity->y_vel = entity->speed;
                entity->flags |= ENTITY_FLAG_ANIMATED;
                entity->texture = global_player_down_texture;
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
            entity->bounds.x += entity->x_vel;
            entity->bounds.y += entity->y_vel;
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
            Colour colour;
            if (entity->flags & ENTITY_FLAG_COLOURED)
            {
                colour = entity->colour;
            }
            else
            {
                colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);
            }

            world_draw_texture(entity->bounds,
                               colour,
                               entity->texture[entity->frame]);
        }
    }
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
render_tiles(GameMap map,
             Rectangle viewport,
             B32 editor_mode)
{
    I32 x, y;

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
            
            if (!tile.texture) { continue; }

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

            world_draw_texture(RECTANGLE((F32)x * TILE_SIZE,
                                         (F32)y * TILE_SIZE,
                                         (F32)TILE_SIZE,
                                         (F32)TILE_SIZE),
                                colour,
                                *(tile.texture));
        }
    }
}

internal GameMap global_map;
internal Texture *global_tile_textures;

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_static_memory, ONE_MB);
    initialise_arena_with_new_memory(&global_frame_memory, 500 * ONE_MB);
    initialise_arena_with_new_memory(&global_asset_memory, ONE_GB);

    initialise_renderer(gl);

    global_ui_font = load_font(gl, FONT_PATH("mononoki.ttf"), 18);

    Texture spritesheet = load_texture(gl, TEXTURE_PATH("spritesheet.png"));

    global_player_down_texture = slice_animation(spritesheet, 48.0f, 0.0f, 16.0f, 16.0f, 4, 1);
    global_player_left_texture = slice_animation(spritesheet, 48.0f, 16.0f, 16.0f, 16.0f, 4, 1);
    global_player_right_texture = slice_animation(spritesheet, 48.0f, 32.0f, 16.0f, 16.0f, 4, 1);
    global_player_up_texture = slice_animation(spritesheet, 48.0f, 48.0f, 16.0f, 16.0f, 4, 1);

    global_tile_textures = slice_animation(spritesheet, 0.0f, 0.0f, 16.0f, 16.0f, 16, 16);

    global_map = create_map(&global_static_memory, 100, 100);
    create_player(&global_map, 0.0f, 0.0f);
}

internal I32
do_tile_selector(PlatformState *input)
{
    static U32 selected_tile_index = 0;
    static F32 x = 16.0f, y = 128.0f;
    static B32 dragging = false;
    B32 mouse_over_tile_selector = false;
    F32 scale = 16.0f;
    I32 width = 16;
    F32 title_bar_height = 24;

    Rectangle title_bar = RECTANGLE(x,
                                    y - title_bar_height,
                                    width * scale,
                                    title_bar_height);

    Rectangle bg = RECTANGLE(x, y,
                             scale * width,
                             scale * width);


    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           title_bar) &&
        input->is_mouse_button_pressed[MOUSE_BUTTON_1])
    {
        dragging = true;
    }
    if (dragging)
    {
        mouse_over_tile_selector = true;
        x = input->mouse_x - 128.0f;
        y = input->mouse_y + title_bar_height / 2;
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1])
        {
            dragging = false;
        }
    }

    blur_screen_region(RECTANGLE(title_bar.x,
                                 title_bar.y,
                                 bg.w,
                                 title_bar.h + bg.h),
                       5);

    ui_fill_rectangle(title_bar, COLOUR(0.0f, 0.0f, 0.0f, 0.8f));
    ui_draw_text(global_ui_font,
                 x + 8.0f,
                 y - title_bar_height / 3,
                 0,
                 COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                 "select tile");

    ui_fill_rectangle(bg, COLOUR(0.0f, 0.0f, 0.0f, 0.5f));

    I32 i;
    for (i = 0;
         i < 16 * 16;
         ++i)
    {
        Colour colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);
        Rectangle rectangle = RECTANGLE(x +
                                        (i % width) * 
                                        scale,
                                        y +
                                        (i / width) * 
                                        scale,
                                        scale,
                                        scale);

        if (point_is_in_region(input->mouse_x, input->mouse_y, rectangle))
        {
            mouse_over_tile_selector = true;
            colour = COLOUR(1.0f, 0.5f, 0.5f, 1.0f);
            if (input->is_mouse_button_pressed[MOUSE_BUTTON_1])
            {
                selected_tile_index = i;
            }
        }

        ui_draw_texture(rectangle,
                        colour,
                        global_tile_textures[i]);

        if (i == selected_tile_index)
        {
            ui_stroke_rectangle(rectangle, colour, 1);
        }
    }

    if (mouse_over_tile_selector) { return -1; }
    else { return selected_tile_index; }
}

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       U64 timestep_in_ns)
{
    static U32 previous_width = 0, previous_height = 0;
    static B32 editor_mode = false;
    static U32 editor_mode_toggle_cooldown_timer = 500;

    prepare_ui();

    if (input->window_width != previous_width ||
        input->window_height != previous_height)
    {
        set_renderer_window_size(gl,
                                 input->window_width,
                                 input->window_height);
    }

    render_tiles(global_map,
                 RECTANGLE(global_camera_x, global_camera_y,
                           input->window_width,
                           input->window_height),
                 editor_mode);

    process_entities(input, &global_map);

    ++editor_mode_toggle_cooldown_timer;
    if (input->is_key_pressed[KEY_E] &&
        editor_mode_toggle_cooldown_timer > 15)
    {
        editor_mode_toggle_cooldown_timer = 0;
        editor_mode = !editor_mode;
    }

    if (editor_mode)
    {
        I32 selected_tile_index = 0;
        F64 fps = (F64)1e9 / (F64)timestep_in_ns;
        I8 frame_time_string[64] = {0};

        snprintf(frame_time_string,
                 64,
                 "frametime : ~%lu nanoseconds\n"
                 "fps       : ~%f",
                 timestep_in_ns,
                 fps);

        ui_draw_text(global_ui_font, 64.0f, 64.0f, 0, COLOUR(0.2f, 0.3f, 1.0f, 1.0f), frame_time_string);

        /* selected_tile_index = do_tile_selector(input); */
        begin_window(input, "level editor", 512.0f, 32.0f, 256.0f);
            B32 tile_selector = do_toggle_button(input, "edit tiles", 128);
            do_button(input, "this is not a toggle", 128);
        end_window();

        if (tile_selector)
        {
            begin_window(input, "tile selector", 32.0f, 32.0f, 121.0f);
                do_toggle_button(input, "1", 16);
                do_toggle_button(input, "2", 16);
                do_toggle_button(input, "3", 16);
                do_toggle_button(input, "4", 16);
                do_toggle_button(input, "5", 16);
                do_toggle_button(input, "6", 16);
                do_toggle_button(input, "7", 16);
                do_toggle_button(input, "8", 16);
                do_toggle_button(input, "9", 16);
            end_window();
        }


        Tile *tile_under_cursor;
        if ((tile_under_cursor = get_tile_under_cursor(input, global_map)))
        {
            world_stroke_rectangle(RECTANGLE(((I32)(input->mouse_x + global_camera_x) / TILE_SIZE) * TILE_SIZE,
                                             ((I32)(input->mouse_y + global_camera_y) / TILE_SIZE) * TILE_SIZE,
                                             TILE_SIZE,
                                             TILE_SIZE),
                                   COLOUR(0.0f, 0.0f, 0.0f, 0.5f),
                                   2);

            if (input->is_mouse_button_pressed[MOUSE_BUTTON_1])
            {
                tile_under_cursor->texture = global_tile_textures + selected_tile_index;
            }
        }
    }

    finish_ui(input);
    process_render_queue(gl);

    previous_width = input->window_width;
    previous_height = input->window_height;

}

void
game_cleanup(OpenGLFunctions *gl)
{
    free(global_static_memory.buffer);
    free(global_frame_memory.buffer);
    free(global_asset_memory.buffer);
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

