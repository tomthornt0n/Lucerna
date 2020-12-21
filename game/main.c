/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 21 Dec 2020
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

internal MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;
internal MemoryArena global_level_memory;

#include "audio.c"
#include "asset_manager.c"
#include "math.c"

Font *global_ui_font;

#include "renderer.c"
#include "dev_ui.c"
#include "entities.c"
#include "serialisation.c"


internal Asset *global_map = NULL;

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_static_memory, ONE_GB);
    initialise_arena_with_new_memory(&global_frame_memory, ONE_MB);
    initialise_arena_with_new_memory(&global_level_memory, ONE_MB);

    initialise_renderer(gl);

    global_ui_font = load_font(gl, FONT_PATH("mononoki.ttf"), 19);

    Asset *spritesheet;
    if (!(spritesheet = asset_from_path(TEXTURE_PATH("spritesheet.png"))))
    {
        spritesheet = new_asset_from_path(&global_static_memory, TEXTURE_PATH("spritesheet.png"));
    }

    load_texture(gl, spritesheet);

    global_player_down_texture = slice_animation(spritesheet->texture, 48.0f, 0.0f, 16.0f, 16.0f, 4, 1);
    global_player_left_texture = slice_animation(spritesheet->texture, 48.0f, 16.0f, 16.0f, 16.0f, 4, 1);
    global_player_right_texture = slice_animation(spritesheet->texture, 48.0f, 32.0f, 16.0f, 16.0f, 4, 1);
    global_player_up_texture = slice_animation(spritesheet->texture, 48.0f, 48.0f, 16.0f, 16.0f, 4, 1);
}

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       U64 timestep_in_ns)
{
    static U32 previous_width = 0, previous_height = 0;
    static I32 game_state = GAME_STATE_PLAYING;
    static U32 editor_mode_toggle_cooldown_timer = 500;

    if (input->window_width != previous_width ||
        input->window_height != previous_height)
    {
        set_renderer_window_size(gl, input->window_width,
                                     input->window_height);
    }

    GameEntity *selected_entity = NULL;
    if (global_map)
    {
        render_tiles(gl, global_map->map, game_state);

        selected_entity = process_entities(gl, input,
                                           &global_map->map,
                                           game_state);
    }

    ++editor_mode_toggle_cooldown_timer;
    if (input->is_key_pressed[KEY_E]            &&
        input->is_key_pressed[KEY_LEFT_CONTROL] &&
        editor_mode_toggle_cooldown_timer > 15)
    {
        editor_mode_toggle_cooldown_timer = 0;
        game_state = game_state == GAME_STATE_PLAYING ?
                     GAME_STATE_EDITOR :
                     GAME_STATE_PLAYING;
    }

    if (game_state == GAME_STATE_EDITOR ||
        game_state == GAME_STATE_TILE_EDITOR)
    {
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
            do_window(input, "level editor", 0.0f, 32.0f, 600.0f)
            {
                if (do_toggle_button(input, "edit tiles", 256.0f))
                {
                    game_state = GAME_STATE_TILE_EDITOR;
                }
                else
                {
                    game_state = GAME_STATE_EDITOR;
                }

                do_dropdown(input, "create entity", 256.0f)
                {
                    if (global_map)
                    {
                        if (do_button(input, "create static object", 256.0f))
                        {
                            create_static_object(&global_map->map,
                                                 RECTANGLE(global_camera_x,
                                                           global_camera_y,
                                                           64.0f, 64.0f),
                                                 TEXTURE_PATH("spritesheet.png"),
                                                 ENTIRE_TEXTURE);
                        }
                        if (do_button(input, "create player", 256.0f))
                        {
                            create_player(&global_map->map,
                                          global_camera_x,
                                          global_camera_y);
                        }
                    }
                    else
                    {
                        do_label("plmf", "please load a map first.", 256.0f);
                    }
                }

                if (do_button(input, "load map", 256.0f))
                {
                    game_state = GAME_STATE_OPEN_MAP;
                }
            }

            if (game_state == GAME_STATE_TILE_EDITOR)
            {
                static SubTexture tile_sub_texture = ENTIRE_TEXTURE;
                B32 solid;

                Asset *tile_texture = asset_from_path(TEXTURE_PATH("spritesheet.png"));
                if (!tile_texture)
                {
                    tile_texture = new_asset_from_path(&global_static_memory,
                                                       TEXTURE_PATH("spritesheet.png"));
                }

                if (!tile_texture->loaded)
                {
                    load_texture(gl, tile_texture);
                }

                do_window(input, "tile selector", 0.0f, 128.0f, 200.0f)
                {

                    do_line_break();

                    do_sprite_picker(input,
                                     "tile sprite picker",
                                     tile_texture,
                                     16.0f,
                                     &tile_sub_texture);

                    solid = do_toggle_button(input, "solid", 150.0f);
                }

                if (global_map)
                {
                    Tile *tile_under_cursor;
                    if ((tile_under_cursor = get_tile_under_cursor(input, global_map->map)) &&
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
                            tile_under_cursor->texture = tile_texture;
                            tile_under_cursor->sub_texture = tile_sub_texture;
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
            }

            if (selected_entity)
            {
                do_window(input, "entity properties", 0.0f, 128.0f, 600.0f)
                {
                    do_line_break();

                    I8 entity_size_label[64] = {0};
                    snprintf(entity_size_label, 64, "size: (%.1f x %.1f)", selected_entity->bounds.w, selected_entity->bounds.h);
                    do_label("entity size", entity_size_label, 200.0f);
                    do_line_break();
                    do_slider_f(input, "entity w slider", 64.0f, 1024.0f, 64.0f, 200.0f, &(selected_entity->bounds.w));
                    do_line_break();
                    do_slider_f(input, "entity h slider", 64.0f, 1024.0f, 64.0f, 200.0f, &(selected_entity->bounds.h));


                    do_line_break();

                    I8 entity_speed_label[64] = {0};
                    snprintf(entity_speed_label, 64, "speed: (%.1f)", selected_entity->speed);
                    do_label("entity speed", entity_speed_label, 200.0f);
                    do_line_break();
                    do_slider_f(input, "entity speed slider", 0.0f, 32.0f, 1.0f, 200.0f, &(selected_entity->speed));


                    do_line_break();

                    do_label("entity sprite picker label", "texture:", 100.0f);
                    do_line_break();
                    do_sprite_picker(input,
                                     "entity texture picker",
                                     selected_entity->texture,
                                     16.0f,
                                     selected_entity->sub_texture);
                }
            }

        }
    }
    else if (game_state == GAME_STATE_OPEN_MAP)
    {
        do_ui(input)
        {
            do_window(input,
                      "open map",
                      input->window_width / 2,
                      input->window_height / 2,
                      512.0f)
            {
                I8 *path = do_text_entry(input, "map path", 512.0f);
                
                do_line_break();

                if (do_button(input, "cancel", 100.0f))
                {
                    game_state = GAME_STATE_EDITOR;
                }
                
                if (do_button(input, "open", 100.0f))
                {
                    Asset *prev_map = global_map;

                    global_map = asset_from_path(path);
                    if (!global_map)
                    {
                        global_map = new_asset_from_path(&global_static_memory,
                                                         path);
                    }

                    
                    if (!load_map(gl, global_map))
                    {
                        fprintf(stderr, "could not load map '%s'\n", path);
                        game_state = GAME_STATE_NEW_MAP;
                    }
                    else
                    {
                        game_state = GAME_STATE_EDITOR;
                    }

                    if (prev_map) { unload_map(gl, prev_map); }
                }
            }
        }
    }
    else if (game_state == GAME_STATE_NEW_MAP)
    {
        do_ui(input)
        {
            do_window(input,
                      "new map",
                      input->window_width / 2,
                      input->window_height / 2,
                      512.0f)
            {
                I8 *path = do_text_entry(input, "map path", 512.0f);

                do_line_break();

                do_label("tmw", "tilemap width: ", 100.0f);
                I32 width = atoi(do_text_entry(input, "tmw e", 100.0f));

                do_line_break();

                do_label("tmh", "tilemap height: ", 100.0f);
                I32 height = atoi(do_text_entry(input, "tmh e", 100.0f));

                do_line_break();

                if (do_button(input, "create new map", 200.0f))
                {
                    global_map = asset_from_path(path);
                    if (!global_map)
                    {
                        global_map = new_asset_from_path(&global_static_memory, path);
                    }

                    create_new_map(width, height, &global_map->map);
                    global_map->type = ASSET_TYPE_MAP;
                    global_map->loaded = true;
                    game_state = GAME_STATE_EDITOR;
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
    if (global_map) { unload_map(gl, global_map); }

    free(global_static_memory.buffer);
    free(global_frame_memory.buffer);
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

