//
// NOTE(tbt): terrible, but only for dev tools so just about acceptable
//

#define PADDING 8.0f
#define STROKE_WIDTH 2.0f

#define SCROLL_SPEED 25.0f

#define SLIDER_THICKNESS 18.0f
#define SLIDER_THUMB_SIZE 12.0f

#define CURSOR_THICKNESS 2.0f

#define BG_COL_1 colour_literal(0.05f, 0.05f, 0.05f, 0.9f)
#define BG_COL_2 colour_literal(0.0f, 0.0f, 0.0f, 0.95f)
#define FG_COL_1 colour_literal(1.0f, 1.0f, 1.0f, 1.0f)

#define BLUR_STRENGTH 1

#define ANIMATION_SPEED 0.05f


internal Rect
_ui_measure_text(Font *font,
                 F32 x, F32 y,
                 U32 wrap_width,
                 S8 string)
{
 Rect result;
 stbtt_aligned_quad q;
 F32 line_start = x;
 
 F32 min_x = x, min_y = y, max_x = 0.0f, max_y = 0.0f;
 
 for (I32 i = 0;
      i < string.len;
      ++i)
 {
  if (string.buffer[i] >= FONT_BAKE_BEGIN &&
      string.buffer[i] < FONT_BAKE_END)
  {
   F32 char_width, char_height;
   
   stbtt_GetBakedQuad(font->char_data,
                      font->texture.width,
                      font->texture.height,
                      string.buffer[i] - 32,
                      &x,
                      &y,
                      &q);
   
   if (q.x0 < min_x)
   {
    min_x = q.x0;
   }
   if (q.y0 < min_y)
   {
    min_y = q.y0;
   }
   if (q.x1 > max_x)
   {
    max_x = q.x1;
   }
   if (q.y1 > max_y)
   {
    max_y = q.y1;
   }
   
   // NOTE(tbt): need to check against the current position as well as
   //            some characters don't have any width, just advance the
   //            current position
   if (x > max_x)
   {
    max_x = x;
   }
   if (y > max_y)
   {
    max_y = y;
   }
   
   
   if (wrap_width && isspace(string.buffer[i]))
   {
    if (x + (q.x1 - q.x0) * 2 >
        line_start + wrap_width)
    {
     y += font->size;
     x = line_start;
    }
   }
  }
  else if (string.buffer[i] == '\n')
  {
   y += font->size;
   x = line_start;
  }
 }
 
 result = rectangle_literal(min_x - PADDING, min_y - PADDING, max_x - min_x + PADDING * 2, max_y - min_y + PADDING * 2);
 return result;
}


typedef enum
{
 UI_NODE_KIND_none,
 
 UI_NODE_KIND_window,
 UI_NODE_KIND_button,
 UI_NODE_KIND_slider,
 UI_NODE_KIND_label,
 UI_NODE_KIND_line_break,
 UI_NODE_KIND_text_entry,
} UINodeKind;

typedef enum
{
 TEXT_ENTRY_STATE_none,
 TEXT_ENTRY_STATE_clicked_out,
 TEXT_ENTRY_STATE_enter,
} TextEntryState;

#define UI_NODE_TEMP_BUFFER_SIZE 8

typedef struct UINode UINode;
struct UINode
{
 B32 exists;                               // NOTE(tbt): mark when a slot in the hash map is in use
 UINodeKind kind;                          // NOTE(tbt): the subtype of widget
 
 // NOTE(tbt): form a tree
 UINode *parent;
 UINode *first_child;
 UINode *last_child;
 UINode *next_sibling;
 
 UINode *next_insertion_point;             // NOTE(tbt): form a stack of where to insert new nodes
 S8 key;                                   // NOTE(tbt): the string used to uniquely identify the widget
 UINode *next_hash;                        // NOTE(tbt): chaining for hash collisions
 UINode *next_under_mouse;                 // NOTE(tbt): linked list of all nodes under the mouse - active widget is the widget in this list with the highest sort depth
 
 Rect bounds;                              // NOTE(tbt): the total area on the screen taken up by the widget
 Rect interactable;                        // NOTE(tbt): the area that can be interacted with
 Rect bg;                                  // NOTE(tbt): an area that is rendered but not able to be interacted with
 
