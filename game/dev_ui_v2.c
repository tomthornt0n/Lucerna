#define UI_SORT_DEPTH 128

#define UI_ANIMATION_SPEED 0.1f

typedef U64 UIWidgetFlags;
typedef enum UIWidgetFlags_ENUM
{
 // NOTE(tbt): drawing
 UI_WIDGET_FLAG_draw_outline    = 1 << 0,
 UI_WIDGET_FLAG_draw_background = 1 << 1,
 UI_WIDGET_FLAG_draw_label      = 1 << 2,
 UI_WIDGET_FLAG_hot_effect      = 1 << 3,
 UI_WIDGET_FLAG_active_effect   = 1 << 4,
 
 // NOTE(tbt): interaction
 UI_WIDGET_FLAG_clickable       = 1 << 5,
 UI_WIDGET_FLAG_draggable_x     = 1 << 6,
 UI_WIDGET_FLAG_draggable_y     = 1 << 7,
} UIWidgetFlags_ENUM;

typedef enum
{
 UI_LAYOUT_PLACEMENT_horizontal,
 UI_LAYOUT_PLACEMENT_vertical,
} UILayoutPlacement;

typedef struct
{
 F32 dim;
 F32 strictness;
} UIDimension;

typedef struct UIWidget UIWidget;
struct UIWidget
{
 UIWidget *next_hash;
 S8 key;
 
 UIWidget *first_child;
 UIWidget *last_child;
 UIWidget *next_sibling;
 UIWidget *parent;
 
 UIWidget *next_insertion_point;
 
 // NOTE(tbt): processed when the widget is updated
 B32_s hot;
 B32_s active;
 B32 clicked;
 B32 dragging;
 F32 drag_x, drag_y;
 F32 drag_x_offset, drag_y_offset;
 
 // NOTE(tbt): passed in
 UIWidgetFlags flags;
 Colour colour;
 Colour background;
 Colour hot_colour;
 Colour hot_background;
 Colour active_colour;
 Colour active_background;
 UIDimension w;
 UIDimension h;
 S8 label;
 UILayoutPlacement children_placement;
 
 // NOTE(tbt): calculated by the measurement pass
 struct
 {
  // NOTE(tbt): measure the desired width and height - final size may be reduced
  F32 w;
  F32 h;
  
  // NOTE(tbt): the maximum amount the width and height can be reduced by to make the layout work
  F32 w_can_loose;
  F32 h_can_loose;
 } measure;
 
 // NOTE(tbt): calculated by the layout pass
 Rect layout;
};

//
// NOTE(tbt): all UI state
//~

struct
{
 // TODO(tbt): structure stacks
 UIDimension w_stack[64];
 U32 w_stack_size;
 
 UIDimension h_stack[64];
 U32 h_stack_size;
 
 Colour colour_stack[64];
 U32 colour_stack_size;
 
 Colour background_stack[64];
 U32 background_stack_size;
 
 Colour hot_colour_stack[64];
 U32 hot_colour_stack_size;
 
 Colour hot_background_stack[64];
 U32 hot_background_stack_size;
 
 Colour active_colour_stack[64];
 U32 active_colour_stack_size;
 
 Colour active_background_stack[64];
 U32 active_background_stack_size;
 
 F32 padding;
 F32 stroke_width;
 
 UIWidget root;
 UIWidget *insertion_point;
 
 UIWidget widget_dict[4096];
 
 // NOTE(tbt): cheat stuff that is normaly explicitly parameterised
 F64 frametime_in_s;
 PlatformState *input;
} global_ui_context = {{{0}}};

//
// NOTE(tbt): style stacks
//~

#define ui_width(_w, _strictness) defer_loop(ui_push_width(_w, _strictness), ui_pop_width())

internal void
ui_push_width(F32 w,
              F32 strictness)
{
 if (global_ui_context.w_stack_size < stack_array_size(global_ui_context.w_stack))
 {
  global_ui_context.w_stack_size += 1;
  global_ui_context.w_stack[global_ui_context.w_stack_size].dim = w;
  global_ui_context.w_stack[global_ui_context.w_stack_size].strictness = strictness;
 }
}

internal void
ui_pop_width(void)
{
 if (global_ui_context.w_stack_size)
 {
  global_ui_context.w_stack_size -= 1;
 }
}

