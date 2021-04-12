//-TODO: 
//
//  - window sorting
//
//~

enum { UI_SORT_DEPTH = 128 };

#define UI_ANIMATION_SPEED (5.0 * frametime_in_s)

//~
typedef U64 UIWidgetFlags;
typedef enum UIWidgetFlags_ENUM
{
 //-NOTE(tbt): drawing
 UI_WIDGET_FLAG_draw_outline                 = 1 <<  0,
 UI_WIDGET_FLAG_draw_background              = 1 <<  1,
 UI_WIDGET_FLAG_draw_label                   = 1 <<  2,
 UI_WIDGET_FLAG_blur_background              = 1 <<  3,
 UI_WIDGET_FLAG_hot_effect                   = 1 <<  4,
 UI_WIDGET_FLAG_active_effect                = 1 <<  5,
 UI_WIDGET_FLAG_toggled_effect               = 1 <<  6,
 UI_WIDGET_FLAG_draw_cursor                  = 1 <<  7,
 
 //-NOTE(tbt): interaction
 UI_WIDGET_FLAG_no_input                     = 1 <<  8,
 UI_WIDGET_FLAG_clickable                    = 1 <<  9,
 UI_WIDGET_FLAG_draggable_x                  = 1 << 10,
 UI_WIDGET_FLAG_draggable_y                  = 1 << 11,
 UI_WIDGET_FLAG_keyboard_focusable           = 1 << 12,
 UI_WIDGET_FLAG_click_on_keyboard_focus_lost = 1 << 13,
 
 //-NOTE(tbt): convenience
 UI_WIDGET_FLAG_draggable       = UI_WIDGET_FLAG_draggable_x | UI_WIDGET_FLAG_draggable_y,
} UIWidgetFlags_ENUM;

//~

typedef enum
{
 UI_LAYOUT_PLACEMENT_vertical,
 UI_LAYOUT_PLACEMENT_horizontal,
} UILayoutPlacement;

typedef struct
{
 F32 dim;
 F32 strictness;
} UIDimension;

//~

typedef struct UIWidget UIWidget;
struct UIWidget
{
 UIWidget *next_hash;
 S8 key;
 
 UIWidget *first_child;
 UIWidget *last_child;
 UIWidget *next_sibling;
 UIWidget *parent;
 U32 child_count;
 
 UIWidget *next_insertion_point;
 
 UIWidget *next_keyboard_focus;
 UIWidget *prev_keyboard_focus;
 
 // NOTE(tbt): processed when the widget is updated
 B32_s hot;
 B32_s active;
 B32 clicked;
 F32 drag_x, drag_y;
 F32 drag_x_offset, drag_y_offset;
 B32_s toggled_transition;
 B32_s keyboard_focused_transition;
 
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
 F32 indent;
 Font *font;
 S8 label;
 UILayoutPlacement children_placement;
 B32 toggled;
 U32 cursor;
 U32 mark;
 
 // NOTE(tbt): calculated by the measurement pass
 struct
 {
  // NOTE(tbt): the desired width and height - may be reduced during layouting
  F32 w;
  F32 h;
  
  // NOTE(tbt): the maximum amount the width and height can be reduced by to make the layout work
  F32 w_can_loose;
  F32 h_can_loose;
  
  // NOTE(tbt): sum of the children's above (including padding)
  F32 children_total_w;
  F32 children_total_h;
  F32 children_total_w_can_loose;
  F32 children_total_h_can_loose;
 } measure;
 
 // NOTE(tbt): calculated by the layout pass
 Rect layout;
 
 Rect interactable;
};

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
 
 F32 indent_stack[64];
 U32 indent_stack_size;
 
 Font *font_stack[64];
 U32 font_stack_size;
 
 F32 padding;
 F32 stroke_width;
 
 UIWidget root;
 UIWidget *insertion_point;
 
 UIWidget *hot;
 
 UIWidget *first_keyboard_focus;
 UIWidget *last_keyboard_focus;
 UIWidget *keyboard_focus;
 
 UIWidget widget_dict[4096];
 
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
 if (global_ui_context.w_stack_size < array_count(global_ui_context.w_stack))
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
 if (global_ui_context.h_stack_size < array_count(global_ui_context.h_stack))
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
 if (global_ui_context.colour_stack_size < array_count(global_ui_context.colour_stack))
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
 if (global_ui_context.background_stack_size < array_count(global_ui_context.background_stack))
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
 if (global_ui_context.active_colour_stack_size < array_count(global_ui_context.active_colour_stack))
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
 if (global_ui_context.active_background_stack_size < array_count(global_ui_context.active_background_stack))
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
 if (global_ui_context.hot_colour_stack_size < array_count(global_ui_context.hot_colour_stack))
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
 if (global_ui_context.hot_background_stack_size < array_count(global_ui_context.hot_background_stack))
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

