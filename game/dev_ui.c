/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 17 Dec 2020
  License : MIT, at end of file
  Notes   : terrible, but only for dev tools so just about acceptable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define PADDING 8.0f
#define STROKE_WIDTH 2.0f

#define SCROLL_SPEED 25.0f

#define SLIDER_THICKNESS 18.0f
#define SLIDER_THUMB_SIZE 12.0f

#define BG_COL_1 COLOUR(0.0f, 0.0f, 0.0f, 0.4f)
#define BG_COL_2 COLOUR(0.0f, 0.0f, 0.0f, 0.8f)
#define FG_COL_1 COLOUR(1.0f, 1.0f, 1.0f, 1.0f)

/* TODO(tbt): cache vertices when rendering text so they don't need to be
              recalculated to find the bounds
*/
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

internal Colour
hsl_to_rgb(F32 h,
           F32 s,
           F32 v,
           F32 a)
{
    F64 p, q, t, ff;
    I64 i;

    if (s <= 0.0f)
    {
        return COLOUR(v, v, v, a);
    }

    if (h >= 360.0f) { h = 0.0f; }
    h /= 60.0f;
    i = (I64)h;
    ff = h - i;

    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch (i)
    {
        case 0:
        {
            return COLOUR(v, t, p, a);
        }
        case 1:
        {
            return COLOUR(q, v, p, a);
        }
        case 2:
        {
            return COLOUR(p, v, t, a);
        }
        case 3:
        {
            return COLOUR(p, q, v, a);
        }
        case 4:
        {
            return COLOUR(t, p, v, a);
        }
        case 5:
        default:
        {
            return COLOUR(v, p, q, a);
        }
    }
}

typedef struct UINode UINode;
struct UINode
{
    UINode *parent, *first_child, *last_child, *next_sibling;
    U32 type;
    I8 *key;
    UINode *next_hash;
    UINode *next_under_mouse;

    Rectangle bounds; /* NOTE(tbt): the total area on the screen taken up by the widget */
    Rectangle interactable; /* NOTE(tbt): the area that can be interacted with to make the widget active */
    Rectangle bg; /* NOTE(tbt): an area that is rendered but not able to be interacted with */
    F32 drag_x, drag_y;
    F32 max_width;
    F32 max_height;
    I32 sort;
    B32 toggled;
    B32 dragging;
    I8 *label;
    F32 value_f;
    F32 scroll;
    B32 hidden;
    Colour colour;
    U32 buffer_size;
    F32 min, max;
};

#define UI_HASH_TABLE_SIZE (4096)

#define ui_hash(string) hash_string((string), UI_HASH_TABLE_SIZE);

U64
hash_string(I8 *string,
            U32 bounds)
{
    U64 hash = 5381;
    I32 c;

    while (c = *string++)
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash % bounds;
}

internal UINode global_ui_state_dict[UI_HASH_TABLE_SIZE] = {{0}};