 F32 drag_x, drag_y;                       // NOTE(tbt): mouse coordinates for sliders and windows
 F32 wrap_width;                           // NOTE(tbt): the maximum width for a widget before it begins to wrap
 F32 texture_width;                        // NOTE(tbt): for sprite pickers
 F32 texture_height;                       // NOTE(tbt): for sprite pickers
 I32 sort;                                 // NOTE(tbt): sort depth for rendering
 B32 toggled;                              // NOTE(tbt): true when a button is enabled
 B32 dragging;                             // NOTE(tbt): true while a window or slider is being dragged
 S8 label;                                 // NOTE(tbt): text displayed on the widget
 F64 slider_value;                         // NOTE(tbt): needs no explanation
 B32 hidden;                               // NOTE(tbt): true if the node exists in the UI tree but should be ignored
 F32 min, max;                             // NOTE(tbt): minimum and maximum value for sliders
 F32 animation_transition;                 // NOTE(tbt): between 0.0 and 1.0 - used to fade between active, hovered and default colours
 U8 temp_buffer[UI_NODE_TEMP_BUFFER_SIZE]; // NOTE(tbt): put random data that needs to be retained with the widget here, e.g. used as a character buffer for the text entries next to sliders
 TextEntryState text_entry_state;          // NOTE(tbt): when a text box is clicked out of, or enter is typed
 B32 draw_horizontal_rule;                 // NOTE(tbt): whether a line break should draw a horizontal line
};

#define UI_HASH_TABLE_SIZE (1024)

#define ui_hash(string) hash_s8((string), UI_HASH_TABLE_SIZE);

internal UINode global_ui_state_dict[UI_HASH_TABLE_SIZE] = {{0}};

internal UINode *
_ui_new_node_from_string(MemoryArena *memory,
                         S8 string)
{
 U64 index = ui_hash(string);
 UINode *result = global_ui_state_dict + index;
 
 if (!result->exists ||
     s8_match(result->key, string))
 {
  memset(result, 0, sizeof(*result));
  goto new_node;
 }
 else
 {
  while (result->next_hash)
  {
   result = result->next_hash;
   
   if (s8_match(result->key, string))
   {
    memset(result, 0, sizeof(*result));
    goto new_node;
   }
  }
 }
 
 result->next_hash = arena_allocate(&global_static_memory,
                                    sizeof(*result->next_hash));
 result = result->next_hash;
 
 new_node:
 result->exists = true;
 result->key = copy_s8(&global_static_memory, string);
 
 return result;
}

internal UINode *
_ui_node_from_string(S8 string)
{
 U64 index = ui_hash(string);
 UINode *result = global_ui_state_dict + index;
 
 if (!result->exists) { return NULL; }
 
 while (result &&
        !s8_match(result->key, string))
 {
  result = result->next_hash;
 }
 
 return result;
}

internal UINode *global_hot_widget, *global_active_widget;
internal UINode *global_widgets_under_mouse;
internal B32 global_is_mouse_over_ui;

internal UINode global_ui_root = {0};
internal UINode *global_ui_insertion_point = &global_ui_root;

internal void
_ui_push_insertion_point(UINode *node)
{
 node->next_insertion_point = global_ui_insertion_point;
 global_ui_insertion_point = node;
}

internal void
_ui_pop_insertion_point(void)
{
 global_ui_insertion_point = global_ui_insertion_point->next_insertion_point;
 if (!global_ui_insertion_point)
 {
  global_ui_insertion_point = &global_ui_root;
 }
}

internal void
_ui_insert_node(UINode *node)
{
 if (global_ui_insertion_point->last_child)
 {
  global_ui_insertion_point->last_child->next_sibling = node;
  global_ui_insertion_point->last_child = node;
 }
 else
 {
  global_ui_insertion_point->first_child = node;
  global_ui_insertion_point->last_child = node;
 }
 node->parent = global_ui_insertion_point;
 node->first_child = NULL;
 node->last_child = NULL;
 node->next_sibling = NULL;
 node->next_under_mouse = NULL;
}

