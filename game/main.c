#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>

#include <immintrin.h>

#define LUCERNA_GAME
#include "lucerna.h"

/*
TODO list:
- unicode support
- multithreaded asset loading? (maybe)
- make the player walk less wierdly
- gameplay!!!!!!!
*/

internal F64 global_time = 0.0;
internal F32 global_exposure = 1.0;

internal MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;
internal MemoryArena global_level_memory;

#include "util.c"
#include "asset_manager.c"
#include "audio.c"

internal Font *global_ui_font;
internal Font *global_title_font;
internal Font *global_normal_font;

typedef enum
{
 GAME_STATE_playing,
 GAME_STATE_editor,
 GAME_STATE_main_menu,
 
 GAME_STATE_MAX,
} GameState;

internal GameState global_game_state = GAME_STATE_main_menu;

typedef void ( *PerGameStateMainFunction)(OpenGLFunctions *, PlatformState *, F64);
internal PerGameStateMainFunction global_main_functions[GAME_STATE_MAX];

AudioSource *global_click_sound = NULL;

#include "types.gen.h"
#include "renderer.c"
#include "dev_ui.c"
#include "funcs.gen.h"
#include "dialogue.c"
#include "entities.c"
#include "editor.c"
#include "main_menu.c"

//
// NOTE(tbt): main loop for game_playing state
//~

internal void
game_playing_main(OpenGLFunctions *gl,
                  PlatformState *input,
                  F64 frametime_in_s)
{
#ifdef LUCERNA_DEBUG
 hot_reload_current_level_art(gl, frametime_in_s);
 hot_reload_player_art(gl, frametime_in_s);
 hot_reload_shaders(gl, frametime_in_s);
#endif
 do_current_level(gl, input, frametime_in_s);
}

//
// NOTE(tbt): initialisation
//~

void
game_init(OpenGLFunctions *gl)
{
 initialise_arena_with_new_memory(&global_static_memory, 1 * ONE_MB);
 initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
 initialise_arena_with_new_memory(&global_level_memory, 27 * ONE_MB);
 
 global_ui_font = load_font(gl, s8_literal("../assets/fonts/mononoki.ttf"), 19);
 global_title_font = load_font(gl, s8_literal("../assets/fonts/ShipporiMincho-Regular.ttf"), 72);
 global_normal_font = load_font(gl, s8_literal("../assets/fonts/ShipporiMincho-Regular.ttf"), 28);
 global_click_sound = load_audio(s8_literal("../assets/audio/click.wav"));
 load_player_art(gl);
 
 initialise_renderer(gl);
 
 set_camera_position(960.0f, 540.0f);
 
 // NOTE(tbt): look up table of main functions per game state
 global_main_functions[GAME_STATE_playing] = game_playing_main;
 global_main_functions[GAME_STATE_editor] = do_level_editor;
 global_main_functions[GAME_STATE_main_menu] = do_main_menu;
}

//
// NOTE(tbt): main loop
//~

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       F64 frametime_in_s)
{
 set_renderer_window_size(gl,
                          input->window_w,
                          input->window_h);
 
 ui_prepare();
 
 global_main_functions[global_game_state](gl, input, frametime_in_s);
 
 //
 // NOTE(tbt): debug extras
 //~
#ifdef LUCERNA_DEBUG
 I8 fps_str[128];
 snprintf(fps_str, 128, "%f ms (%f fps)", frametime_in_s * 1000.0, 1.0 / frametime_in_s);
 ui_draw_text(global_ui_font, 16.0f, 16.0f, 0, colour_literal(1.0f, 1.0f, 1.0f, 1.0f), s8_literal(fps_str));
 
 I8 pos_str[128];
 snprintf(pos_str, 128, "%f %f", global_current_level.player.x, global_current_level.player.y);
 ui_draw_text(global_ui_font, 16.0f, 64.0f, 0, colour_literal(1.0f, 1.0f, 1.0f, 1.0f), s8_literal(pos_str));
 
 if (is_key_pressed(input,
                    KEY_v,
                    INPUT_MODIFIER_ctrl))
 {
  static B32 vsync = true;
  platform_set_vsync((vsync = !vsync));
 }
 else if (is_key_pressed(input,
                         KEY_f,
                         INPUT_MODIFIER_ctrl))
 {
  platform_toggle_fullscreen();
 }
 else if (is_key_pressed(input,
                         KEY_e,
                         INPUT_MODIFIER_ctrl))
 {
  if (global_game_state == GAME_STATE_editor)
  {
   global_editor_selected_entity = NULL;
   
   serialise_level(&global_current_level);
   
   // NOTE(tbt): reset camera position
   set_camera_position(global_current_level.bg.width >> 1,
                       global_current_level.bg.height >> 1);
   
   global_game_state = GAME_STATE_playing;
  }
  else if (global_game_state == GAME_STATE_playing)
  {
   global_game_state = GAME_STATE_editor;
  }
 }
 
 ui_draw_text(global_normal_font,
              global_renderer_window_w - 200,
              32.0f,
              200.0f,
              colour_literal(1.0f, 0.0f, 0.0f, 1.0f),
              s8_literal("debug build"));
#endif
 
 ui_finish(input);
 
 process_render_queue(gl);
 
 global_time += frametime_in_s;
}

void
game_cleanup(OpenGLFunctions *gl)
{
}

