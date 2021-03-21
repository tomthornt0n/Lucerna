typedef struct
{
 F32 x, y;
 F32 x_velocity, y_velocity;
 Rect collision_bounds;
} Player;

internal struct
{
 Texture texture;
 
 S8 texture_path;
 U64 last_modified;
 
 SubTexture forward_head;
 SubTexture forward_left_arm;
 SubTexture forward_right_arm;
 SubTexture forward_lower_body;
 SubTexture forward_torso;
 
 SubTexture left_jacket;
 SubTexture left_leg;
 SubTexture left_head;
 SubTexture left_arm;
 
 SubTexture right_jacket;
 SubTexture right_leg;
 SubTexture right_head;
 SubTexture right_arm;
} global_player_art = {0};

internal void
load_player_art(OpenGLFunctions *gl)
{
 Texture texture;
 
 global_player_art.texture_path = s8_literal("../assets/textures/player.png");
 
 if (load_texture(gl, global_player_art.texture_path, &texture))
 {
  global_player_art.forward_head = sub_texture_from_texture(&texture, 16.0f, 16.0f, 88.0f, 175.0f);
  global_player_art.forward_left_arm = sub_texture_from_texture(&texture, 112.0f, 16.0f, 43.0f, 212.0f);
  global_player_art.forward_right_arm = sub_texture_from_texture(&texture, 160.0f, 16.0f, 29.0f, 216.0f);
  global_player_art.forward_lower_body = sub_texture_from_texture(&texture, 192.0f, 16.0f, 131.0f, 320.0f);
  global_player_art.forward_torso = sub_texture_from_texture(&texture, 336.0f, 16.0f, 175.0f, 310.0f);
  
  global_player_art.left_jacket = sub_texture_from_texture(&texture, 16.0f, 352.0f, 110.0f, 312.0f);
  global_player_art.left_leg = sub_texture_from_texture(&texture, 128.0f, 352.0f, 71.0f, 246.0f);
  global_player_art.left_head = sub_texture_from_texture(&texture, 208.0f, 352.0f, 113.0f, 203.0f);
  global_player_art.left_arm = sub_texture_from_texture(&texture, 464.0f, 352.0f, 42.0f, 213.0f);
  
  global_player_art.right_jacket = sub_texture_from_texture(&texture, 126.0f, 352.0f, -110.0f, 312.0f);
  global_player_art.right_leg = sub_texture_from_texture(&texture, 199.0f, 352.0f, -71.0f, 246.0f);
  global_player_art.right_head = sub_texture_from_texture(&texture, 336.0f, 352.0f, 113.0f, 203.0f);
  global_player_art.right_arm = sub_texture_from_texture(&texture, 512.0f, 352.0f, 42.0f, 213.0f);
  
  global_player_art.texture = texture;
  global_player_art.last_modified = platform_get_file_modified_time_p(global_player_art.texture_path);
 }
 else
 {
  debug_log("*** could not load player art ***\n");
 }
}

internal void
hot_reload_player_art(OpenGLFunctions *gl,
                      F64 frametime_in_s)
{
 static F64 time = 0.0;
 time += frametime_in_s;
 
 F64 refresh_time = 2.0; // NOTE(tbt): refresh every 2 seconds
 if (time > refresh_time)
 {
  time = 0.0;
  
  U64 last_modified = platform_get_file_modified_time_p(global_player_art.texture_path);
  if (last_modified > global_player_art.last_modified)
  {
   global_player_art.last_modified = last_modified;
   debug_log("player texture modified...\n");
   
   Texture texture;
   if (load_texture(gl, global_player_art.texture_path, &texture))
   {
    unload_texture(gl, &global_player_art.texture);
    global_player_art.texture = texture;
    debug_log("successfully hot reloaded player art\n");
   }
   else
   {
    debug_log("error hot reloading player art\n");
   }
  }
 }
}

typedef struct
{
 S8 path;
 
 S8 fg_path;
 S8 bg_path;
 S8 music_path;
 
 U64 fg_last_modified;
 U64 bg_last_modified;
 
 LevelKind kind;
 Entity *entities;
 Player player;
 
 Texture fg;
 Texture bg;
 AudioSource *music;
 
 F32 y_offset_per_x;
 F32 exposure;
 F32 player_scale;
 F32 player_spawn_x;
 F32 player_spawn_y;
} Level;

internal Level global_current_level = {0};

internal Entity *global_editor_selected_entity = NULL;