internal void
_delete_ui_node(UINode *node)
{
 if (node)
 {
  memset(node, 0, sizeof(*node));
  
  for (UINode *child = node->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   _delete_ui_node(child);
  }
 }
}

internal void
ui_delete_node(S8 name)
{
 UINode *node = _ui_node_from_string(name);
 _delete_ui_node(node);
}

#define ui_do_window(_input, _name, _title, _init_x, _init_y, _max_w) for (I32 i = (_ui_begin_window((_input), (_name), (_title), (_init_x), (_init_y), (_max_w)), 0); !i; (++i, _ui_pop_insertion_point()))

internal void
_ui_begin_window(PlatformState *input,
                 S8 name,
                 S8 title,
                 F32 initial_x, F32 initial_y,
                 F32 max_w)
{
 UINode *node;
 static UINode *last_active_window = NULL;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
  node->drag_x = initial_x;
  node->drag_y = initial_y;
 }
 
 node->label = copy_s8(&global_frame_memory, title);
 
 _ui_insert_node(node);
 _ui_push_insertion_point(node);
 
 node->wrap_width = max_w;
 if (last_active_window == node)
 {
  // NOTE(tbt): 10 is chosen arbitrarily. will cause sorting errors if
  //            the tree is more than 10 deep
  // TODO(tbt): deal with this better
  node->sort = UI_SORT_DEPTH + 10;
 }
 else
 {
  node->sort = UI_SORT_DEPTH;
 }
 
 node->kind = UI_NODE_KIND_window;
 
 if (is_point_in_region(input->mouse_x,
                        input->mouse_y,
                        node->interactable) &&
     !global_hot_widget)
 {
  node->next_under_mouse = global_widgets_under_mouse;
  global_widgets_under_mouse = node;
  
 }
 
 if (is_point_in_region(input->mouse_x,
                        input->mouse_y,
                        node->bounds))
 {
  global_is_mouse_over_ui = true;
 }
 
 if (global_hot_widget == node &&
     input->is_mouse_button_down[MOUSE_BUTTON_left] &&
     !global_active_widget)
 {
  node->dragging = true;
  global_active_widget = node;
  last_active_window = node;
 }
 
 if (node->dragging)
 {
  global_is_mouse_over_ui = true;
  
  node->drag_x = clamp_f(floor(input->mouse_x - node->interactable.w * 0.5f), 0.0f, global_renderer_window_w - PADDING);
  node->drag_y = clamp_f(floor(input->mouse_y - node->interactable.h * 0.5f), 0.0f, global_renderer_window_h - PADDING);
  
  if (!global_hot_widget)
  {
   global_active_widget = node;
  }
  
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   node->dragging = false;
  }
 }
}

internal B32
ui_do_bit_toggle_button(PlatformState *input,
                        S8 name,
                        S8 label,
                        U64 *mask,
                        I32 bit,
                        F32 width)
{
 UINode *node;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
 }
 node->label = copy_s8(&global_frame_memory, label);
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_button;
 node->wrap_width = width;
 
 if (mask != NULL) { node->toggled = *mask & (1 << bit); };
 
 if (!node->hidden)
 {
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left] &&
      global_active_widget == node)
  {
   node->toggled = !node->toggled;
   cm_play(global_click_sound);
   if (mask != NULL)
   {
    if (node->toggled)
    {
     *mask |= (1 << bit);
    }
    else
    {
     *mask &= ~(1 << bit);
    }
   }
  }
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->interactable) &&
      !global_active_widget)
  {
   node->next_under_mouse = global_widgets_under_mouse;
   global_widgets_under_mouse = node;
  }
  if (global_hot_widget == node &&
      input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   global_active_widget = node;
  }
 }
 
 return node->toggled;
}

