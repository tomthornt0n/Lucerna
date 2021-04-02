#define UI_SORT_DEPTH 128

#define UI_ANIMATION_SPEED 0.1f

typedef U64 UIWidgetFlags;
typedef enum UIWidgetFlags_ENUM
{
 UI_WIDGET_FLAG_draw_outline    = 1 << 0,
 UI_WIDGET_FLAG_draw_background = 1 << 1,
 UI_WIDGET_FLAG_draw_label      = 1 << 2,
 
 UI_WIDGET_FLAG_clickable       = 1 << 3,
} UIWidgetFlags_ENUM;

typedef enum
{
 UI_LAYOUT_PLACEMENT_hidden,
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
 
 B32 touched;
 
 UIWidget *first_child;
 UIWidget *last_child;
 UIWidget *next_sibling;
 UIWidget *parent;
 
 UIWidget *next_insertion_point;
 
 // NOTE(tbt): 'smooth' booleans
 F32 hot;
 F32 active;
 
 UIWidgetFlags flags;
 
 struct
 {
  Colour colour;
  UIDimension w;
  UIDimension h;
  S8 label;
  UILayoutPlacement children_placement;
 } in;
 
 struct
 {
  // NOTE(tbt): measure the desired width and height - final size may be different
  F32 w;
  F32 h;
 } measure;
 
 Rect layout;
};

//
// NOTE(tbt): all UI state
//~

