/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 01 Jan 2021
  License : N/A
  Notes   : terrible, but only for dev tools so just about acceptable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define PADDING 8.0f
#define STROKE_WIDTH 2.0f

#define SCROLL_SPEED 25.0f

#define SLIDER_THICKNESS 18.0f
#define SLIDER_THUMB_SIZE 12.0f

#define CURSOR_THICKNESS 2.0f

#define BG_COL_1 COLOUR(0.0f, 0.0f, 0.0f, 0.4f)
#define BG_COL_2 COLOUR(0.0f, 0.0f, 0.0f, 0.8f)
#define FG_COL_1 COLOUR(1.0f, 1.0f, 1.0f, 0.9f)

// TODO(tbt): cache vertices when rendering text so they don't need to be
//            recalculated to find the bounds
internal Rectangle
get_text_bounds(Font *font,
                F32 x, F32 y,
                U32 wrap_width,
                I8 *string)
{
    Rectangle result;
    stbtt_aligned_quad q;
    F32 line_start = x;

    F32 min_x = x, min_y = y, max_x = 0.0f, max_y = 0.0f;

    while (*string)
    {
        if (*string >=32 &&
            *string < 128)
        {
            F32 char_width, char_height;
    
            stbtt_GetBakedQuad(font->char_data,
                               font->texture.width,
                               font->texture.height,
                               *string - 32,
                               &x,
                               &y,
                               &q,
                               1);

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

            if (wrap_width && isspace(*string))
            {
                if (x + (q.x1 - q.x0) * 2 >
                    line_start + wrap_width)
                {
                    y += font->size;
                    x = line_start;
                }
            }
        }
        else if (*string == '\n')
        {
            y += font->size;
            x = line_start;
        }

        ++string;
    }

    result = RECTANGLE(min_x, min_y, max_x - min_x, max_y - min_y);
    return result;
}

typedef struct UINode UINode;
struct UINode
{
    U32 type;
    UINode *parent, *first_child, *last_child, *next_sibling;
    I8 *key;
    UINode *next_hash;
    UINode *next_under_mouse;

    Rectangle bounds;       // NOTE(tbt): the total area on the screen taken up by the widget
    Rectangle interactable; // NOTE(tbt): the area that can be interacted with
    Rectangle bg;           // NOTE(tbt): an area that is rendered but not able to be interacted with
    F32 drag_x, drag_y;
    F32 max_width;
    F32 max_height;
    I32 sort;
    B32 toggled;
    B32 dragging;
    I8 *label;
    F64 value_f;
    I64 value_i;
    F32 scroll;
    B32 hidden;
    F32 min, max;
    Asset *texture;
};

#define UI_HASH_TABLE_SIZE (1024)

#define ui_hash(string) hash_string((string), UI_HASH_TABLE_SIZE);

internal UINode global_ui_state_dict[UI_HASH_TABLE_SIZE] = {{0}};

internal UINode *
new_widget_state_from_string(MemoryArena *memory,
                             I8 *string)
{
    U64 index = ui_hash(string);

    if (global_ui_state_dict[index].key &&
        strcmp(global_ui_state_dict[index].key, string))
    {
        // NOTE(tbt): hash collision

        UINode *widget = arena_allocate(memory, sizeof(*widget));
        widget->key = arena_allocate(memory, strlen(string) + 1);

        strcpy(widget->key, string);

        UINode *tail = &global_ui_state_dict[index];
        while (tail->next_hash) { fprintf(stderr, "%s: %s\n", tail->key, tail->label); tail = tail->next_hash; }
        tail->next_hash = widget;

        return widget;
    }

    global_ui_state_dict[index].key = arena_allocate(memory,
                                                     strlen(string) + 1);
    strcpy(global_ui_state_dict[index].key, string);

    return global_ui_state_dict + index;
}

internal UINode *
widget_state_from_string(I8 *string)
{
    U64 index = ui_hash(string);
    UINode *result = global_ui_state_dict + index;

    while (result)
    {
        if (!result->key) { return NULL; }

        if (strcmp(result->key, string))
        {
            result = result->next_hash;
        }
        else
        {
            break;
        }
    }

    return result;
}

enum
{
    UI_NODE_TYPE_NONE,