//-

#define ui_height(_h, _strictness) defer_loop(ui_push_height(_h, _strictness), ui_pop_height())

internal void
ui_push_height(F32 h,
               F32 strictness)
{
 if (global_ui_context.h_stack_size < stack_array_size(global_ui_context.h_stack))
 {
  global_ui_context.h_stack_size += 1;
  global_ui_context.h_stack[global_ui_context.h_stack_size].dim = h;
  global_ui_context.h_stack[global_ui_context.h_stack_size].strictness = strictness;
 }
}

internal void
ui_pop_height(void)
{
 if (global_ui_context.h_stack_size)
 {
  global_ui_context.h_stack_size -= 1;
 }
}

//-

#define ui_colour(_colour) defer_loop(ui_push_colour(_colour), ui_pop_colour())

internal void
ui_push_colour(Colour colour)
{
 if (global_ui_context.colour_stack_size < stack_array_size(global_ui_context.colour_stack))
 {
  global_ui_context.colour_stack_size += 1;
  global_ui_context.colour_stack[global_ui_context.colour_stack_size] = colour;
 }
}

internal void
ui_pop_colour(void)
{
 if (global_ui_context.colour_stack_size)
 {
  global_ui_context.colour_stack_size -= 1;
 }
}

//-

#define ui_background(_colour) defer_loop(ui_push_background(_colour), ui_pop_background())

internal void
ui_push_background(Colour colour)
{
 if (global_ui_context.background_stack_size < stack_array_size(global_ui_context.background_stack))
 {
  global_ui_context.background_stack_size += 1;
  global_ui_context.background_stack[global_ui_context.background_stack_size] = colour;
 }
}

internal void
ui_pop_background(void)
{
 if (global_ui_context.background_stack_size)
 {
  global_ui_context.background_stack_size -= 1;
 }
}

//-

#define ui_active_colour(_colour) defer_loop(ui_push_active_colour(_colour), ui_pop_active_colour())

internal void
ui_push_active_colour(Colour colour)
{
 if (global_ui_context.active_colour_stack_size < stack_array_size(global_ui_context.active_colour_stack))
 {
  global_ui_context.active_colour_stack_size += 1;
  global_ui_context.active_colour_stack[global_ui_context.active_colour_stack_size] = colour;
 }
}

internal void
ui_pop_active_colour(void)
{
 if (global_ui_context.active_colour_stack_size)
 {
  global_ui_context.active_colour_stack_size -= 1;
 }
}

//-

#define ui_active_background(_colour) defer_loop(ui_push_active_background(_colour), ui_pop_active_background())

internal void
ui_push_active_background(Colour colour)
{
 if (global_ui_context.active_background_stack_size < stack_array_size(global_ui_context.active_background_stack))
 {
  global_ui_context.active_background_stack_size += 1;
  global_ui_context.active_background_stack[global_ui_context.active_background_stack_size] = colour;
 }
}

internal void
ui_pop_active_background(void)
{
 if (global_ui_context.active_background_stack_size)
 {
  global_ui_context.active_background_stack_size -= 1;
 }
}

//-

#define ui_hot_colour(_colour) defer_loop(ui_push_hot_colour(_colour), ui_pop_hot_colour())

internal void
ui_push_hot_colour(Colour colour)
{
 if (global_ui_context.hot_colour_stack_size < stack_array_size(global_ui_context.hot_colour_stack))
 {
  global_ui_context.hot_colour_stack_size += 1;
  global_ui_context.hot_colour_stack[global_ui_context.hot_colour_stack_size] = colour;
 }
}

internal void
ui_pop_hot_colour(void)
{
 if (global_ui_context.hot_colour_stack_size)
 {
  global_ui_context.hot_colour_stack_size -= 1;
 }
}

//-

#define ui_hot_background(_colour) defer_loop(ui_push_hot_background(_colour), ui_pop_hot_background())

internal void
ui_push_hot_background(Colour colour)
{
 if (global_ui_context.hot_background_stack_size < stack_array_size(global_ui_context.hot_background_stack))
 {
  global_ui_context.hot_background_stack_size += 1;
  global_ui_context.hot_background_stack[global_ui_context.hot_background_stack_size] = colour;
 }
}

