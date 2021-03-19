internal void
do_level_editor(OpenGLFunctions *gl,
                PlatformState *input,
                F64 frametime_in_s)
{
 F32 aspect;
 stop_audio_source(global_current_level.music);
 
 //
 // NOTE(tbt): editor state
 //~
 
 static B32 editing_entities = false;
 static B32 editing_level_meta = false;
 static B32 opening_level = false;
 static B32 saving_level = false;
 static B32 creating_level = false;
 static U8 open_level_path_buffer[64] = {0};
 static LevelDescriptor level_descriptor = {0};
 
 //
 // NOTE(tbt): draw background
 //~
 
 aspect =
  (F32)global_current_level.bg.height /
  (F32)global_current_level.bg.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        &global_current_level.bg,
                        ENTIRE_TEXTURE);
 
 //
 // NOTE(tbt): keyboard shortcuts
 //~
 
 if (is_key_pressed(input,
                    KEY_s,
                    INPUT_MODIFIER_ctrl))
 {
  saving_level = true;
 }
 
 if (is_key_pressed(input,
                    KEY_o,
                    INPUT_MODIFIER_ctrl))
 {
  opening_level = !opening_level;
 }
 
 if (is_key_pressed(input,
                    KEY_e,
                    INPUT_MODIFIER_ctrl |
                    INPUT_MODIFIER_shift))
 {
  editing_entities = !editing_entities;
 }
 
 //
 // NOTE(tbt): main editor window
 //~
 
 if (!saving_level &&
     !opening_level &&
     !creating_level)
 {
  ui_do_window(input,
               s8_literal("main editor window"),
               s8_literal("editor"),
               900.0f, 256.0f,
               400.0f)
  {
   ui_do_dropdown(input,
                  s8_literal("editor file menu"),
                  s8_literal("file"),
                  150.0f)
   {
    if (ui_do_button(input,
                     s8_literal("editor open level button"),
                     s8_literal("open level"),
                     150.0f))
    {
     opening_level = true;
    }
    
    if (ui_do_button(input,
                     s8_literal("editor new level button"),
                     s8_literal("new level"),
                     150.0f))
    {
     creating_level = true;
    }
    
    if (ui_do_button(input,
                     s8_literal("editor save level button"),
                     s8_literal("save level"),
                     150.0f))
    {
     saving_level = true;
    }
   }
   
   ui_do_dropdown(input,
                  s8_literal("editor edit menu"),
                  s8_literal("edit"),
                  150.0f)
   {
    if (ui_do_button(input,
                     s8_literal("editor enter entity edit mode"),
                     s8_literal("edit entities"),
                     150.0f))
    {
     editing_entities = true;
     global_editor_selected_entity = NULL;
     ui_delete_node(s8_literal("gen Entity editor"));
    }
    
    if (ui_do_button(input,
                     s8_literal("edit level meta button"),
                     s8_literal("edit level metadata"),
                     150.0f))
    {
     if (!editing_level_meta)
     {
      memset(&level_descriptor, 0, sizeof(level_descriptor));
      level_descriptor_from_level(&level_descriptor, &global_current_level);
      editing_level_meta = true;
     }
    }
   }
  }
 }
 
 //
 // NOTE(tbt): save level
 //~
 
 if (saving_level)
 {
  serialise_level(&global_current_level);
  set_current_level(gl, global_current_level.path, false);
  saving_level = false;
 }
 
 //
 // NOTE(tbt): open level ui
 //~
 
 if (opening_level || creating_level)
 {
  ui_do_window(input,
               s8_literal("editor open level window"),
               s8_literal("open level"),
               960, 540, 500.0f)
  {
   B32 enter_typed = (TEXT_ENTRY_STATE_enter == ui_do_text_entry(input,
                                                                 s8_literal("editor open level text entry"),
                                                                 open_level_path_buffer,
                                                                 NULL,
                                                                 sizeof(open_level_path_buffer)));
   
   ui_do_line_break();
   
   B32 open_pressed = ui_do_button(input,
                                   s8_literal("editor open level open button"),
                                   s8_literal("open"),
                                   150.0f);
   
   B32 cancel_pressed = ui_do_button(input,
                                     s8_literal("editor open level cancel button"),
                                     s8_literal("cancel"),
                                     150.0f);
   
   if (open_pressed || enter_typed)
   {
    S8List *path_list = NULL;
    path_list = push_s8_to_list(&global_frame_memory, path_list, s8_literal(".level"));
    path_list = push_s8_to_list(&global_frame_memory, path_list, s8_literal(open_level_path_buffer));
    path_list = push_s8_to_list(&global_frame_memory, path_list, s8_literal("../assets/levels/"));
    
    S8 path = expand_s8_list(&global_frame_memory, path_list);
    
    if (creating_level)
    {
     set_current_level_as_new_level(gl, path);
     editing_level_meta = true;
    }
    else
    {
     set_current_level(gl, path, false);
    }
   }
   
   if (open_pressed || cancel_pressed || enter_typed)
   {
    ui_delete_node(s8_literal("editor open level window"));
    opening_level = false;
    creating_level = false;
    memset(open_level_path_buffer, 0, sizeof(open_level_path_buffer));
   }
  }
 }
 
 //
 // NOTE(tbt): edit level metadata
 //~
 
 if (editing_level_meta)
 {
  // NOTE(tbt): preview player spawn location and scale
  world_fill_rectangle(rectangle_literal(level_descriptor.player_spawn_x - 1.0f,
                                         0.0f,
                                         2.0f,
                                         global_renderer_window_h),
                       colour_literal(1.0f, 0.0f, 0.0f, 0.4f));
  
  F32 y_offset = 0.0f;
  if (level_descriptor.floor_gradient < -0.001 ||
      level_descriptor.floor_gradient > 0.001)
  {
   y_offset = level_descriptor.player_spawn_x * tan(-level_descriptor.floor_gradient);
  }
  
  world_fill_rotated_rectangle(rectangle_literal(0.0f,
                                                 level_descriptor.player_spawn_y + y_offset - 1.0f,
                                                 global_renderer_window_w / cos(level_descriptor.floor_gradient),
                                                 2.0f),
                               level_descriptor.floor_gradient,
                               colour_literal(1.0f, 0.0f, 0.0f, 0.4f));
  
  F32 _player_scale = level_descriptor.player_scale / (global_renderer_window_h - level_descriptor.player_spawn_y);
  world_stroke_rectangle(rectangle_literal(level_descriptor.player_spawn_x + PLAYER_COLLISION_X * _player_scale,
                                           level_descriptor.player_spawn_y + PLAYER_COLLISION_Y * _player_scale,
                                           PLAYER_COLLISION_W * _player_scale,
                                           PLAYER_COLLISION_H * _player_scale),
                         colour_literal(1.0f, 0.0f, 0.0f, 0.4f),
                         2.0f);
  
  S8List *metadata_editor_window_title_list = NULL;
  metadata_editor_window_title_list = push_s8_to_list(&global_frame_memory, metadata_editor_window_title_list, global_current_level.path);
  metadata_editor_window_title_list = push_s8_to_list(&global_frame_memory, metadata_editor_window_title_list, s8_literal("edit level metadata - "));
  
  ui_do_window(input,
               s8_literal("level metadata editor"),
               expand_s8_list(&global_frame_memory, metadata_editor_window_title_list),
               32.0f, 32.0f, 1200.0f)
  {
   do_level_descriptor_editor_ui(input, &level_descriptor);
   ui_do_horizontal_rule();
   
   if (ui_do_button(input,
                    s8_literal("level metadata editor save button"),
                    s8_literal("save"),
                    15.0f))
   {
    ui_delete_node(s8_literal("level metadata editor"));
    _current_level_from_level_descriptor(gl, &level_descriptor);
    saving_level = true;
    editing_level_meta = false;
   }
   
   if (ui_do_button(input,
                    s8_literal("level metadata editor cancel button"),
                    s8_literal("cancel"),
                    15.0f))
   {
    ui_delete_node(s8_literal("level metadata editor"));
    memset(&level_descriptor, 0, sizeof(level_descriptor));
    editing_level_meta = false;
   }
  }
 }
 
 //
 // NOTE(tbt): edit entities
 //~
 
 if (editing_entities &&
     !editing_level_meta)
 {
  static Entity *active = NULL;
  static Entity *dragging = NULL;
  static Entity *resizing = NULL;
  
  ui_do_window(input,
               s8_literal("entity editor window"),
               s8_literal("entity editor"),
               64.0f, 64.0f,
               400.0f)
  {
   if (ui_do_button(input,
                    s8_literal("create entity button"),
                    s8_literal("create entity"),
                    150.0f))
   {
    Entity *e = allocate_and_push_entity(&global_current_level.entities);
    
    e->bounds.x = 16.0f;
    e->bounds.y = 16.0f;
    e->bounds.w = 16.0f;
    e->bounds.h = 16.0f;
    
    global_editor_selected_entity = e;
   }
   
   if (ui_do_button(input,
                    s8_literal("editor exit entity edit mode"),
                    s8_literal("exit"),
                    150.0f))
   {
    editing_entities = false;
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
       !input->is_mouse_button_down[MOUSE_BUTTON_left])
   {
    global_editor_selected_entity = e;
   }
   
   if (e == global_editor_selected_entity)
   {
    fill_rectangle(e->bounds, colour_literal(0.0f, 0.0f, 1.0f, 0.8f), UI_SORT_DEPTH, global_projection_matrix);
    
    if (input->is_key_down[KEY_delete])
    {
     e->flags |= 1 << ENTITY_FLAG_marked_for_removal;
     global_editor_selected_entity = NULL;
    }
   }
   stroke_rectangle(e->bounds, colour_literal(0.0f, 0.0f, 1.0f, 0.8f), 2.0f, UI_SORT_DEPTH, global_projection_matrix);
   
   if (is_point_in_region(MOUSE_WORLD_X,
                          MOUSE_WORLD_Y,
                          e->bounds) &&
       !global_is_mouse_over_ui)
   {
    fill_rectangle(e->bounds, colour_literal(0.0f, 0.0f, 1.0f, 0.15f), UI_SORT_DEPTH, global_projection_matrix);
    if (input->is_mouse_button_down[MOUSE_BUTTON_left])
    {
     if (global_editor_selected_entity == e)
     {
      dragging = e;
     }
     else if (!dragging)
     {
      active = e;
     }
    }
    else if (input->is_mouse_button_down[MOUSE_BUTTON_right] &&
             e == global_editor_selected_entity)
    {
     resizing = e;
    }
   }
  }
  
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   active = NULL;
   dragging = NULL;
  }
  
  if (!global_is_mouse_over_ui)
  {
   if (dragging)
   {
    dragging->bounds.x = MOUSE_WORLD_X - dragging->bounds.w / 2;
    dragging->bounds.y = MOUSE_WORLD_Y - dragging->bounds.h / 2;
   }
   
   if (resizing)
   {
    resizing->bounds.w = MOUSE_WORLD_X - resizing->bounds.x;
    resizing->bounds.h = MOUSE_WORLD_Y - resizing->bounds.y;
    
    if (!input->is_mouse_button_down[MOUSE_BUTTON_right])
    {
     resizing = NULL;
    }
   }
  }
  
  if (global_editor_selected_entity)
  {
   ui_do_window(input,
                s8_literal("entity editor"),
                s8_literal("entity editor"),
                32.0f, 32.0f, 800.0f)
   {
    do_entity_editor_ui(input, global_editor_selected_entity);
   }
  }
 }
 
 //~NOTE(tbt): draw foreground
 aspect =
  (F32)global_current_level.fg.height /
  (F32)global_current_level.fg.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        &global_current_level.fg,
                        ENTIRE_TEXTURE);
 
 //~NOTE(tbt): camera movement
 {
  F32 speed = 256.0f * frametime_in_s;
  
  set_camera_position(global_camera_x +
                      input->is_key_down[KEY_right] * speed +
                      input->is_key_down[KEY_left] * -speed,
                      global_camera_y +
                      input->is_key_down[KEY_down] * speed +
                      input->is_key_down[KEY_up] * -speed);
 }
 
}