    UI_NODE_TYPE_WINDOW,
    UI_NODE_TYPE_BUTTON,
    UI_NODE_TYPE_DROPDOWN,
    UI_NODE_TYPE_SLIDER_F,
    UI_NODE_TYPE_SLIDER_I,
    UI_NODE_TYPE_LABEL,
    UI_NODE_TYPE_LINE_BREAK,
    UI_NODE_TYPE_SCROLL_PANEL,
    UI_NODE_TYPE_COLOUR_PICKER,
    UI_NODE_TYPE_ENTRY,
    UI_NODE_TYPE_SPRITE_PICKER,
};

internal UINode *global_hot_widget, *global_active_widget;
internal UINode *global_keyboard_focus;
internal UINode *global_widgets_under_mouse;
internal B32 global_is_mouse_over_ui;

internal UINode global_ui_root = {0};
internal UINode *global_current_ui_parent = &global_ui_root; // NOTE(tbt): new nodes are added as children to this

internal void
begin_window(PlatformState *input,
             I8 *name,
             F32 initial_x, F32 initial_y,
             F32 max_w)
{
    UINode *node;
    static UINode *last_active_window = NULL;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
        node->drag_x = initial_x;
        node->drag_y = initial_y;
        node->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(node->label, name);
    }

    if (global_ui_root.last_child)
    {
        global_ui_root.last_child->next_sibling = node;
        global_ui_root.last_child = node;
    }
    else
    {
        global_ui_root.first_child = node;
        global_ui_root.last_child = node;
    }
    node->parent = global_current_ui_parent;
    global_current_ui_parent = node;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->max_width = max_w;
    if (last_active_window == node)
    {
        node->sort = UI_SORT_DEPTH;
    }
    else
    {
        // NOTE(tbt): 10 is chosen arbitrarily. will cause sorting errors if
        //            the tree is more than 10 deep
        // TODO(tbt): deal with this better
        node->sort = UI_SORT_DEPTH + 10;
    }

    node->type = UI_NODE_TYPE_WINDOW;

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           node->interactable) &&
        !global_hot_widget)
    {
        node->next_under_mouse = global_widgets_under_mouse;
        global_widgets_under_mouse = node;

    }

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           node->bounds))
    {
        global_is_mouse_over_ui = true;
    }

    if (global_hot_widget == node &&
        input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT] &&
        !global_active_widget)
    {
        node->dragging = true;
        global_active_widget = node;
        last_active_window = node;
    }

    if (node->dragging)
    {
        node->drag_x = clamp_f(floor(input->mouse_x - node->interactable.w * 0.5f), 0.0f, global_renderer_window_w - PADDING);
        node->drag_y = clamp_f(floor(input->mouse_y - node->interactable.h * 0.5f), 0.0f, global_renderer_window_h - PADDING);

        if (!global_hot_widget)
        {
            global_active_widget = node;
        }

        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            node->dragging = false;
        }
    }
}

internal void
end_window(void)
{
    assert(global_current_ui_parent->type == UI_NODE_TYPE_WINDOW);
    global_current_ui_parent = global_current_ui_parent->parent;
}

#define do_window(_input, _name, _init_x, _init_y, _max_w) for (I32 i = (begin_window((_input), (_name), (_init_x), (_init_y), (_max_w)), 0); !i; (++i, end_window()))

internal B32
do_bit_toggle_button(PlatformState *input,
                     I8 *name,
                     U64 *mask,
                     I32 bit,
                     F32 width)
{
    UINode *node;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
        node->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(node->label, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_BUTTON;
    node->max_width = width;

    if (mask != NULL) { node->toggled = *mask & BIT(bit); };

    if (!node->hidden)
    {
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT] &&
            global_active_widget == node)
        {
            node->toggled = !node->toggled;
            if (mask != NULL)
            {
                if (node->toggled)
                {
                    *mask |= BIT(bit);
                }
                else
                {
                    *mask &= ~BIT(bit);
                }
            }
        }

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
        }
    }

    return node->toggled;
}

internal B32
do_toggle_button(PlatformState *input,
                 I8 *name,
                 F32 width)
{
    return do_bit_toggle_button(input, name, NULL, 0, width);
}