internal void
ui_pop_hot_background(void)
{
 if (global_ui_context.hot_background_stack_size)
 {
  global_ui_context.hot_background_stack_size -= 1;
 }
}

//
// NOTE(tbt): common
//~

internal void
ui_push_insertion_point(UIWidget *widget)
{
 widget->next_insertion_point = global_ui_context.insertion_point;
 global_ui_context.insertion_point = widget;
}

internal void
ui_pop_insertion_point(void)
{
 if (global_ui_context.insertion_point)
 {
  global_ui_context.insertion_point = global_ui_context.insertion_point->next_insertion_point;
 }
}

internal void
ui_set_styles(UIWidget *widget)
{
 widget->w = global_ui_context.w_stack[global_ui_context.w_stack_size];
 widget->h = global_ui_context.h_stack[global_ui_context.h_stack_size];
 widget->colour = global_ui_context.colour_stack[global_ui_context.colour_stack_size];
 widget->background = global_ui_context.background_stack[global_ui_context.background_stack_size];
 widget->hot_colour = global_ui_context.hot_colour_stack[global_ui_context.hot_colour_stack_size];
 widget->hot_background = global_ui_context.hot_background_stack[global_ui_context.hot_background_stack_size];
 widget->active_colour = global_ui_context.active_colour_stack[global_ui_context.active_colour_stack_size];
 widget->active_background = global_ui_context.active_background_stack[global_ui_context.active_background_stack_size];
}

internal void
ui_insert_widget(UIWidget *widget)
{
 widget->parent = global_ui_context.insertion_point;
 if (global_ui_context.insertion_point->last_child)
 {
  global_ui_context.insertion_point->last_child->next_sibling = widget;
 }
 else
 {
  global_ui_context.insertion_point->first_child = widget;
 }
 global_ui_context.insertion_point->last_child = widget;
}

internal UIWidget *
ui_create_stateless_widget(void)
{
 UIWidget *result = arena_allocate(&global_frame_memory, sizeof(*result));
 
 ui_set_styles(result);
 ui_insert_widget(result);
 
 return result;
}

internal UIWidget *
ui_create_widget(S8 identifier)
{
 UIWidget *result = NULL;
 
 S8List *identifier_list = NULL;
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   identifier);
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   global_ui_context.insertion_point->key);
 
 S8 _identifier = join_s8_list(&global_frame_memory, identifier_list);
 
 U32 index = hash_s8(_identifier,
                     stack_array_size(global_ui_context.widget_dict));
 
 UIWidget *chain= global_ui_context.widget_dict + index;
 
 if (s8_match(chain->key, _identifier))
  // NOTE(tbt): matching widget directly in hash table slot
 {
  result = chain;
 }
 else if (NULL == chain->key.buffer)
  // NOTE(tbt): hash table slot unused
 {
  result = chain;
  result->key = copy_s8(&global_static_memory, _identifier);
 }
 else
 {
  // NOTE(tbt): look through the chain for a matching widget
  for (UIWidget *widget = chain;
       NULL != widget;
       widget = widget->next_hash)
  {
   if (s8_match(widget->key, _identifier))
   {
    result = widget;
    break;
   }
  }
  
  // NOTE(tbt): push a new widget to the chain if a match is not found
  if (NULL == result)
  {
   result = arena_allocate(&global_static_memory, sizeof(*result));
   result->next_hash = global_ui_context.widget_dict[index].next_hash;
   result->key = copy_s8(&global_static_memory, _identifier);
   global_ui_context.widget_dict[index].next_hash = result;
  }
 }
 
 // NOTE(tbt): copy styles from context style stacks
 ui_set_styles(result);
 
 // NOTE(tbt): reset per frame data
 result->first_child = NULL;
 result->last_child = NULL;
 result->next_sibling = NULL;
 result->next_insertion_point = NULL;
 
 ui_insert_widget(result);
 
 return result;
}

