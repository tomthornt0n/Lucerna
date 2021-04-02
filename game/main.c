#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>

#define LUCERNA_GAME
#include "lucerna.h"

//-
// TODO(tbt):
//   - save game system
//   - improved dev ui (or just use microui?)
//   - make the player walk less weirdly
//   - second pass on menus
//   - cutscenes?
//   - gameplay!!!!!!!
//   - asset packing for release builds
//-

internal F64 global_time = 0.0;
internal F32 global_exposure = 1.0;

internal F64 global_audio_master_level = 0.8;

internal MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;
internal MemoryArena global_level_memory;
internal MemoryArena global_temp_memory;

#include "locale.c"
#include "asset_manager.c"
#include "cmixer.c"

internal Font *global_ui_font;

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

cm_Source *global_click_sound = NULL;

#include "types.gen.h"
#include "renderer.c"
// #include "dev_ui.c"
#include "dev_ui_v2.c"
#include "funcs.gen.h"
#include "dialogue.c"
#include "entities.c"
// #include "editor.c"
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
// NOTE(tbt): audio
//~

internal void
cmixer_lock_handler(cm_Event *e)
{
 if (e->type == CM_EVENT_LOCK)
 {
  platform_get_audio_lock();
 }
 else if (e->type == CM_EVENT_UNLOCK)
 {
  platform_release_audio_lock();
 }
}

void
game_audio_callback(void *buffer,
                    U32 buffer_size)
{
 cm_process(buffer, buffer_size / 2);
}

//
// NOTE(tbt): initialisation
//~

void
game_init(OpenGLFunctions *gl)
{
 initialise_arena_with_new_memory(&global_static_memory, 4 * ONE_MB);
 initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
 initialise_arena_with_new_memory(&global_level_memory, 100 * ONE_MB);
 initialise_arena_with_new_memory(&global_temp_memory, 100 * ONE_MB);
 
 set_locale(gl, LOCALE_en_gb);
 
 cm_init(AUDIO_SAMPLERATE);
 cm_set_lock(cmixer_lock_handler);
 cm_set_master_gain(global_audio_master_level);
 
 global_ui_font = load_font(gl,
                            &global_static_memory,
                            s8_literal("../assets/fonts/mononoki.ttf"),
                            19,
                            32, 255);
 
 global_click_sound = cm_new_source_from_file("../assets/audio/click.wav");
 
 load_player_art(gl);
 
 initialise_renderer(gl);
 
 ui_initialise();
 
 set_camera_position(960.0f, 540.0f);
 
 // NOTE(tbt): look up table of main functions per game state
 global_main_functions[GAME_STATE_playing] = game_playing_main;
 global_main_functions[GAME_STATE_editor] = do_main_menu /* edit_current_level */;
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
 
 gl->Clear(GL_COLOR_BUFFER_BIT);
 
 ui_prepare(input, frametime_in_s);
 
 global_main_functions[global_game_state](gl, input, frametime_in_s);
 
 //
 // NOTE(tbt): debug extras
 //~
#ifdef LUCERNA_DEBUG
 U8 debug_overlay_str[256];
 snprintf(debug_overlay_str,
          sizeof(debug_overlay_str),
          "frametime  : %f ms (%f fps)\n"
          "player pos : %f %f",
          frametime_in_s * 1000.0,
          1.0 / frametime_in_s,
          global_current_level.player.x,
          global_current_level.player.y);
 
 draw_s8(global_ui_font,
         16.0f, 16.0f,
         -1.0f,
         colour_literal(1.0f, 1.0f, 1.0f, 1.0f),
         s8_literal(debug_overlay_str),
         UI_SORT_DEPTH, global_ui_projection_matrix);
 
 
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
   set_current_level(gl, global_current_level.path,
                     false,
                     false,
                     global_current_level.player.x,
                     global_current_level.player.y);
   
   // NOTE(tbt): reset camera position
   set_camera_position(global_current_level.bg.width >> 1,
                       global_current_level.bg.height >> 1);
   
   global_game_state = GAME_STATE_playing;
  }
  else if (global_game_state == GAME_STATE_playing)
  {
   cm_stop(global_current_level.music);
   global_game_state = GAME_STATE_editor;
  }
 }
 
 draw_s8(global_ui_font,
         global_rcx.window.w - 200,
         32.0f,
         200.0f,
         colour_literal(1.0f, 0.0f, 0.0f, 1.0f),
         s8_literal("debug build"),
         UI_SORT_DEPTH, global_ui_projection_matrix);
#endif
 
 ui_finish();
 
 renderer_flush_message_queue(gl);
 
 arena_free_all(&global_frame_memory);
 
 global_time += frametime_in_s;
}

void
game_cleanup(OpenGLFunctions *gl)
{
}

