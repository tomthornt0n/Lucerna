internal Entity *global_editor_selected_entity = NULL;

internal Asset *global_current_level = NULL;

internal void
set_current_level(OpenGLFunctions *gl,
                  S8 path)
{
 fprintf(stderr, "\n** changing level to '%.*s' **\n", (I32)path.len, path.buffer);
 global_editor_selected_entity = NULL;
 
 fprintf(stderr, " retreiving asset\n");
 Asset *new_level = asset_from_path(path);
 
 if (!new_level->loaded)
 {
  fprintf(stderr, " asset is not already loaded\n");
  if (global_current_level)
  {
   fprintf(stderr, " there is already a level loaded - serialising currently loaded entities\n");
   serialise_entities(global_current_level->level.entities,
                      global_current_level->level.entities_source_path);
  }
  
  fprintf(stderr, " 'freeing' all entities\n");
  _global_entity_next_index = 0;
  _global_entity_free_list = NULL;
  fprintf(stderr, " unloading all assets\n");
  unload_all_assets(gl);
  fprintf(stderr, " freeing level memory\n");
  arena_free_all(&global_level_memory);
  
  
  fprintf(stderr, " parsing level descriptor\n");
  load_level(new_level);
  
  fprintf(stderr, " deserialising entities from '%.*s'\n", (I32)new_level->level.entities_source_path.len, new_level->level.entities_source_path.buffer);
  deserialise_entities(new_level, new_level->level.entities_source_path);
 }
 else
 {
  fprintf(stderr, " asset is already loaded\n");
 }
 
 fprintf(stderr, " reseting camera position\n");
 load_texture(gl, new_level->level.bg);
 set_camera_position(new_level->level.bg->texture.width >> 1,
                     new_level->level.bg->texture.height >> 1);
 
 fprintf(stderr, " assigning `global_current_level`\n");
 global_current_level = new_level;
}


internal void do_entities(OpenGLFunctions *gl, Entity *entities, Player *player);

internal void
do_level(OpenGLFunctions *gl,
         PlatformState *input,
         F64 frametime_in_s,
         Asset *asset)
{
 load_level(asset);
 
 if (asset->loaded &&
     asset->kind == ASSET_KIND_level)
 {
  F32 aspect;
  
  aspect = (F32)asset->level.bg->texture.height / (F32)asset->level.bg->texture.width;
  world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                           SCREEN_WIDTH_IN_WORLD_UNITS,
                                           SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                         WHITE,
                         asset->level.bg,
                         ENTIRE_TEXTURE);
  
  play_audio_source(asset->level.music);
  
  do_entities(gl, asset->level.entities, &asset->level.player);
  
  do_player(gl, input, frametime_in_s, &asset->level.player);
  
  aspect = (F32)asset->level.fg->texture.height / (F32)asset->level.fg->texture.width;
  world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                           SCREEN_WIDTH_IN_WORLD_UNITS,
                                           SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                         WHITE,
                         asset->level.fg,
                         ENTIRE_TEXTURE);
 }
}

internal void
do_current_level(OpenGLFunctions *gl,
                 PlatformState *input,
                 F64 frametime_in_s)
{
 do_level(gl, input, frametime_in_s, global_current_level);
}
