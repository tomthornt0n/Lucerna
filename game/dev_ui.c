/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 07 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

typedef struct WidgetState WidgetState;
struct WidgetState
{

    I8 *key;
    WidgetState *next;

    F32 x, y;
    B32 toggled;
    B32 dragging;
    I32 value_i;
    F32 value_f;
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
    UI_NODE_TYPE_TOGGLE,
    UI_NODE_TYPE_SLIDER,
};

typedef struct UINode UINode;
struct UINode
{
    UINode *first_child, *next_sibling;
    I32 type;
    WidgetState *state;

    union
    {
        struct WindowNode
        {
            F32 width, height;
        } window;

        struct ButtonNode
        {
            F32 width;
        } button;

        struct SliderNode
        {
            F32 max;
        } slider;
    };
};

internal UINode global_ui_root = {0};
/* NOTE(tbt): the parent of new nodes */
internal UINode *global_current_ui_root = &global_ui_root;

internal void
begin_window(I8 *name,
             Rectangle bounds)
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
        state->x = bounds.x;
        state->y = bounds.y;
    }
    node->state = state;

    node->window.width = bounds.w;
    node->window.height = bounds.h;

    node->type = UI_NODE_TYPE_WINDOW;
    if (!state->label)
    {
        state->label = arena_allocate(&global_static_memory, strlen(name) + 1);
    }
    strcpy(state->label, name);
}

internal void
end_window(void)
{
    global_current_ui_root = &global_ui_root;
}

#define PADDING 8
#define TITLE_BAR_HEIGHT 24
#define BG_COL_1 COLOUR(0.0f, 0.0f, 0.0f, 0.5f)
#define BG_COL_2 COLOUR(0.0f, 0.0f, 0.0f, 0.8f)
#define FG_COL_1 COLOUR(1.0f, 1.0f, 1.0f, 1.0f)

internal void
process_ui_node(PlatformState *input,
                UINode *node)
{
    UINode *child = node->first_child;
    while (child)
    {
        process_ui_node(input, child);
        child = child->next_sibling;
    }

    switch (node->type)
    {
        case UI_NODE_TYPE_NONE: 
        {
            return;
        }
        case UI_NODE_TYPE_WINDOW:
        {
            Rectangle bg = RECTANGLE(node->state->x,
                                     node->state->y,
                                     node->window.width,
                                     node->window.height);

            Rectangle title_bar = RECTANGLE(node->state->x,
                                            node->state->y,
                                            node->window.width,
                                            TITLE_BAR_HEIGHT);

            if (point_is_in_region(input->mouse_x,
                                   input->mouse_y,
                                   title_bar) &&
                input->is_mouse_button_pressed[MOUSE_BUTTON_1])
            {
                node->state->dragging = true;
            }
            if (node->state->dragging)
            {
                node->state->x = input->mouse_x - 128.0f;
                node->state->y = input->mouse_y + TITLE_BAR_HEIGHT / 2;
                if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1])
                {
                    node->state->dragging = false;
                }
            }

            blur_screen_region(bg, 5);
            ui_fill_rectangle(bg, BG_COL_1);
            ui_fill_rectangle(title_bar, BG_COL_2);
            ui_draw_text(global_ui_font,
                         title_bar.x + PADDING,
                         title_bar.y + TITLE_BAR_HEIGHT - PADDING,
                         node->window.width,
                         FG_COL_1,
                         node->state->label);

            break;
        }
        default:
        {
            fprintf(stderr, "TODO: finish dev ui\n");
            break;
        }
    }
}

internal void
finish_ui(PlatformState *input)
{
    process_ui_node(input, &global_ui_root);
    memset(&global_ui_root, 0, sizeof(global_ui_root));
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

