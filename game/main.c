/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 23 Dec 2020
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

#include "audio.c"
#include "asset_manager.c"
#include "math.c"

Font *global_ui_font;

#include "renderer.c"
#include "dev_ui.c"
#include "entities.c"
#include "serialisation.c"

internal Asset *global_map = NULL;

#include "editor.c"

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

    unload_texture(gl, spritesheet);
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

    if (game_state == GAME_STATE_EDITOR)
    {
        do_ui(input)
        {
            do_editor(gl, input);
        }
    }
    else if (game_state == GAME_STATE_PLAYING)
    {
        if (global_map)
        {
            render_tiles(gl, global_map->map, false);
            process_entities(gl, input, &global_map->map);
        }
    }

    process_render_queue(gl);

    previous_width = input->window_width;
    previous_height = input->window_height;

}

void
game_cleanup(OpenGLFunctions *gl)
{
    if (global_map)
    {
        save_map(global_map);
        unload_map(gl, global_map);
    }

    free(global_static_memory.buffer);
    free(global_frame_memory.buffer);
}