// NOTE(tbt): returns true when clicked, just like a normal button
internal B32
ui_do_toggle_button(PlatformState *input,
                    S8 name,
                    S8 label,
                    F32 width,
                    B32 *toggle)
{
 UINode *node;
 B32 clicked = false;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
 }
 node->label = copy_s8(&global_frame_memory, label);
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_button;
 node->wrap_width = width;
 
 node->toggled = *toggle;
 
 if (!node->hidden)
 {
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left] &&
      global_active_widget == node)
  {
   *toggle = !(*toggle);
   cm_play(global_click_sound);
   clicked = true;
  }
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->interactable) &&
      !global_active_widget)
  {
   node->next_under_mouse = global_widgets_under_mouse;
   global_widgets_under_mouse = node;
  }
  if (global_hot_widget == node &&
      input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   global_active_widget = node;
  }
 }
 
 return clicked;
}

internal B32
ui_do_toggle_button_temp(PlatformState *input,
                         S8 name,
                         S8 label,
                         F32 width)
{
 B32 result;
 ui_do_toggle_button(input, name, label, width, &result);
 return result;
}

internal B32
ui_do_button(PlatformState *input,
             S8 name,
             S8 label,
             F32 width)
{
 UINode *node;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
 }
 node->label = copy_s8(&global_frame_memory, label);
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_button;
 node->wrap_width = width;
 
 if (!node->hidden)
 {
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left] &&
      global_active_widget == node)
  {
   node->toggled = true;
   cm_play(global_click_sound);
  }
  else
  {
   node->toggled = false;
  }
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->interactable) &&
      !global_active_widget)
  {
   node->next_under_mouse = global_widgets_under_mouse;
   global_widgets_under_mouse = node;
  }
  
  if (global_hot_widget == node &&
      input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   global_active_widget = node;
  }
 }
 
 return node->toggled;
}

#define ui_do_dropdown(_input, _name, _label, _width) for (I32 i = (_ui_begin_dropdown((_input), (_name), (_label), (_width)), 0); !i; (++i, _ui_pop_insertion_point()))

internal void
_ui_begin_dropdown(PlatformState *input,
                   S8 name,
                   S8 label,
                   F32 width)
{  
 UINode *node;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
 }
 
 _ui_insert_node(node);
 _ui_push_insertion_point(node);
 
 node->kind = UI_NODE_KIND_button;
 node->wrap_width = width;
 node->label = copy_s8(&global_frame_memory, label);
 
 if (!node->hidden)
 {
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left] &&
      global_active_widget == node)
  {
   node->toggled = !node->toggled;
   cm_play(global_click_sound);
  }
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->bg))
  {
   global_is_mouse_over_ui = true;
  }
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->bounds) &&
      !global_active_widget)
  {
   node->next_under_mouse = global_widgets_under_mouse;
   global_widgets_under_mouse = node;
  }
  if (global_hot_widget == node &&
      input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   global_active_widget = node;
  }
 }
}

internal UINode *
_ui_do_text_entry(PlatformState *input,
                  S8 name,
                  U8 *buffer,
                  U64 *len,
                  U64 capacity)
{
 UINode *node;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
 }
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_text_entry;
 node->wrap_width = capacity * global_ui_font->estimate_char_width + PADDING * 2;
 
 node->text_entry_state = TEXT_ENTRY_STATE_none;
 
 if (!node->hidden)
 {
  if (global_active_widget == node)
  {
   if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
   {
    node->toggled = !node->toggled;
    if (!node->toggled)
    {
     node->text_entry_state = TEXT_ENTRY_STATE_clicked_out;
    }
    cm_play(global_click_sound);
   }
  }
  else if (global_active_widget && node->toggled)
  {
   node->text_entry_state = TEXT_ENTRY_STATE_clicked_out;
   node->toggled = false;
  }
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->interactable) &&
      !global_active_widget)
  {
   node->next_under_mouse = global_widgets_under_mouse;
   global_widgets_under_mouse = node;
  }
  
  U64 index = strlen(buffer);
  
  if (node->toggled)
  {
   for (PlatformEvent *event = input->events;
        NULL != event;
        event = event->next)
   {
    if (event->kind == PLATFORM_EVENT_key_typed)
    {
     if (index < (capacity - 1))
     {
      buffer[index] = event->character;
      index += 1;
      buffer[index] = 0;
     }
    }
    else if (event->kind == PLATFORM_EVENT_key_press &&
             event->key == KEY_backspace &&
             index > 0)
    {
     index -= 1;
     buffer[index] = 0;
    }
    else if (event->key == KEY_enter)
    {
     node->text_entry_state = TEXT_ENTRY_STATE_enter;
     node->toggled = false;
    }
   }
  }
  
  node->label.buffer = buffer;
  node->label.len = index;
  
  if (len)
  {
   *len = index;
  }
  
  if (global_hot_widget == node &&
      input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   global_active_widget = node;
  }
 }
 
 return node;
}