//-

#define ui_indent(_indent) defer_loop(ui_push_indent(_indent), ui_pop_indent())

internal void
ui_push_indent(F32 indent)
{
 if (global_ui_context.indent_stack_size < array_count(global_ui_context.indent_stack))
 {
  global_ui_context.indent_stack_size += 1;
  global_ui_context.indent_stack[global_ui_context.indent_stack_size] = indent;
 }
}

internal void
ui_pop_indent(void)
{
 if (global_ui_context.indent_stack_size)
 {
  global_ui_context.indent_stack_size -= 1;
 }
}

//-

#define ui_font(_font) defer_loop(ui_push_font(_font), ui_pop_font())

internal void
ui_push_font(Font *font)
{
 if (global_ui_context.font_stack_size < array_count(global_ui_context.font_stack))
 {
  global_ui_context.font_stack_size += 1;
  global_ui_context.font_stack[global_ui_context.font_stack_size] = font;
 }
}

internal void
ui_pop_font(void)
{
 if (global_ui_context.font_stack_size)
 {
  global_ui_context.font_stack_size -= 1;
 }
}

//
// NOTE(tbt): internal
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
 widget->indent = global_ui_context.indent_stack[global_ui_context.indent_stack_size];
 widget->font = global_ui_context.font_stack[global_ui_context.font_stack_size];
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
 
 global_ui_context.insertion_point->child_count += 1;
}

internal UIWidget *
ui_widget_from_string(S8 identifier)
{
 UIWidget *result = NULL;
 
 S8List *identifier_list = NULL;
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   identifier);
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   s8("\\"));
 identifier_list = push_s8_to_list(&global_frame_memory,
                                   identifier_list,
                                   global_ui_context.insertion_point->key);
 S8 _identifier = join_s8_list(&global_frame_memory, identifier_list);
 
 U32 index = hash_s8(_identifier,
                     array_count(global_ui_context.widget_dict));
 
 UIWidget *chain = global_ui_context.widget_dict + index;
 
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
   result = arena_push(&global_static_memory, sizeof(*result));
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
 result->next_keyboard_focus = NULL;
 result->child_count = 0;
 
 ui_insert_widget(result);
 
 return result;
}

internal void
ui_update_widget(UIWidget *widget)
{
 PlatformState *input = global_ui_context.input;
 F64 frametime_in_s = global_ui_context.frametime_in_s;
 
 if ((widget->flags & UI_WIDGET_FLAG_draw_label ||
      widget->flags & UI_WIDGET_FLAG_draw_cursor) &&
     NULL == widget->label.buffer)
 {
  widget->label = s8("ERROR: label was NULL");
 }
 
 widget->clicked = false;
 
 if (widget->flags & UI_WIDGET_FLAG_keyboard_focusable)
 {
  if (global_ui_context.last_keyboard_focus)
  {
   global_ui_context.last_keyboard_focus->next_keyboard_focus = widget;
  }
  else
  {
   global_ui_context.first_keyboard_focus = widget;
  }
  widget->prev_keyboard_focus = global_ui_context.last_keyboard_focus;
  global_ui_context.last_keyboard_focus = widget;
  
  if (widget == global_ui_context.keyboard_focus)
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable &&
       is_key_pressed(input, KEY_enter, 0))
   {
    widget->clicked = true;
   }
   
   widget->keyboard_focused_transition = min_f(widget->keyboard_focused_transition + UI_ANIMATION_SPEED, 1.0f);
  }
  else
  {
   widget->keyboard_focused_transition = max_f(widget->keyboard_focused_transition - UI_ANIMATION_SPEED, 0.0f);
  }
 }
 
 if (widget == global_ui_context.hot)
 {
  widget->hot += UI_ANIMATION_SPEED;
  
  if (is_mouse_button_pressed(input, MOUSE_BUTTON_left, 0))
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable)
   {
    widget->active = true;
    widget->clicked = true;
    cm_play(global_click_sound);
   }
   
   if (widget->flags & UI_WIDGET_FLAG_draggable_x ||
       widget->flags & UI_WIDGET_FLAG_draggable_y)
   {
    widget->active = true;
    widget->drag_x_offset = input->mouse_x - widget->layout.x;
    widget->drag_y_offset = input->mouse_y - widget->layout.y;
   }
  }
  else
  {
   if (widget->flags & UI_WIDGET_FLAG_clickable)
   {
    widget->active -= UI_ANIMATION_SPEED;
   }
  }
 }
 else
 {
  widget->hot -= UI_ANIMATION_SPEED;
 }
 
 if (widget->active &&
     widget->flags & UI_WIDGET_FLAG_draggable)
 {
  if (widget->flags & UI_WIDGET_FLAG_draggable_x)
  {
   widget->drag_x = input->mouse_x - widget->drag_x_offset - widget->parent->layout.x;
  }
  if (widget->flags & UI_WIDGET_FLAG_draggable_y)
  {
   widget->drag_y = input->mouse_y - widget->drag_y_offset - widget->parent->layout.y;
  }
  
  if (!input->is_mouse_button_down[MOUSE_BUTTON_left])
  {
   widget->active = false;
  }
 }
 
 if (widget->flags & UI_WIDGET_FLAG_toggled_effect)
 {
  if (widget->toggled)
  {
   widget->toggled_transition = min_f(widget->toggled_transition + UI_ANIMATION_SPEED, 1.0f);
  }
  else
  {
   widget->toggled_transition = max_f(widget->toggled_transition - UI_ANIMATION_SPEED, 0.0f);
  }
 }
 
 widget->hot = clamp_f(widget->hot, 0.0f, 1.0f);
 widget->active = clamp_f(widget->active, 0.0f, 1.0f);
}

