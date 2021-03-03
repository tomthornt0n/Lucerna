#define LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE 64

typedef struct
{
 F32 spawn_x;
 F32 spawn_y;
 char bg_buffer[LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE];
 char fg_buffer[LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE];
 char music_buffer[LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE];
 char entities_buffer[LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE];
 F32 exposure;
 B32 is_memory;
} EditorLevelMetadata;;

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
 static EditorLevelMetadata level_metadata = {0};
 
 //
 // NOTE(tbt): draw background
 //~
 
 aspect =
  (F32)global_current_level.bg->texture.height /
  (F32)global_current_level.bg->texture.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        global_current_level.bg,
                        ENTIRE_TEXTURE);
 
 //
 // NOTE(tbt): main editor window
 //~
 
 ui_do_window(input,
              s8_literal("main editor window"),
              s8_literal("editor"),
              900.0f, 256.0f,
              400.0f)
 {
  if (ui_do_toggle_button(input,
                          s8_literal("toggle entity edit mode"),
                          s8_literal("edit entities"),
                          150.0f,
                          &editing_entities))
  {
   if (!editing_entities)
   {
    ui_delete_node(s8_literal("gen Entity editor"));
    global_editor_selected_entity = false;
   }
  }
  
  if (ui_do_button(input,
                   s8_literal("edit level meta button"),
                   s8_literal("edit level"),
                   150.0f))
  {
   if (!editing_level_meta)
   {
    editing_level_meta = true;
    
    LevelDescriptor *ld = &(global_current_level.level_descriptor->level_descriptor);
    level_metadata.spawn_x = ld->player_spawn_x;
    level_metadata.spawn_y = ld->player_spawn_y;
    memcpy(level_metadata.bg_buffer, ld->bg_path.buffer, ld->bg_path.len);
    memcpy(level_metadata.fg_buffer, ld->fg_path.buffer, ld->fg_path.len);
    memcpy(level_metadata.music_buffer, ld->music_path.buffer, ld->music_path.len);
    memcpy(level_metadata.entities_buffer, ld->entities_path.buffer, ld->entities_path.len);
    level_metadata.exposure = ld->exposure;
    level_metadata.is_memory = ld->is_memory;
   }
  }
 }
 
 //
 // NOTE(tbt): edit level meta
 //~
 
 if (editing_level_meta)
 {
  ui_do_window(input,
               s8_literal("level meta editor window"),
               s8_literal("level meta editor"),
               64.0f, 64.0f,
               1000.0f)
  {
   
   ui_do_line_break();
   ui_do_slider_f(input, s8_literal("level meta editor spawn x slider"), 0.0f, 1920.0f, 1.0f, 523.0f, &level_metadata.spawn_x);
   ui_do_label(s8_literal("level meta editor spawn x slider label"), s8_literal("player spawn x"), -1.0f);
   
   ui_do_line_break();
   ui_do_slider_f(input, s8_literal("level meta editor spawn y slider"), 0.0f, 1080.0f, 1.0f, 523.0f, &level_metadata.spawn_y);
   ui_do_label(s8_literal("level meta editor spawn y slider label"), s8_literal("player spawn y"), -1.0f);
   
   ui_do_line_break();
   ui_do_text_entry(input, s8_literal("level meta editor bg entry"), level_metadata.bg_buffer, NULL, LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE);
   ui_do_label(s8_literal("level meta editor bg entry label"), s8_literal("background path"), -1.0f);
   
   ui_do_line_break();
   ui_do_text_entry(input, s8_literal("level meta editor fg entry"), level_metadata.fg_buffer, NULL, LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE);
   ui_do_label(s8_literal("level meta editor fg entry label"), s8_literal("foreground path"), -1.0f);
   
   ui_do_line_break();
   ui_do_text_entry(input, s8_literal("level meta editor music entry"), level_metadata.music_buffer, NULL, LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE);
   ui_do_label(s8_literal("level meta editor music entry label"), s8_literal("music path"), -1.0f);
   
   ui_do_line_break();
   ui_do_text_entry(input, s8_literal("level meta editor entities entry"), level_metadata.entities_buffer, NULL, LEVEL_META_EDITOR_TEXT_ENTRY_BUFFER_SIZE);
   ui_do_label(s8_literal("level meta editor entities entry label"), s8_literal("entities path"), -1.0f);
   
   ui_do_line_break();
   ui_do_slider_f(input, s8_literal("level meta editor exposure slider"), 0.0f, 3.0f, 0.05f, 150.0f, &level_metadata.exposure);
   ui_do_label(s8_literal("level meta editor exposure label"), s8_literal("exposure"), -1.0f);
   
   ui_do_line_break();
   ui_do_toggle_button(input, s8_literal("level meta editor is memory toggle"), s8_literal("is memory?"), 150.0f, &level_metadata.is_memory);
   
   ui_do_horizontal_rule();
   
   if (ui_do_button(input,
                    s8_literal("save level meta button"),
                    s8_literal("save"),
                    150.0f))
   {
    LevelDescriptor *ld = &(global_current_level.level_descriptor->level_descriptor);
    ld->player_spawn_x = level_metadata.spawn_x;
    ld->player_spawn_y = level_metadata.spawn_y;
    ld->bg_path = s8_from_cstring(&global_level_memory, level_metadata.bg_buffer);
    ld->fg_path = s8_from_cstring(&global_level_memory, level_metadata.fg_buffer);
    ld->music_path = s8_from_cstring(&global_level_memory, level_metadata.music_buffer);
    ld->entities_path = s8_from_cstring(&global_level_memory, level_metadata.entities_buffer);
    ld->exposure = level_metadata.exposure;
    ld->is_memory = level_metadata.is_memory;
    serialise_level_descriptor(global_current_level.level_descriptor);
    
    editing_level_meta = false;
    ui_delete_node(s8_literal("level meta editor window"));
    memset(&level_metadata, 0, sizeof(level_metadata));
    
    unload_level_descriptor(global_current_level.level_descriptor);
    set_current_level(gl, global_current_level.level_descriptor);
   }
   
   if (ui_do_button(input,
                    s8_literal("cancel level meta edit button"),
                    s8_literal("cancel"),
                    15.0f))
   {
    editing_level_meta = false;
    ui_delete_node(s8_literal("level meta editor window"));
    memset(&level_metadata, 0, sizeof(level_metadata));
   }
  }
 }
 
 //
 // NOTE(tbt): edit entities
 //~
 
 if (editing_entities)
 {
  static Entity *active = NULL;
  static Entity *dragging = NULL;
  static Entity *resizing = NULL;
  Entity *selected = global_editor_selected_entity;
  
  ui_do_window(input,
               s8_literal("entity editor window"),
               s8_literal("entity editor"),
               64.0f, 64.0f,
               400.0f)
  {
   if (ui_do_button(input, s8_literal("create entity button"), s8_literal("create entity"), 150.0f))
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
    ui_delete_node(s8_literal("gen Entity editor"));
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
                          e->bounds) &&
       !global_is_mouse_over_ui)
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
    
    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_right])
    {
     resizing = NULL;
    }
   }
  }
  
  if (selected)
  {
   do_entity_editor_ui(input, selected);
  }
  
  global_editor_selected_entity = selected;
 }
 
 //~NOTE(tbt): draw foreground
 aspect =
  (F32)global_current_level.fg->texture.height /
  (F32)global_current_level.fg->texture.width;
 
 world_draw_sub_texture(rectangle_literal(0.0f, 0.0f,
                                          SCREEN_WIDTH_IN_WORLD_UNITS,
                                          SCREEN_WIDTH_IN_WORLD_UNITS * aspect),
                        WHITE,
                        global_current_level.fg,
                        ENTIRE_TEXTURE);
 
 //~NOTE(tbt): camera movement
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