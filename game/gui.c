/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 30 Nov 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

internal U32
hash_string(I8 *string)
{
    U64 hash = 5381;
    
    I32 c;
    while (c = *string++)
    {
        hash += (hash << 5) + c;
    }

    return hash;
}

internal B32
point_is_in_region(F32 x, F32 y,
                   Rectangle region)
{
    if (x < region.x              ||
        y < region.y              ||
        x > (region.x + region.w) ||
        y > (region.y + region.h))
    {
        return false;
    }
    return true;
}

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

enum
{
    UI_FLAG_DRAW_RECTANGLE = 1 << 0,
    UI_FLAG_DRAW_LABEL     = 1 << 1,
    /* NOTE(tbt): 'toggleable' takes precedence over 'clickable' */
    UI_FLAG_CLICKABLE      = 1 << 2, UI_FLAG_TOGGLEABLE = 1 << 3, 
    UI_FLAG_PADDING        = 1 << 4,
    UI_FLAG_ANIMATE        = 1 << 5,
};

typedef struct
{
    U64 flags;
    I8 *key;
    Rectangle bounds;
    F32 x, y;
    I8 *label;
    B32 pressed;
    F32 scroll_value;
} GuiWidgetState;

/* TODO(tbt): growable hash map */
#define MAX_WIDGETS 96

GuiWidgetState global_ui_state[MAX_WIDGETS];

I32 global_hot_item_id = 0;
I32 global_active_item_id = 0;

internal void
initialise_ui(void)
{
    memset(global_ui_state, 0, sizeof(global_ui_state));
}

internal void
ui_prepare(void)
{
    global_hot_item_id = 0;
}

internal void
ui_finish(PlatformState *input)
{
    if (!(input->is_mouse_button_pressed[MOUSE_BUTTON_1]))
    {
        global_active_item_id = 0;
    }
    else
    {
        if (!global_active_item_id)
        {
            global_active_item_id = -1;
        }
    }
}

#define PADDING             8
#define STROKE_WIDTH        2
#define BACKGROUND_COLOUR   COLOUR(0.11f, 0.145f, 0.2f, 0.3f)
#define FOREGROUND_COLOUR   COLOUR(1.0f, 1.0f, 1.0f, 0.6f)
#define ACCENT_COLOUR       COLOUR(0.329f, 0.416f, 0.6f, 0.9f)

GuiWidgetState *
get_widget_state(I8 *identifier)
{
    GuiWidgetState *widget;

    widget = &(global_ui_state[hash_string(identifier) % MAX_WIDGETS]);

    if (!widget->key)
    {
        widget->key = arena_allocate(&global_permanent_memory,
                                     strlen(identifier) + 1);

        memcpy(widget->key, identifier, strlen(identifier) + 1);
    }
    else
    {
        /* TODO(tbt): deal with hashing collisions */
        assert(0 == strcmp(widget->key, identifier));
    }

    return widget;
}

void
do_ui_widget(PlatformState *input,
             GuiWidgetState *widget)
{

    Colour bg_col = BACKGROUND_COLOUR;
    Colour fg_col = FOREGROUND_COLOUR;

    U32 id = (U32)(widget - global_ui_state);

    if (widget->flags & UI_FLAG_PADDING)
    {
        widget->bounds.x -= PADDING;
        widget->bounds.y -= PADDING;
        widget->bounds.w += PADDING * 2;
        widget->bounds.h += PADDING * 2;
    }

    if (point_is_in_region(input->mouse_x,
                           input->mouse_y,
                           widget->bounds))
    {
        global_hot_item_id = id;

        if (widget->flags & UI_FLAG_TOGGLEABLE ||
            widget->flags & UI_FLAG_CLICKABLE)
        if (global_active_item_id == 0 &&
            input->is_mouse_button_pressed[MOUSE_BUTTON_1])
        {
            global_active_item_id = id;
        }
    }

    if (widget->flags & UI_FLAG_ANIMATE)
    {
        if (global_hot_item_id == id)
        {
            bg_col.a = 0.8;

            if (global_active_item_id == id)
            {
                widget->bounds.x += 2;
                widget->bounds.y += 2;
                widget->bounds.w -= 2 * 2;
                widget->bounds.h -= 2 * 2;

                fg_col = ACCENT_COLOUR;
            }
        }
        else
        {
            if (widget->flags & UI_FLAG_TOGGLEABLE &&
                widget->pressed)
            {
                bg_col = FOREGROUND_COLOUR;
                fg_col = ACCENT_COLOUR;
            }
        }
    }

    if (widget->flags & UI_FLAG_TOGGLEABLE)
    {
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_1] &&
            global_hot_item_id == id                        &&
            global_active_item_id == id)
        {
            widget->pressed = !(widget->pressed);
        }

    }
    else if (widget->flags & UI_FLAG_CLICKABLE)
    {
        if (global_active_item_id == id)
        {
            widget->pressed = true;
        }
        else
        {
            widget->pressed = false;
        }
    }

    if (widget->flags & UI_FLAG_DRAW_RECTANGLE)
    {
        blur_screen_region(widget->bounds);
        ui_fill_rectangle(widget->bounds, bg_col);
        ui_stroke_rectangle(widget->bounds, fg_col, 1);
    }

    if (widget->flags & UI_FLAG_DRAW_LABEL)
    {
        ui_draw_text(global_ui_font,
                     widget->x, widget->y,
                     widget->bounds.w,
                     fg_col,
                     widget->label);
    }
}

internal B32
do_button(PlatformState *input,
          I8 *identifier,
          F32 x, F32 y,
          U32 width,
          I8 *label)
{
    GuiWidgetState *widget = get_widget_state(identifier);

    widget->label = arena_allocate(&global_frame_memory, strlen(label) + 1);
    memcpy(widget->label, label, strlen(label) + 1);

    widget->x = x;
    widget->y = y;
    widget->bounds = get_text_bounds(global_ui_font,
                                     x, y,
                                     width,
                                     label);

    widget->flags = UI_FLAG_DRAW_RECTANGLE |
                    UI_FLAG_PADDING        |
                    UI_FLAG_ANIMATE        |
                    UI_FLAG_DRAW_LABEL     |
                    UI_FLAG_CLICKABLE;

    do_ui_widget(input, widget);

    return widget->pressed;
}

internal B32
do_toggle_button(PlatformState *input,
                 I8 *identifier,
                 F32 x, F32 y,
                 U32 width,
                 I8 *label)
{
    GuiWidgetState *widget = get_widget_state(identifier);

    widget->label = arena_allocate(&global_frame_memory, strlen(label) + 1);
    memcpy(widget->label, label, strlen(label) + 1);

    widget->x = x;
    widget->y = y;
    widget->bounds = get_text_bounds(global_ui_font,
                                     x, y,
                                     width,
                                     label);

    widget->flags = UI_FLAG_DRAW_RECTANGLE |
                    UI_FLAG_ANIMATE        |
                    UI_FLAG_PADDING        |
                    UI_FLAG_DRAW_LABEL     |
                    UI_FLAG_TOGGLEABLE;

    do_ui_widget(input, widget);

    return widget->pressed;
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