internal void
ui_update_widget(UIWidget *widget)
{
 PlatformState *input = global_ui_context.input;
 F64 frametime_in_s = global_ui_context.frametime_in_s;
 
 if (widget->flags & UI_WIDGET_FLAG_draw_label &&
     NULL == widget->label.buffer)
 {
  widget->label = s8_literal("ERROR: label was NULL");
 }
 
 widget->clicked = false;
 
 if (is_point_in_region(input->mouse_x,
                        input->mouse_y,
                        widget->layout))
 {
  widget->hot = min_f(widget->hot + UI_ANIMATION_SPEED, 1.0f);
  
  if (is_mouse_button_pressed(input,
                              MOUSE_BUTTON_left,
                              0))
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable)
   {
    widget->active = true;
    widget->clicked = true;
   }
   
   if (widget->flags & UI_WIDGET_FLAG_draggable_x ||
       widget->flags & UI_WIDGET_FLAG_draggable_y)
   {
    widget->dragging = true;
    widget->drag_x_offset = input->mouse_x - widget->layout.x;
    widget->drag_y_offset = input->mouse_y - widget->layout.y;
   }
  }
  else
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable)
   {
    widget->active = max_f(widget->active - UI_ANIMATION_SPEED, 0.0f);
   }
  }
 }
 else
 {
  widget->hot = max_f(widget->hot - UI_ANIMATION_SPEED, 0.0f);
 }
 
 if (widget->dragging)
 {
  if (widget->flags & UI_WIDGET_FLAG_draggable_x)
  {
   widget->drag_x = input->mouse_x;
  }
  if (widget->flags & UI_WIDGET_FLAG_draggable_y)
  {
   widget->drag_y = input->mouse_y;
  }
  
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   widget->dragging = false;
  }
 }
}

internal void
ui_measurement_pass(UIWidget *root)
{
 root->measure.w = root->w.dim;
 root->measure.h = root->h.dim;
 
 if (root->flags & UI_WIDGET_FLAG_draw_label)
 {
  Rect label_bounds = measure_s32(global_ui_font,
                                  0.0f, 0.0f,
                                  root->measure.w,
                                  s32_from_s8(&global_frame_memory,
                                              root->label));
  if (label_bounds.w > root->measure.w)
  {
   root->measure.w = label_bounds.w;
  }
  
  if (label_bounds.h > root->measure.h)
  {
   root->measure.h = label_bounds.h;
  }
 }
 
 root->measure.w_can_loose = (1.0f - root->w.strictness) * root->measure.w;
 root->measure.h_can_loose = (1.0f - root->h.strictness) * root->measure.h;
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_measurement_pass(child);
 }
}

// TODO(tbt): un-bork
internal void
ui_layout_pass(UIWidget *root,
               F32 solved_w, F32 solved_h,
               F32 curr_x, F32 curr_y)
{
 if (root->flags & UI_WIDGET_FLAG_draggable_x)
 {
  curr_x = root->drag_x - root->drag_x_offset;
 }
 if (root->flags & UI_WIDGET_FLAG_draggable_y)
 {
  curr_y = root->drag_y - root->drag_y_offset;
 }
 F32 x = curr_x;
 F32 y = curr_y;
 
 F32 total_w = 0.0f;
 F32 total_h = 0.0f;
 F32 total_w_can_loose = 0.0f;
 F32 total_h_can_loose = 0.0f;
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_layout_pass(child,
                 child->measure.w,
                 child->measure.h,
                 curr_x, curr_y);
  
  if (child->flags & UI_WIDGET_FLAG_draggable_x ||
      child->flags & UI_WIDGET_FLAG_draggable_y)
  {
   continue;
  }
  
  if (root->children_placement == UI_LAYOUT_PLACEMENT_vertical)
  {
   F32 advance = child->layout.h + global_ui_context.padding;
   curr_y += advance;
   total_h += advance;
   total_w = max_f(total_w, child->layout.w);
  }
  else if (root->children_placement == UI_LAYOUT_PLACEMENT_horizontal)
  {
   F32 advance = child->layout.w + global_ui_context.padding;
   curr_x += advance;
   total_w += advance;
   total_h = max_f(total_h, child->layout.h);
  }
  
  total_w_can_loose += child->measure.w_can_loose;
  total_h_can_loose += child->measure.h_can_loose;
 }
 
 if (total_w > solved_w ||
     total_h > solved_h)
 {
  // NOTE(tbt): calculate how much needs to be lost
  F32 w_to_loose = total_w - solved_w;
  F32 h_to_loose = total_h - solved_h;
  
  // NOTE(tbt): calculate the proportion of what can be lost to actually loose
  F32 w_proportion = min_f(1.0f, w_to_loose / total_w_can_loose);
  F32 h_proportion = min_f(1.0f, h_to_loose / total_h_can_loose);
  
  // NOTE(tbt): distribute the amount to loose between children, redoing layout to accomodate
  F32 _curr_x = x, _curr_y = y;
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   ui_layout_pass(child,
                  child->measure.w - child->measure.w_can_loose * w_proportion,
                  child->measure.h - child->measure.h_can_loose * h_proportion,
                  _curr_x, _curr_y);
   
   if (child->flags & UI_WIDGET_FLAG_draggable_x ||
       child->flags & UI_WIDGET_FLAG_draggable_y)
   {
    continue;
   }
   
   if (root->children_placement == UI_LAYOUT_PLACEMENT_vertical)
   {
    F32 advance = child->layout.h + global_ui_context.padding;
    _curr_y += advance;
    total_h += advance;
    total_w = max_f(total_w, child->layout.w);
   }
   else if (root->children_placement == UI_LAYOUT_PLACEMENT_horizontal)
   {
    F32 advance = child->layout.w + global_ui_context.padding;
    _curr_x += advance;
    total_w += advance;
    total_h = max_f(total_h, child->layout.h);
   }
  }
 }
 
 root->layout = rectangle_literal(x, y, solved_w, solved_h);
}