struct
{
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

#define ui_width(_w, _strictness) _defer_loop(_ui_push_width(_w, _strictness), _ui_pop_width())

internal void
_ui_push_width(F32 w,
               F32 strictness)
{
 if (global_ui_context.w_stack_size < _stack_array_size(global_ui_context.w_stack))
 {
  global_ui_context.w_stack_size += 1;
  global_ui_context.w_stack[global_ui_context.w_stack_size].dim = w;
  global_ui_context.w_stack[global_ui_context.w_stack_size].strictness = strictness;
 }
}

internal void
_ui_pop_width(void)
{
 if (global_ui_context.w_stack_size)
 {
  global_ui_context.w_stack_size -= 1;
 }
}

//-

#define ui_height(_h, _strictness) _defer_loop(_ui_push_height(_h, _strictness), _ui_pop_height())

internal void
_ui_push_height(F32 h,
                F32 strictness)
{
 if (global_ui_context.h_stack_size < _stack_array_size(global_ui_context.h_stack))
 {
  global_ui_context.h_stack_size += 1;
  global_ui_context.h_stack[global_ui_context.h_stack_size].dim = h;
  global_ui_context.h_stack[global_ui_context.h_stack_size].strictness = strictness;
 }
}

internal void
_ui_pop_height(void)
{
 if (global_ui_context.h_stack_size)
 {
  global_ui_context.h_stack_size -= 1;
 }
}

//-

#define ui_colour(_colour) _defer_loop(_ui_push_colour(_colour), _ui_pop_colour())

internal void
_ui_push_colour(Colour colour)
{
 if (global_ui_context.colour_stack_size < _stack_array_size(global_ui_context.colour_stack))
 {
  global_ui_context.colour_stack_size += 1;
  global_ui_context.colour_stack[global_ui_context.colour_stack_size] = colour;
 }
}

internal void
_ui_pop_colour(void)
{
 if (global_ui_context.colour_stack_size)
 {
  global_ui_context.colour_stack_size -= 1;
 }
}

//-

#define ui_background(_colour) _defer_loop(_ui_push_background(_colour), _ui_pop_background())

internal void
_ui_push_background(Colour colour)
{
 if (global_ui_context.background_stack_size < _stack_array_size(global_ui_context.background_stack))
 {
  global_ui_context.background_stack_size += 1;
  global_ui_context.background_stack[global_ui_context.background_stack_size] = colour;
 }
}

internal void
_ui_pop_background(void)
{
 if (global_ui_context.background_stack_size)
 {
  global_ui_context.background_stack_size -= 1;
 }
}

//-

#define ui_active_colour(_colour) _defer_loop(_ui_push_active_colour(_colour), _ui_pop_active_colour())

internal void
_ui_push_active_colour(Colour colour)
{
 if (global_ui_context.active_colour_stack_size < _stack_array_size(global_ui_context.active_colour_stack))
 {
  global_ui_context.active_colour_stack_size += 1;
  global_ui_context.active_colour_stack[global_ui_context.active_colour_stack_size] = colour;
 }
}

internal void
_ui_pop_active_colour(void)
{
 if (global_ui_context.active_colour_stack_size)
 {
  global_ui_context.active_colour_stack_size -= 1;
 }
}

//-

#define ui_active_background(_colour) _defer_loop(_ui_push_active_background(_colour), _ui_pop_active_background())

internal void
_ui_push_active_background(Colour colour)
{
 if (global_ui_context.active_background_stack_size < _stack_array_size(global_ui_context.active_background_stack))
 {
  global_ui_context.active_background_stack_size += 1;
  global_ui_context.active_background_stack[global_ui_context.active_background_stack_size] = colour;
 }
}

internal void
_ui_pop_active_background(void)
{
 if (global_ui_context.active_background_stack_size)
 {
  global_ui_context.active_background_stack_size -= 1;
 }
}

//-

#define ui_hot_colour(_colour) _defer_loop(_ui_push_hot_colour(_colour), _ui_pop_hot_colour())

internal void
_ui_push_hot_colour(Colour colour)
{
 if (global_ui_context.hot_colour_stack_size < _stack_array_size(global_ui_context.hot_colour_stack))
 {
  global_ui_context.hot_colour_stack_size += 1;
  global_ui_context.hot_colour_stack[global_ui_context.hot_colour_stack_size] = colour;
 }
}

internal void
_ui_pop_hot_colour(void)
{
 if (global_ui_context.hot_colour_stack_size)
 {
  global_ui_context.hot_colour_stack_size -= 1;
 }
}

//-

#define ui_hot_background(_colour) _defer_loop(_ui_push_hot_background(_colour), _ui_pop_hot_background())

internal void
_ui_push_hot_background(Colour colour)
{
 if (global_ui_context.hot_background_stack_size < _stack_array_size(global_ui_context.hot_background_stack))
 {
  global_ui_context.hot_background_stack_size += 1;
  global_ui_context.hot_background_stack[global_ui_context.hot_background_stack_size] = colour;
 }
}

internal void
_ui_pop_hot_background(void)
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
ui_insert_widget(UIWidget *widget)
{
 widget->parent = global_ui_context.insertion_point;
 if (global_ui_context.insertion_point->last_child)
 {
  global_ui_context.insertion_point->last_child->next_sibling = widget;
 }
 else
 {
  global_ui_context.insertion_point->last_child = widget;
 }
 global_ui_context.insertion_point->first_child = widget;
}

internal UIWidget *
ui_create_stateless_widget(void)
{
 UIWidget *result = arena_allocate(&global_frame_memory, sizeof(*result));
 
 // NOTE(tbt): copy styles from context style stacks
 result->in.w = global_ui_context.w_stack[global_ui_context.w_stack_size];
 result->in.h = global_ui_context.h_stack[global_ui_context.h_stack_size];
 result->in.colour = global_ui_context.colour_stack[global_ui_context.colour_stack_size];
 
 ui_insert_widget(result);
 
 return result;
}

internal UIWidget *
ui_create_widget(S8 identifier)
{
 UIWidget *result = NULL;
 
 S8List *_identifier = NULL;
 _identifier = push_s8_to_list(&global_frame_memory,
                               _identifier,
                               identifier);
 _identifier = push_s8_to_list(&global_frame_memory,
                               _identifier,
                               global_ui_context.insertion_point->key);
 
 U32 index = hash_s8(join_s8_list(&global_frame_memory, _identifier),
                     _stack_array_size(global_ui_context.widget_dict));
 
 // NOTE(tbt): first look through the chain for a match
 for (UIWidget *widget = global_ui_context.widget_dict + index;
      NULL != widget;
      widget = widget->next_hash)
 {
  if (s8_match(widget->key, identifier))
  {
   result = widget;
  }
 }
 
 // NOTE(tbt): if not found, look through the chain for a free widget
 if (NULL == result)
 {
  for (UIWidget *widget = global_ui_context.widget_dict + index;
       NULL != widget;
       widget = widget->next_hash)
  {
   if (!widget->touched)
   {
    result = widget;
   }
  }
  
  // NOTE(tbt): push a new widget to the chain if there are none free
  if (NULL == result)
  {
   result = arena_allocate(&global_static_memory, sizeof(*result));
   result->next_hash = global_ui_context.widget_dict[index].next_hash;
   global_ui_context.widget_dict[index].next_hash = result;
  }
 }
 
 // NOTE(tbt): copy styles from context style stacks
 result->in.w = global_ui_context.w_stack[global_ui_context.w_stack_size];
 result->in.h = global_ui_context.h_stack[global_ui_context.h_stack_size];
 result->in.colour = global_ui_context.colour_stack[global_ui_context.colour_stack_size];
 
 // NOTE(tbt): reset per frame pointers
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
 
 if (is_point_in_region(input->mouse_x,
                        input->mouse_y,
                        widget->layout))
 {
  widget->hot = min_f(widget->hot + UI_ANIMATION_SPEED, 1.0f);
  
  if (widget->flags & UI_WIDGET_FLAG_clickable)
  {
   if (input->is_mouse_button_down[MOUSE_BUTTON_left])
   {
    widget->active = 1.0f;
   }
   else
   {
    widget->hot = max_f(widget->hot - UI_ANIMATION_SPEED, 0.0f);
   }
  }
 }
 else
 {
  widget->hot = max_f(widget->hot - UI_ANIMATION_SPEED, 0.0f);
 }
}

internal void
ui_measurement_pass(UIWidget *root)
{
 root->measure.w = root->in.w.dim;
 root->measure.h = root->in.h.dim;
 
 if (root->flags & UI_WIDGET_FLAG_draw_label)
 {
  Rect label_bounds = measure_s32(global_ui_font,
                                  0.0f, 0.0f,
                                  root->measure.w,
                                  s32_from_s8(&global_frame_memory,
                                              root->in.label));
  if (label_bounds.w > root->measure.w)
  {
   root->measure.w = label_bounds.w;
  }
  
  if (label_bounds.h > root->measure.h)
  {
   root->measure.h = label_bounds.h;
  }
 }
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_measurement_pass(child);
 }
}