internal B32
do_button(PlatformState *input,
          I8 *name,
          F32 width)
{
    UINode *node;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
        node->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(node->label, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_BUTTON;
    node->max_width = width;

    if (!node->hidden)
    {
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT] &&
            global_active_widget == node)
        {
            node->toggled = true;
        }
        else
        {
            node->toggled = false;
        }

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
        }
    }

    return node->toggled;
}

internal void
begin_dropdown(PlatformState *input,
               I8 *name,
               F32 width)
{  
    UINode *node;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
        node->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(node->label, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    global_current_ui_parent = node;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_DROPDOWN;
    node->max_width = width;

    if (!node->hidden)
    {
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT] &&
            global_active_widget == node)
        {
            node->toggled = !node->toggled;
        }

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->bg))
        {
            global_is_mouse_over_ui = true;
        }

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->bounds) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
        }
    }
}

internal void
end_dropdown(void)
{
    assert(global_current_ui_parent->type == UI_NODE_TYPE_DROPDOWN);
    global_current_ui_parent = global_current_ui_parent->parent;
}

#define do_dropdown(_input, _name, _width) for (I32 i = (begin_dropdown((_input), (_name), (_width)), 0); !i; (++i, end_dropdown()))

internal void
do_slider_i(PlatformState *input,
            I8 *name,
            F32 min, F32 max,
            F32 snap,
            F32 width,
            I64 *value)
{
    UINode *node;
    
    *value = clamp_i(*value, min, max);

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_SLIDER_I;
    node->min = min;
    node->max = max;
    node->max_width = width;

    if (!node->hidden)
    {
        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
            node->dragging = true;
        }

        if (node->dragging)
        {
            F32 thumb_x = clamp_f(input->mouse_x - node->bounds.x,
                                  0.0f, width - SLIDER_THUMB_SIZE);

            *value = thumb_x / (width - SLIDER_THUMB_SIZE) *
                               (node->max - node->min) + node->min;

            if (snap > 0.0f)
            {
                *value = floor(*value / snap) * snap;
            }

            if (!global_hot_widget)
            {
                global_active_widget = node;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                node->dragging = false;
            }
        }

        node->value_i = *value;
    }
}

internal void
do_slider_lf(PlatformState *input,
             I8 *name,
             F32 min, F32 max,
             F32 snap,
             F32 width,
             F64 *value)
{
    UINode *node;

    *value = clamp_f(*value, min, max);

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_SLIDER_F;
    node->min = min;
    node->max = max;
    node->max_width = width;

    if (!node->hidden)
    {
        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
            node->dragging = true;
        }

        if (node->dragging)
        {
            F32 thumb_x = clamp_f(input->mouse_x - node->bounds.x,
                                  0.0f, width - SLIDER_THUMB_SIZE);

            *value = thumb_x / (width - SLIDER_THUMB_SIZE) *
                                (node->max - node->min) + node->min;

            if (snap > 0.0f)
            {
                *value = floor(*value / snap) * snap;
            }

            if (!global_hot_widget)
            {
                global_active_widget = node;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                node->dragging = false;
            }
        }

        node->value_f = *value;
    }
}

internal void
do_slider_f(PlatformState *input,
            I8 *name,
            F32 min, F32 max,
            F32 snap,
            F32 width,
            F32 *value)
{
    UINode *node;

    *value = clamp_f(*value, min, max);

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_SLIDER_F;
    node->min = min;
    node->max = max;
    node->max_width = width;

    if (!node->hidden)
    {
        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
            node->dragging = true;
        }

        if (node->dragging)
        {
            F32 thumb_x = clamp_f(input->mouse_x - node->bounds.x,
                                  0.0f, width - SLIDER_THUMB_SIZE);

            *value = thumb_x / (width - SLIDER_THUMB_SIZE) *
                                (node->max - node->min) + node->min;

            if (snap > 0.0f)
            {
                *value = floor(*value / snap) * snap;
            }

            if (!global_hot_widget)
            {
                global_active_widget = node;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                node->dragging = false;
            }
        }

        node->value_f = *value;
    }
}

internal void
do_label(I8 *name,
         I8 *label,
         F32 width)
{
    UINode *node;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
    }

    node->label = arena_allocate(&global_frame_memory, strlen(label) + 1);
    strcpy(node->label, label);

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_LABEL;
    node->max_width = width;
}

