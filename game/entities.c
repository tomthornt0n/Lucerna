typedef enum
{
 LEVEL_KIND_world = POST_PROCESSING_KIND_world,
 LEVEL_KIND_memory = POST_PROCESSING_KIND_memory,
} LevelKind;

typedef struct
{
 Asset *level_descriptor;
 Asset *fg, *bg, *music;
 Entity *entities;
 S8 entities_path;
 Player player;
 LevelKind kind;
 F32 exposure;
} Level;

internal Level global_current_level = {0};

internal Entity *global_editor_selected_entity = NULL;


#define MAX_ENTITIES 120
internal U32 _global_entity_next_index = 0;
internal Entity _global_dummy_entity = {0};
internal Entity _global_entity_pool[MAX_ENTITIES] = {{0}};
internal Entity *_global_entity_free_list = NULL;

internal void
push_entity(Entity *entity,
            Entity **entities)
{
 entity->next = *entities;
 *entities = entity;
}

internal Entity *
allocate_and_push_entity(Entity **entities)
{
 Entity *result;
 
 if (_global_entity_free_list)
 {
  result = _global_entity_free_list;
  _global_entity_free_list = _global_entity_free_list->next_free;
 }
 else if (_global_entity_next_index < MAX_ENTITIES)
 {
  result = &_global_entity_pool[_global_entity_next_index++];
 }
 else
 {
  result = &_global_dummy_entity;
 }
 
 memset(result, 0, sizeof(*result));
 
 push_entity(result, entities);
 
 return result;
}

internal void
serialise_entities(Entity *entities,
                   S8 path)
{
 PlatformFile *file = platform_open_file(path);
 
 for (Entity *e = entities;
      NULL != e;
      e = e->next)
 {
  serialise_entity(e, file);
 }
 
 platform_close_file(&file);
}

internal void
deserialise_entities(Entity **entities,
                     S8 path)
{
 temporary_memory_begin(&global_static_memory);
 
 S8 file = platform_read_entire_file(&global_static_memory,
                                     path);
 
 if (file.buffer)
 {
  U8 *buffer = file.buffer;
  
  while (buffer - file.buffer < file.len)
  {
   Entity *e = allocate_and_push_entity(entities);
   deserialise_entity(e, &buffer);
  }
 }
 
 temporary_memory_end(&global_static_memory);
}

internal void
set_current_level(OpenGLFunctions *gl,
                  Asset *level_descriptor)
{
 global_editor_selected_entity = NULL;
 
 if (!level_descriptor->loaded)
 {
  _global_entity_next_index = 0;
  _global_entity_free_list = NULL;
  global_current_level.entities = NULL;
  unload_all_assets(gl);
  arena_free_all(&global_level_memory);
  
  load_level_descriptor(level_descriptor);
  
  deserialise_entities(&global_current_level.entities,
                       level_descriptor->level_descriptor.entities_path);
  
  global_current_level.player.x = level_descriptor->level_descriptor.player_spawn_x;
  global_current_level.player.y = level_descriptor->level_descriptor.player_spawn_y;
  global_current_level.fg = asset_from_path(level_descriptor->level_descriptor.fg_path);
  global_current_level.bg = asset_from_path(level_descriptor->level_descriptor.bg_path);
  global_current_level.music = asset_from_path(level_descriptor->level_descriptor.music_path);
  global_current_level.entities_path = level_descriptor->level_descriptor.entities_path;
  global_current_level.exposure = level_descriptor->level_descriptor.exposure; 
  global_current_level.kind = level_descriptor->level_descriptor.is_memory ? LEVEL_KIND_memory : LEVEL_KIND_world; 
  global_current_level.level_descriptor = level_descriptor;
  
  global_exposure = global_current_level.exposure;
  
  load_texture(gl, global_current_level.bg);
  
  set_camera_position(global_current_level.bg->texture.width >> 1,
                      global_current_level.bg->texture.height >> 1);
  
 }
}

internal void
do_entities(OpenGLFunctions *gl,
            Entity *entities,
            Player *player)
{
 for (Entity *e = entities;
      NULL != e;
      e = e->next)
 {
  B32 colliding_with_player = are_rectangles_intersecting(e->bounds, player->collision_bounds);
  
  if (e->flags & (1 << ENTITY_FLAG_fade_out))
  {
   if (colliding_with_player)
   {
    F32 player_centre_x = player->collision_bounds.x + (player->collision_bounds.w / 2.0f);
    F32 player_centre_y = player->collision_bounds.y + (player->collision_bounds.h / 2.0f);
    
    F32 fade;
    
    switch(e->fade_out_direction)
    {
     case FADE_OUT_DIR_n:
     {
      fade = (player_centre_y - e->bounds.y) / e->bounds.h;
      break;
     }
     case FADE_OUT_DIR_e:
     {
      fade = ((e->bounds.x + e->bounds.w) - player_centre_x) / e->bounds.w;
      break;
     }
     case FADE_OUT_DIR_s:
     {
      fade = ((e->bounds.y + e->bounds.h) - player_centre_y) / e->bounds.h;
      break;
     }
     case FADE_OUT_DIR_w:
     {
      fade = (player_centre_x - e->bounds.x) / e->bounds.w;
      break;
     }
    }
    
    fade = clamp_f(fade, 0.0f, 1.0f);
    
    global_exposure = fade * global_current_level.exposure;
    set_audio_master_level(fade * DEFAULT_AUDIO_MASTER_LEVEL);
   }
   
  }
  
  if (e->flags & (1 << ENTITY_FLAG_teleport) &&
      colliding_with_player)
  {
   Entity old_entity = *e;
   set_current_level(gl, asset_from_path(s8_literal(e->teleport_to_level)));
   if (!old_entity.teleport_to_default_spawn)
   {
    player->x = old_entity.teleport_to_x;
    player->y = old_entity.teleport_to_y;
   }
  }
 }
}

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
 
 do_post_processing(global_exposure, global_current_level.kind, UI_SORT_DEPTH - 1);
}
