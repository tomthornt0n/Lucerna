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

internal void set_current_level(OpenGLFunctions *gl, S8 path);

internal void
do_entities(OpenGLFunctions *gl,
            Entity *entities,
            Player *player)
{
 for (Entity *e = entities;
      NULL != e;
      e = e->next)
 {
  B32 colliding_with_player = rectangles_are_intersecting(e->bounds, player->collision_bounds);
  
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
    
    global_exposure = fade * DEFAULT_EXPOSURE;
    set_audio_master_level(fade * DEFAULT_AUDIO_MASTER_LEVEL);
   }
   
  }
  
  if (e->flags & (1 << ENTITY_FLAG_teleport) &&
      colliding_with_player)
  {
   Entity old_entity = *e;
   set_current_level(gl, s8_literal(e->teleport_to_level));
   if (!old_entity.teleport_to_default_spawn)
   {
    player->x = old_entity.teleport_to_x;
    player->y = old_entity.teleport_to_y;
   }
  }
 }
}
