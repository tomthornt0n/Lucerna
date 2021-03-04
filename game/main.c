#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>

#include <immintrin.h>

#include "lucerna.h"

/*
TODO list:
- audio streaming? (if there is some long music and it becomes an issue)
- multithreaded asset loading?
- gameplay!!!!!!!
*/

internal F64 global_time = 0.0;

internal F32 global_exposure = 1.0;

internal MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;
internal MemoryArena global_level_memory;

#define LCDDL_AS_LIBRARY
#include "lcddl.c"

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

#include "renderer.c"
#include "dev_ui.c"
#include "types.gen.h"
#include "funcs.gen.h"
#include "entities.c"
#include "editor.c"
#include "main_menu.c"

void
game_init(OpenGLFunctions *gl)
{
 lcddl_initialise();
 
 initialise_arena_with_new_memory(&global_static_memory, 1 * ONE_MB);
 initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
 initialise_arena_with_new_memory(&global_level_memory, 27 * ONE_MB);
 
 global_ui_font = load_font(gl, s8_literal("../assets/fonts/mononoki.ttf"), 19);
 global_title_font = load_font(gl, s8_literal("../assets/fonts/ShipporiMincho-Regular.ttf"), 72);
 global_normal_font = load_font(gl, s8_literal("../assets/fonts/ShipporiMincho-Regular.ttf"), 28);
 
 initialise_renderer(gl);
 
 load_player_art(gl);
 
 set_current_level(gl, asset_from_path(s8_literal("../assets/levels/office_1.level")));
 
 global_main_functions[GAME_STATE_playing] = do_current_level;
 global_main_functions[GAME_STATE_editor] = do_level_editor;
 global_main_functions[GAME_STATE_main_menu] = do_main_menu;
}

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
 
 I8 fps_str[128];
 snprintf(fps_str, 128, "%f ms (%f fps)", frametime_in_s * 1000.0, 1.0 / frametime_in_s);
 ui_draw_text(global_ui_font, 16.0f, 16.0f, 0, colour_literal(1.0f, 1.0f, 1.0f, 1.0f), s8_literal(fps_str));
 
 I8 pos_str[128];
 snprintf(pos_str, 128, "%f %f", global_current_level.player.x, global_current_level.player.y);
 ui_draw_text(global_ui_font, 16.0f, 64.0f, 0, colour_literal(1.0f, 1.0f, 1.0f, 1.0f), s8_literal(pos_str));
 
 //
 // NOTE(tbt): editor toggle
 //~
#ifdef LUCERNA_DEBUG
 if (is_key_typed(input, ctrl('v')))
 {
  platform_set_vsync(true);
 }
 else if (is_key_typed(input, ctrl('d')))
 {
  platform_set_vsync(false);
 }
 else if (is_key_typed(input, ctrl('f')))
 {
  platform_toggle_fullscreen();
 }
 else if (is_key_typed(input, ctrl('e')))
 {
  global_game_state = (global_game_state == GAME_STATE_editor) ? GAME_STATE_playing : GAME_STATE_editor;
  
  // NOTE(tbt): save entities
  serialise_entities(global_current_level.entities,
                     global_current_level.level_descriptor->level_descriptor.entities_path);
  
  // NOTE(tbt): reset camera position
  set_camera_position(global_current_level.bg->texture.width >> 1,
                      global_current_level.bg->texture.height >> 1);
 }
#endif
 
 ui_finish(input);
 
 process_render_queue(gl);
 
 global_time += frametime_in_s;
}

void
game_cleanup(OpenGLFunctions *gl)
{
}

