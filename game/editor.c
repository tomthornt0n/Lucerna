/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 23 Dec 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

enum
{
    EDITOR_FLAG_TILE_EDIT    = 1 << 0,
    EDITOR_FLAG_LOAD_LEVEL   = 1 << 1,
    EDITOR_FLAG_NEW_LEVEL    = 1 << 2,
    EDITOR_FLAG_LOAD_TEXTURE = 1 << 3,
};

internal GameEntity *
editor_proccess_entities(OpenGLFunctions *gl,
                         PlatformState *input,
                         GameMap *map,
                         U64 editor_flags)
{
    static GameEntity *selected = NULL, *active = NULL, *dragging = NULL;

    GameEntity *entity, *previous = NULL;
    for (entity = map->entities;
         entity;
         entity = entity->next)
    {
        if (entity->flags & ENTITY_FLAG_DELETED &&
            !editor_flags)
        {
            --map->entity_count;

            if (previous)
            {
                previous->next = entity->next;
            }
            else
            {
                map->entities = entity->next;
            }
        }

        if (entity->flags & ENTITY_FLAG_RENDER_TEXTURE)
        {
            Texture texture;

            if (entity->texture)
            {
                if (!entity->texture->loaded)
                {
                    load_texture(gl, entity->texture);
                }
                texture = entity->texture->texture;
            }
            else
            {
                texture = global_flat_colour_texture;
            }

            Colour colour;
            if (entity->flags & ENTITY_FLAG_COLOURED)
            {
                colour = entity->colour;
            }
            else
            {
                colour = COLOUR(1.0f, 1.0f, 1.0f, 1.0f);
            }

            if (editor_flags & EDITOR_FLAG_TILE_EDIT)
            {
                colour.a = 0.4f;
            }

            world_draw_sub_texture(entity->bounds,
                                   colour,
                                   texture,
                                   entity->sub_texture ?
                                   entity->sub_texture[entity->frame] :
                                   ENTIRE_TEXTURE);
        }

        /* NOTE(tbt): editor stuff */
        if (!editor_flags)
        {
            if (point_is_in_region(global_camera_x + input->mouse_x,
                                   global_camera_y + input->mouse_y,
                                   entity->bounds) &&
                !global_is_mouse_over_ui)
            {
                stroke_rectangle(entity->bounds,
                                 COLOUR(1.0f, 1.0f, 1.0f, 0.8f),
                                 2.0f,
                                 3,
                                 global_projection_matrix);

                if (input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
                {
                    if (selected == entity)
                    {
                        dragging = entity;
                    }
                    else
                    {
                        active = entity;
                    }
                }
            }

            if (input->is_key_pressed[KEY_ESCAPE])
            {
                selected = NULL;
            }

            if (active == entity &&
                !input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                    selected = entity;
            }

            if (selected == entity)
            {
                fill_rectangle(entity->bounds,
                               COLOUR(0.0f, 0.0f, 1.0f, 0.3f),
                               3,
                               global_projection_matrix);

                if (input->is_key_pressed[KEY_DEL])
                {
                    fprintf(stderr, "deleting entity\n");
                    entity->flags |= ENTITY_FLAG_DELETED;
                    selected = NULL;
                }
            }
        }
        else
        {
            selected = NULL;
        }

        previous = entity;
    }

    if (!editor_flags)
    {
        if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
        {
            active = NULL;
        }

        if (dragging)
        {
            F32 drag_x = input->mouse_x + global_camera_x;
            F32 drag_y = input->mouse_y + global_camera_y;

            if (input->is_key_pressed[KEY_LEFT_ALT])
            {
                dragging->bounds.x = drag_x;
                dragging->bounds.y = drag_y;
            }
            else
            {
                F32 grid_snap = 4.0f;
                dragging->bounds.x = floor(drag_x / grid_snap) * grid_snap;
                dragging->bounds.y = floor(drag_y / grid_snap) * grid_snap;
            }

            if (!input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT] ||
                global_is_mouse_over_ui)
            {
                dragging = NULL;
            }
        }
    }

    return selected;
}

