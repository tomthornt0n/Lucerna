/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 04 Jan 2021
  License : N/A
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

#include "strings.c"
#include "asset_manager.c"
#include "audio.c"

internal Font *global_ui_font;

#include "renderer.c"
#include "dev_ui.c"
#include "tiles.c"
#include "entities.c"
#include "serialisation.c"
#include "maps.c"
#include "save_game.c"

#include "editor.c"

internal TileMap global_tile_map;

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_static_memory, 10 * ONE_MB);
    initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
    initialise_arena_with_new_memory(&global_level_memory, 2 * ONE_MB);

    global_ui_font = load_font(gl, "../assets/fonts/mononoki.ttf", 19);

    initialise_auto_tiler(gl);
    initialise_tile_map(&global_static_memory, &global_tile_map, 12, 12);

    initialise_renderer(gl);
}

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       F64 frametime_in_s)
{
    //
    // NOTE(tbt): recalculate projection matrix, set viewport, etc. when window
    //            size changes
    //

    static U32 previous_width = 0, previous_height = 0;
    if (input->window_width != previous_width ||
        input->window_height != previous_height)
    {
        set_renderer_window_size(gl, input->window_width,
                                     input->window_height);
    }

    //
    // NOTE(tbt): debug stuff - fps display and keybindings for enabling and
    //            disbling v-sync
    //

    if (input->is_mouse_button_pressed[MOUSE_BUTTON_left])
    {
        place_tile(&global_tile_map, TILE_KIND_grass, (input->mouse_x + global_camera_x) / 64, (input->mouse_y + global_camera_y) / 64);
    }
    else if (input->is_mouse_button_pressed[MOUSE_BUTTON_right])
    {
        place_tile(&global_tile_map, TILE_KIND_dirt, (input->mouse_x + global_camera_x) / 64, (input->mouse_y + global_camera_y) / 64);
    }
    else if (input->is_mouse_button_pressed[MOUSE_BUTTON_middle])
    {
        place_tile(&global_tile_map, TILE_KIND_trees, (input->mouse_x + global_camera_x) / 64, (input->mouse_y + global_camera_y) / 64);
    }

    I8 fps_str[16];
    gcvt(1.0 / frametime_in_s, 14, fps_str);
    ui_draw_text(global_ui_font, 16.0f, 16.0f, 0, colour_literal(0.0f, 0.0f, 0.0f, 1.0f), s8_from_cstring(&global_static_memory, fps_str));

    for (KeyTyped *key = input->keys_typed;
         NULL != key;
         key = key->next)
    {
        if (key->key == control_and('e'))
        {
            platform_set_vsync(true);
        }
        else if (key->key == control_and('d'))
        {
            platform_set_vsync(false);
        }
    }

    //
    // NOTE(tbt): actually draw everything
    //

    render_tile_map(&global_tile_map);

    process_render_queue(gl);

    previous_width = input->window_width;
    previous_height = input->window_height;
}

void
game_cleanup(OpenGLFunctions *gl)
{
    return;
}