#define PLAYER_COLLISION_X    0.0f
#define PLAYER_COLLISION_Y -155.0f
#define PLAYER_COLLISION_W  175.0f
#define PLAYER_COLLISION_H  605.0f

internal void
do_player(OpenGLFunctions *gl,
          PlatformState *input,
          F64 frametime_in_s,
          Player *player)
{
 // TODO(tbt): better walk animation
 
 F32 player_speed = (128.0f + sin(global_time * 0.2) * 5.0f) * frametime_in_s;
 
 F32 animation_y_offset = -sin(global_time * 6.0f) * 2.0f;
 
 player->x_velocity =
  input->is_key_down[KEY_a] * -player_speed +
  input->is_key_down[KEY_d] * player_speed;
 
 player->y_velocity = player->x_velocity * global_current_level.y_offset_per_x;
 
 player->x += player->x_velocity;
 player->y += player->y_velocity;
 
 F32 scale = global_current_level.player_scale / (global_renderer_window_h - player->y);
 
 //
 // NOTE(tbt): walk right animation
 //~
 
 if (player->x_velocity >  0.01f)
 {
  world_draw_sub_texture(rectangle_literal(player->x + 32.0f * scale,
                                           player->y + (-153.0f + animation_y_offset) * scale,
                                           113.0f * scale, 203.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.right_head);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f + 2.0f) * 0.1f + 0.03f) * scale,
                                 colour_literal(0.5f, 0.5f, 0.5f, 1.0f),
                                 &global_player_art.texture,
                                 global_player_art.right_leg);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f) * 0.1f + 0.03f) * scale,
                                 WHITE,
                                 &global_player_art.texture,
                                 global_player_art.right_leg);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + animation_y_offset * scale,
                                           110.0f * scale, 312.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.right_jacket);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f * scale,
                                                   player->y + (28.0f + animation_y_offset) * scale,
                                                   42.0f * scale, 213.0f * scale),
                                 (sin(global_time * 2.8f) * 0.12f) * scale,
                                 WHITE,
                                 &global_player_art.texture,
                                 global_player_art.right_arm);
 }
 
 //
 // NOTE(tbt): walk left animation
 //~
 
 else if (player->x_velocity < -0.01f)
 {
  world_draw_sub_texture(rectangle_literal(player->x + 32 * scale,
                                           player->y + (-153.0f + animation_y_offset) * scale,
                                           113.0f * scale, 203.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.left_head);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 76.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f + 2.0f) * 0.1f - 0.03f) * scale,
                                 colour_literal(0.5f, 0.5f, 0.5f, 1.0f),
                                 &global_player_art.texture,
                                 global_player_art.left_leg);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 76.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f) * 0.1f - 0.03f) * scale,
                                 WHITE,
                                 &global_player_art.texture,
                                 global_player_art.left_leg);
  
  world_draw_sub_texture(rectangle_literal(player->x + 64 * scale,
                                           player->y + animation_y_offset * scale,
                                           110.0f * scale, 312.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.left_jacket);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 86.0f * scale,
                                                   player->y + (28.0f + animation_y_offset) * scale,
                                                   42.0f * scale, 213.0f * scale),
                                 (sin(global_time * 2.8f) * 0.12f) * scale,
                                 WHITE,
                                 &global_player_art.texture,
                                 global_player_art.left_arm);
 }
 
 //
 // NOTE(tbt): idle animation
 //~
 
 else
 {
  world_draw_sub_texture(rectangle_literal(player->x + 22.0f * scale,
                                           player->y + 128.0f * scale,
                                           131.0f * scale, 320.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.forward_lower_body);
  
  world_draw_sub_texture(rectangle_literal(player->x + 41.0f * scale,
                                           player->y - 155.0f * scale,
                                           88.0f * scale, 175.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.forward_head);
  
  world_draw_sub_texture(rectangle_literal(player->x + 135.0f * scale,
                                           player->y + (25.0f + sin(global_time + 2.0f) * 7.0f) * scale,
                                           29.0f * scale, 216.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.forward_right_arm);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + (25.0f + sin(global_time + 2.0f) * 7.0f) * scale,
                                           43.0f * scale, 212.0f * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.forward_left_arm);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + (sin(global_time + 2.0f) * 3.0f) * scale,
                                           175.0f * scale, 310.0 * scale),
                         WHITE,
                         &global_player_art.texture,
                         global_player_art.forward_torso);
 }
 
 player->collision_bounds = rectangle_literal(player->x + PLAYER_COLLISION_X * scale,
                                              player->y + PLAYER_COLLISION_Y * scale,
                                              PLAYER_COLLISION_W * scale, PLAYER_COLLISION_H * scale);
}

