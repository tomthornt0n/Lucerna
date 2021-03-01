internal void
do_level_editor(PlatformState *input,
                F64 frametime_in_s)
{
 F32 aspect;
 stop_audio_source(global_current_level.music);
 
 // NOTE(tbt): draw background
 aspect =
  (F32)global_current_level.bg->texture.height /
  (F32)global_current_level.bg->texture.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        global_current_level.bg,
                        ENTIRE_TEXTURE);
 
 // NOTE(tbt): edit entities
 {
  static Entity *active = NULL;
  static Entity *dragging = NULL;
  static Entity *resizing = NULL;
  Entity *selected = global_editor_selected_entity;
  
  do_window(input,
            s8_literal("editor main window"),
            s8_literal("editor"),
            900.0f, 256.0f,
            400.0f)
  {
   if (do_button(input, s8_literal("create entity button"), s8_literal("create entity"), 150.0f))
   {
    Entity *e = allocate_and_push_entity(&global_current_level.entities);
    
    e->bounds.x = 16.0f;
    e->bounds.y = 16.0f;
    e->bounds.w = 16.0f;
    e->bounds.h = 16.0f;
    
    selected = e;
   }
  }
  
  Entity *prev = NULL;
  for (Entity *e = global_current_level.entities;
       NULL != e;
       prev = e, e = e->next)
  {
   if (e->flags & 1 << ENTITY_FLAG_marked_for_removal)
   {
    if (prev)
    {
     prev->next = e->next;
    }
    else
    {
     global_current_level.entities = e->next;
    }
    
    e->next_free = _global_entity_free_list;
    _global_entity_free_list = e;
    continue;
   }
   
   if (active == e &&
       !input->is_mouse_button_pressed[MOUSE_BUTTON_left])
   {
    selected = e;
    delete_ui_node(s8_literal("gen Entity editor"));
   }
   
   if (selected == e)
   {
    fill_rectangle(e->bounds, colour_literal(0.0f, 0.0f, 1.0f, 0.8f), UI_SORT_DEPTH, global_projection_matrix);
    if (input->is_key_pressed[KEY_del])
    {
     e->flags |= 1 << ENTITY_FLAG_marked_for_removal;
     fprintf(stderr, "del");
    }
   }
   stroke_rectangle(e->bounds, colour_literal(0.0f, 0.0f, 1.0f, 0.8f), 2.0f, UI_SORT_DEPTH, global_projection_matrix);
   
   if (is_point_in_region(MOUSE_WORLD_X,
                          MOUSE_WORLD_Y,
                          e->bounds))
   {
    fill_rectangle(e->bounds, colour_literal(0.0f, 0.0f, 1.0f, 0.15f), UI_SORT_DEPTH, global_projection_matrix);
    if (input->is_mouse_button_pressed[MOUSE_BUTTON_left])
    {
     if (selected == e)
     {
      dragging = e;
     }
     else if (!dragging)
     {
      active = e;
     }
    }
    else if (input->is_mouse_button_pressed[MOUSE_BUTTON_right] &&
             e == selected)
    {
     resizing = e;
    }
   }
  }
  
  if (!input->is_mouse_button_pressed[MOUSE_BUTTON_left])
  {
   active = NULL;
   dragging = NULL;
  }
  
  if (dragging)
  {
   dragging->bounds.x = MOUSE_WORLD_X - dragging->bounds.w / 2;
   dragging->bounds.y = MOUSE_WORLD_Y - dragging->bounds.h / 2;
  }
  
  if (resizing)
  {
   resizing->bounds.w = MOUSE_WORLD_X - resizing->bounds.x;
   resizing->bounds.h = MOUSE_WORLD_Y - resizing->bounds.y;
   
   if (!input->is_mouse_button_pressed[MOUSE_BUTTON_right])
   {
    resizing = NULL;
   }
  }
  
  if (selected)
  {
   do_entity_editor_ui(input, selected);
  }
  
  global_editor_selected_entity = selected;
 }
 
 // NOTE(tbt): draw foreground
 aspect =
  (F32)global_current_level.fg->texture.height /
  (F32)global_current_level.fg->texture.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        global_current_level.fg,
                        ENTIRE_TEXTURE);
 
 // NOTE(tbt): camera movement
 {
  F32 speed = 256.0f * frametime_in_s;
  
  set_camera_position(global_camera_x +
                      input->is_key_pressed[KEY_right] * speed +
                      input->is_key_pressed[KEY_left] * -speed,
                      global_camera_y +
                      input->is_key_pressed[KEY_down] * speed +
                      input->is_key_pressed[KEY_up] * -speed);
 }
 
}