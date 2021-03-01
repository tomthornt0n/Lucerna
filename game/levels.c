typedef struct
{
 Asset *fg, *bg, *music;
 Entity *entities;
 Player player;
 S8 entities_path;
} Level;

internal Level global_current_level = {0};

internal Entity *global_editor_selected_entity = NULL;

internal void
set_current_level(OpenGLFunctions *gl,
                  S8 path)
{
 global_editor_selected_entity = NULL;
 
 Asset *new_level = asset_from_path(path);
 
 if (!new_level->loaded)
 {
  _global_entity_next_index = 0;
  _global_entity_free_list = NULL;
  global_current_level.entities = NULL;
  unload_all_assets(gl);
  arena_free_all(&global_level_memory);
  
  load_level_descriptor(new_level);
  
  deserialise_entities(&global_current_level.entities,
                       new_level->level_descriptor.entities_path);
  
  global_current_level.player.x = new_level->level_descriptor.player_spawn_x;
  global_current_level.player.y = new_level->level_descriptor.player_spawn_y;
  
  global_current_level.fg = asset_from_path(new_level->level_descriptor.fg_path);
  global_current_level.bg = asset_from_path(new_level->level_descriptor.bg_path);
  global_current_level.music = asset_from_path(new_level->level_descriptor.music_path);
  global_current_level.entities_path = new_level->level_descriptor.entities_path;
  
  load_texture(gl, global_current_level.bg);
  
  set_camera_position(global_current_level.bg->texture.width >> 1,
                      global_current_level.bg->texture.height >> 1);
  
 }
}


internal void do_entities(OpenGLFunctions *gl, Entity *entities, Player *player);

internal void
do_current_level(OpenGLFunctions *gl,
                 PlatformState *input,
                 F64 frametime_in_s)
{
 F32 aspect;
 
 aspect =
  (F32)global_current_level.bg->texture.height /
  (F32)global_current_level.bg->texture.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        global_current_level.bg,
                        ENTIRE_TEXTURE);
 
 play_audio_source(global_current_level.music);
 
 do_entities(gl, global_current_level.entities, &global_current_level.player);
 
 do_player(gl, input, frametime_in_s, &global_current_level.player);
 
 aspect =
  (F32)global_current_level.fg->texture.height /
  (F32)global_current_level.fg->texture.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        global_current_level.fg,
                        ENTIRE_TEXTURE);
}