internal void
ui_render_pass(UIWidget *root)
{
 Colour foreground = root->colour;
 Colour background = root->background;
 if (root->flags & UI_WIDGET_FLAG_hot_effect)
 {
  foreground = colour_lerp(foreground, root->hot_colour, 1 - root->hot);
  background = colour_lerp(background, root->hot_background, 1 - root->hot);
 }
 if (root->flags & UI_WIDGET_FLAG_active_effect)
 {
  foreground = colour_lerp(foreground, root->active_colour, 1 - root->active);
  background = colour_lerp(background, root->active_background, 1 - root->active);
 }
 
 F32 hot_offset_amount = 0.5f;
 Rect rect = offset_rectangle(root->layout,
                              (root->flags & UI_WIDGET_FLAG_hot_effect) * hot_offset_amount *
                              root->hot,
                              0.0f);
 
 //-
 
 if (root->flags & UI_WIDGET_FLAG_draw_background)
 {
  // TODO(tbt): proper sorting
  blur_screen_region(rect, 1, UI_SORT_DEPTH);
  fill_rectangle(rect, background, UI_SORT_DEPTH, global_ui_projection_matrix);
 }
 
 //-
 
 if (root->flags & UI_WIDGET_FLAG_draw_label)
 {
  // TODO(tbt): proper sorting
  mask_rectangle(rect)
   draw_s8(global_ui_font,
           rect.x + global_ui_context.padding,
           rect.y + global_ui_font->vertical_advance,
           rect.w,
           foreground,
           root->label,
           UI_SORT_DEPTH,
           global_ui_projection_matrix);
 }
 
 //-
 
 if (root->flags & UI_WIDGET_FLAG_draw_outline)
 {
  // TODO(tbt): proper sorting
  stroke_rectangle(rect,
                   foreground,
                   global_ui_context.stroke_width,
                   UI_SORT_DEPTH,
                   global_ui_projection_matrix);
 }
 
 //-
 
 mask_rectangle(rect)
 {
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   ui_render_pass(child);
  }
 }
}

//
// NOTE(tbt): api
//~

internal void
ui_initialise(void)
{
 // NOTE(tbt): fill bottoms of style stacks with default values
 global_ui_context.w_stack[0] = (UIDimension){ 999999999.0f, 0.0f };
 global_ui_context.h_stack[0] = (UIDimension){ 999999999.0f, 0.0f };
 global_ui_context.colour_stack[0] = colour_literal(1.0f, 0.967f, 0.982f, 1.0f);
 global_ui_context.background_stack[0] = colour_literal(0.0f, 0.0f, 0.0f, 0.8f);
 global_ui_context.hot_colour_stack[0] = colour_literal(0.0f, 0.0f, 0.0f, 0.5f);
 global_ui_context.hot_background_stack[0] = colour_literal(1.0f, 0.967f, 0.982f, 1.0f);
 global_ui_context.active_colour_stack[0] = colour_literal(1.0f, 0.967f, 0.982f, 1.0f);
 global_ui_context.active_background_stack[0] = colour_literal(0.0f, 0.033f, 0.018f, 0.5f);
 
 global_ui_context.padding = 8.0f;
 global_ui_context.stroke_width = 2.0f;
}