internal void
do_line_break(void)
{
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_LINE_BREAK;
}

internal void
begin_scroll_panel(PlatformState *input,
                   I8 *name,
                   F32 width,
                   F32 height)
{
    UINode *node;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
        node->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(node->label, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    global_current_ui_parent = node;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_SCROLL_PANEL;
    node->max_width = width;
    node->max_height = height;

    if (!node->hidden)
    {
        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;

        }
        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
            node->dragging = true;
        }

        if (node->dragging)
        {
            node->drag_y = input->mouse_y - node->bg.y -
                            node->interactable.h * 0.5f;

            if (!global_hot_widget)
            {
                global_active_widget = node;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                node->dragging = false;
            }
        }

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->bounds))
        {
            node->scroll -= input->mouse_scroll * SCROLL_SPEED;
        }
    }
}

internal void
end_scroll_panel(void)
{
    assert(global_current_ui_parent->type == UI_NODE_TYPE_SCROLL_PANEL);
    global_current_ui_parent = global_current_ui_parent->parent;
}

#define do_scroll_panel(_input, _name, _width, _height) for (I32 i = (begin_scroll_panel((_input), (_name), (_width), (_height)), 0); !i; (++i, end_scroll_panel()))

internal void
do_text_entry(PlatformState *input,
              I8 *name,
              I8 *buffer,
              U32 buffer_size)
{
    // TODO(tbt): proper cursor

    UINode *node;

    // NOTE(tbt): approximate width of one character on average by taking the width of 'e'
    F32 char_width = get_text_bounds(global_ui_font, 0.0f, 0.0f, 0.0f, "e").w;

    // NOTE(tbt): allow a few characters for padding
    F32 width = (buffer_size + 4) * char_width;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->label = buffer;
    node->max_width = width;
    node->type = UI_NODE_TYPE_ENTRY;

    if (!node->hidden)
    {
        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->bounds) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }

        if (global_hot_widget == node &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            global_active_widget = node;
        }

        U32 len = strlen(node->label);

        if (global_keyboard_focus == node)
        {
            node->toggled = true;

            KeyTyped *key = input->keys_typed;
            while (key)
            {
                if (key->key == 8 && len > 0)
                {
                    node->label[--len] = 0;
                }
                else if (isprint(key->key) && len < buffer_size)
                {
                    node->label[len++] = key->key;
                }
                key = key->next;
            }
        }
        else
        {
            node->toggled = false;
        }
    }
}

internal void
do_sprite_picker(PlatformState *input,
                 I8 *name,
                 Asset *texture,
                 F32 width,
                 F32 snap,
                 SubTexture *sub_texture)
{
    UINode *node;

    if (!(node = widget_state_from_string(name)))
    {
        node = new_widget_state_from_string(&global_static_memory, name);
    }

    if (global_current_ui_parent->last_child)
    {
        global_current_ui_parent->last_child->next_sibling = node;
        global_current_ui_parent->last_child = node;
    }
    else
    {
        global_current_ui_parent->first_child = node;
        global_current_ui_parent->last_child = node;
    }
    node->parent = global_current_ui_parent;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    node->next_under_mouse = NULL;

    node->type = UI_NODE_TYPE_SPRITE_PICKER;
    node->texture = texture;
    node->max_width = width;
    if (texture &&
        texture->loaded)
    {
        node->max_height = texture->texture.height * (width /
                                                      texture->texture.width);
    }
    else
    {
        node->max_height = width;
    }

    if (!node->hidden)
    {
        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               node->interactable) &&
            !global_active_widget)
        {
            node->next_under_mouse = global_widgets_under_mouse;
            global_widgets_under_mouse = node;
        }

        if (texture)
        {
            if (global_hot_widget == node &&
                input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                global_active_widget = node;
                node->dragging = true;

                if (sub_texture)
                {
                    sub_texture->min_x = floor((input->mouse_x - node->bounds.x) / snap) * snap;
                    sub_texture->min_x /= texture->texture.width;

                    sub_texture->min_y = floor((input->mouse_y - node->bounds.y) / snap) * snap;
                    sub_texture->min_y /= texture->texture.height;
                }
            }

            if (node->dragging)
            {
                F32 drag_x = clamp_f(input->mouse_x - node->bounds.x, 0.0f, node->max_width);
                F32 drag_y = clamp_f(input->mouse_y - node->bounds.y, 0.0f, node->max_height);
                
                if (sub_texture)
                {
                    sub_texture->max_x = (ceil(drag_x / snap) * snap) / node->max_width;
                    sub_texture->max_y = (ceil(drag_y / snap) * snap) / node->max_height;
                }

                if (!global_hot_widget)
                {
                    global_active_widget = node;
                }

                if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
                {
                    node->dragging = false;
                }
            }
        }

        F32 selected_x, selected_y, selected_w, selected_h;
        if (sub_texture)
        {
            selected_x = sub_texture->min_x * node->max_width;
            selected_y = sub_texture->min_y * node->max_height;
            selected_w = sub_texture->max_x * node->max_width - selected_x;
            selected_h = sub_texture->max_y * node->max_height - selected_y;
        }

        node->bg = RECTANGLE(node->bounds.x + selected_x,
                             node->bounds.y + selected_y,
                             selected_w, selected_h);
    }
}