internal UINode *
new_widget_state_from_string(MemoryArena *memory,
                             I8 *string)
{
    U64 index = ui_hash(string);

    if (global_ui_state_dict[index].key &&
        strcmp(global_ui_state_dict[index].key, string))
        /* NOTE(tbt): hash collision */
    {
        UINode *tail;

        UINode *widget = arena_allocate(memory, sizeof(*widget));
        widget->key = arena_allocate(memory, strlen(string) + 1);

        strcpy(widget->key, string);

        while (tail->next_hash) { tail = tail->next_hash; }
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
    UI_NODE_TYPE_LABEL,
    UI_NODE_TYPE_LINE_BREAK,
    UI_NODE_TYPE_SCROLL_PANEL,
    UI_NODE_TYPE_COLOUR_PICKER,
    UI_NODE_TYPE_ENTRY,
};

internal UINode *global_hot_widget, *global_active_widget;
internal UINode *global_widgets_under_mouse;
internal B32 global_is_mouse_over_ui;

internal UINode global_ui_root = {0};
/* NOTE(tbt): the parent of new nodes */
internal UINode *global_current_ui_parent = &global_ui_root;

internal void
begin_window(PlatformState *input,
             I8 *name,
             F32 initial_x, F32 initial_y,
             F32 max_w)
{
    UINode *node;
    static UINode *last_active_window = NULL;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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
        node->sort = 15;
    }
    else
    {
        node->sort = 6;
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
        node->drag_x = clamp_f(floor(input->mouse_x - node->interactable.w / 2), 0.0f, global_renderer_window_w - PADDING);
        node->drag_y = clamp_f(floor(input->mouse_y - node->interactable.h / 2), 0.0f, global_renderer_window_h - PADDING);

        if (!global_hot_widget)
        {
            global_hot_widget = node;
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
do_toggle_button(PlatformState *input,
                 I8 *name,
                 F32 width)
{
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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
            node->toggled = !node->toggled;
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
do_button(PlatformState *input,
          I8 *name,
          F32 width)
{
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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

internal F32
do_slider_f(PlatformState *input,
            I8 *name,
            F32 min, F32 max,
            F32 width)
{
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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
            if (input->is_key_pressed[KEY_LEFT_ALT])
            {
                node->drag_x = (input->mouse_x - node->bounds.x) * 0.4f;
            }
            else
            {
                node->drag_x = input->mouse_x - node->bounds.x;
            }

            if (!global_hot_widget)
            {
                global_hot_widget = node;
                global_active_widget = node;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                node->dragging = false;
            }
        }
    }

    return node->value_f;
}

internal void
do_label(I8 *name,
         F32 width)
{
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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
                            node->interactable.h / 2.0f;

            if (!global_hot_widget)
            {
                global_hot_widget = node;
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

internal Colour
do_colour_picker(PlatformState *input,
                 I8 *name,
                 F32 size,
                 F32 hue,
                 F32 alpha)
{
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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

    node->type = UI_NODE_TYPE_COLOUR_PICKER;
    node->max_width = size;
    node->max_height = size;
    node->value_f = hue;
    node->colour.a = alpha;

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
            node->drag_x = input->mouse_x - node->bounds.x;
            node->drag_y = input->mouse_y - node->bounds.y;

            if (!global_hot_widget)
            {
                global_hot_widget = node;
                global_active_widget = node;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                node->dragging = false;
            }
        }
    }

    return node->colour;
}

internal I8 *
do_text_entry(PlatformState *input,
              I8 *name,
              F32 width)
{
    UINode *node;

    static UINode *last_active_widget = NULL;

    /* NOTE(tbt): approximate width of one character on average by taking the
                  width of 'e'
    */
    F32 char_width = get_text_bounds(global_ui_font, 0.0f, 0.0f, 0.0f, "e").w;

    /* NOTE(tbt): allow one character for NULL, one for padding and one for
                  safety
    */
    U32 buffer_size = width / char_width - 3;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

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

    if (node->buffer_size != buffer_size)
    {
        node->label = arena_allocate(&global_static_memory, buffer_size);
    }

    node->buffer_size = buffer_size;

    node->type = UI_NODE_TYPE_ENTRY;
    node->max_width = width;

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

        if (global_active_widget != last_active_widget &&
            global_active_widget != NULL)
        {
            last_active_widget = global_active_widget;
        }

        U32 len = strlen(node->label);

        if (last_active_widget == node)
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

    return node->label;
}

internal void
layout_and_render_ui_node(PlatformState *input,
                          UINode *node,
                          F32 x, F32 y)
{
    switch (node->type)
    {
        case UI_NODE_TYPE_WINDOW:
        {
            Rectangle title_bounds = get_text_bounds(global_ui_font,
                                                     0, 0,
                                                     node->max_width,
                                                     node->label);

            node->bounds = RECTANGLE(node->drag_x,
                                     node->drag_y,
                                     title_bounds.w + PADDING * 2,
                                     0.0f);

            node->bg = RECTANGLE(node->drag_x,
                                 node->drag_y + title_bounds.h + PADDING,
                                 title_bounds.w + PADDING * 2,
                                 0.0f);

            node->interactable = RECTANGLE(node->drag_x,
                                           node->drag_y,
                                           title_bounds.w + PADDING * 2,
                                           title_bounds.h + PADDING);

            UINode *child = node->first_child;
            /* NOTE(tbt): keep track of where to place the next child */
            F32 current_x = node->bg.x + PADDING;
            F32 current_y = node->bg.y + PADDING;
            F32 tallest = 0.0f; /* NOTE(tbt): the height of the tallest widget in the current row */

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

                layout_and_render_ui_node(input,
                                          child,
                                          current_x,
                                          current_y);

                if (child->type != UI_NODE_TYPE_WINDOW)
                {
                    child->sort = node->sort + 1;

                    current_x += child->bounds.w + PADDING;

                    if (child->bounds.h > tallest)
                    {
                        tallest = child->bounds.h;
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
                      node->interactable.x + PADDING,
                      node->interactable.y +
                      global_ui_font->size,
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

                layout_and_render_ui_node(input,
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
        case UI_NODE_TYPE_SLIDER_F:
        {
            F32 thumb_x = clamp_f(x + node->drag_x,
                                  x,
                                  x + node->max_width -
                                  SLIDER_THUMB_SIZE);

            node->bounds = RECTANGLE(x, y,
                                     node->max_width,
                                     SLIDER_THICKNESS);
            node->bg = node->bounds;
            node->interactable = RECTANGLE(thumb_x, y,
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

            node->value_f =
                ((thumb_x - x) / (node->max_width - SLIDER_THUMB_SIZE)) *
                (node->max - node->min) + node->min;

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
            /* NOTE(tbt): keep track of where to place the next child */
            F32 current_x = node->bounds.x + PADDING;
            F32 current_y = node->bounds.y + PADDING;
            /* NOTE(tbt): the height of the tallest widget in the current row */
            F32 tallest = 0.0f;
            /* NOTE(tbt): store the total height of the widgets in the panel */
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
                        layout_and_render_ui_node(input,
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
                             node->sort,
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
            /* NOTE(tbt): line break nodes don't actually do anything, just act
                          as a hint to the parent when handling layouting
            */
            break;
        }
        case UI_NODE_TYPE_COLOUR_PICKER:
        {
            F32 thumb_x = clamp_f(node->drag_x,
                                  0.0f,
                                  node->max_width - SLIDER_THUMB_SIZE);

            F32 thumb_y = clamp_f(node->drag_y,
                                  0.0f,
                                  node->max_width - SLIDER_THUMB_SIZE);

            node->bounds = RECTANGLE(x, y, node->max_width, node->max_width);

            node->bg = node->bounds;
            node->interactable = RECTANGLE(x + thumb_x, y + thumb_y,
                                           SLIDER_THUMB_SIZE,
                                           SLIDER_THUMB_SIZE);

            node->colour = hsl_to_rgb(node->value_f,
                                      thumb_y / (node->max_width -
                                                 SLIDER_THUMB_SIZE),
                                      thumb_x / (node->max_width -
                                                 SLIDER_THUMB_SIZE),
                                      node->colour.a);

            if (node->dragging)
            {
                fill_rectangle(node->bounds,
                               node->colour,
                               node->sort,
                               global_ui_projection_matrix);
            }
            else
            {
                F32 hue = node->value_f;

                Gradient gradient;
                gradient.tl = hsl_to_rgb(hue, 0.0f, 0.0f, 1.0f);
                gradient.tr = hsl_to_rgb(hue, 0.0f, 1.0f, 1.0f);
                gradient.bl = hsl_to_rgb(hue, 1.0f, 0.0f, 1.0f);
                gradient.br = hsl_to_rgb(hue, 1.0f, 1.0f, 1.0f);

                draw_gradient(node->bounds,
                              gradient,
                              node->sort,
                              global_ui_projection_matrix);
            }

            if (global_active_widget == node)
            {
                fill_rectangle(node->interactable,
                               BG_COL_2,
                               node->sort,
                               global_ui_projection_matrix);
            }
            else if (global_hot_widget == node)
            {
                fill_rectangle(node->interactable,
                               BG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);
            }

            stroke_rectangle(node->interactable,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            stroke_rectangle(node->bounds,
                             FG_COL_1,
                             STROKE_WIDTH,
                             node->sort,
                             global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_ENTRY:
        {
            node->bounds = RECTANGLE(x, y,
                                     node->max_width,
                                     global_ui_font->size + PADDING);
            node->bg = node->bounds;
            node->interactable = node->bounds;

            if (node->toggled)
            {
                fill_rectangle(node->bounds,
                               FG_COL_1,
                               node->sort,
                               global_ui_projection_matrix);

                stroke_rectangle(node->bounds,
                                 BG_COL_1,
                                 STROKE_WIDTH,
                                 node->sort,
                                 global_ui_projection_matrix);

                draw_text(global_ui_font,
                          x + PADDING,
                          y + global_ui_font->size,
                          node->max_width,
                          BG_COL_1,
                          node->label,
                          node->sort,
                          global_ui_projection_matrix);
            }
            else
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
                          x + PADDING,
                          y + global_ui_font->size,
                          node->max_width,
                          FG_COL_1,
                          node->label,
                          node->sort,
                          global_ui_projection_matrix);
            }

            break;
        }
        default:
        {
            UINode *child = node->first_child;

            while (child)
            {
                layout_and_render_ui_node(input, child, x, y);
                child = child->next_sibling;
            }
        }
    }
}

internal void
prepare_ui(void)
{
    global_widgets_under_mouse = NULL;
}

internal void
finish_ui(PlatformState *input)
{
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

    layout_and_render_ui_node(input, &global_ui_root, 0, 0);
    memset(&global_ui_root, 0, sizeof(global_ui_root));
    global_ui_root.parent = &global_ui_root;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
    {
        global_active_widget = NULL;
    }
}

/*
MIT License

Copyright (c) 2020 Tom Thornton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

