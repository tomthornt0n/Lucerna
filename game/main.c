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

#define LC_GAME
#include "lucerna.h"

internal void
print_opengl_errors(OpenGLFunctions *gl)
{
#ifdef LUCERNA_DEBUG
 GLenum error;
 while ((error = gl->GetError()) != GL_NO_ERROR)
 {
  fprintf(stderr,
          "OpenGL error: %s\n",
          error == GL_INVALID_ENUM ? "GL_INVALID_ENUM" :
          error == GL_INVALID_VALUE ? "GL_INVALID_VALUE":
          error == GL_INVALID_OPERATION ? "GL_INVALID_OPERATION":
          error == GL_INVALID_FRAMEBUFFER_OPERATION ? "GL_INVALID_FRAMEBUFFER_OPERATION":
          error == GL_OUT_OF_MEMORY ? "GL_OUT_OF_MEMORY":
          error == GL_STACK_UNDERFLOW ? "GL_STACK_UNDERFLOW":
          error == GL_STACK_OVERFLOW ? "GL_STACK_OVERFLOW":
          NULL);
 }
#endif
}

internal MemoryArena global_static_memory;
internal MemoryArena global_frame_memory;
internal MemoryArena global_level_memory;

#include "asset_manager.c"
#include "audio.c"

internal Font *global_ui_font;

#include "renderer.c"
#include "dev_ui.c"
#include "tiles.c"
#include "tile_map_editor.c"
#include "entities.c"
#include "serialisation.c"
#include "maps.c"

internal TileMap global_tile_map;

void
game_init(OpenGLFunctions *gl)
{
 initialise_arena_with_new_memory(&global_static_memory, ONE_GB);
 initialise_arena_with_new_memory(&global_frame_memory, 2 * ONE_MB);
 initialise_arena_with_new_memory(&global_level_memory, 2 * ONE_MB);
 
 global_ui_font = load_font(gl, s8_literal("../assets/fonts/mononoki.ttf"), 19);
 
 initialise_auto_tiler(gl);
 if (!deserialise_tile_map(&global_static_memory, &global_tile_map, s8_literal("test.tile_map")))
 {
  initialise_tile_map(&global_static_memory, &global_tile_map, 100, 100);
 }
 
 initialise_renderer(gl);
 
 push_player(&global_static_memory, 128.0f, 128.0f);
}


void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input,
                       F64 frametime_in_s)
{
 set_renderer_window_size(gl, input->window_width,
                          input->window_height);
 
 prepare_ui();
 
 static B32 editor_mode = false;
 
 if (is_key_typed(input, ctrl('e')))
 {
  editor_mode = !editor_mode;
 }
 
 if (editor_mode)
 {
  edit_tile_map(input, frametime_in_s, &global_tile_map);
 }
 else
 {
  world_draw_sub_texture(rectangle_literal(0.0f, 0.0f, global_renderer_window_w, global_renderer_window_h), colour_literal(1.0f, 1.0f, 1.0f, 1.0f), asset_from_path(s8_literal("../assets/textures/office_1/background.png")), ENTIRE_TEXTURE);
  process_entities(gl, input, frametime_in_s);
  world_draw_sub_texture(rectangle_literal(0.0f, 0.0f, global_renderer_window_w, global_renderer_window_h), colour_literal(1.0f, 1.0f, 1.0f, 1.0f), asset_from_path(s8_literal("../assets/textures/office_1/foreground.png")), ENTIRE_TEXTURE);
  do_bloom(UI_SORT_DEPTH - 1);
 }
 
 I8 fps_str[128];
 snprintf(fps_str, 128, "%f ms (%f fps)", frametime_in_s * 1000.0, 1.0 / frametime_in_s);
 ui_draw_text(global_ui_font, 16.0f, 16.0f, 0, colour_literal(0.0f, 0.0f, 0.0f, 1.0f), s8_from_cstring(&global_frame_memory, fps_str));
 
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
 
 finish_ui(input);
 
 process_render_queue(gl);
}

void
game_cleanup(OpenGLFunctions *gl)
{
 serialise_tile_map(&global_tile_map, s8_literal("test.tile_map"));
}