internal void
do_editor(OpenGLFunctions *gl,
          PlatformState *input)
{
    static U64 editor_flags = 0;

    GameEntity *selected_entity = NULL;
    if (global_map)
    {
        render_tiles(gl,
                     global_map->map,
                     editor_flags & EDITOR_FLAG_TILE_EDIT);

        selected_entity = editor_proccess_entities(gl,
                                                   input,
                                                   &global_map->map,
                                                   editor_flags);
    }

    if (!global_keyboard_focus)
    {
        F32 editor_camera_speed = 8.0f;

        set_camera_position(global_camera_x +
                            input->is_key_pressed[KEY_D] * editor_camera_speed +
                            input->is_key_pressed[KEY_A] * -editor_camera_speed,
                            global_camera_y +
                            input->is_key_pressed[KEY_S] * editor_camera_speed +
                            input->is_key_pressed[KEY_W] * -editor_camera_speed);
    }

    do_window(input, "level editor", 0.0f, 32.0f, 600.0f)
    {
        if (do_toggle_button(input, "edit tiles", 256.0f))
        {
            editor_flags |= EDITOR_FLAG_TILE_EDIT;
        }
        else
        {
            editor_flags &= ~EDITOR_FLAG_TILE_EDIT;
        }
    
        do_dropdown(input, "create entity", 256.0f)
        {
            if (global_map)
            {
                if (do_button(input, "create static object", 256.0f))
                {
                    create_static_object(&global_map->map,
                                         RECTANGLE(global_camera_x,
                                                   global_camera_y,
                                                   64.0f, 64.0f),
                                         TEXTURE_PATH("spritesheet.png"),
                                         ENTIRE_TEXTURE);
                }
                if (do_button(input, "create player", 256.0f))
                {
                    create_player(&global_map->map,
                                  global_camera_x,
                                  global_camera_y);
                }
            }
            else
            {
                do_label("plmf", "please load a map first.", 256.0f);
            }
        }
    
        if (do_button(input, "load map", 256.0f))
        {
            editor_flags |= EDITOR_FLAG_LOAD_LEVEL;
        }
    }
    
    if (editor_flags & EDITOR_FLAG_TILE_EDIT)
    {
        static Asset *tile_texture = NULL;
        static SubTexture tile_sub_texture = ENTIRE_TEXTURE;
        B32 solid;
        
        selected_entity = NULL;
    
        do_window(input, "tile selector", 0.0f, 128.0f, 200.0f)
        {
            do_label("tile sprite picker label", "texture:", 100.0f);
            do_line_break();
            do_sprite_picker(input,
                             "tile sprite picker",
                             tile_texture,
                             256.0f,
                             16.0f,
                             &tile_sub_texture);
            do_line_break();
            do_dropdown(input, "tile texture asset", 200.0f)
            {
                Asset *asset = global_map->map.assets;
                while (asset)
                {
                    if (asset->type == ASSET_TYPE_TEXTURE &&
                        asset->loaded)
                    {
                        if (do_button(input, asset->path, 200.0f))
                        {
                            tile_texture = asset;
                        }
                    }
                    asset = asset->next_in_level;
                }

                if (do_button(input, "load new texture", 200.0f))
                {
                    editor_flags |= EDITOR_FLAG_LOAD_TEXTURE;
                }
            }
    
            do_line_break();
    
            solid = do_toggle_button(input, "solid", 150.0f);
        }
    
        if (global_map)
        {
            Tile *tile_under_cursor;
            if ((tile_under_cursor = get_tile_under_cursor(input, global_map->map)) &&
                !global_is_mouse_over_ui)
            {
                world_stroke_rectangle(RECTANGLE(((I32)(input->mouse_x + global_camera_x) / TILE_SIZE) * TILE_SIZE,
                                                 ((I32)(input->mouse_y + global_camera_y) / TILE_SIZE) * TILE_SIZE,
                                                 TILE_SIZE,
                                                 TILE_SIZE),
                                       COLOUR(0.0f, 0.0f, 0.0f, 0.5f),
                                       2);
    
                if (input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
                {
                    tile_under_cursor->texture = tile_texture;
                    tile_under_cursor->sub_texture = tile_sub_texture;
                    tile_under_cursor->visible = true;
                    tile_under_cursor->solid = solid;
                }
                else if (input->is_mouse_button_pressed[MOUSE_BUTTON_RIGHT])
                {
                    tile_under_cursor->visible = false;
                    tile_under_cursor->solid = false;
                }
            }
        }
    }
    
    if (selected_entity)
    {
        do_window(input, "entity properties", 0.0f, 128.0f, 600.0f)
        {
            do_line_break();
    
            I8 entity_size_label[64] = {0};
            snprintf(entity_size_label, 64, "size: (%.1f x %.1f)", selected_entity->bounds.w, selected_entity->bounds.h);
            do_label("entity size", entity_size_label, 200.0f);
            do_line_break();
            do_slider_f(input, "entity w slider", 64.0f, 1024.0f, 64.0f, 200.0f, &(selected_entity->bounds.w));
            do_line_break();
            do_slider_f(input, "entity h slider", 64.0f, 1024.0f, 64.0f, 200.0f, &(selected_entity->bounds.h));
    
    
            do_line_break();
    
            I8 entity_speed_label[64] = {0};
            snprintf(entity_speed_label, 64, "speed: (%.1f)", selected_entity->speed);
            do_label("entity speed", entity_speed_label, 200.0f);
            do_line_break();
            do_slider_f(input, "entity speed slider", 0.0f, 32.0f, 1.0f, 200.0f, &(selected_entity->speed));
    
            do_line_break();
    
            do_label("entity sprite picker label", "texture:", 100.0f);
            do_line_break();
            do_sprite_picker(input,
                             "entity sprite picker",
                             selected_entity->texture,
                             256.0f,
                             16.0f,
                             selected_entity->sub_texture);
            do_line_break();
            do_dropdown(input, "entity texture asset", 200.0f)
            {
                Asset *asset = global_map->map.assets;
                while (asset)
                {
                    if (asset->type == ASSET_TYPE_TEXTURE &&
                        asset->loaded)
                    {
                        if (do_button(input, asset->path, 200.0f))
                        {
                            selected_entity->texture = asset;
                        }
                    }
                    asset = asset->next_in_level;
                }

                if (do_button(input, "load new texture", 200.0f))
                {
                    editor_flags |= EDITOR_FLAG_LOAD_TEXTURE;
                }
            }
        }
    }

    if (editor_flags & EDITOR_FLAG_LOAD_LEVEL)
    {
        do_ui(input)
        {
            do_window(input,
                      "open map",
                      input->window_width / 2,
                      input->window_height / 2,
                      512.0f)
            {
                I8 *path = do_text_entry(input, "map path", 512.0f);
                
                do_line_break();

                if (do_button(input, "cancel", 100.0f))
                {
                    editor_flags = 0;
                }
                
                if (do_button(input, "open", 100.0f))
                {
                    if (global_map)
                    {
                        save_map(global_map);
                        unload_map(gl, global_map);
                    }

                    global_map = asset_from_path(path);
                    if (!global_map)
                    {
                        global_map = new_asset_from_path(&global_static_memory,
                                                         path);
                    }
                    
                    I32 status = load_map(gl, global_map);
                    if (status == -1)
                    {
                        fprintf(stderr, "could not open file '%s'\n", path);
                        fprintf(stderr, "create a new map?\n", path);

                        editor_flags &= !EDITOR_FLAG_LOAD_LEVEL;
                        editor_flags |= EDITOR_FLAG_NEW_LEVEL;
                    }
                    else if (status == 0)
                    {
                        fprintf(stderr, "failure while loading map '%s'\n", path);
                        editor_flags = 0;
                    }
                    else
                    {
                        selected_entity = NULL;
                        editor_flags = 0;
                    }
                }
            }
        }
    }

    if (editor_flags & EDITOR_FLAG_NEW_LEVEL)
    {
        do_ui(input)
        {
            do_window(input,
                      "new map",
                      input->window_width / 2,
                      input->window_height / 2,
                      512.0f)
            {
                I8 *path = do_text_entry(input, "map path", 512.0f);

                do_line_break();

                do_label("tmw", "tilemap width: ", 100.0f);
                I32 width = atoi(do_text_entry(input, "tmw e", 100.0f));

                do_line_break();

                do_label("tmh", "tilemap height: ", 100.0f);
                I32 height = atoi(do_text_entry(input, "tmh e", 100.0f));

                do_line_break();

                if (do_button(input, "create new map", 200.0f))
                {
                    global_map = asset_from_path(path);
                    if (!global_map)
                    {
                        global_map = new_asset_from_path(&global_static_memory, path);
                    }

                    create_new_map(width, height, &global_map->map);
                    global_map->type = ASSET_TYPE_MAP;
                    global_map->loaded = true;
                    selected_entity = NULL;
                    editor_flags = 0;
                }

                if (do_button(input, "cancel", 200.0f))
                {
                    editor_flags = 0;
                }
            }
        }
    }

    if (editor_flags & EDITOR_FLAG_LOAD_TEXTURE)
    {
        do_ui(input)
        {
            do_window(input,
                      "load texture",
                      input->window_width / 2,
                      input->window_height / 2,
                      512.0f)
            {
                I8 *path = do_text_entry(input, "texture path", 512.0f);
                
                do_line_break();

                if (do_button(input, "cancel", 100.0f))
                {
                    editor_flags = 0;
                }
                
                if (do_button(input, "open", 100.0f))
                {
                    Asset *texture = asset_from_path(path);
                    if (!texture)
                    {
                        texture = new_asset_from_path(&global_static_memory,
                                                      path);
                    }
                    
                    load_texture(gl, texture);

                    texture->next_in_level = global_map->map.assets;
                    global_map->map.assets = texture;

                    if (selected_entity)
                    {
                        selected_entity->texture = texture;
                    }

                    editor_flags = 0;
                }
            }
        }
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