internal TextEntryState
ui_do_text_entry(PlatformState *input,
                 S8 name,
                 U8 *buffer,
                 U64 *len,
                 U64 capacity)
{
 return _ui_do_text_entry(input,
                          name,
                          buffer,
                          len,
                          capacity)->text_entry_state;
}


internal void
ui_do_slider_lf(PlatformState *input,
                S8 name,
                F32 min, F32 max,
                F32 snap,
                F32 width,
                F64 *value)
{
 UINode *node;
 
 *value = clamp_f(*value, min, max);
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
  snprintf(node->temp_buffer, UI_NODE_TEMP_BUFFER_SIZE, "%f", *value);
 }
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_slider;
 node->min = min;
 node->max = max;
 node->wrap_width = width;
 
 if (!node->hidden)
 {
  S8 text_name;
  text_name.buffer = arena_allocate(&global_frame_memory, name.len + 1);
  text_name.len = name.len + 1;
  memcpy(text_name.buffer, name.buffer, name.len);
  text_name.buffer[name.len] = '~';
  UINode *text = _ui_do_text_entry(input, text_name, node->temp_buffer, NULL, 8);
  
  if (is_point_in_region(input->mouse_x,
                         input->mouse_y,
                         node->interactable) &&
      !global_active_widget)
  {
   node->next_under_mouse = global_widgets_under_mouse;
   global_widgets_under_mouse = node;
  }
  if (global_hot_widget == node &&
      input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   global_active_widget = node;
   if (!node->dragging)
   {
    node->dragging = true;
    cm_play(global_click_sound);
   }
  }
  
  if (node->dragging)
  {
   global_is_mouse_over_ui = true;
   
   F32 thumb_x = clamp_f(input->mouse_x - node->bounds.x,
                         0.0f, width - SLIDER_THUMB_SIZE);
   
   *value = thumb_x / (width - SLIDER_THUMB_SIZE) * (node->max - node->min) + node->min;
   
   if (snap > 0.0f)
   {
    *value = floor(*value / snap) * snap;
   }
   
   snprintf(node->temp_buffer, 8, "%f", *value);
   
   if (!global_hot_widget)
   {
    global_active_widget = node;
   }
   
   if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
   {
    node->dragging = false;
    cm_play(global_click_sound);
   }
  }
  else if (text->text_entry_state)
  {
   *value = atof(node->temp_buffer);
  }
  else if (!text->toggled)
  {
   snprintf(node->temp_buffer, 8, "%f", *value);
  }
  
  node->slider_value = *value;
 }
}

internal void
ui_do_slider_f(PlatformState *input,
               S8 name,
               F32 min, F32 max,
               F32 snap,
               F32 width,
               F32 *value)
{
 F64 x = (F64) *value;
 ui_do_slider_lf(input, name, min, max, snap, width, &x);
 *value = (F32)x;
}

internal void
ui_do_slider_l(PlatformState *input,
               S8 name,
               F32 min, F32 max,
               F32 snap,
               F32 width,
               I64 *value)
{
 F64 x = (F64) *value;
 ui_do_slider_lf(input, name, min, max, snap, width, &x);
 *value = (I64)x;
}

internal void
ui_do_slider_i(PlatformState *input,
               S8 name,
               F32 min, F32 max,
               F32 snap,
               F32 width,
               I32 *value)
{
 F64 x = (F64) *value;
 ui_do_slider_lf(input, name, min, max, snap, width, &x);
 *value = (I32)x;
}