internal void
ui_layout_pass(UIWidget *root,
               F32 curr_x, F32 curr_y,
               UILayoutPlacement placement)
{
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_layout_pass(child, curr_x, curr_y, root->in.children_placement);
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
}

internal void
ui_prepare(PlatformState *input,
           F64 frametime_in_s)
{
 memset(&global_ui_context.root, 0, sizeof(global_ui_context.root));
 global_ui_context.insertion_point = &global_ui_context.root;
 global_ui_context.input = input;
 global_ui_context.frametime_in_s = frametime_in_s;
}

internal void
ui_finish(void)
{
 
}

//-

internal void
ui_label(S8 string)
{
 UIWidget *widget = ui_create_stateless_widget();
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 ui_update_widget(widget);
}

//-

#define ui_row() _defer_loop(_ui_begin_row(), ui_pop_insertion_point())

internal void
_ui_begin_row(void)
{
 UIWidget *widget = ui_create_stateless_widget();
 widget->in.children_placement = UI_LAYOUT_PLACEMENT_horizontal;
 ui_push_insertion_point(widget);
}

//-

#define ui_window(_title) _defer_loop(_ui_begin_window(_title), (ui_pop_insertion_point(), ui_pop_insertion_point()))

internal void
_ui_begin_window(S8 title)
{
 UIWidget *window = ui_create_widget(title);
 window->flags =
  UI_WIDGET_FLAG_draw_outline |
  UI_WIDGET_FLAG_draw_background;
 window->in.children_placement = UI_LAYOUT_PLACEMENT_vertical;
 ui_update_widget(window);
 
 ui_push_insertion_point(window);
 {
  // NOTE(tbt): title bar
  ui_width(999999999.0f, 0.0f) ui_row()
  {
   ui_label(title);
  }
  
  // NOTE(tbt): main body
  UIWidget *body = ui_create_stateless_widget();
  body->in.children_placement = UI_LAYOUT_PLACEMENT_vertical;
  ui_update_widget(body);
  
  ui_push_insertion_point(body);
 }
}