/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 04 Jan 2021
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

internal void
edit_tile_map(PlatformState *input,
              F64 frametime_in_s,
              TileMap *tile_map)
{
    static U32 brush_kind = TILE_KIND_grass;
    static I32 brush_size = 2;
    
    if (is_key_typed(input, '['))
    {
        brush_size -= 1;
    }
    else if (is_key_typed(input, ']'))
    {
        brush_size += 1;
    }
    
    brush_kind = (brush_kind + input->mouse_scroll) % TILE_KIND_MAX;
    
    F32 camera_speed = 256.0f * frametime_in_s;
    set_camera_position(global_camera_x + (input->is_key_pressed[KEY_d] * camera_speed + input->is_key_pressed[KEY_a] * -camera_speed),
                        global_camera_y + (input->is_key_pressed[KEY_s] * camera_speed + input->is_key_pressed[KEY_w] * -camera_speed));
    
    do_window(input, s8_literal("w_tme"), s8_literal("Tile Map Editor"), 32, 32, 512.0f)
    {
        do_line_break();
        
        do_label(s8_literal("l_tmebt"), s8_literal("Tile Kind: "), 128.0f);
        do_dropdown(input, s8_literal("d_tmebt"), s8_from_tile_kind(brush_kind), 256.0f)
        {
            for (I32 tile_kind = TILE_KIND_none;
                 tile_kind < TILE_KIND_MAX;
                 ++tile_kind)
            {
                if (do_button(input, s8_from_tile_kind(tile_kind), s8_from_tile_kind(tile_kind), 256.0f)) { brush_kind = tile_kind; }
            }
        }
        
        do_line_break();
        
        do_label(s8_literal("l_tmebs"), s8_literal("Brush Size:"), 128.0f);
        do_slider_i(input, s8_literal("si_tmebs"), 0, 16, 1, 150.0f, &brush_size);
    }
    
    if (!global_is_mouse_over_ui)
    {
        TileMap preview = *tile_map;
        U64 tile_count = tile_map->width * tile_map->height;
        preview.kinds = arena_allocate(&global_frame_memory, tile_count * sizeof(*preview.kinds));
        preview.tiles = arena_allocate(&global_frame_memory, tile_count * sizeof(*preview.tiles));
        memcpy(preview.kinds, tile_map->kinds, tile_count * sizeof(*tile_map->kinds));
        
        for (I32 x = -brush_size;
             x <= brush_size;
             ++x)
        {
            for (I32 y = -brush_size;
                 y <= brush_size;
                 ++y)
            {
                if (x * x + y * y <= brush_size * brush_size + brush_size)
                {
                    U64 tile_x = (input->mouse_x + global_camera_x) / TILE_SIZE + x;
                    U64 tile_y = (input->mouse_y + global_camera_y) / TILE_SIZE + y;
                    
                    if (tile_x < tile_map->width &&
                        tile_y < tile_map->height)
                    {
                        preview.kinds[tile_x + tile_y * tile_map->width] = brush_kind * !(input->is_mouse_button_pressed[MOUSE_BUTTON_right]);
                    }
                    
                    stroke_rectangle(rectangle_literal(floor((input->mouse_x + global_camera_x) / TILE_SIZE + x) * TILE_SIZE,
                                                       floor((input->mouse_y + global_camera_y) / TILE_SIZE + y) * TILE_SIZE,
                                                       TILE_SIZE, TILE_SIZE),
                                     colour_literal(1.0f, 1.0f, 1.0f, 0.5f),
                                     2.0f,
                                     WORLD_SORT_DEPTH + 1,
                                     global_projection_matrix);
                }
            }
        }
        
        _refresh_auto_tiling(&preview);
        
        render_tile_map(&preview);
        
        if (input->is_mouse_button_pressed[MOUSE_BUTTON_left] ||
            input->is_mouse_button_pressed[MOUSE_BUTTON_right])
        {
            memcpy(tile_map->kinds, preview.kinds, tile_count * sizeof(*preview.kinds));
            memcpy(tile_map->tiles, preview.tiles, tile_count * sizeof(*preview.tiles));
        }
    }
    else
    {
        render_tile_map(tile_map);
    }
}

