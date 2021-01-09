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

#define MAP_PATH_BUFFER_SIZE 64

typedef struct GameEntity GameEntity;
typedef struct Tile Tile;
struct GameMap
{
    I8 path[MAP_PATH_BUFFER_SIZE];
    U64 entity_count;
    GameEntity *entities;
    U32 tilemap_width, tilemap_height;
    Tile *tilemap;
} global_map = {{0}};

internal I32 load_most_recent_save_for_current_map(OpenGLFunctions *gl);
internal B32 save_game(void);

#include "entities.c"
#include "serialisation.c"
#include "maps.c"
#include "save_game.c"

#include "editor.c"

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_static_memory, 10 * ONE_MB);
    initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
    initialise_arena_with_new_memory(&global_level_memory, 2 * ONE_MB);

    initialise_renderer(gl);

    global_ui_font = load_font(gl, FONT_PATH("mononoki.ttf"), 19);

    // NOTE(tbt): initialise the free list for pooled entities
    for (I32 i = 0;
         i < ENTITY_POOL_SIZE;
         ++i)
    {
        global_entity_pool[i].next = global_entity_free_list;
        global_entity_free_list = global_entity_pool + i;
    }
}

internal I32 global_game_state = GAME_STATE_PLAYING;

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
    // NOTE(tbt): toggle between the game and the editor
    //

    for (KeyTyped *key = input->keys_typed;
         NULL != key;
         key = key->next)
    {
        if (key->key == CTRL('e'))
        {
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
                global_keyboard_focus = NULL;
            }
        }
    }

    //
    // NOTE(tbt): update and render the editor when in editor mode
    //

    if (global_game_state == GAME_STATE_EDITOR)
    {
        do_ui(gl, input)
        {
            do_editor(gl, input, frametime_in_s);
        }
    }

    //
    // NOTE(tbt): draw the tilemap, update and draw entities while the game is
    //            being played
    //

    else if (global_game_state == GAME_STATE_PLAYING)
    {
        if (global_map.tilemap) { render_tiles(gl, false); }
        process_entities(gl, input, frametime_in_s);
    }

    //
    // NOTE(tbt): debug stuff - fps display and keybindings for enabling and
    //            disbling v-sync
    //

    I8 fps_str[16];
    gcvt(1.0 / frametime_in_s, 14, fps_str);
    ui_draw_text(global_ui_font, 16.0f, 16.0f, 0, BG_COL_2, fps_str);

    if (input->is_key_pressed[KEY_LEFT_CONTROL])
    {
        if (input->is_key_pressed[KEY_V])
        {
            platform_set_vsync(true);
        }
        else if (input->is_key_pressed[KEY_D])
        {
            platform_set_vsync(false);
        }
    }

    //
    // NOTE(tbt): actually draw everything
    //

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
    free(global_level_memory.buffer);
}

