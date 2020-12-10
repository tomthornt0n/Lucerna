/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 10 Dec 2020
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

            if (wrap_width)
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

    Rectangle interactable, bg; /* NOTE(tbt): the region which is interactable,
                                              e.g. the title bar of a window or
                                              the thumb of a slider, and the
                                              overall space of the widget
                                */
    F32 x, y;
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
};

typedef struct UINode UINode;
struct UINode
{
    UINode *first_child, *next_sibling;
    I32 type;
    WidgetState *state;
};

WidgetState *global_hot_widget, *global_active_widget;

internal UINode global_ui_root = {0};
/* NOTE(tbt): the parent of new nodes */
internal UINode *global_current_ui_root = &global_ui_root;

internal void
begin_window(PlatformState *input,
             I8 *name,
             F32 initial_x, F32 initial_y,
             F32 max_w)
{
    WidgetState *state;
    UINode *node;

    assert(global_current_ui_root == &global_ui_root);

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    node->next_sibling = global_current_ui_root->first_child;
    global_current_ui_root->first_child = node;
    global_current_ui_root = node;

    if (!(state = widget_state_from_string(name)))
    {
        state = new_widget_state_from_string(&global_static_memory, name);
        state->x = initial_x;
        state->y = initial_y;
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(state->label, name);
    }
    node->state = state;

    state->max_width = max_w;

    node->type = UI_NODE_TYPE_WINDOW;

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           state->interactable) &&
        !global_hot_widget)
    {
        global_hot_widget = state;

        if (input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
            !global_active_widget)
        {
            state->sort = 7;
            state->dragging = true;
            global_active_widget = state;
        }
    }

    if (state->dragging)
    {
        F32 new_x = input->mouse_x - state->interactable.w / 2;
        F32 new_y = input->mouse_y - TITLE_BAR_HEIGHT / 2;
        if (new_x > 0 &&
            new_x + state->bg.w < input->window_width)
        {
            state->x = new_x;
        }
        if (new_y > 0 &&
            new_y + state->bg.h + state->interactable.h <
            input->window_height)
        {
            state->y = new_y;
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
    global_current_ui_root = &global_ui_root;
}

internal B32
do_toggle_button(PlatformState *input,
                 I8 *name,
                 F32 width)
{
    WidgetState *state;
    UINode *node;

    node = arena_allocate(&global_frame_memory,
                          sizeof(*node));

    node->next_sibling = global_current_ui_root->first_child;
    global_current_ui_root->first_child = node;

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
                           state->interactable))
    {
        if (!global_hot_widget)
        {
            global_hot_widget = state;

            if (input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
                !global_active_widget)
            {
                global_active_widget = state;
            }
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

    node->next_sibling = global_current_ui_root->first_child;
    global_current_ui_root->first_child = node;

    if (!(state = widget_state_from_string(name)))
    {
        state = new_widget_state_from_string(&global_static_memory, name);
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
        strcpy(state->label, name);
    }
    node->state = state;

    node->type = UI_NODE_TYPE_BUTTON;
    state->max_width = width;
    state->toggled = false;

    if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
        global_active_widget == state)
    {
        state->toggled = true;
    }

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           state->interactable))
    {
        if (!global_hot_widget ||
            global_hot_widget->sort < state->sort)
        {
            global_hot_widget = state;

            if (input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
                !global_active_widget)
            {
                global_active_widget = state;
            }
        }
    }

    return state->toggled;
}

