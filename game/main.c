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

#include "asset_manager.c"
#include "audio.c"

internal Font *global_ui_font;

#include "renderer.c"
#include "dev_ui.c"

typedef struct GameEntity GameEntity;
typedef struct Tile Tile;
struct GameMap
{
    I8 path[64];
    U64 entity_count;
    GameEntity *entities;
    U32 tilemap_width, tilemap_height;
    Tile *tilemap;
} global_map = {{0}};
#include "entities.c"
#include "serialisation.c"
#include "maps.c"

#include "editor.c"

void
test_job_1(void *arg,
           U64 memory_size, void *memory)
{
    // NOTE(tbt): test jobs without memory
    assert(!memory && !memory_size);

    fprintf(stderr, "this was printed by another thread\n");
}

void
test_job_2(void *arg,
           U64 memory_size, void *memory)
{
    // NOTE(tbt): test jobs with memory
    assert(memory && memory_size);

    snprintf(memory, memory_size, "msg from another thread: %s", arg);
}

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_static_memory, 10 * ONE_MB);
    initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
    initialise_arena_with_new_memory(&global_level_memory, 2 * ONE_MB);

    initialise_renderer(gl);

    global_ui_font = load_font(gl, FONT_PATH("mononoki.ttf"), 19);

    I8 msg[] = "hello from another thread!\n";
    PlatformJobHandle *job_1 = platform_enqueue_job(test_job_1, NULL, 0);
    PlatformJobHandle *job_2 = platform_enqueue_job_with_memory(test_job_2, msg, strlen(msg) + 1, 64);

    platform_wait_for_job(job_1);
    I8 *msg_from_thread = platform_wait_for_job(job_2);

    fprintf(stderr, msg_from_thread);
    free(msg_from_thread);
}

internal I32 global_game_state = GAME_STATE_PLAYING;

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       U64 timestep_in_ns)
{
    static U32 previous_width = 0, previous_height = 0;
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

        if (global_game_state == GAME_STATE_EDITOR)
        {
            save_map();
            global_game_state = GAME_STATE_PLAYING;
            global_editor_selected_entity = NULL;
        }
        else if (global_game_state == GAME_STATE_PLAYING)
        {
            load_map(gl, global_map.path); // NOTE(tbt): reload map when entering the editor
            global_game_state = GAME_STATE_EDITOR;
        }
    }

    if (global_game_state == GAME_STATE_EDITOR)
    {
        do_ui(gl, input)
        {
            do_editor(gl, input);
        }
    }
    else if (global_game_state == GAME_STATE_PLAYING)
    {
        if (global_map.tilemap) { render_tiles(gl, false); }
        process_entities(gl, input);
    }


    process_render_queue(gl);

    previous_width = input->window_width;
    previous_height = input->window_height;

}

void
game_cleanup(OpenGLFunctions *gl)
{
    if (global_game_state == GAME_STATE_EDITOR)
    {
        save_map();
    }

    free(global_static_memory.buffer);
    free(global_frame_memory.buffer);
}

