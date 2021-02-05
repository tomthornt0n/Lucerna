/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 04 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
 ENTITY_FLAG_is_player,
 ENTITY_FLAG_can_move,
 ENTITY_FLAG_render_sub_texture,
 ENTITY_FLAG_sinusoidal_bob,
};

typedef struct Entity Entity;
struct Entity
{
 U64 flags;
 Entity *next_sibling;
 Entity *first_child;
 
 F32 x, y; // NOTE(tbt): relative to the parents position
 
 struct
 {
  F32 x_velocity;
  F32 y_velocity;
 } movement;
 
 struct
 {
  Rect rectangle; // NOTE(tbt): position is relative t `.x` and `.y`
  Colour colour;
  Asset *texture;
  SubTexture sub_texture;
 } render_sub_texture;
 
 struct
 {
  F64 x;
  F32 y_coefficient;
  F32 x_coefficient;
 } sinusoidal_bob;
};

Entity *global_entities = NULL;

internal Entity *
push_child_entity(MemoryArena *memory,
                  Entity *parent,
                  Entity e)
{
 Entity *result = arena_allocate(memory, sizeof(*result));
 *result = e;
 result->next_sibling = parent->first_child;
 parent->first_child = result;
 
 return result;
}

internal Entity *
push_entity(MemoryArena *memory,
            Entity e)
{
 Entity *result = arena_allocate(memory, sizeof(*result));
 *result = e;
 result->next_sibling = global_entities;
 global_entities = result;
 
 return result;
}

internal Entity *
push_player(MemoryArena *memory,
            F32 x, F32 y)
{
 Entity _player = {0};
 Entity _child = {0};
 
 _player.flags =
  (1 << ENTITY_FLAG_is_player) |
  (1 << ENTITY_FLAG_can_move);
 
 _player.x = x;
 _player.y = y;
 
 Entity *player = push_entity(memory, _player);
 
 // NOTE(tbt): torso
 _child.flags = (1 << ENTITY_FLAG_render_sub_texture) | (1 << ENTITY_FLAG_sinusoidal_bob);
 
 _child.x = 0.0f;
 _child.y = 0.0f;
 
 _child.render_sub_texture.rectangle = rectangle_literal(0.0f, 0.0f, 175.0f, 310.0f);
 _child.render_sub_texture.colour = colour_literal(1.0f, 1.0f, 1.0f, 1.0f);
 _child.render_sub_texture.texture = asset_from_path(s8_literal("../assets/textures/player/torso.png"));
 _child.render_sub_texture.sub_texture = ENTIRE_TEXTURE;
 
 _child.sinusoidal_bob.x_coefficient = 2.0f;
 _child.sinusoidal_bob.y_coefficient = 3.0f;
 
 push_child_entity(memory, player, _child);
 
 
 // NOTE(tbt): left arm
 _child.flags = (1 << ENTITY_FLAG_render_sub_texture) | (1 << ENTITY_FLAG_sinusoidal_bob);
 
 _child.x = 0.0f;
 _child.y = 25.0f;
 
 _child.render_sub_texture.rectangle = rectangle_literal(0.0f, 0.0f, 43.0f, 212.0f);
 _child.render_sub_texture.colour = colour_literal(1.0f, 1.0f, 1.0f, 1.0f);
 _child.render_sub_texture.texture = asset_from_path(s8_literal("../assets/textures/player/left_arm.png"));
 _child.render_sub_texture.sub_texture = ENTIRE_TEXTURE;
 
 _child.sinusoidal_bob.x_coefficient = 3.0f;
 _child.sinusoidal_bob.y_coefficient = 7.0f;
 
 push_child_entity(memory, player, _child);
 
 
 // NOTE(tbt): right arm
 _child.flags = (1 << ENTITY_FLAG_render_sub_texture) | (1 << ENTITY_FLAG_sinusoidal_bob);
 
 _child.x = 135.0f;
 _child.y = 25.0f;
 
 _child.render_sub_texture.rectangle = rectangle_literal(0.0f, 0.0f, 29.0f, 216.0f);
 _child.render_sub_texture.colour = colour_literal(1.0f, 1.0f, 1.0f, 1.0f);
 _child.render_sub_texture.texture = asset_from_path(s8_literal("../assets/textures/player/right_arm.png"));
 _child.render_sub_texture.sub_texture = ENTIRE_TEXTURE;
 
 _child.sinusoidal_bob.x = 0.1f;
 _child.sinusoidal_bob.x_coefficient = 3.0f;
 _child.sinusoidal_bob.y_coefficient = 7.0f;
 
 push_child_entity(memory, player, _child);
 
 
 // NOTE(tbt): head
 _child.flags = (1 << ENTITY_FLAG_render_sub_texture);
 
 _child.x = 43.0f;
 _child.y = -150.0f;
 
 _child.render_sub_texture.rectangle = rectangle_literal(0.0f, 0.0f, 89.0f, 167.0f);
 _child.render_sub_texture.colour = colour_literal(1.0f, 1.0f, 1.0f, 1.0f);
 _child.render_sub_texture.texture = asset_from_path(s8_literal("../assets/textures/player/head.png"));
 _child.render_sub_texture.sub_texture = ENTIRE_TEXTURE;
 
 push_child_entity(memory, player, _child);
 
 
 // NOTE(tbt): lower body
 _child.flags = (1 << ENTITY_FLAG_render_sub_texture);
 
 _child.x = 22.0f;
 _child.y = 128.0f;
 
 _child.render_sub_texture.rectangle = rectangle_literal(0.0f, 0.0f, 131.0f, 320.0f);
 _child.render_sub_texture.colour = colour_literal(1.0f, 1.0f, 1.0f, 1.0f);
 _child.render_sub_texture.texture = asset_from_path(s8_literal("../assets/textures/player/lower_body.png"));
 _child.render_sub_texture.sub_texture = ENTIRE_TEXTURE;
 
 push_child_entity(memory, player, _child);
 
 
 return player;
}