#define MAX_ENTITIES 120
internal U32 _global_entity_next_index = 0;
internal Entity _global_dummy_entity = {0};
internal Entity _global_entity_pool[MAX_ENTITIES] = {{0}};
internal Entity *_global_entity_free_list = NULL;

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
 
 result->next = *entities;
 *entities = result;
 
 return result;
}

internal void
level_descriptor_from_level(LevelDescriptor *level_descriptor,
                            Level *level)
{
 level_descriptor->kind = level->kind;
 level_descriptor->player_spawn_x = level->player_spawn_x;
 level_descriptor->player_spawn_y = level->player_spawn_y;
 level_descriptor->exposure = level->exposure;
 level_descriptor->floor_gradient = atan(level->y_offset_per_x);
 level_descriptor->player_scale = level->player_scale;
 memcpy(level_descriptor->bg_path, level->bg_path.buffer, level->bg_path.len);
 memcpy(level_descriptor->fg_path, level->fg_path.buffer, level->fg_path.len);
 if (level->music)
 {
  memcpy(level_descriptor->music_path, level->music_path.buffer, level->music_path.len);
 }
}

internal void
serialise_level(Level *level)
{
 debug_log("serialising level %.*s\n", (I32)level->path.len, level->path.buffer);
 
 PlatformFile *file = platform_open_file_ex(level->path,
                                            PLATFORM_OPEN_FILE_read | PLATFORM_OPEN_FILE_write | PLATFORM_OPEN_FILE_always_create);
 if (file)
 {
  LevelDescriptor level_descriptor = {0};
  level_descriptor_from_level(&level_descriptor, &global_current_level);
  serialise_level_descriptor(&level_descriptor, file);
  
  for (Entity *e = level->entities;
       NULL != e;
       e = e->next)
  {
   serialise_entity(e, file);
  }
  
  platform_close_file(&file);
 }
}

internal B32
level_from_level_descriptor(OpenGLFunctions *gl,
                            MemoryArena *memory,
                            Level *level,
                            LevelDescriptor *level_descriptor)
{
 B32 success = true;
 
 level->kind = level_descriptor->kind;
 level->player_spawn_x = level_descriptor->player_spawn_x;
 level->player_spawn_y = level_descriptor->player_spawn_y;
 level->exposure = level_descriptor->exposure;
 level->y_offset_per_x = tan(level_descriptor->floor_gradient);
 level->player_scale = level_descriptor->player_scale;
 
 if (load_texture(gl, s8_literal(level_descriptor->bg_path), &level->bg))
 {
  level->bg_path = copy_s8(memory, s8_literal(level_descriptor->bg_path));
  level->bg_last_modified = platform_get_file_modified_time_p(level->bg_path);
 }
 else
 {
  level->bg_path.len = 0;
  level->bg_path.buffer = NULL;
  level->bg_last_modified = 0;
  level->bg = DUMMY_TEXTURE;
  success = false;
  debug_log("could not load background - using dummy background texture\n");
 }
 
 if (load_texture(gl, s8_literal(level_descriptor->fg_path), &level->fg))
 {
  level->fg_path = copy_s8(memory, s8_literal(level_descriptor->fg_path));
  level->fg_last_modified = platform_get_file_modified_time_p(level->fg_path);
 }
 else
 {
  level->fg_path.len = 0;
  level->fg_path.buffer = NULL;
  level->fg_last_modified = 0;
  level->fg = DUMMY_TEXTURE;
  success = false;
  debug_log("could not load foreground - using dummy foreground texture\n");
 }
 
 if (level->music = load_audio(s8_literal(level_descriptor->music_path)))
 {
  level->music_path = copy_s8(memory, s8_literal(level_descriptor->music_path));
 }
 else
 {
  success = false;
  debug_log("could not load music\n");
 }
 
 return success;
}

#if 0
internal B32
_current_level_from_level_descriptor(OpenGLFunctions *gl,
                                     LevelDescriptor *level_descriptor)
{
 return level_from_level_descriptor(gl, &global_level_memory, &global_current_level, level_descriptor);
}
#endif