/* NOTE(tbt): this function is especially bad */
/* TODO(tbt): improve it */
internal Rectangle
layout_and_render_ui_node(PlatformState *input,
                          UINode *node,
                          F32 x, F32 y)
{
    switch (node->type)
    {
        case UI_NODE_TYPE_WINDOW:
        {
            node->state->bg = RECTANGLE(node->state->x,
                                        node->state->y + TITLE_BAR_HEIGHT,
                                        0.0f, 0.0f);

            node->state->interactable = RECTANGLE(node->state->x,
                                                  node->state->y,
                                                  0.0f,
                                                  TITLE_BAR_HEIGHT);

            if (global_active_widget &&
                global_active_widget != node->state)
            {
                node->state->sort = 5;
            }

            UINode *child = node->first_child;
            F32 current_x = node->state->bg.x;
            F32 current_y = node->state->bg.y;
            F32 max_w = 0.0f;
            F32 max_h = 0.0f;
            F32 total_h;

            while (child)
            {
                F32 child_width = 0.0f;

                if (child->state)
                {
                    child->state->sort = node->state->sort + 1;
                    child_width = child->state->bg.w;
                }

                if (current_x + child_width + PADDING >
                    node->state->x + node->state->max_width)
                {
                    current_x = node->state->bg.x;
                    current_y += max_h + PADDING;
                    max_h = 0.0f;
                }

                Rectangle child_bounds =
                    layout_and_render_ui_node(input,
                                              child,
                                              current_x,
                                              current_y);

                current_x += child_bounds.w + PADDING;

                if (child_bounds.h > max_h)
                {
                    max_h = child_bounds.h;
                }
                if (current_x - node->state->x > max_w)
                {
                    max_w = current_x - node->state->x;
                }

                child = child->next_sibling;
            }

            total_h = current_y - node->state->y + max_h;

            node->state->bg.w = max_w + PADDING;
            node->state->bg.h = total_h;
            node->state->interactable.w = max_w + PADDING;

            F32 min_w = get_text_bounds(global_ui_font,
                                        node->state->interactable.x + PADDING,
                                        node->state->interactable.y +
                                        TITLE_BAR_HEIGHT -
                                        PADDING,
                                        node->state->max_width,
                                        node->state->label).w;

            if (node->state->bg.w < min_w ||
                node->state->interactable.w < min_w)
            {
                node->state->interactable.w = min_w + PADDING * 2;
                node->state->bg.w = min_w + PADDING * 2;
                fprintf(stderr, "bellow min w\n");
            }

            Rectangle bounds = RECTANGLE(node->state->interactable.x,
                                         node->state->interactable.y,
                                         node->state->interactable.w,
                                         node->state->interactable.h +
                                         node->state->bg.h);

            blur_screen_region(bounds, node->state->sort);

            fill_rectangle(node->state->bg,
                           BG_COL_1,
                           node->state->sort,
                           UI_PROJECTION_MATRIX);

            fill_rectangle(node->state->interactable,
                           BG_COL_2,
                           node->state->sort,
                           UI_PROJECTION_MATRIX);

            draw_text(global_ui_font,
                      node->state->interactable.x + PADDING,
                      node->state->interactable.y + TITLE_BAR_HEIGHT - PADDING,
                      node->state->bg.w,
                      FG_COL_1,
                      node->state->label,
                      node->state->sort,
                      UI_PROJECTION_MATRIX);


            return bounds;
        }
        case UI_NODE_TYPE_BUTTON:
        {
            Colour fg_col = FG_COL_1;
            F32 y_shift;

            node->state->interactable = get_text_bounds(global_ui_font,
                                                        x + PADDING, y,
                                                        node->state->max_width,
                                                        node->state->label);
            y_shift = (node->state->interactable.y +
                       node->state->interactable.h) -
                      y - node->state->interactable.h - 
                      PADDING;

            node->state->interactable.y -= y_shift;
            node->state->interactable.w += PADDING * 2;
            node->state->interactable.h += PADDING * 2;

            node->state->bg = node->state->interactable;

            if (node->state->toggled)
            {
                fg_col = BG_COL_1;
            }

            if (global_active_widget == node->state)
            {
                fill_rectangle(node->state->interactable,
                               BG_COL_2,
                               node->state->sort,
                               UI_PROJECTION_MATRIX);
            }
            else if (global_hot_widget == node->state)
            {
                fill_rectangle(node->state->interactable,
                               BG_COL_1,
                               node->state->sort,
                               UI_PROJECTION_MATRIX);
            }
            else if (node->state->toggled)
            {
                fill_rectangle(node->state->interactable,
                               FG_COL_1,
                               node->state->sort,
                               UI_PROJECTION_MATRIX);
            }

            stroke_rectangle(node->state->interactable,
                             fg_col,
                             1,
                             node->state->sort,
                             UI_PROJECTION_MATRIX);

            draw_text(global_ui_font,
                      x + PADDING * 2,
                      y - y_shift + PADDING,
                      node->state->max_width,
                      fg_col,
                      node->state->label,
                      node->state->sort,
                      UI_PROJECTION_MATRIX);

            return node->state->interactable;
        }
        default:
        {
            UINode *child = node->first_child;

            while (child)
            {
                layout_and_render_ui_node(input, child, x, y);
                child = child->next_sibling;
            }

            return RECTANGLE(0.0f, 0.0f, 0.0f, 0.0f);
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