internal void
_process_entities(OpenGLFunctions *gl,
                  PlatformState *input,
                  F64 frametime_in_s,
                  Entity *entities,
                  F32 parent_x, F32 parent_y)
{
 for (Entity *e = entities;
      NULL != e;
      e = e->next_sibling)
 {
  if (e->flags & (1 << ENTITY_FLAG_sinusoidal_bob))
  {
   e->render_sub_texture.rectangle.y += frametime_in_s * (e->sinusoidal_bob.y_coefficient * sin(e->sinusoidal_bob.x));
   e->sinusoidal_bob.x += frametime_in_s * e->sinusoidal_bob.x_coefficient;
  }
  
  if (e->flags & (1 << ENTITY_FLAG_can_move))
  {
   e->x += e->movement.x_velocity;
   e->y += e->movement.y_velocity;
  }
  
  if (e->flags & (1 << ENTITY_FLAG_is_player))
  {
   F32 player_speed = 128.0f * frametime_in_s;
   
   e->movement.x_velocity =
    input->is_key_pressed[KEY_a] * -player_speed +
    input->is_key_pressed[KEY_d] * player_speed;
   
   e->movement.y_velocity =
    input->is_key_pressed[KEY_w] * -player_speed +
    input->is_key_pressed[KEY_s] * player_speed;
   
   if (e->movement.x_velocity >  0.01f ||
       e->movement.x_velocity < -0.01f ||
       e->movement.y_velocity >  0.01f ||
       e->movement.y_velocity < -0.01f)
   {
    e->flags |= (1 << ENTITY_FLAG_sinusoidal_bob);
   }
   else
   {
    e->flags &= ~(1 << ENTITY_FLAG_sinusoidal_bob);
    e->render_sub_texture.rectangle.y /= 1.3f;
    e->sinusoidal_bob.x = 0.0;
   }
  }
  
  if (e->flags & (1 << ENTITY_FLAG_render_sub_texture))
  {
   world_draw_sub_texture(offset_rectangle(e->render_sub_texture.rectangle,
                                           parent_x + e->x,
                                           parent_y + e->y),
                          e->render_sub_texture.colour,
                          e->render_sub_texture.texture,
                          e->render_sub_texture.sub_texture);
  }
  
  _process_entities(gl,
                    input,
                    frametime_in_s,
                    e->first_child,
                    e->x,
                    e->y);
 }
}

internal void
process_entities(OpenGLFunctions *gl,
                 PlatformState *input,
                 F64 frametime_in_s)
{
 _process_entities(gl, input, frametime_in_s, global_entities, 0.0f, 0.0f);
}
