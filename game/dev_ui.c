/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 13 Dec 2020
  License : MIT, at end of file
  Notes   : this is full of terrible bugs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define PADDING 8
#define TITLE_BAR_HEIGHT 24
#define BG_COL_1 COLOUR(0.0f, 0.0f, 0.0f, 0.5f)
#define BG_COL_2 COLOUR(0.0f, 0.0f, 0.0f, 0.8f)
#define FG_COL_1 COLOUR(1.0f, 1.0f, 1.0f, 1.0f)

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

typedef struct WidgetState WidgetState;
struct WidgetState
{

    I8 *key;
    U32 ID;
    WidgetState *next;

    Rectangle bounds; /* NOTE(tbt): the total area on the screen taken up by the widget */
    Rectangle interactable; /* NOTE(tbt): the area that can be interacted with to make the widget active */
    Rectangle bg; /* NOTE(tbt): an area that is rendered but not able to be interacted with */
    F32 drag_x, drag_y;
    F32 max_width;
    I32 sort;
    B32 toggled;
    B32 dragging;
    I8 *label;
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

internal WidgetState global_ui_state_dict[UI_HASH_TABLE_SIZE] = {{0}};

internal WidgetState *
new_widget_state_from_string(MemoryArena *memory,
                             I8 *string)
{
    U64 index = ui_hash(string);

    if (global_ui_state_dict[index].key &&
        strcmp(global_ui_state_dict[index].key, string))
        /* NOTE(tbt): hash collision */
    {
        WidgetState *tail;

        WidgetState *widget = arena_allocate(memory, sizeof(*widget));
        widget->key = arena_allocate(memory, strlen(string) + 1);

        strcpy(widget->key, string);

        while (tail->next) { tail = tail->next; }
        tail->next = widget;

        return widget;
    }

    global_ui_state_dict[index].key = arena_allocate(memory,
                                                     strlen(string) + 1);
    strcpy(global_ui_state_dict[index].key, string);

    return global_ui_state_dict + index;
}

internal WidgetState *
widget_state_from_string(I8 *string)
{
    U64 index = ui_hash(string);
    WidgetState *result = global_ui_state_dict + index;

    while (result)
    {
        if (!result->key) { return NULL; }

        if (strcmp(result->key, string))
        {
            result = result->next;
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
};

typedef struct UINode UINode;
struct UINode
{
    UINode *parent, *first_child, *next_sibling;
    I32 type;
    WidgetState *state;
};

WidgetState *global_hot_widget, *global_active_widget;

internal UINode global_ui_root = {0};
/* NOTE(tbt): the parent of new nodes */
internal UINode *global_current_ui_parent = &global_ui_root;

internal void
begin_window(PlatformState *input,
             I8 *name,
             F32 initial_x, F32 initial_y,
             F32 max_w)
{
    WidgetState *state;
    UINode *node;
    static WidgetState *last_active_window = NULL;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    node->next_sibling = global_current_ui_parent->first_child;
    global_current_ui_parent->first_child = node;
    node->parent = global_current_ui_parent;
    global_current_ui_parent= node;

    if (!(state = widget_state_from_string(name)))
    {
        state = new_widget_state_from_string(&global_static_memory, name);
        state->drag_x = initial_x;
        state->drag_y = initial_y;
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(state->label, name);
    }
    node->state = state;

    state->max_width = max_w;
    if (last_active_window == state)
    {
        state->sort = 8;
    }
    else
    {
        state->sort = 6;
    }

    node->type = UI_NODE_TYPE_WINDOW;

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           state->bounds) &&
        !global_hot_widget)
    {
        global_hot_widget = state;

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               state->interactable)        &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
            !global_active_widget)
        {
            state->dragging = true;
            global_active_widget = state;
            last_active_window = state;
        }
    }

    if (state->dragging)
    {
        F32 new_x = input->mouse_x - state->interactable.w / 2;
        F32 new_y = input->mouse_y - state->interactable.h / 2;
        if (new_x > 0 &&
            new_x + state->bg.w < input->window_width)
        {
            state->drag_x = new_x;
        }
        if (new_y > 0 &&
            new_y + state->bg.h + state->interactable.h <
            input->window_height)
        {
            state->drag_y = new_y;
        }

        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1])
        {
            state->dragging = false;
        }
    }
}