internal B32
set_current_level(OpenGLFunctions *gl,
                  S8 path,
                  B32 persist_exposure)
{
 B32 success = true;
 
 Level temp_level = {0};
 LevelDescriptor level_descriptor = {0};
 
 debug_log("loading level %.*s\n", (I32)path.len, path.buffer);
 
 // NOTE(tbt): read level file
 arena_temporary_memory(&global_static_memory)
 {
  S8 file = platform_read_entire_file_p(&global_static_memory, path);
  
  if (file.buffer)
  {
   U8 *read = file.buffer;
   
   // NOTE(tbt): load level descriptor
   deserialise_level_descriptor(&level_descriptor, &read);
   if (level_from_level_descriptor(gl, &global_frame_memory, &temp_level, &level_descriptor))
   {
    // NOTE(tbt): save path temporarily
    S8 temp_path = copy_s8(&global_frame_memory, path);
    
    // NOTE(tbt): unload existing level
    unload_texture(gl, &global_current_level.fg);
    unload_texture(gl, &global_current_level.bg);
    unload_audio(global_current_level.music);
    global_current_level.entities = NULL;
    _global_entity_next_index = 0;
    _global_entity_free_list = NULL;
    arena_free_all(&global_level_memory);
    
    temp_level.path = copy_s8(&global_level_memory, temp_path);
    
    // NOTE(tbt): load entities
    while (read - file.buffer < file.len)
    {
     Entity *e = allocate_and_push_entity(&temp_level.entities);
     deserialise_entity(e, &read);
    }
    
    // NOTE(tbt): save newly loaded level globally
    memcpy(&global_current_level, &temp_level, sizeof(global_current_level));
    global_current_level.player.x = global_current_level.player_spawn_x;
    global_current_level.player.y = global_current_level.player_spawn_y;
    if (!persist_exposure)
    {
     global_exposure = global_current_level.exposure;
    }
    
    // NOTE(tbt): copy saved paths from frame to level memory
    global_current_level.fg_path = copy_s8(&global_level_memory, global_current_level.fg_path);
    global_current_level.bg_path = copy_s8(&global_level_memory, global_current_level.bg_path);
    global_current_level.music_path = copy_s8(&global_level_memory, global_current_level.music_path);
   }
   else
   {
    debug_log("failure loading level '%.*s' - error while initialising level from level descriptor\n",
              (I32)path.len, path.buffer);
    success = false;
   }
  }
  else
  {
   debug_log("failure loading level '%.*s' - could not read level file\n",
             (I32)path.len, path.buffer);
   success = false;
  }
 }
 
 return success;
}

internal void
set_current_level_as_new_level(OpenGLFunctions *gl,
                               S8 path)
{
 LevelDescriptor level_descriptor = {0};
 
 debug_log("creating level %.*s\n", (I32)path.len, path.buffer);
 
 // NOTE(tbt): save path temporarily
 S8 temp_path = copy_s8(&global_frame_memory, path);
 
 // NOTE(tbt): unload existing level
 unload_texture(gl, &global_current_level.fg);
 unload_texture(gl, &global_current_level.bg);
 unload_audio(global_current_level.music);
 global_current_level.entities = NULL;
 _global_entity_next_index = 0;
 _global_entity_free_list = NULL;
 arena_free_all(&global_level_memory);
 
 // NOTE(tbt): setup new level
 global_current_level.path = copy_s8(&global_level_memory, temp_path);
 platform_write_entire_file_p(path, &level_descriptor, sizeof(level_descriptor));
 level_from_level_descriptor(gl, &global_level_memory, &global_current_level, &level_descriptor);
}