internal void
ui_prepare(PlatformState *input,
           F64 frametime_in_s)
{
 memset(&global_ui_context.root, 0, sizeof(global_ui_context.root));
 global_ui_context.root.children_placement = UI_LAYOUT_PLACEMENT_vertical;
 global_ui_context.insertion_point = &global_ui_context.root;
 global_ui_context.input = input;
 global_ui_context.frametime_in_s = frametime_in_s;
}

internal void
ui_finish(PlatformState *input)
{
 ui_measurement_pass(&global_ui_context.root);
 ui_layout_pass(&global_ui_context.root,
                input->window_w, input->window_h,
                0.0f, 0.0f);
 ui_render_pass(&global_ui_context.root);
}

//-

internal void
ui_label(S8 string)
{
 UIWidget *widget = ui_create_stateless_widget();
 
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->label = string;
 
 ui_update_widget(widget);
}

//-

#define ui_row() defer_loop(ui_begin_row(), ui_pop_insertion_point())

internal void
ui_begin_row(void)
{
 UIWidget *widget = ui_create_stateless_widget();
 widget->children_placement = UI_LAYOUT_PLACEMENT_horizontal;
 ui_push_insertion_point(widget);
}

//-

internal B32
ui_button(S8 text)
{
 UIWidget *widget = ui_create_widget(text);
 widget->flags |= UI_WIDGET_FLAG_draw_background;
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->flags |= UI_WIDGET_FLAG_draw_outline;
 widget->flags |= UI_WIDGET_FLAG_clickable;
 widget->flags |= UI_WIDGET_FLAG_hot_effect;
 widget->flags |= UI_WIDGET_FLAG_active_effect;
 widget->label = text;
 ui_update_widget(widget);
 
 return widget->clicked;
}

//-

internal void
ui_toggle_button(S8 text,
                 B32 *toggled)
{
 UIWidget *widget = ui_create_widget(text);
 widget->flags |= UI_WIDGET_FLAG_draw_background;
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->flags |= UI_WIDGET_FLAG_draw_outline;
 widget->flags |= UI_WIDGET_FLAG_clickable;
 widget->flags |= UI_WIDGET_FLAG_hot_effect;
 widget->flags |= UI_WIDGET_FLAG_active_effect;
 widget->label = text;
 ui_update_widget(widget);
 
 if (widget->clicked)
 {
  *toggled = !(*toggled);
 }
}

//-

#define ui_window(_title) defer_loop(ui_push_window(_title), (ui_pop_insertion_point(), ui_pop_insertion_point()))

internal void
ui_push_window(S8 string)
{
 UIWidget *window = ui_create_widget(string);
 window->flags |= UI_WIDGET_FLAG_draw_background;
 window->flags |= UI_WIDGET_FLAG_draggable_x;
 window->flags |= UI_WIDGET_FLAG_draggable_y;
 window->children_placement = UI_LAYOUT_PLACEMENT_vertical;
 ui_update_widget(window);
 
 ui_push_insertion_point(window);
 {
  // NOTE(tbt): title bar
  ui_height(32.0f, 1.0f)
  {
   UIWidget *title = ui_create_widget(s8_from_format_string(&global_frame_memory,
                                                            "_%.*s_title",
                                                            (I32)string.size,
                                                            string.buffer));
   title->flags |= UI_WIDGET_FLAG_draw_background;
   title->flags |= UI_WIDGET_FLAG_draw_label;
   title->label = string;
   ui_update_widget(title);
  }
  
  // NOTE(tbt): main body
  UIWidget *body = ui_create_stateless_widget();
  body->children_placement = UI_LAYOUT_PLACEMENT_vertical;
  ui_update_widget(body);
  
  ui_push_insertion_point(body);
 }
}