internal void
end_window(void)
{
    assert(global_current_ui_parent->type = UI_NODE_TYPE_WINDOW);
    global_current_ui_parent = global_current_ui_parent->parent;
}

#define do_window(_input, _name, _init_x, _init_y, _max_w) for (I32 i = (begin_window((_input), (_name), (_init_x), (_init_y), (_max_w)), 0); !i; (++i, end_window()))

internal B32
do_toggle_button(PlatformState *input,
                 I8 *name,
                 F32 width)
{
    WidgetState *state;
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    node->next_sibling = global_current_ui_parent->first_child;
    global_current_ui_parent->first_child = node;
    node->parent = global_current_ui_parent;

    if (!(state = widget_state_from_string(name)))
    {
        state = new_widget_state_from_string(&global_static_memory, name);
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(state->label, name);
    }
    node->state = state;

    node->type = UI_NODE_TYPE_BUTTON;
    state->max_width = width;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
        global_active_widget == state)
    {
        state->toggled = !state->toggled;
    }

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           state->bg) &&
        !global_active_widget)
    {
        global_hot_widget = state;

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               state->interactable)        &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_1])
        {
            global_active_widget = state;
        }
    }

    return state->toggled;
}

internal B32
do_button(PlatformState *input,
          I8 *name,
          F32 width)
{
    WidgetState *state;
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    node->next_sibling = global_current_ui_parent->first_child;
    global_current_ui_parent->first_child = node;
    node->parent = global_current_ui_parent;

    if (!(state = widget_state_from_string(name)))
    {
        state = new_widget_state_from_string(&global_static_memory, name);
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(state->label, name);
    }
    node->state = state;

    node->type = UI_NODE_TYPE_BUTTON;
    state->max_width = width;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
        global_active_widget == state)
    {
        state->toggled = true;
    }
    else
    {
        state->toggled = false;
    }

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           state->bg) &&
        !global_active_widget)
    {
        global_hot_widget = state;

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               state->interactable)        &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_1])
        {
            global_active_widget = state;
        }
    }

    return state->toggled;
}

internal void
begin_dropdown(PlatformState *input,
               I8 *name,
               F32 width)
{
    WidgetState *state;
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    node->next_sibling = global_current_ui_parent->first_child;
    global_current_ui_parent->first_child = node;
    node->parent = global_current_ui_parent;
    global_current_ui_parent = node;

    if (!(state = widget_state_from_string(name)))
    {
        state = new_widget_state_from_string(&global_static_memory, name);
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(state->label, name);
    }
    node->state = state;

    node->type = UI_NODE_TYPE_DROPDOWN;
    state->max_width = width;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
        global_active_widget == state)
    {
        state->toggled = !state->toggled;
    }

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           state->bounds) &&
        !global_active_widget)
    {
        global_hot_widget = state;

        if (point_is_in_region(input->mouse_x,
                               input->mouse_y,
                               state->interactable)        &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_1])
        {
            global_active_widget = state;
        }
    }
}

internal void
end_dropdown(void)
{
    assert(global_current_ui_parent->type = UI_NODE_TYPE_DROPDOWN);
    global_current_ui_parent = global_current_ui_parent->parent;
}

#define do_dropdown(_input, _name, _width) for (I32 i = (begin_dropdown((_input), (_name), (_width)), 0); !i; (++i, end_dropdown()))