internal void
ui_measurement_pass(UIWidget *root)
{
 root->measure.w = root->w.dim;
 root->measure.h = root->h.dim;
 root->measure.children_total_w = 0.0f;
 root->measure.children_total_h = 0.0f;
 root->measure.children_total_w_can_loose = 0.0f;
 root->measure.children_total_h_can_loose = 0.0f;
 root->measure.w_can_loose = (1.0f - root->w.strictness) * root->measure.w;
 root->measure.h_can_loose = (1.0f - root->h.strictness) * root->measure.h;
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_measurement_pass(child);
  root->measure.children_total_w += child->measure.w + global_ui_context.padding + child->indent;
  root->measure.children_total_h += child->measure.h + global_ui_context.padding;
  root->measure.children_total_w_can_loose += child->measure.w_can_loose;
  root->measure.children_total_h_can_loose += child->measure.h_can_loose;
 }
}

internal void
ui_layout_pass(UIWidget *root,
               F32 x, F32 y)
{
 x += root->indent;
 
 if (root->flags & UI_WIDGET_FLAG_draggable_x)
 {
  x = root->parent->layout.x + root->drag_x;
 }
 if (root->flags & UI_WIDGET_FLAG_draggable_y)
 {
  y = root->parent->layout.y + root->drag_y;
 }
 
 root->layout = rect(x, y, root->measure.w, root->measure.h);
 
 if (root->parent)
 {
  root->interactable = rect_at_intersection(root->layout,
                                            root->parent->interactable);
 }
 else
 {
  root->interactable = root->layout;
 }
 
 if (root->children_placement == UI_LAYOUT_PLACEMENT_vertical)
 {
  F32 to_loose = root->measure.children_total_h - root->measure.h;
  
  F32 proportion = 0.0f;
  if (root->measure.children_total_h_can_loose > 0.0f &&
      to_loose > 0.0f)
  {
   proportion = clamp_f(to_loose / root->measure.children_total_h_can_loose, 0.0f, 1.0f);
  }
  
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   child->measure.h -= child->measure.h_can_loose * proportion;
   if (child->measure.w > root->measure.w)
   {
    child->measure.w -= min_f(child->measure.w - root->measure.w, child->measure.w_can_loose);
   }
   ui_layout_pass(child, x, y);
   y += child->layout.h + global_ui_context.padding;
  }
 }
 else if (root->children_placement == UI_LAYOUT_PLACEMENT_horizontal)
 {
  F32 to_loose = root->measure.children_total_w - root->measure.w;
  
  F32 proportion = 0.0f;
  if (root->measure.children_total_w_can_loose > 0.0f &&
      to_loose > 0.0f)
  {
   proportion = clamp_f(to_loose / root->measure.children_total_w_can_loose, 0.0f, 1.0f);
  }
  
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   child->measure.w -= child->measure.w_can_loose * proportion;
   if (child->measure.h > root->measure.h)
   {
    child->measure.h -= min_f(child->measure.h - root->measure.h, child->measure.h_can_loose);
   }
   ui_layout_pass(child, x, y);
   x += child->layout.w + global_ui_context.padding;
  }
 }
}

