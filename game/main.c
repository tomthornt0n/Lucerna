/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 18 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <immintrin.h>
#include <ctype.h>

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
#include "entities.c"
#include "serialisation.c"

internal GameMap global_map;
internal SubTexture *global_tile_textures;

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_static_memory, ONE_MB);
    initialise_arena_with_new_memory(&global_frame_memory, ONE_MB);
    initialise_arena_with_new_memory(&global_asset_memory, 500 * ONE_MB);

    initialise_renderer(gl);

    global_ui_font = load_font(gl, FONT_PATH("mononoki.ttf"), 18);

    Asset *spritesheet = new_asset_from_path(&global_static_memory,
                                             TEXTURE_PATH("spritesheet.png"));
    load_texture(gl, spritesheet);

    global_player_down_texture = slice_animation(spritesheet->texture, 48.0f, 0.0f, 16.0f, 16.0f, 4, 1);
    global_player_left_texture = slice_animation(spritesheet->texture, 48.0f, 16.0f, 16.0f, 16.0f, 4, 1);
    global_player_right_texture = slice_animation(spritesheet->texture, 48.0f, 32.0f, 16.0f, 16.0f, 4, 1);
    global_player_up_texture = slice_animation(spritesheet->texture, 48.0f, 48.0f, 16.0f, 16.0f, 4, 1);

    global_tile_textures = slice_animation(spritesheet->texture, 0.0f, 0.0f, 16.0f, 16.0f, 16, 16);

    /*
    global_map = create_map(&global_static_memory, 100, 100);
    create_player(&global_map, 0.0f, 0.0f);
    */

    global_map = read_map(&global_static_memory, "test.map");
}

/*
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
        input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
    {
        dragging = true;
    }
    if (dragging)
    {
        mouse_over_tile_selector = true;
        x = input->mouse_x - 128.0f;
        y = input->mouse_y + title_bar_height / 2;
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
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
            if (input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
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
*/

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       U64 timestep_in_ns)
{
    static U32 previous_width = 0, previous_height = 0;
    static B32 editor_mode = false, tile_edit = false, entity_edit = false;
    static U32 editor_mode_toggle_cooldown_timer = 500;

    if (input->window_width != previous_width ||
        input->window_height != previous_height)
    {
        set_renderer_window_size(gl,
                                 input->window_width,
                                 input->window_height);
    }

    render_tiles(gl,
                 global_map,
                 tile_edit);

    process_entities(gl, input, &global_map);

    ++editor_mode_toggle_cooldown_timer;
    if (input->is_key_pressed[KEY_E]            &&
        input->is_key_pressed[KEY_LEFT_CONTROL] &&
        editor_mode_toggle_cooldown_timer > 15)
    {
        editor_mode_toggle_cooldown_timer = 0;
        editor_mode = !editor_mode;
    }

    if (editor_mode)
    {
        static I32 selected_tile_index = 0;
        F64 fps = (F64)1e9 / (F64)timestep_in_ns;
        I8 frame_time_string[64] = {0};

        snprintf(frame_time_string,
                 64,
                 "frametime : ~%lu nanoseconds\n"
                 "fps       : ~%f",
                 timestep_in_ns,
                 fps);

        ui_draw_text(global_ui_font, 64.0f, 64.0f, 0, COLOUR(0.2f, 0.3f, 1.0f, 1.0f), frame_time_string);

        do_ui(input)
        {
            do_window(input, "level editor", 512.0f, 32.0f, 600.0f)
            {
                tile_edit = do_toggle_button(input, "edit tiles", 256.0f);
                entity_edit = do_toggle_button(input, "edit entities", 256.0f);
            }

            if (tile_edit)
            {
                B32 solid;

                do_window(input, "tile selector", 32.0f, 32.0f, 200.0f)
                {
                    do_dropdown(input, "choose tile", 150.0f)
                    {
                        if (do_button(input, "dirt top left", 150.0f)) { selected_tile_index = 0; }
                        if (do_button(input, "dirt top", 150.0f)) { selected_tile_index = 1; }
                        if (do_button(input, "dirt top right", 150.0f)) { selected_tile_index = 2; }
                        if (do_button(input, "dirt left", 150.0f)) { selected_tile_index = 16; }
                        if (do_button(input, "dirt", 150.0f)) { selected_tile_index = 17; }
                        if (do_button(input, "dirt right", 150.0f)) { selected_tile_index = 18; }
                        if (do_button(input, "dirt bottom left", 150.0f)) { selected_tile_index = 32; }
                        if (do_button(input, "dirt bottom", 150.0f)) { selected_tile_index = 33; }
                        if (do_button(input, "dirt bottom right", 150.0f)) { selected_tile_index = 34; }
                        if (do_button(input, "grass 1", 150.0f)) { selected_tile_index = 48; }
                        if (do_button(input, "grass 2", 150.0f)) { selected_tile_index = 64; }
                        if (do_button(input, "grass 3", 150.0f)) { selected_tile_index = 80; }
                    }

                    do_line_break();

                    solid = do_toggle_button(input, "solid", 150.0f);
                }

                Tile *tile_under_cursor;
                if ((tile_under_cursor = get_tile_under_cursor(input, global_map)) &&
                    !global_is_mouse_over_ui)
                {
                    world_stroke_rectangle(RECTANGLE(((I32)(input->mouse_x + global_camera_x) / TILE_SIZE) * TILE_SIZE,
                                                     ((I32)(input->mouse_y + global_camera_y) / TILE_SIZE) * TILE_SIZE,
                                                     TILE_SIZE,
                                                     TILE_SIZE),
                                           COLOUR(0.0f, 0.0f, 0.0f, 0.5f),
                                           2);

                    if (input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
                    {
                        tile_under_cursor->texture = asset_from_path(TEXTURE_PATH("spritesheet.png"));
                        tile_under_cursor->sub_texture = global_tile_textures[selected_tile_index];
                        tile_under_cursor->visible = true;
                        tile_under_cursor->solid = solid;
                    }
                    else if (input->is_mouse_button_pressed[MOUSE_BUTTON_RIGHT])
                    {
                        tile_under_cursor->visible = false;
                        tile_under_cursor->solid = false;
                    }
                }
            }

            if (entity_edit)
            {
                do_window(input, "entity properties", 32.0f, 32.0f, 200.0f)
                {
                    do_label("TODO(tbt): level editor", 150.0f);
                }
            }
        }
    }

    process_render_queue(gl);

    previous_width = input->window_width;
    previous_height = input->window_height;

}

void
game_cleanup(OpenGLFunctions *gl)
{
    write_map(&global_map, "test.map");

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