internal void
layout_and_render_ui_node(PlatformState *input,
                          UINode *node,
                          F32 x, F32 y)
{
    switch (node->type)
    {
        case UI_NODE_TYPE_WINDOW:
        {
            Rectangle title_bounds =
                get_text_bounds(global_ui_font,
                                0, 0,
                                node->state->max_width,
                                node->state->label);

            F32 title_bar_height = max_f(TITLE_BAR_HEIGHT, title_bounds.h + PADDING);

            node->state->bounds = RECTANGLE(node->state->drag_x,
                                            node->state->drag_y,
                                            title_bounds.w + PADDING * 2,
                                            0.0f);

            node->state->bg = RECTANGLE(node->state->drag_x,
                                        node->state->drag_y + title_bar_height,
                                        title_bounds.w + PADDING * 2,
                                        0.0f);

            node->state->interactable = RECTANGLE(node->state->drag_x,
                                                  node->state->drag_y,
                                                  title_bounds.w + PADDING * 2,
                                                  title_bar_height);

            UINode *child = node->first_child;
            /* NOTE(tbt): keep track of where to place the next child */
            F32 current_x = node->state->bg.x + PADDING;
            F32 current_y = node->state->bg.y + PADDING;
            F32 max_h = 0.0f; /* NOTE(tbt): the height of the tallest widget in the current row */

            while (child)
            {
                if (current_x + child->state->bounds.w + PADDING >
                    node->state->drag_x + node->state->max_width)
                {
                    current_x = node->state->bg.x + PADDING;
                    current_y += max_h + PADDING;
                    max_h = 0.0f;
                }

                layout_and_render_ui_node(input,
                                          child,
                                          current_x,
                                          current_y);

                if (child->type != UI_NODE_TYPE_WINDOW)
                {
                    child->state->sort = node->state->sort + 1;

                    current_x += child->state->bounds.w + PADDING;

                    if (child->state->bounds.h > max_h)
                    {
                        max_h = child->state->bounds.h;
                    }
                }

                if (current_x - node->state->bounds.x >
                    node->state->bounds.w)
                {
                    node->state->interactable.w =
                        current_x - node->state->interactable.x;

                    node->state->bounds.w =
                        current_x - node->state->bounds.x;

                    node->state->bg.w =
                        current_x - node->state->bg.x;
                }

                if ((current_y + max_h) - node->state->bg.y >
                    node->state->bg.h)
                {
                    node->state->bounds.h =
                        (current_y + max_h) - node->state->bounds.y +
                        2 * PADDING;

                    node->state->bg.h =
                        (current_y + max_h) - node->state->bg.y +
                        2 * PADDING;
                }

                child = child->next_sibling;
            }

            blur_screen_region(node->state->bounds, node->state->sort);

            fill_rectangle(node->state->bg,
                           BG_COL_1,
                           node->state->sort,
                           global_ui_projection_matrix);

            fill_rectangle(node->state->interactable,
                           BG_COL_2,
                           node->state->sort,
                           global_ui_projection_matrix);

            draw_text(global_ui_font,
                      node->state->interactable.x + PADDING,
                      node->state->interactable.y +
                      TITLE_BAR_HEIGHT - PADDING,
                      node->state->max_width,
                      FG_COL_1,
                      node->state->label,
                      node->state->sort,
                      global_ui_projection_matrix);

            break;
        }
        case UI_NODE_TYPE_BUTTON:
        {
            Colour fg_col = FG_COL_1;

            Rectangle label_bounds = get_text_bounds(global_ui_font,
                                                     x, y + global_ui_font->size,
                                                     node->state->max_width,
                                                     node->state->label);


            node->state->bounds = RECTANGLE(label_bounds.x,
                                            label_bounds.y,
                                            label_bounds.w + 2 * PADDING,
                                            label_bounds.h + 2 * PADDING);
            node->state->bg = node->state->bounds;
            node->state->interactable = node->state->bounds;

            if (global_hot_widget == node->state)
            {
                fill_rectangle(node->state->bounds,
                               BG_COL_1,
                               node->state->sort,
                               global_ui_projection_matrix);

                stroke_rectangle(node->state->bounds,
                                 FG_COL_1,
                                 1,
                                 node->state->sort,
                                 global_ui_projection_matrix);

                draw_text(global_ui_font,
                          x + PADDING, y + global_ui_font->size + PADDING,
                          node->state->max_width,
                          FG_COL_1,
                          node->state->label,
                          node->state->sort,
                          global_ui_projection_matrix);
            }
            else
            {
                if (node->state->toggled)
                {
                    fill_rectangle(node->state->bounds,
                                   FG_COL_1,
                                   node->state->sort,
                                   global_ui_projection_matrix);

                    stroke_rectangle(node->state->bounds,
                                     BG_COL_1,
                                     1,
                                     node->state->sort,
                                     global_ui_projection_matrix);

                    draw_text(global_ui_font,
                              x + PADDING, y + global_ui_font->size + PADDING,
                              node->state->max_width,
                              BG_COL_1,
                              node->state->label,
                              node->state->sort,
                              global_ui_projection_matrix);
                }
                else
                {
                    stroke_rectangle(node->state->bounds,
                                     FG_COL_1,
                                     1,
                                     node->state->sort,
                                     global_ui_projection_matrix);

                    draw_text(global_ui_font,
                              x + PADDING, y + global_ui_font->size + PADDING,
                              node->state->max_width,
                              FG_COL_1,
                              node->state->label,
                              node->state->sort,
                              global_ui_projection_matrix);
                }
            }

            break;
        }
        case UI_NODE_TYPE_DROPDOWN:
        {
            Rectangle label_bounds = get_text_bounds(global_ui_font,
                                                     x, y + global_ui_font->size,
                                                     node->state->max_width,
                                                     node->state->label);


            node->state->bounds = RECTANGLE(label_bounds.x,
                                            label_bounds.y,
                                            label_bounds.w + 2 * PADDING,
                                            label_bounds.h + 2 * PADDING);
            node->state->bg = node->state->bounds;
            node->state->interactable = node->state->bounds;

            if (node->state->toggled)
            {
                node->state->bg.y += node->state->interactable.h;
                node->state->bg.h += 0.0f;

                UINode *child = node->first_child;
                /* NOTE(tbt): keep track of where to place the next child */
                F32 current_y = node->state->bg.y + PADDING;

                while (child)
                {
                    if (child->type == UI_NODE_TYPE_WINDOW) { continue; }

                    child->state->sort = node->state->sort + 1;

                    layout_and_render_ui_node(input,
                                              child,
                                              node->state->bg.x + PADDING,
                                              current_y);

                    node->state->bg.h += child->state->bounds.h + PADDING;
                    current_y += child->state->bounds.h + PADDING;

                    if (child->state->toggled)
                    {
                        node->state->toggled = false;
                    }

                    child = child->next_sibling;
                }

                blur_screen_region(node->state->bg,
                                   node->state->sort);
                fill_rectangle(node->state->bg,
                               BG_COL_1,
                               node->state->sort,
                               global_ui_projection_matrix);

                if (!point_is_in_region(input->mouse_x,
                                        input->mouse_y,
                                        node->state->bg) &&
                    !point_is_in_region(input->mouse_x,
                                        input->mouse_y,
                                        node->state->bounds))
                {
                    node->state->toggled = false;
                }

            }

            if (global_hot_widget == node->state)
            {
                fill_rectangle(node->state->bounds,
                               BG_COL_1,
                               node->state->sort,
                               global_ui_projection_matrix);
            }

            stroke_rectangle(node->state->bounds,
                             FG_COL_1,
                             1,
                             node->state->sort,
                             global_ui_projection_matrix);

            draw_text(global_ui_font,
                      x + PADDING, y + global_ui_font->size + PADDING,
                      node->state->max_width,
                      FG_COL_1,
                      node->state->label,
                      node->state->sort,
                      global_ui_projection_matrix);

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
    global_hot_widget = NULL;
}

internal void
finish_ui(PlatformState *input)
{
    layout_and_render_ui_node(input, &global_ui_root, 0, 0);
    memset(&global_ui_root, 0, sizeof(global_ui_root));
    global_ui_root.parent = &global_ui_root;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1])
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