internal void
ui_do_label(S8 name,
            S8 label,
            F32 width)
{
 UINode *node;
 
 if (!(node = _ui_node_from_string(name)))
 {
  node = _ui_new_node_from_string(&global_static_memory, name);
 }
 
 node->label = copy_s8(&global_frame_memory, label);
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_label;
 node->wrap_width = width;
}

internal void
_ui_do_line_break(B32 rule)
{
 UINode *node;
 
 node = arena_allocate(&global_frame_memory,
                       sizeof(*node));
 node->draw_horizontal_rule = rule;
 
 _ui_insert_node(node);
 
 node->kind = UI_NODE_KIND_line_break;
}

internal void
ui_do_line_break(void)
{
 _ui_do_line_break(false);
}

internal void
ui_do_horizontal_rule(void)
{
 _ui_do_line_break(true);
}

internal void
_ui_layout_and_render_node(PlatformState *input,
                           UINode *node,
                           F32 x, F32 y)
{
 switch (node->kind)
 {
  case UI_NODE_KIND_window:
  {
   Rect title_bounds = _ui_measure_text(global_ui_font,
                                        node->drag_x,
                                        node->drag_y,
                                        node->wrap_width,
                                        node->label);
   
   node->bounds = rectangle_literal(title_bounds.x,
                                    title_bounds.y,
                                    title_bounds.w,
                                    0.0f);
   
   node->bg = rectangle_literal(title_bounds.x,
                                title_bounds.y + title_bounds.h,
                                title_bounds.w,
                                0.0f);
   
   node->interactable = title_bounds;
   
   UINode *child;
   // NOTE(tbt): keep track of where to place the next child
   F32 current_x = node->bg.x + PADDING;
   F32 current_y = node->bg.y + PADDING;
   F32 tallest = 0.0f; // NOTE(tbt): the height of the tallest widget in the current row
   
   U32 horizontal_rule_count = 0;
   F32 horizontal_rules[256];
   
   for (child = node->first_child;
        NULL != child;
        child = child->next_sibling)
   {
    if (child->kind == UI_NODE_KIND_line_break)
    {
     current_x = node->bg.x + PADDING;
     current_y += tallest + PADDING;
     tallest = 0.0f;
     
     if (child->draw_horizontal_rule)
     {
      horizontal_rules[horizontal_rule_count++] = current_y + PADDING;
      current_y += 3 * PADDING;
     }
    }
    else
    {
     if (current_x + child->bounds.w + PADDING >
         node->drag_x + node->wrap_width)
     {
      current_x = node->bg.x + PADDING;
      current_y += tallest + PADDING;
      tallest = 0.0f;
     }
     
     _ui_layout_and_render_node(input,
                                child,
                                current_x,
                                current_y);
     
     if (child->kind != UI_NODE_KIND_window)
     {
      child->sort = node->sort + 1;
      
      current_x += child->bounds.w + PADDING;
      
      if (child->bounds.y + child->bounds.h - current_y >
          tallest)
      {
       tallest = child->bounds.y +
        child->bounds.h -
        current_y;
      }
     }
     
     if (current_x - node->bounds.x > node->bounds.w)
     {
      node->interactable.w = current_x - node->interactable.x;
      
      node->bounds.w = current_x - node->bounds.x;
      
      node->bg.w = current_x - node->bg.x;
     }
     
     if ((current_y + tallest) - node->bg.y > node->bg.h)
     {
      node->bounds.h = (current_y + tallest) -
       node->bounds.y +
       2 * PADDING;
      
      node->bg.h = (current_y + tallest) -
       node->bg.y +
       2 * PADDING;
     }
    }
   }
   
   blur_screen_region(node->bounds, BLUR_STRENGTH, node->sort);
   
   fill_rectangle(node->bg,
                  BG_COL_1,
                  node->sort,
                  global_ui_projection_matrix);
   
   fill_rectangle(node->interactable,
                  BG_COL_2,
                  node->sort,
                  global_ui_projection_matrix);
   
   draw_text(global_ui_font,
             node->drag_x,
             node->drag_y,
             node->wrap_width,
             FG_COL_1,
             node->label,
             node->sort,
             global_ui_projection_matrix);
   
   
   for (U32 i = 0;
        i < horizontal_rule_count;
        ++i)
   {
    fill_rectangle(rectangle_literal(node->bounds.x + PADDING, horizontal_rules[i],
                                     node->bounds.w - PADDING * 2,
                                     STROKE_WIDTH),
                   FG_COL_1,
                   node->sort,
                   global_ui_projection_matrix);
   }
   
   break;
  }
  case UI_NODE_KIND_button:
  {
   node->bounds = node->bg = node->interactable = _ui_measure_text(global_ui_font,
                                                                   x + PADDING, y + global_ui_font->size,
                                                                   node->wrap_width,
                                                                   node->label);
   
   if (node->toggled)
   {
    if (node->animation_transition < 1.0f) { node->animation_transition += 0.05f; }
    
    fill_rectangle(node->bounds,
                   colour_lerp(FG_COL_1,
                               colour_literal(0.0f, 0.0f, 0.0f, 0.0f),
                               node->animation_transition),
                   node->sort,
                   global_ui_projection_matrix);
    
    draw_text(global_ui_font,
              x + PADDING, y + global_ui_font->size,
              node->wrap_width,
              BG_COL_1,
              node->label,
              node->sort,
              global_ui_projection_matrix);
    
   }
   else
   {
    stroke_rectangle(node->bounds,
                     FG_COL_1,
                     STROKE_WIDTH,
                     node->sort,
                     global_ui_projection_matrix);
    
    if (node == global_hot_widget)
    {
     if (node->animation_transition < 1.0f) { node->animation_transition += 0.05f; }
    }
    else
    {
     if (node->animation_transition > 0.0f) { node->animation_transition -= 0.05f; }
    }
    
    fill_rectangle(node->bounds,
                   colour_lerp(FG_COL_1,
                               colour_literal(0.0f, 0.0f, 0.0f, 0.0f),
                               node->animation_transition),
                   node->sort,
                   global_ui_projection_matrix);
    
    draw_text(global_ui_font,
              x + PADDING, y + global_ui_font->size,
              node->wrap_width,
              colour_lerp(BG_COL_1,
                          FG_COL_1,
                          node->animation_transition),
              node->label,
              node->sort,
              global_ui_projection_matrix);
   }
   
   if (node->first_child)
   {
    node->bg = rectangle_literal(node->bounds.x,
                                 node->bounds.y + node->bounds.h,
                                 node->bounds.w,
                                 0.0f);
    
    F32 current_y = node->bg.y + PADDING;
    
    
    for (UINode *child = node->first_child;
         NULL != child;
         child = child->next_sibling)
    {
     if (child->kind == UI_NODE_KIND_window) { continue; }
     
     if (node->toggled)
     {
      child->hidden = false;
     }
     else
     {
      child->hidden = true;
      continue;
     }
     
     child->sort = node->sort + 2;
     
     child->wrap_width = min_f(child->wrap_width,
                               node->bounds.w);
     
     _ui_layout_and_render_node(input,
                                child,
                                node->bg.x + PADDING,
                                current_y);
     
     node->bg.h += child->bounds.h + PADDING;
     current_y += child->bounds.h + PADDING;
     
     if (child->bounds.w + PADDING * 2 >
         node->bg.w)
     {
      node->bg.w = child->bounds.w + PADDING * 2;
     }
    }
    
    if (node->toggled)
    {
     node->bg.h += PADDING;
     
     if (!is_point_in_region(input->mouse_x, input->mouse_y, node->bg) &&
         !is_point_in_region(input->mouse_x, input->mouse_y, node->bounds))
     {
      node->toggled = false;
     }
     
     blur_screen_region(node->bg, BLUR_STRENGTH, node->sort + 1);
     fill_rectangle(node->bg, BG_COL_2, node->sort + 1, global_ui_projection_matrix);
    }
   }
   
   break;
  }
  case UI_NODE_KIND_slider:
  {
   F32 thumb_x;
   
   thumb_x = ((node->slider_value - node->min) /
              (node->max - node->min)) *
    (node->wrap_width - SLIDER_THUMB_SIZE);
   
   node->bounds = rectangle_literal(x, y,
                                    node->wrap_width,
                                    SLIDER_THICKNESS);
   node->bg = node->bounds;
   node->interactable = rectangle_literal(x + thumb_x, y,
                                          SLIDER_THUMB_SIZE,
                                          SLIDER_THICKNESS);
   
   stroke_rectangle(node->bg,
                    FG_COL_1,
                    STROKE_WIDTH,
                    node->sort,
                    global_ui_projection_matrix);
   
   if (global_hot_widget == node ||
       node->dragging)
   {
    if (node->animation_transition < 1.0f)
    {
     node->animation_transition += ANIMATION_SPEED;
    }
   }
   else if (node->animation_transition > 0.0f)
   {
    node->animation_transition -= ANIMATION_SPEED;
   }
   
   fill_rectangle(node->interactable,
                  colour_lerp(FG_COL_1,
                              colour_literal(0.0f, 0.0f, 0.0f, 0.0f),
                              node->animation_transition),
                  node->sort,
                  global_ui_projection_matrix);
   
   stroke_rectangle(node->interactable,
                    FG_COL_1,
                    STROKE_WIDTH,
                    node->sort,
                    global_ui_projection_matrix);
   
   break;
  }
  case UI_NODE_KIND_label:
  {
   Rect label_bounds = _ui_measure_text(global_ui_font,
                                        x, y,
                                        node->wrap_width,
                                        node->label);
   
   
   node->bounds = rectangle_literal(label_bounds.x,
                                    label_bounds.y +
                                    global_ui_font->size,
                                    label_bounds.w,
                                    label_bounds.h);
   node->bg = node->bounds;
   node->interactable = node->bounds;
   
   draw_text(global_ui_font,
             x, y + global_ui_font->size,
             node->wrap_width,
             FG_COL_1,
             node->label,
             node->sort,
             global_ui_projection_matrix);
   break;
  }
  case UI_NODE_KIND_text_entry:
  {
   node->bounds = rectangle_literal(x, y, node->wrap_width, global_ui_font->size);
   node->bounds.h += PADDING;
   node->bg = node->interactable = node->bounds;
   
   if (node == global_hot_widget ||
       node->toggled)
   {
    if (node->animation_transition < 1.0f) { node->animation_transition += 0.05f; }
   }
   else
   {
    if (node->animation_transition > 0.0f) { node->animation_transition -= 0.05f; }
   }
   
   fill_rectangle(node->bounds,
                  colour_lerp(FG_COL_1,
                              BG_COL_1,
                              node->animation_transition),
                  node->sort,
                  global_ui_projection_matrix);
   
   stroke_rectangle(node->bounds,
                    FG_COL_1,
                    STROKE_WIDTH,
                    node->sort,
                    global_ui_projection_matrix);
   
   draw_text(global_ui_font,
             x + PADDING, y + global_ui_font->size,
             node->wrap_width,
             colour_lerp(BG_COL_2,
                         FG_COL_1,
                         node->animation_transition),
             node->label,
             node->sort,
             global_ui_projection_matrix);
  }
  default:
  {
   UINode *child = node->first_child;
   
   while (child)
   {
    _ui_layout_and_render_node(input, child, x, y);
    child = child->next_sibling;
   }
  }
 }
}

internal void
ui_prepare(void)
{
 global_widgets_under_mouse = NULL;
 global_is_mouse_over_ui = false;
}

internal void
ui_finish(PlatformState *input)
{
 global_is_mouse_over_ui = global_is_mouse_over_ui ||
  (global_widgets_under_mouse != NULL)              ||
  (global_active_widget != NULL);
 
 global_hot_widget = NULL;
 UINode *widget_under_mouse = global_widgets_under_mouse;
 while (widget_under_mouse)
 {
  if (!global_hot_widget ||
      widget_under_mouse->sort >= global_hot_widget->sort)
  {
   global_hot_widget = widget_under_mouse;
  }
  widget_under_mouse = widget_under_mouse->next_under_mouse;
 }
 
 _ui_layout_and_render_node(input, &global_ui_root, 0, 0);
 
 memset(&global_ui_root, 0, sizeof(global_ui_root));
 
 if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
 {
  global_active_widget = NULL;
 }
}
