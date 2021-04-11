#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>

#define LUCERNA_GAME
#include "lucerna.h"

//-TODO:
//
//   - save game system
//   - make the player walk less weirdly
//   - second pass on menus
//   - cutscenes?
//   - asset packing for release builds
//   - gameplay!!!!!!!
//
//~

#define gl_func(_type, _func) internal PFNGL ## _type ## PROC gl ## _func;
#include "gl_funcs.h"

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

typedef void ( *PerGameStateMainFunction)(PlatformState *, F64);
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
// NOTE(tbt): test new ui stuff
//~

internal void
demo_ui(PlatformState *input,
        F64 frametime_in_s,
        S8 title)
{
 static F32 width  = 656.0f;
 static F32 height = 574.0f;
 
 ui_width(width, 1.0f) ui_height(height, 1.0f) ui_window(title)
 {
  ui_height(36.0f, 0.8f)
  {
   static B32 toggled;
   
   ui_width(999999.0f, 0.0f) ui_label(s8_from_format_string(&global_frame_memory,
                                                            "window dimensions: %f x %f",
                                                            width, height));
   if (global_ui_context.hot)
   {
    ui_width(999999.0f, 0.0f) ui_label(s8_concatenate(&global_frame_memory,
                                                      &global_frame_memory,
                                                      2,
                                                      s8("hot widget: "),
                                                      global_ui_context.hot->key));
   }
   else
   {
    ui_width(999999.0f, 0.0f) ui_label(s8("no hot widget currently"));
   }
   
   ui_width(99999999.0f, 0.0f) ui_row()
   {
    ui_width(150.0f, 0.0f)
    {
     if (ui_button(s8("make taller")))
     {
      height += 16.0f;
     }
     
     if (ui_button(s8("make wider")))
     {
      width += 16.0f;
     }
     
     if (ui_button(s8("make shorter")))
     {
      height -= 16.0f;
     }
     
     if (ui_button(s8("make narrower")))
     {
      width -= 16.0f;
     }
    }
   }
   
   static B32 is_memory = false;
   ui_width(200.0f, 0.5f) ui_toggle_button(s8("level is memory"), &is_memory);
   
   if (is_memory)
   {
    global_current_level.kind = LEVEL_KIND_memory;
   }
   else
   {
    global_current_level.kind = LEVEL_KIND_world;
   }
   
   ui_width(200.0f, 1.0f) ui_label(s8("list of toggles:"));
   
   ui_width(120.0f, 0.5f) ui_indent(32.0f)
   {
    static B32 toggleds[5] = {0};
    for (I32 i = 0;
         i < array_count(toggleds);
         ++i)
    {
     F32 col_transition = clamp_f((F32)i / (F32)array_count(toggleds),
                                  0.0f, 1.0f);
     ui_colour(colour_literal(1.0f, col_transition, 1.0f - col_transition, 1.0f))
      ui_toggle_button(s8_from_format_string(&global_frame_memory,
                                             "- toggle %d", i),
                       toggleds + i);
    }
   }
   
   ui_width(500.0f, 1.0f) ui_row()
   {
    ui_width(100.0f, 1.0f) ui_label(s8("player x:"));
    ui_width(300.0f, 1.0f) ui_slider_f32(s8("player x slider"),
                                         &global_current_level.player.x,
                                         0.0f, 1920.0f);
   }
   ui_width(500.0f, 1.0f) ui_row()
   {
    ui_width(100.0f, 1.0f) ui_label(s8("player y:"));
    ui_width(300.0f, 1.0f) ui_slider_f32(s8("player y slider"),
                                         &global_current_level.player.y,
                                         0.0f, 1080.0f);
   }
   ui_width(500.0f, 1.0f) ui_row()
   {
    ui_width(100.0f, 1.0f) ui_label(s8("exposure:"));
    ui_width(300.0f, 1.0f) ui_slider_f32(s8("exposure slider"),
                                         &global_exposure,
                                         0.0f, 3.0f);
   }
   
   ui_width(500.0f, 1.0f)
   {
    S8 test = ui_line_edit(s8("test line edit"));
    ui_label(test);
   }
  }
 }
}

//
// NOTE(tbt): main loop for game_playing state
//~

internal void
game_playing_main(PlatformState *input,
                  F64 frametime_in_s)
{
#ifdef LUCERNA_DEBUG
 hot_reload_current_level_art(frametime_in_s);
 hot_reload_player_art(frametime_in_s);
 hot_reload_shaders(frametime_in_s);
#endif
 do_current_level(input, frametime_in_s);
 ui_prepare(input, frametime_in_s);
 demo_ui(input, frametime_in_s, s8("test"));
 ui_finish();
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
 // NOTE(tbt): copy OpenGLFunctions struct to global function pointers
#define gl_func(_type, _func) gl ## _func = gl-> ## _func;
#include "gl_funcs.h"
 
 initialise_arena_with_new_memory(&global_static_memory, 4 * ONE_MB);
 initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
 initialise_arena_with_new_memory(&global_level_memory, 100 * ONE_MB);
 initialise_arena_with_new_memory(&global_temp_memory, 100 * ONE_MB);
 
 set_locale(LOCALE_en_gb);
 
 cm_init(AUDIO_SAMPLERATE);
 cm_set_lock(cmixer_lock_handler);
 cm_set_master_gain(global_audio_master_level);
 
 global_ui_font = load_font(&global_static_memory,
                            s8("../assets/fonts/mononoki.ttf"),
                            19,
                            32, 255);
 
 global_click_sound = cm_new_source_from_file("../assets/audio/click.wav");
 
 load_player_art();
 
 initialise_renderer();
 
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
game_update_and_render(PlatformState *input,
                       F64 frametime_in_s)
{
 set_renderer_window_size(input->window_w, input->window_h);
 
 glClear(GL_COLOR_BUFFER_BIT);
 
 global_main_functions[global_game_state](input, frametime_in_s);
 
 //
 // NOTE(tbt): debug extras
 //~
#if defined LUCERNA_DEBUG
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
         s8(debug_overlay_str),
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
   set_current_level(global_current_level.path,
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
         s8("debug build"),
         UI_SORT_DEPTH, global_ui_projection_matrix);
#endif
 
 renderer_flush_message_queue();
 
 arena_free_all(&global_frame_memory);
 
 global_time += frametime_in_s;
}

void
game_cleanup(void)
{
}

