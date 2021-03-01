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

internal void
do_player(OpenGLFunctions *gl,
          PlatformState *input,
          F64 frametime_in_s,
          Player *player)
{
 F32 player_speed = (128.0f + sin(global_time * 0.2) * 5.0f) * frametime_in_s;
 
 F32 animation_y_offset = sin(global_time + 2.0f) * 3.0f;
 
 player->x_velocity =
  input->is_key_pressed[KEY_a] * -player_speed +
  input->is_key_pressed[KEY_d] * player_speed;
 
 player->x += player->x_velocity;
 player->y += player->y_velocity;
 
 if (player->x_velocity >  0.01f)
 {
  world_draw_sub_texture(rectangle_literal(player->x + 32.0f,
                                           player->y - 153.0f + animation_y_offset,
                                           113.0f, 203.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.right_head);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f,
                                                   player->y + 205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset,
                                                   71.0f, 246.0f),
                                 sin(global_time * 3.0f + 2.0f) * 0.1f + 0.03f,
                                 colour_literal(0.5f, 0.5f, 0.5f, 1.0f),
                                 global_player_art.texture,
                                 global_player_art.right_leg);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f,
                                                   player->y + 205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset,
                                                   71.0f, 246.0f),
                                 sin(global_time * 3.0f) * 0.1f + 0.03f,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.right_leg);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + animation_y_offset,
                                           110.0f, 312.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.right_jacket);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 40.0f,
                                                   player->y + 28.0f + animation_y_offset,
                                                   42.0f, 213.0f),
                                 sin(global_time * 2.8f) * 0.12f,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.right_arm);
 }
 else if (player->x_velocity < -0.01f)
 {
  world_draw_sub_texture(rectangle_literal(player->x + 32,
                                           player->y - 153.0f + animation_y_offset,
                                           113.0f, 203.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.left_head);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 76.0f,
                                                   player->y + 205.0f + sin(global_time * 3.0f + 2.0f) * 7.0f + animation_y_offset,
                                                   71.0f, 246.0f),
                                 sin(global_time * 3.0f + 2.0f) * 0.1f - 0.03f,
                                 colour_literal(0.5f, 0.5f, 0.5f, 1.0f),
                                 global_player_art.texture,
                                 global_player_art.left_leg);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 76.0f,
                                                   player->y + 205.0f + sin(global_time * 3.0f) * 7.0f + animation_y_offset,
                                                   71.0f, 246.0f),
                                 sin(global_time * 3.0f) * 0.1f - 0.03f,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.left_leg);
  
  world_draw_sub_texture(rectangle_literal(player->x + 64,
                                           player->y + animation_y_offset,
                                           110.0f, 312.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.left_jacket);
  
  world_draw_rotated_sub_texture(rectangle_literal(player->x + 86.0f,
                                                   player->y + 28.0f + animation_y_offset,
                                                   42.0f, 213.0f),
                                 sin(global_time * 2.8f) * 0.12f,
                                 WHITE,
                                 global_player_art.texture,
                                 global_player_art.left_arm);
 }
 else
 {
  world_draw_sub_texture(rectangle_literal(player->x + 22.0f,
                                           player->y + 128.0f,
                                           131.0f, 320.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_lower_body);
  
  world_draw_sub_texture(rectangle_literal(player->x + 41.0f,
                                           player->y - 155.0f,
                                           88.0f, 175.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_head);
  
  world_draw_sub_texture(rectangle_literal(player->x + 135.0f,
                                           player->y + 25.0f + sin(global_time + 2.0f) * 7.0f,
                                           29.0f, 216.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_right_arm);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + 25.0f + sin(global_time + 2.0f) * 7.0f,
                                           43.0f, 212.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_left_arm);
  
  world_draw_sub_texture(rectangle_literal(player->x,
                                           player->y + sin(global_time + 2.0f) * 3.0f,
                                           175.0f, 310.0f),
                         WHITE,
                         global_player_art.texture,
                         global_player_art.forward_torso);
 }
 
 player->collision_bounds = rectangle_literal(player->x,
                                              player->y - 155.0f,
                                              175.0f, 605.0f);
}