internal void
layout_and_render_ui_node(OpenGLFunctions *gl,
                          PlatformState *input,
                          UINode *node,
                          F32 x, F32 y)
{
    switch (node->type)
    {
        case UI_NODE_TYPE_WINDOW:
        {
            Rectangle title_bounds = get_text_bounds(global_ui_font,
                                                     node->drag_x + PADDING,
                                                     node->drag_y,
                                                     node->max_width,
                                                     node->label);

            node->bounds = RECTANGLE(node->drag_x,
                                     node->drag_y,
                                     title_bounds.w + PADDING * 2,
                                     0.0f);

            node->bg = RECTANGLE(node->drag_x,
                                 node->drag_y + title_bounds.h + PADDING * 2,
                                 title_bounds.w + PADDING * 2,
                                 0.0f);

            node->interactable = RECTANGLE(node->drag_x,
                                           node->drag_y,
                                           title_bounds.w + PADDING * 2,
                                           title_bounds.h + PADDING * 2);

            UINode *child = node->first_child;
            // NOTE(tbt): keep track of where to place the next child
            F32 current_x = node->bg.x + PADDING;
            F32 current_y = node->bg.y + PADDING;
            F32 tallest = 0.0f; // NOTE(tbt): the height of the tallest widget in the current row

            while (child)
            {
                if (child->type == UI_NODE_TYPE_LINE_BREAK ||
                    current_x + child->bounds.w + PADDING >
                    node->drag_x + node->max_width)
                {
                    current_x = node->bg.x + PADDING;
                    current_y += tallest + PADDING;
                    tallest = 0.0f;
                }

                layout_and_render_ui_node(gl,
                                          input,
                                          child,
                                          current_x,
                                          current_y);

                if (child->type != UI_NODE_TYPE_WINDOW)
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

                child = child->next_sibling;
            }

            blur_screen_region(node->bounds, node->sort);

            fill_rectangle(node->bg,
                           BG_COL_1,
                           node->sort,
                           global_ui_projection_matrix);

            fill_rectangle(node->interactable,
                           BG_COL_2,
                           node->sort,
                           global_ui_projection_matrix);

            draw_text(global_ui_font,
                      title_bounds.x,
                      title_bounds.y + global_ui_font->size + PADDING * 2,
                      node->max_width,
                      FG_COL_1,
                      node->label,
                      node->sort,
                      global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_BUTTON:
        {
            Rectangle label_bounds = get_text_bounds(global_ui_font,
                                                     x, y + global_ui_font->size,
                                                     node->max_width,
                                                     node->label);


            node->bounds = RECTANGLE(label_bounds.x,
                                     label_bounds.y,
                                     label_bounds.w + 2 * PADDING,
                                     label_bounds.h + 2 * PADDING);
            node->bg = node->bounds;
            node->interactable = node->bounds;

            if (global_hot_widget == node)
            {
                fill_rectangle(node->bounds,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);

                stroke_rectangle(node->bounds,
                                 FG_COL_1,
                                 STROKE_WIDTH,
                                 node->sort,
                                 global_ui_projection_matrix);

                draw_text(global_ui_font,
                          x + PADDING, y + global_ui_font->size + PADDING,
                          node->max_width,
                          FG_COL_1,
                          node->label,
                          node->sort,
                          global_ui_projection_matrix);
            }
            else if (global_active_widget == node)
            {
                fill_rectangle(node->bounds,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);

                stroke_rectangle(node->bounds,
                                 FG_COL_1,
                                 STROKE_WIDTH,
                                 node->sort,
                                 global_ui_projection_matrix);

                draw_text(global_ui_font,
                          x + PADDING, y + global_ui_font->size + PADDING,
                          node->max_width,
                          FG_COL_1,
                          node->label,
                          node->sort,
                          global_ui_projection_matrix);
            }
            else
            {
                if (node->toggled)
                {
                    fill_rectangle(node->bounds,
                                   FG_COL_1,
                                   node->sort,
                                   global_ui_projection_matrix);

                    draw_text(global_ui_font,
                              x + PADDING, y + global_ui_font->size + PADDING,
                              node->max_width,
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

                    draw_text(global_ui_font,
                              x + PADDING, y + global_ui_font->size + PADDING,
                              node->max_width,
                              FG_COL_1,
                              node->label,
                              node->sort,
                              global_ui_projection_matrix);
                }
            }

            break;
        }
        case UI_NODE_TYPE_DROPDOWN:
        {
            Rectangle label_bounds = get_text_bounds(global_ui_font,
                                                     x, y + global_ui_font->size,
                                                     node->max_width,
                                                     node->label);


            node->bounds = RECTANGLE(label_bounds.x,
                                     label_bounds.y,
                                     label_bounds.w + 2 * PADDING,
                                     label_bounds.h + 2 * PADDING);
            node->interactable = node->bounds;
            node->bg = RECTANGLE(node->bounds.x,
                                 node->bounds.y + node->bounds.h,
                                 node->bounds.w,
                                 0.0f);

            UINode *child = node->first_child;
            F32 current_y = node->bg.y + PADDING;

            while (child)
            {
                if (child->type == UI_NODE_TYPE_WINDOW) { continue; }

                if (node->toggled)
                {
                    child->hidden = false;
                }
                else
                {
                    child->hidden = true;
                    child = child->next_sibling;
                    continue;
                }

                child->sort = node->sort + 2;

                child->max_width = min_f(child->max_width,
                                                node->bounds.w);

                layout_and_render_ui_node(gl,
                                          input,
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

                child = child->next_sibling;
            }

            if (node->toggled)
            {
                node->bg.h += PADDING * 2;

                blur_screen_region(node->bg,
                                   node->sort + 1);

                fill_rectangle(node->bg,
                               BG_COL_1,
                               node->sort + 1,
                               global_ui_projection_matrix);

                if (!point_is_in_region(input->mouse_x,
                                        input->mouse_y,
                                        node->bg) &&
                    !point_is_in_region(input->mouse_x,
                                        input->mouse_y,
                                        node->bounds))
                {
                    node->toggled = false;
                }
            }

            if (global_hot_widget == node)
            {
                fill_rectangle(node->bounds,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);
            }
            else if (global_active_widget == node)
            {
                fill_rectangle(node->bounds,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);
            }

            stroke_rectangle(node->bounds,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            draw_text(global_ui_font,
                      x + PADDING, y + global_ui_font->size + PADDING,
                      node->max_width,
                      FG_COL_1,
                      node->label,
                      node->sort,
                      global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_SLIDER_I:
        {
            F32 thumb_x = ((node->value_i - node->min) /
                           (node->max - node->min)) *
                          (node->max_width - SLIDER_THUMB_SIZE);

            node->bounds = RECTANGLE(x, y,
                                     node->max_width,
                                     SLIDER_THICKNESS);
            node->bg = node->bounds;
            node->interactable = RECTANGLE(x + thumb_x, y,
                                           SLIDER_THUMB_SIZE,
                                           SLIDER_THICKNESS);

            stroke_rectangle(node->bg,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            if (global_hot_widget == node)
            {
                fill_rectangle(node->interactable,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);
            }
            if (node->dragging)
            {
                fill_rectangle(node->interactable,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);
            }

            stroke_rectangle(node->interactable,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_SLIDER_F:
        {
            F32 thumb_x;

            thumb_x = ((node->value_f - node->min) /
                       (node->max - node->min)) *
                      (node->max_width - SLIDER_THUMB_SIZE);

            node->bounds = RECTANGLE(x, y,
                                     node->max_width,
                                     SLIDER_THICKNESS);
            node->bg = node->bounds;
            node->interactable = RECTANGLE(x + thumb_x, y,
                                           SLIDER_THUMB_SIZE,
                                           SLIDER_THICKNESS);

            stroke_rectangle(node->bg,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            if (global_hot_widget == node)
            {
                fill_rectangle(node->interactable,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);
            }
            if (node->dragging)
            {
                fill_rectangle(node->interactable,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);
            }

            stroke_rectangle(node->interactable,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_LABEL:
        {
            Rectangle label_bounds = get_text_bounds(global_ui_font,
                                                     x, y,
                                                     node->max_width,
                                                     node->label);


            node->bounds = RECTANGLE(label_bounds.x,
                                     label_bounds.y +
                                     global_ui_font->size,
                                     label_bounds.w,
                                     label_bounds.h);
            node->bg = node->bounds;
            node->interactable = node->bounds;

            draw_text(global_ui_font,
                      x, y + global_ui_font->size,
                      node->max_width,
                      FG_COL_1,
                      node->label,
                      node->sort,
                      global_ui_projection_matrix);
            break;
        }
        case UI_NODE_TYPE_SCROLL_PANEL:
        {
            F32 thumb_height = SLIDER_THUMB_SIZE;
            F32 overflow = node->value_f - node->max_height;

            if (node->dragging)
            {
                node->scroll = node->drag_y /
                               (node->max_height - thumb_height) *
                               overflow;
            }

            node->scroll = clamp_f(node->scroll, 0.0f, overflow);

            F32 thumb_y = (node->scroll / overflow) *
                          (node->max_height - thumb_height);

            node->bounds = RECTANGLE(x, y, SLIDER_THICKNESS, node->max_height);

            UINode *child = node->first_child;
            // NOTE(tbt): keep track of where to place the next child
            F32 current_x = node->bounds.x + PADDING;
            F32 current_y = node->bounds.y + PADDING;
            // NOTE(tbt): the height of the tallest widget in the current row
            F32 tallest = 0.0f;
            // NOTE(tbt): store the total height of the widgets in the panel
            node->value_f = 0.0f;

            while (child)
            {
                if (child->type == UI_NODE_TYPE_LINE_BREAK ||
                    current_x + child->bounds.w + PADDING - x > node->max_width)
                {
                    current_x = x + PADDING;
                    current_y += tallest + PADDING;
                    tallest = 0.0f;
                }

                if (current_x + child->bounds.w + PADDING - node->bounds.x >
                    node->bounds.w)
                {
                    node->bounds.w = current_x + child->bounds.w + PADDING -
                                     node->bounds.x + SLIDER_THICKNESS;
                }

                if (current_y - node->scroll + child->bounds.h < node->bounds.y
                        ||
                    current_y - node->scroll > node->bounds.y + node->bounds.h)
                {
                    child->hidden = true;
                }
                else
                {
                    child->hidden = false;
                    mask_rectangular_region(node->bounds,
                                            child->sort)
                    {
                        layout_and_render_ui_node(gl,
                                                  input,
                                                  child,
                                                  current_x,
                                                  current_y - node->scroll);
                    }
                }
                
                if (child->type != UI_NODE_TYPE_WINDOW)
                {
                    child->sort = node->sort + 1;

                    current_x += child->bounds.w + PADDING;

                    tallest = max_f(tallest, child->bounds.h);
                }

                child = child->next_sibling;
            }

            node->value_f = current_y + tallest + PADDING * 2 - node->bounds.y;

            node->bg = RECTANGLE(node->bounds.x + node->bounds.w -
                                 SLIDER_THICKNESS,
                                 y,
                                 SLIDER_THICKNESS,
                                 node->bounds.h);

            node->interactable = RECTANGLE(node->bg.x,
                                           y + thumb_y,
                                           SLIDER_THICKNESS,
                                           thumb_height);

            if (global_hot_widget == node)
            {
                fill_rectangle(node->interactable,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);

                stroke_rectangle(node->interactable,
                                 FG_COL_1,
                                 STROKE_WIDTH,
                                 node->sort,
                                 global_ui_projection_matrix);
            }
            else if (global_active_widget == node)
            {
                fill_rectangle(node->interactable,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);

                stroke_rectangle(node->interactable,
                                 FG_COL_1,
                                 STROKE_WIDTH,
                                 node->sort,
                                 global_ui_projection_matrix);
            }
            else
            {
                fill_rectangle(node->interactable,
                                 FG_COL_1,
                                 node->sort,
                                 global_ui_projection_matrix);
            }

            stroke_rectangle(node->bounds,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort + 2,
                             global_ui_projection_matrix);

            stroke_rectangle(node->bg,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_LINE_BREAK:
        {
            // NOTE(tbt): line break nodes don't actually do anything, just act
            // NOTE(tbt): as a hint to the parent when handling layouting

            break;
        }
        case UI_NODE_TYPE_ENTRY:
        {
            node->bounds = RECTANGLE(x, y,
                                     node->max_width,
                                     global_ui_font->size + PADDING);
            node->bg = node->bounds;
            node->interactable = node->bounds;

            if (global_active_widget == node)
            {
                global_keyboard_focus = node;
            }

            if (node->toggled)
            {
                fill_rectangle(node->bounds,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);

                Rectangle text_bounds = get_text_bounds(global_ui_font,
                                                        x + PADDING,
                                                        y + global_ui_font->size,
                                                        node->max_width,
                                                        node->label);

                if (strlen(node->label))
                {
                    fill_rectangle(RECTANGLE(text_bounds.x +
                                             text_bounds.w + CURSOR_THICKNESS,
                                             text_bounds.y,
                                             CURSOR_THICKNESS,
                                             global_ui_font->size),
                                   FG_COL_1,
                                   node->sort,
                                   global_ui_projection_matrix);
                }
                else
                {
                    fill_rectangle(RECTANGLE(x + CURSOR_THICKNESS,
                                             y + PADDING,
                                             CURSOR_THICKNESS,
                                             global_ui_font->size),
                                   FG_COL_1,
                                   node->sort,
                                   global_ui_projection_matrix);
                }
            }
            else
            {
                fill_rectangle(node->bounds,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);
            }

            stroke_rectangle(node->bounds,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            draw_text(global_ui_font,
                      x + PADDING,
                      y + global_ui_font->size,
                      node->max_width,
                      FG_COL_1,
                      node->label,
                      node->sort,
                      global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_SPRITE_PICKER:
        {
            node->bounds = RECTANGLE(x, y, node->max_width, node->max_height);
            node->interactable = node->bounds;

            stroke_rectangle(node->bounds,
                             FG_COL_1,
                             -STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            fill_rectangle(node->bounds,
                           BG_COL_1,
                           node->sort,
                           global_ui_projection_matrix);

            draw_texture(gl,
                         node->bounds,
                         COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                         node->texture,
                         node->sort,
                         global_ui_projection_matrix);

            stroke_rectangle(node->bg,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            break;
        }
        default:
        {
            UINode *child = node->first_child;

            while (child)
            {
                layout_and_render_ui_node(gl, input, child, x, y);
                child = child->next_sibling;
            }
        }
    }
}

#define do_ui(_gl, _input) for (I32 i = (prepare_ui(), 0); !i; (++i, finish_ui((_gl), (_input))))

internal void
prepare_ui(void)
{
    global_widgets_under_mouse = NULL;
    global_is_mouse_over_ui = false;
}

internal void
finish_ui(OpenGLFunctions *gl,
          PlatformState *input)
{
    global_is_mouse_over_ui = global_is_mouse_over_ui              ||
                              (global_widgets_under_mouse != NULL) ||
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

    layout_and_render_ui_node(gl, input, &global_ui_root, 0, 0);
    memset(&global_ui_root, 0, sizeof(global_ui_root));
    global_ui_root.parent = &global_ui_root;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
    {
        global_active_widget = NULL;
    }

    if (input->is_key_pressed[KEY_ESCAPE] ||
        (global_active_widget &&
         global_active_widget->type != UI_NODE_TYPE_ENTRY))
    {
        global_keyboard_focus = NULL;
    }
}