internal void
ui_render_pass(UIWidget *root)
{
 Colour foreground = root->colour;
 Colour background = root->background;
 
 if (root->flags & UI_WIDGET_FLAG_toggled_effect)
 {
  foreground = colour_lerp(foreground, root->hot_colour, (F32)(1 - root->toggled_transition));
  background = colour_lerp(background, root->hot_background, (F32)(1 - root->toggled_transition));
 }
 if (root->flags & UI_WIDGET_FLAG_keyboard_focusable)
 {
  foreground = colour_lerp(foreground, root->active_colour, 1.0f - root->keyboard_focused_transition);
  background = colour_lerp(background, root->active_background, 1.0f - root->keyboard_focused_transition);
 }
 if (root->flags & UI_WIDGET_FLAG_hot_effect)
 {
  foreground = colour_lerp(foreground, root->hot_colour, 1.0f - (root->hot * root->hot));
  background = colour_lerp(background, root->hot_background, 1.0f - (root->hot * root->hot));
 }
 if (root->flags & UI_WIDGET_FLAG_active_effect)
 {
  foreground = colour_lerp(foreground, root->active_colour, 1.0f - root->active);
  background = colour_lerp(background, root->active_background, 1.0f - root->active);
 }
 
 F32 offset_scale = 5.0f;
 F32 x_offset = 0;
 x_offset += !!(root->flags & UI_WIDGET_FLAG_hot_effect) * root->hot * root->hot;
 x_offset += !!(root->flags & UI_WIDGET_FLAG_toggled_effect) * root->toggled_transition;
 x_offset += !!(root->flags & UI_WIDGET_FLAG_keyboard_focusable) * root->keyboard_focused_transition;
 
 Rect final_rectangle = offset_rect(root->layout,
                                    x_offset * offset_scale,
                                    0.0f);
 
 Rect mask  = rect_at_intersection(offset_rect(root->interactable,
                                               x_offset * offset_scale,
                                               0.0f),
                                   renderer_current_mask());
 
 if (root->flags & UI_WIDGET_FLAG_blur_background)
 {
  blur_screen_region(final_rectangle, 1, UI_SORT_DEPTH);
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_background)
 {
  fill_rectangle(final_rectangle, background, UI_SORT_DEPTH, global_ui_projection_matrix);
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_label)
 {
  mask_rectangle(mask) draw_s8(root->font,
                               final_rectangle.x + global_ui_context.padding,
                               final_rectangle.y + root->font->vertical_advance,
                               -1.0f,
                               foreground,
                               root->label,
                               UI_SORT_DEPTH,
                               global_ui_projection_matrix);
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_cursor)
 {
  // NOTE(tbt): will not be happy about any newlines
  // TODO(tbt): do something less stupid
  
  F32 cursor_width = 2.0f;
  
  S8 to_mark = root->label;
  to_mark.size = root->mark;
  Rect to_mark_bounds = measure_s32(root->font,
                                    final_rectangle.x + global_ui_context.padding,
                                    final_rectangle.y + root->font->vertical_advance,
                                    -1.0f,
                                    s32_from_s8(&global_frame_memory, to_mark));
  
  
  S8 to_cursor = root->label;
  to_cursor.size = root->cursor;
  Rect to_cursor_bounds = measure_s32(root->font,
                                      final_rectangle.x + global_ui_context.padding,
                                      final_rectangle.y + root->font->vertical_advance,
                                      -1.0f,
                                      s32_from_s8(&global_frame_memory, to_cursor));
  
  mask_rectangle(mask)
  {
   fill_rectangle(rect(to_mark_bounds.x + to_mark_bounds.w,
                       final_rectangle.y + global_ui_context.padding,
                       (to_cursor_bounds.x + to_cursor_bounds.w) - (to_mark_bounds.x + to_mark_bounds.w),
                       final_rectangle.h - global_ui_context.padding * 2),
                  colour_literal(0.2f, 0.6f, 0.23f, 0.5f),
                  UI_SORT_DEPTH,
                  global_ui_projection_matrix);
   
   fill_rectangle(rect(to_cursor_bounds.x + to_cursor_bounds.w,
                       final_rectangle.y + global_ui_context.padding,
                       cursor_width,
                       final_rectangle.h - global_ui_context.padding * 2),
                  foreground,
                  UI_SORT_DEPTH,
                  global_ui_projection_matrix);
  }
 }
 
 if (root->flags & UI_WIDGET_FLAG_draw_outline)
 {
  stroke_rectangle(final_rectangle,
                   foreground,
                   global_ui_context.stroke_width,
                   UI_SORT_DEPTH,
                   global_ui_projection_matrix);
 }
 
 mask_rectangle(mask)
 {
  for (UIWidget *child = root->first_child;
       NULL != child;
       child = child->next_sibling)
  {
   ui_render_pass(child);
  }
 }
}

internal void
ui_recursively_find_hot_widget(PlatformState *input,
                               UIWidget *root)
{
 if (is_point_in_rect(input->mouse_x,
                      input->mouse_y,
                      root->interactable) &&
     !(root->flags & UI_WIDGET_FLAG_no_input))
 {
  global_ui_context.hot = root;
 }
 
 for (UIWidget *child = root->first_child;
      NULL != child;
      child = child->next_sibling)
 {
  ui_recursively_find_hot_widget(input, child);
 }
}