internal void
do_entities(OpenGLFunctions *gl,
            F64 frametime_in_s,
            Entity *entities,
            Player *player)
{
 static DialogueState dialogue_state = {0};
 
 for (Entity *e = entities;
      NULL != e;
      e = e->next)
 {
  //
  // NOTE(tbt): process triggers
  //~
  
  if (are_rectangles_intersecting(e->bounds, player->collision_bounds))
  {
   if (e->triggers & (1 << ENTITY_TRIGGER_player_intersecting))
   {
    e->triggers &= ~(1 << ENTITY_TRIGGER_player_entered);
   }
   else
   {
    e->triggers |= (1 << ENTITY_TRIGGER_player_entered);
    e->triggers |= (1 << ENTITY_TRIGGER_player_intersecting);
   }
  }
  else
  {
   if (e->triggers & (1 << ENTITY_TRIGGER_player_intersecting))
   {
    e->triggers |= (1 << ENTITY_TRIGGER_player_left);
    e->triggers &= ~(1 << ENTITY_TRIGGER_player_intersecting);
   }
   else
   {
    e->triggers &= ~(1 << ENTITY_TRIGGER_player_left);
   }
  }
  
  //
  // NOTE(tbt): process flags
  //~
  
  // NOTE(tbt): fade out
  if (e->flags & (1 << ENTITY_FLAG_fade_out) &&
      e->triggers & (1 << ENTITY_TRIGGER_player_intersecting))
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
   set_audio_master_level(fade);
  }
  
  // NOTE(tbt): teleports
  if (e->flags & (1 << ENTITY_FLAG_teleport) &&
      e->triggers & (1 << ENTITY_TRIGGER_player_entered))
  {
   // NOTE(tbt): make a copy of the current entity, so we can read the spawn position from it once the new level has been loaded
   Entity _e = *e;
   
   S8List *level_path = NULL;
   level_path = push_s8_to_list(&global_frame_memory,
                                level_path,
                                s8_literal(e->teleport_to_level));
   level_path = push_s8_to_list(&global_frame_memory,
                                level_path,
                                s8_literal("../assets/levels/"));
   
   B32 success = set_current_level(gl, expand_s8_list(&global_frame_memory, level_path), !e->teleport_do_not_persist_exposure);
   
   if (!success)
   {
    platform_quit();
   }
   
   if (_e.teleport_to_non_default_spawn)
   {
    player->x = _e.teleport_to_x;
    player->y = _e.teleport_to_y;
   }
  }
  
  // NOTE(tbt): dialogue
  if (e->flags & (1 << ENTITY_FLAG_trigger_dialogue) &&
      e->triggers & (1 << ENTITY_TRIGGER_player_entered))
  {
   S8List *dialogue_path = NULL;
   dialogue_path = push_s8_to_list(&global_frame_memory,
                                   dialogue_path,
                                   s8_literal(e->dialogue_path));
   dialogue_path = push_s8_to_list(&global_frame_memory,
                                   dialogue_path,
                                   s8_literal("../assets/dialogue/"));
   
   play_dialogue(&dialogue_state,
                 load_dialogue(&global_level_memory,
                               expand_s8_list(&global_frame_memory, dialogue_path)),
                 e->dialogue_x, e->dialogue_y,
                 WHITE);
   if (!e->repeat_dialogue)
   {
    e->flags &= ~(1 << ENTITY_FLAG_trigger_dialogue);
   }
  }
 }
 
 do_dialogue(&dialogue_state, frametime_in_s);
}

internal void
hot_reload_current_level_art(OpenGLFunctions *gl,
                             F64 frametime_in_s)
{
 static F64 time = 0.0;
 time += frametime_in_s;
 
 F64 refresh_time = 2.0; // NOTE(tbt): refresh every 2 seconds
 if (time > refresh_time)
 {
  time = 0.0;
  
  // TODO(tbt): hot reload music
  
  // NOTE(tbt): foreground
  {
   
   U64 fg_last_modified = platform_get_file_modified_time_p(global_current_level.fg_path);
   if (fg_last_modified > global_current_level.fg_last_modified)
   {
    Texture texture;
    
    global_current_level.fg_last_modified = fg_last_modified;
    debug_log("foreground modified...\n");
    
    if (load_texture(gl, global_current_level.fg_path, &texture))
    {
     unload_texture(gl, &global_current_level.fg);
     global_current_level.fg = texture;
     
     debug_log("successfully hot reloaded foreground\n");
    }
    else
    {
     debug_log("error hot reloading foreground\n");
    }
   }
  }
  
  // NOTE(tbt): background
  {
   U64 bg_last_modified = platform_get_file_modified_time_p(global_current_level.bg_path);
   if (bg_last_modified > global_current_level.bg_last_modified)
   {
    Texture texture;
    
    global_current_level.bg_last_modified = bg_last_modified;
    debug_log("background modified...\n");
    
    if (load_texture(gl, global_current_level.bg_path, &texture))
    {
     unload_texture(gl, &global_current_level.bg);
     global_current_level.bg = texture;
     
     debug_log("successfully hot reloaded background\n");
    }
    else
    {
     debug_log("error hot reloading background\n");
    }
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
  (F32)global_current_level.bg.height /
  (F32)global_current_level.bg.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        &global_current_level.bg,
                        ENTIRE_TEXTURE);
 
 play_audio_source(global_current_level.music);
 
 do_entities(gl, frametime_in_s, global_current_level.entities, &global_current_level.player);
 
 do_player(gl, input, frametime_in_s, &global_current_level.player);
 
 aspect =
  (F32)global_current_level.fg.height /
  (F32)global_current_level.fg.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        &global_current_level.fg,
                        ENTIRE_TEXTURE);
 
 do_post_processing(global_exposure,
                    global_current_level.kind,
                    UI_SORT_DEPTH - 2);
}
