typedef struct
{
 F32 x, y;
 F32 x_velocity, y_velocity;
 Rect collision_bounds;
} Player;

internal struct
{
 Asset *texture;
 
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
} global_player_art;

internal void
load_player_art(OpenGLFunctions *gl)
{
 Asset *texture = asset_from_path(s8_literal("../assets/textures/player.png"));
 set_asset_persist(texture, true);
 load_texture(gl, texture);
 global_player_art.texture = texture;
 
 global_player_art.forward_head = sub_texture_from_texture(gl, texture, 16.0f, 16.0f, 88.0f, 175.0f);
 global_player_art.forward_left_arm = sub_texture_from_texture(gl, texture, 112.0f, 16.0f, 43.0f, 212.0f);
 global_player_art.forward_right_arm = sub_texture_from_texture(gl, texture, 160.0f, 16.0f, 29.0f, 216.0f);
 global_player_art.forward_lower_body = sub_texture_from_texture(gl, texture, 192.0f, 16.0f, 131.0f, 320.0f);
 global_player_art.forward_torso = sub_texture_from_texture(gl, texture, 336.0f, 16.0f, 175.0f, 310.0f);
 
 global_player_art.left_jacket = sub_texture_from_texture(gl, texture, 16.0f, 352.0f, 110.0f, 312.0f);
 global_player_art.left_leg = sub_texture_from_texture(gl, texture, 128.0f, 352.0f, 71.0f, 246.0f);
 global_player_art.left_head = sub_texture_from_texture(gl, texture, 208.0f, 352.0f, 113.0f, 203.0f);
 global_player_art.left_arm = sub_texture_from_texture(gl, texture, 464.0f, 352.0f, 42.0f, 213.0f);
 
 global_player_art.right_jacket = sub_texture_from_texture(gl, texture, 126.0f, 352.0f, -110.0f, 312.0f);
 global_player_art.right_leg = sub_texture_from_texture(gl, texture, 199.0f, 352.0f, -71.0f, 246.0f);
 global_player_art.right_head = sub_texture_from_texture(gl, texture, 336.0f, 352.0f, 113.0f, 203.0f);
 global_player_art.right_arm = sub_texture_from_texture(gl, texture, 512.0f, 352.0f, 42.0f, 213.0f);
}

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
 Player player;
 LevelKind kind;
 F32 y_offset_per_x;
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
 // TODO(tbt): better rotation function for legs
 
 F32 player_speed = (128.0f + sin(global_time * 0.2) * 5.0f) * frametime_in_s;
 
 F32 animation_y_offset = -sin(global_time * 6.0f) * 2.0f;
 
 player->x_velocity =
  input->is_key_pressed[KEY_a] * -player_speed +
  input->is_key_pressed[KEY_d] * player_speed;
 
 player->y_velocity = player->x_velocity * global_current_level.y_offset_per_x;
 
 player->x += player->x_velocity;
 player->y += player->y_velocity;
 
 F32 scale = global_current_level.level_descriptor->level_descriptor.player_scale * (1.0f / (global_renderer_window_h - player->y));
 
 if (player->x_velocity >  0.01f)
 {
  world_draw_sub_texture(rectangle_literal(player->x + 32.0f * scale,
                                           player->y + (-153.0f + animation_y_offset) * scale,
                                           113.0f * scale, 203.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.right_head);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f + 2.0f) * 0.1f + 0.03f) * scale,
                                 colour_literal(0.5f, 0.5f, 0.5f, 1.0f),
                                 global_player_art.texture,
                                 global_player_art.right_leg);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f) * 0.1f + 0.03f) * scale,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.right_leg);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + animation_y_offset * scale,
                                           110.0f * scale, 312.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.right_jacket);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f * scale,
                                                   player->y + (28.0f + animation_y_offset) * scale,
                                                   42.0f * scale, 213.0f * scale),
                                 (sin(global_time * 2.8f) * 0.12f) * scale,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.right_arm);
 }
 else if (player->x_velocity < -0.01f)
 {
  world_draw_sub_texture(rectangle_literal(player->x + 32 * scale,
                                           player->y + (-153.0f + animation_y_offset) * scale,
                                           113.0f * scale, 203.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.left_head);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 76.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f + 2.0f) * 0.1f - 0.03f) * scale,
                                 colour_literal(0.5f, 0.5f, 0.5f, 1.0f),
                                 global_player_art.texture,
                                 global_player_art.left_leg);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 76.0f * scale,
                                                   player->y + (205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset) * scale,
                                                   71.0f * scale, 246.0f * scale),
                                 (sin(global_time * 3.0f) * 0.1f - 0.03f) * scale,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.left_leg);
  
  world_draw_sub_texture(rectangle_literal(player->x + 64 * scale,
                                           player->y + animation_y_offset * scale,
                                           110.0f * scale, 312.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.left_jacket);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 86.0f * scale,
                                                   player->y + (28.0f + animation_y_offset) * scale,
                                                   42.0f * scale, 213.0f * scale),
                                 (sin(global_time * 2.8f) * 0.12f) * scale,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.left_arm);
 }
 else
 {
  world_draw_sub_texture(rectangle_literal(player->x + 22.0f * scale,
                                           player->y + 128.0f * scale,
                                           131.0f * scale, 320.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_lower_body);
  
  world_draw_sub_texture(rectangle_literal(player->x + 41.0f * scale,
                                           player->y - 155.0f * scale,
                                           88.0f * scale, 175.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_head);
  
  world_draw_sub_texture(rectangle_literal(player->x + 135.0f * scale,
                                           player->y + (25.0f + sin(global_time + 2.0f) * 7.0f) * scale,
                                           29.0f * scale, 216.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_right_arm);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + (25.0f + sin(global_time + 2.0f) * 7.0f) * scale,
                                           43.0f * scale, 212.0f * scale),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_left_arm);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + (sin(global_time + 2.0f) * 3.0f) * scale,
                                           175.0f * scale, 310.0 * scale),
                         WHITE,
                         global_player_art.texture,
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
 fprintf(stderr, "\n***** changing level *****\n");
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
  global_current_level.kind = level_descriptor->level_descriptor.is_memory ? LEVEL_KIND_memory : LEVEL_KIND_world; 
  global_current_level.y_offset_per_x = tan(level_descriptor->level_descriptor.floor_gradient);
  global_current_level.level_descriptor = level_descriptor;
  
  global_exposure = level_descriptor->level_descriptor.exposure;
  
  load_texture(gl, global_current_level.bg);
  
  set_camera_position(global_current_level.bg->texture.width >> 1,
                      global_current_level.bg->texture.height >> 1);
  
 }
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
  // NOTE(tbt): enable codepaths depending on flags
  //~
  
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
   
   global_exposure = fade * global_current_level.level_descriptor->level_descriptor.exposure;
   set_audio_master_level(fade * DEFAULT_AUDIO_MASTER_LEVEL);
  }
  
  if (e->flags & (1 << ENTITY_FLAG_teleport) &&
      e->triggers & (1 << ENTITY_TRIGGER_player_entered))
  {
   Entity old_entity = *e;
   set_current_level(gl, asset_from_path(s8_literal(e->teleport_to_level)));
   if (!old_entity.teleport_to_default_spawn)
   {
    player->x = old_entity.teleport_to_x;
    player->y = old_entity.teleport_to_y;
   }
  }
  
  if (e->flags & (1 << ENTITY_FLAG_trigger_dialogue) &&
      e->triggers & (1 << ENTITY_TRIGGER_player_entered))
  {
   play_dialogue(&dialogue_state,
                 asset_from_path(s8_literal(e->dialogue_path)),
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
 
 do_entities(gl, frametime_in_s, global_current_level.entities, &global_current_level.player);
 
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
 
 do_post_processing(global_exposure,
                    global_current_level.kind,
                    UI_SORT_DEPTH - 2);
}