internal void
ui_set_keyboard_focus(UIWidget *focus)
{
 if (global_ui_context.keyboard_focus &&
     global_ui_context.keyboard_focus->flags & UI_WIDGET_FLAG_click_on_keyboard_focus_lost)
 {
  global_ui_context.keyboard_focus->clicked = true;
 }
 global_ui_context.keyboard_focus = focus;
}

internal void
ui_defered_input(PlatformState *input)
{
 ui_recursively_find_hot_widget(global_ui_context.input,
                                &global_ui_context.root);
 
 for (PlatformEvent *e = input->events;
      NULL != e;
      e = e->next)
 {
  if (e->kind == PLATFORM_EVENT_key_press)
  {
   if (e->key == KEY_tab)
   {
    if (global_ui_context.keyboard_focus)
    {
     if (e->modifiers & INPUT_MODIFIER_shift)
     {
      global_ui_context.keyboard_focus = global_ui_context.keyboard_focus->prev_keyboard_focus;
     }
     else
     {
      global_ui_context.keyboard_focus = global_ui_context.keyboard_focus->next_keyboard_focus;
     }
    }
    else
    {
     if (e->modifiers & INPUT_MODIFIER_shift)
     {
      global_ui_context.keyboard_focus = global_ui_context.last_keyboard_focus;
     }
     else
     {
      global_ui_context.keyboard_focus = global_ui_context.first_keyboard_focus;
     }
    }
   }
   else if (e->key == KEY_esc)
   {
    global_ui_context.keyboard_focus = NULL;
   }
  }
 }
 if (global_ui_context.input->is_mouse_button_down[MOUSE_BUTTON_left])
 {
  if (!global_ui_context.hot ||
      !(global_ui_context.hot->flags & UI_WIDGET_FLAG_keyboard_focusable))
  {
   global_ui_context.keyboard_focus = NULL;
  }
  else if (global_ui_context.keyboard_focus)
  {
   global_ui_context.keyboard_focus = global_ui_context.hot;
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
 global_ui_context.active_background_stack[0] = colour_literal(0.018f, 0.018f, 0.018f, 0.8f);
 global_ui_context.indent_stack[0] = 0.0f;
 global_ui_context.font_stack[0] = global_ui_font;
 global_ui_context.padding = 8.0f;
 global_ui_context.stroke_width = 2.0f;
}

internal void
ui_prepare(PlatformState *input,
           F64 frametime_in_s)
{
 global_ui_context.insertion_point = &global_ui_context.root;
 global_ui_context.input = input;
 global_ui_context.frametime_in_s = frametime_in_s;
 global_ui_context.first_keyboard_focus = NULL;
 global_ui_context.last_keyboard_focus = NULL;
 memset(&global_ui_context.root, 0, sizeof(global_ui_context.root));
}

internal void
ui_finish(void)
{
 PlatformState *input = global_ui_context.input;
 
 global_ui_context.hot = NULL;
 global_ui_context.root.layout = rect(0.0f, 0.0f, global_rcx.window.w, global_rcx.window.h);
 global_ui_context.root.w = (UIDimension){ input->window_w, 1.0f };
 global_ui_context.root.h = (UIDimension){ input->window_h, 1.0f };
 ui_defered_input(input);
 ui_measurement_pass(&global_ui_context.root);
 ui_layout_pass(&global_ui_context.root, 0.0f, 0.0f);
 ui_render_pass(&global_ui_context.root);
}

//~

internal void
ui_label(S8 text)
{
 UIWidget *widget = ui_widget_from_string(text);
 
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->flags |= UI_WIDGET_FLAG_no_input;
 widget->label = text;
 
 ui_update_widget(widget);
}

//~

#define ui_row() defer_loop(ui_push_layout(UI_LAYOUT_PLACEMENT_horizontal), ui_pop_insertion_point())

#define ui_column() defer_loop(ui_push_layout(UI_LAYOUT_PLACEMENT_vertical), ui_pop_insertion_point())

internal void
ui_push_layout(UILayoutPlacement advancement_direction)
{
 UIWidget *widget = ui_widget_from_string(s8_from_format_string(&global_frame_memory,
                                                                "__child_%u",
                                                                global_ui_context.insertion_point->child_count));
 widget->flags |= UI_WIDGET_FLAG_no_input;
 widget->children_placement = advancement_direction;
 ui_update_widget(widget);
 ui_push_insertion_point(widget);
}


//~

internal B32
ui_button(S8 text)
{
 UIWidget *widget;
 
 ui_background(colour_literal(0.0f, 0.0f, 0.0f, 0.0f))
 {
  widget = ui_widget_from_string(text);
  widget->flags |= UI_WIDGET_FLAG_draw_background;
  widget->flags |= UI_WIDGET_FLAG_draw_label;
  widget->flags |= UI_WIDGET_FLAG_draw_outline;
  widget->flags |= UI_WIDGET_FLAG_hot_effect;
  widget->flags |= UI_WIDGET_FLAG_active_effect;
  widget->flags |= UI_WIDGET_FLAG_clickable;
  widget->flags |= UI_WIDGET_FLAG_keyboard_focusable;
  widget->label = text;
  ui_update_widget(widget);
 }
 return widget->clicked;
}

//~

internal void
ui_toggle_button(S8 text,
                 B32 *toggled)
{
 F64 frametime_in_s = global_ui_context.frametime_in_s;
 
 UIWidget *widget;
 ui_background(colour_literal(0.0f, 0.0f, 0.0f, 0.0f))
 {
  widget = ui_widget_from_string(text);
  widget->flags |= UI_WIDGET_FLAG_draw_background;
  widget->flags |= UI_WIDGET_FLAG_draw_label;
  widget->flags |= UI_WIDGET_FLAG_draw_outline;
  widget->flags |= UI_WIDGET_FLAG_hot_effect;
  widget->flags |= UI_WIDGET_FLAG_active_effect;
  widget->flags |= UI_WIDGET_FLAG_toggled_effect;
  widget->flags |= UI_WIDGET_FLAG_clickable;
  widget->flags |= UI_WIDGET_FLAG_keyboard_focusable;
  widget->label = text;
  widget->toggled = *toggled;
  ui_update_widget(widget);
 }
 
 if (widget->clicked)
 {
  *toggled = !(*toggled);
 }
}

//~

#define ui_window(_title) defer_loop(ui_push_window(_title), (ui_pop_indent(), ui_pop_insertion_point(), ui_pop_insertion_point()))

internal void
ui_push_window(S8 identifier)
{
 PlatformState *input = global_ui_context.input;
 
 UIWidget *window = ui_widget_from_string(identifier);
 window->flags |= UI_WIDGET_FLAG_draw_background;
 window->flags |= UI_WIDGET_FLAG_blur_background;
 window->flags |= UI_WIDGET_FLAG_draggable;
 ui_update_widget(window);
 
 ui_push_insertion_point(window);
 {
  ui_height(32.0f, 1.0f) ui_indent(0.0f)
  {
   UIWidget *title = ui_widget_from_string(s8("__title"));
   title->flags |= UI_WIDGET_FLAG_draw_background;
   title->flags |= UI_WIDGET_FLAG_draw_label;
   title->flags |= UI_WIDGET_FLAG_no_input;
   title->label = identifier;
   ui_update_widget(title);
  }
  
  ui_indent(global_ui_context.padding)
  {
   UIWidget *body = ui_widget_from_string(s8("__body"));
   body->flags |= UI_WIDGET_FLAG_no_input;
   ui_update_widget(body);
   ui_push_insertion_point(body);
  }
 }
}

//~

internal void
ui_slider_f32(S8 identifier,
              F32 *value,
              F32 min, F32 max)
{
 F32 slider_thickness = 16.0f, slider_thumb_size = 24.0f;
 
 UIWidget *track, *thumb;
 
 ui_height(slider_thickness, 1.0f)
 {
  track = ui_widget_from_string(identifier);
  track->flags |= UI_WIDGET_FLAG_draw_outline;
  track->label = identifier;
  ui_update_widget(track);
  ui_push_insertion_point(track);
  {
   ui_width(slider_thumb_size, 1.0f)
   {
    thumb = ui_widget_from_string(s8("__thumb"));
    thumb->flags |= UI_WIDGET_FLAG_draw_outline;
    thumb->flags |= UI_WIDGET_FLAG_draw_background;
    thumb->flags |= UI_WIDGET_FLAG_hot_effect;
    thumb->flags |= UI_WIDGET_FLAG_active_effect;
    thumb->flags |= UI_WIDGET_FLAG_draggable_x;
    ui_update_widget(thumb);
   }
  } ui_pop_insertion_point();
 }
 
 if (thumb->active)
 {
  // NOTE(tbt): calculate value from drag position
  *value = ((thumb->layout.x - track->layout.x) / (track->layout.w - thumb->layout.w)) * (max - min) + min;
 }
 else
 {
  // NOTE(tbt): calculate drag position from value
  thumb->drag_x = ((*value - min) / (max - min)) * (track->layout.w - thumb->layout.w);
 }
 
 thumb->drag_x = clamp_f(thumb->drag_x, 0.0f, track->layout.w - thumb->layout.w);
}

//~

internal B32
ui_line_edit(S8 identifier,
             S8 *result,
             U64 capacity)
{
 PlatformState *input = global_ui_context.input;
 
 UIWidget *widget = ui_widget_from_string(identifier);
 
 B32 done = false;
 if (widget->clicked)
 {
  if (widget == global_ui_context.keyboard_focus)
  {
   global_ui_context.keyboard_focus = NULL;
   done = true;
  }
  else
  {
   global_ui_context.keyboard_focus = widget;
  }
 }
 
 widget->label = *result;
 widget->cursor = min_u(widget->label.size, widget->cursor);
 widget->mark = min_u(widget->label.size, widget->mark);
 
 enum
 {
  ACTION_left,
  ACTION_right,
  ACTION_backspace,
  ACTION_delete,
  ACTION_home,
  ACTION_end,
  ACTION_character_entry,
  ACTION_delete_range,
  ACTION_copy,
  ACTION_cut,
  ACTION_MAX,
 };
 enum
 {
  ACTION_FLAG_move_cursor = 1 << 0,
  ACTION_FLAG_move_mark   = 1 << 1,
  ACTION_FLAG_set_cursor  = 1 << 2,
  ACTION_FLAG_stick_mark  = 1 << 3,
  ACTION_FLAG_delete      = 1 << 4,
  ACTION_FLAG_insert      = 1 << 5,
  ACTION_FLAG_copy        = 1 << 6,
 };
 struct LineEditAction
 {
  U8 flags;
  CursorAdvanceMode advance_mode;
  CursorAdvanceDirection advance_direction;
  U32 set_cursor_to;
  U8 to_insert[4];
  U32 to_insert_size;
 };
 struct LineEditAction actions[ACTION_MAX] =
 {
  [ACTION_left] =
  {
   .flags = ACTION_FLAG_move_cursor,
   .advance_direction = CURSOR_ADVANCE_DIRECTION_backwards
  },
  
  [ACTION_right] =
  {
   .flags = ACTION_FLAG_move_cursor,
   .advance_direction = CURSOR_ADVANCE_DIRECTION_forwards,
  },
  
  [ACTION_backspace] =
  {
   .flags = (ACTION_FLAG_move_cursor |
             ACTION_FLAG_stick_mark |
             ACTION_FLAG_delete),
   .advance_direction = CURSOR_ADVANCE_DIRECTION_backwards,
  },
  
  [ACTION_delete] =
  {
   .flags = (ACTION_FLAG_move_mark |
             ACTION_FLAG_stick_mark |
             ACTION_FLAG_delete),
   .advance_direction = CURSOR_ADVANCE_DIRECTION_forwards,
  },
  
  [ACTION_home] =
  {
   .flags = ACTION_FLAG_set_cursor,
   .set_cursor_to = 0,
  },
  
  [ACTION_end] =
  {
   .flags = ACTION_FLAG_set_cursor,
   .set_cursor_to = widget->label.size,
  },
  
  [ACTION_character_entry] =
  {
   .flags = (ACTION_FLAG_move_cursor |
             ACTION_FLAG_stick_mark |
             ACTION_FLAG_insert),
   .advance_direction = CURSOR_ADVANCE_DIRECTION_forwards,
  },
  
  [ACTION_delete_range] =
  {
   .flags = ACTION_FLAG_delete,
  },
  
  [ACTION_copy] =
  {
   .flags = ACTION_FLAG_copy,
  },
  
  [ACTION_cut] =
  {
   .flags = (ACTION_FLAG_copy |
             ACTION_FLAG_delete),
  },
 };
 struct LineEditAction action = {0};
 
 if (global_ui_context.keyboard_focus == widget)
 {
  widget->flags |= UI_WIDGET_FLAG_draw_cursor;
  
  for (PlatformEvent *e = input->events;
       NULL != e;
       e = e->next)
  {
   if (e->kind == PLATFORM_EVENT_key_typed)
   {
    action.to_insert_size = utf8_from_codepoint(action.to_insert, e->character);
    if (widget->cursor + action.to_insert_size + (widget->label.size - widget->cursor) <= capacity)
    {
     action.flags             = actions[6].flags;
     action.advance_direction = actions[6].advance_direction;
     action.advance_mode      = actions[6].advance_mode;
     if (widget->cursor != widget->mark)
     {
      // TODO(tbt): ideally this wouldn't be a hard coded special case
      U32 start = min_u(widget->cursor, widget->mark);
      U32 end = max_u(widget->cursor, widget->mark);
      
      memcpy(widget->label.buffer + start,
             widget->label.buffer + end,
             widget->label.size - end);
      widget->label.size -= end - start;
      widget->cursor = start;
     }
    }
   }
   else if (e->kind == PLATFORM_EVENT_key_press)
   {
    B32 range_selected = widget->cursor != widget->mark;
    if (e->key == KEY_left)           { action = actions[ACTION_left]; }
    else if (e->key == KEY_right)     { action = actions[ACTION_right]; }
    else if (e->key == KEY_backspace) { action = actions[range_selected ? ACTION_delete_range : ACTION_backspace]; }
    else if (e->key == KEY_delete)    { action = actions[range_selected ? ACTION_delete_range : ACTION_delete]; }
    else if (e->key == KEY_home)      { action = actions[ACTION_home]; }
    else if (e->key == KEY_end)       { action = actions[ACTION_end]; }
    else if (e->key == KEY_c && (e->modifiers & INPUT_MODIFIER_ctrl)) { action = actions[ACTION_copy]; }
    else if (e->key == KEY_x && (e->modifiers & INPUT_MODIFIER_ctrl)) { action = actions[ACTION_cut]; }
    else if (e->key == KEY_v && (e->modifiers & INPUT_MODIFIER_ctrl))
    {
     S8 to_paste = platform_get_clipboard_text(&global_frame_memory);
     {
      U32 index_to_copy_to = widget->cursor + to_paste.size;
      memcpy(widget->label.buffer + index_to_copy_to,
             widget->label.buffer + widget->cursor,
             capacity - index_to_copy_to);
     }
     memcpy(widget->label.buffer + widget->cursor,
            to_paste.buffer,
            to_paste.size);
     widget->label.size += to_paste.size;
     widget->cursor += to_paste.size;
    }
    else { goto skip_action; }
    
    if (e->modifiers & INPUT_MODIFIER_ctrl) { action.advance_mode = CURSOR_ADVANCE_MODE_word; }
    else                                    { action.advance_mode = CURSOR_ADVANCE_MODE_char; }
    if (!(e->modifiers & INPUT_MODIFIER_shift)) {action.flags |= ACTION_FLAG_stick_mark; }
   }
  }
  
  if (action.flags & ACTION_FLAG_move_mark)
  {
   advance_cursor(widget->label,
                  action.advance_direction,
                  action.advance_mode,
                  &widget->mark);
  }
  
  if (action.flags & ACTION_FLAG_copy)
  {
   U32 start = min_u(widget->cursor, widget->mark);
   U32 end = max_u(widget->cursor, widget->mark);
   
   S8 to_copy = widget->label;
   to_copy.buffer += start;
   to_copy.size = end - start;
   platform_set_clipboard_text(to_copy);
  }
  
  if (action.flags & ACTION_FLAG_insert)
  {
   {
    U32 index_to_copy_to = widget->cursor + action.to_insert_size;
    memcpy(widget->label.buffer + index_to_copy_to,
           widget->label.buffer + widget->cursor,
           capacity - index_to_copy_to);
   }
   memcpy(widget->label.buffer + widget->cursor,
          action.to_insert,
          action.to_insert_size);
   widget->label.size += action.to_insert_size;
  }
  
  if (action.flags & ACTION_FLAG_set_cursor)
  {
   widget->cursor = action.set_cursor_to;
  }
  
  if (action.flags & ACTION_FLAG_move_cursor)
  {
   advance_cursor(widget->label,
                  action.advance_direction,
                  action.advance_mode,
                  &widget->cursor);
  }
  
  if (action.flags & ACTION_FLAG_delete)
  {
   U32 start = min_u(widget->cursor, widget->mark);
   U32 end = max_u(widget->cursor, widget->mark);
   
   memcpy(widget->label.buffer + start,
          widget->label.buffer + end,
          widget->label.size - end);
   widget->label.size -= end - start;
   widget->cursor = start;
  }
  
  if (action.flags & ACTION_FLAG_stick_mark)
  {
   widget->mark = widget->cursor;
  }
  
  skip_action:
  
  recalculate_s8_length(&widget->label);
 }
 else
 {
  widget->flags &= ~UI_WIDGET_FLAG_draw_cursor;
 }
 
 widget->flags |= UI_WIDGET_FLAG_draw_background;
 widget->flags |= UI_WIDGET_FLAG_draw_label;
 widget->flags |= UI_WIDGET_FLAG_draw_outline;
 widget->flags |= UI_WIDGET_FLAG_active_effect;
 widget->flags |= UI_WIDGET_FLAG_clickable;
 widget->flags |= UI_WIDGET_FLAG_keyboard_focusable;
 widget->flags |= UI_WIDGET_FLAG_click_on_keyboard_focus_lost;
 ui_update_widget(widget);
 
 *result = widget->label;
 
 return done;
}