/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 29 Dec 2020
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define PATH_ENTRY_BUFFER_SIZE   32
#define NUMBER_ENTRY_BUFFER_SIZE 8

enum
{
    EDITOR_FLAG_TILE_EDIT       = 1 << 0,
    EDITOR_FLAG_LOAD_LEVEL      = 1 << 1,
    EDITOR_FLAG_NEW_LEVEL       = 1 << 2,
    EDITOR_FLAG_LOAD_TEXTURE    = 1 << 3,
    EDITOR_FLAG_CREATE_TELEPORT = 1 << 4,
};

internal void
editor_proccess_entities(OpenGLFunctions *gl,
                         PlatformState *input,
                         U64 editor_flags)
{
    static GameEntity *active = NULL, *dragging = NULL;

    GameEntity *entity, *previous = NULL;
    for (entity = global_map.entities;
         entity;
         entity = entity->next)
    {
        if (entity->flags & BIT(ENTITY_FLAG_DELETED) &&
            !editor_flags)
        {
            --global_map.entity_count;

            if (previous)
            {
                previous->next = entity->next;
            }
            else
            {
                global_map.entities = entity->next;
            }
        }

        if (entity->flags & BIT(ENTITY_FLAG_RENDER_GRADIENT))
        {
            world_draw_gradient(entity->bounds, entity->gradient);
        }

        if (entity->flags & BIT(ENTITY_FLAG_RENDER_TEXTURE))
        {
            if (!entity->texture) { continue; }

            Colour colour;
            if (entity->flags & BIT(ENTITY_FLAG_COLOURED))
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

            world_draw_sub_texture(gl,
                                   entity->bounds,
                                   colour,
                                   entity->texture,
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
                    if (global_editor_selected_entity == entity)
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
                global_editor_selected_entity = NULL;
            }

            if (active == entity &&
                !input->is_mouse_button_pressed[MOUSE_BUTTON_LEFT])
            {
                    global_editor_selected_entity = entity;
            }

            if (global_editor_selected_entity == entity)
            {
                fill_rectangle(entity->bounds,
                               COLOUR(0.0f, 0.0f, 1.0f, 0.3f),
                               3,
                               global_projection_matrix);

                if (input->is_key_pressed[KEY_DEL])
                {
                    fprintf(stderr, "deleting entity\n");
                    entity->flags |= BIT(ENTITY_FLAG_DELETED);
                    global_editor_selected_entity = NULL;
                }
            }
        }
        else
        {
            global_editor_selected_entity = NULL;
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
}

internal void
do_editor(OpenGLFunctions *gl,
          PlatformState *input)
{
    static U64 editor_flags = 0;

    if (global_map.tilemap)
    {
        render_tiles(gl, editor_flags & EDITOR_FLAG_TILE_EDIT);
        editor_proccess_entities(gl, input, editor_flags);
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
            if (global_map.tilemap)
            {
                if (do_button(input, "create static object", 256.0f))
                {
                    create_static_object(RECTANGLE(global_camera_x,
                                                   global_camera_y,
                                                   64.0f, 64.0f),
                                         TEXTURE_PATH("spritesheet.png"),
                                         ENTIRE_TEXTURE);
                }
                if (do_button(input, "create player", 256.0f))
                {
                    create_player(gl, global_camera_x, global_camera_y);
                }
                if (do_button(input, "create teleport", 256.0f))
                {
                    editor_flags |= EDITOR_FLAG_CREATE_TELEPORT;
                }
            }
            else
            {
                do_label("plmf", "please load a map first.", 256.0f);
            }
        }
    
        do_dropdown(input, "file", 256.0f)
        {
            if (do_button(input, "load map", 256.0f))
            {
                editor_flags |= EDITOR_FLAG_LOAD_LEVEL;
            }

            if (do_button(input, "save", 256.0f))
            {
                save_map();
            }
        }
    }
    
    if (editor_flags & EDITOR_FLAG_TILE_EDIT)
    {
        static Asset *tile_texture = NULL;
        static SubTexture tile_sub_texture = ENTIRE_TEXTURE;
        B32 solid;
        
        global_editor_selected_entity = NULL;
    
        do_window(input, "tile selector", 0.0f, 128.0f, 200.0f)
        {
            if (global_map.tilemap)
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
                    Asset *asset = global_loaded_assets;
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
                        asset = asset->next_loaded;
                    }

                    if (do_button(input, "load new texture", 200.0f))
                    {
                        editor_flags |= EDITOR_FLAG_LOAD_TEXTURE;
                    }
                }
    
                do_line_break();
    
                solid = do_toggle_button(input, "solid", 150.0f);
            }
            else
            {
                do_label("teplam", "please load a map first.", 250.0f);
            }
        }
    
        if (global_map.tilemap)
        {
            Tile *tile_under_cursor;
            if ((tile_under_cursor = get_tile_under_cursor(input)) &&
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
    
    if (global_editor_selected_entity)
    {
        do_window(input, "entity properties", 0.0f, 128.0f, 600.0f)
        {
            I8 entity_size_label[64] = {0};
            snprintf(entity_size_label, 64, "size: (%.1f x %.1f)", global_editor_selected_entity->bounds.w, global_editor_selected_entity->bounds.h);
            do_label("entity size", entity_size_label, 200.0f);
            do_line_break();
            do_slider_f(input, "entity w slider", 64.0f, 1024.0f, 64.0f, 200.0f, &(global_editor_selected_entity->bounds.w));
            do_line_break();
            do_slider_f(input, "entity h slider", 64.0f, 1024.0f, 64.0f, 200.0f, &(global_editor_selected_entity->bounds.h));
    
            if (global_editor_selected_entity->flags & BIT(ENTITY_FLAG_PLAYER_MOVEMENT))
            {
                do_line_break();

                I8 entity_speed_label[64] = {0};
                snprintf(entity_speed_label, 64, "speed: (%.1f)", global_editor_selected_entity->speed);
                do_label("entity speed", entity_speed_label, 200.0f);
                do_line_break();
                do_slider_f(input, "entity speed slider", 0.0f, 32.0f, 1.0f, 200.0f, &(global_editor_selected_entity->speed));
            }
    
            if (global_editor_selected_entity->flags & BIT(ENTITY_FLAG_RENDER_TEXTURE) &&
                !(global_editor_selected_entity->flags & BIT(ENTITY_FLAG_ANIMATED)))
            {
                do_line_break();
                do_label("entity sprite picker label", "texture:", 100.0f);
                do_line_break();

                if (global_editor_selected_entity->texture &&
                    global_editor_selected_entity->sub_texture)
                {
                    do_sprite_picker(input,
                                     "entity sprite picker",
                                     global_editor_selected_entity->texture,
                                     256.0f,
                                     16.0f,
                                     global_editor_selected_entity->sub_texture);
                }

                do_line_break();

                do_dropdown(input, "texture asset", 200.0f)
                {
                    Asset *asset = global_loaded_assets;
                    while (asset)
                    {
                        if (asset->type == ASSET_TYPE_TEXTURE &&
                            asset->loaded)
                        {
                            if (do_button(input, asset->path, 200.0f))
                            {
                                global_editor_selected_entity->texture = asset;
                            }
                        }
                        asset = asset->next_loaded;
                    }

                    if (do_button(input, "load new texture", 200.0f))
                    {
                        editor_flags |= EDITOR_FLAG_LOAD_TEXTURE;
                    }
                }
            }

            if (global_editor_selected_entity->flags & BIT(ENTITY_FLAG_TELEPORT_PLAYER))
            {
                do_line_break();
                Gradient teleport_gradients[] = {
                    GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f),
                             COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f)),
                
                    GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f),
                             COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f)),
                
                    GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.0f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f),
                             COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.7f)),
                
                    GRADIENT(COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f),
                             COLOUR(0.0f, 0.0f, 0.0f, 0.7f), COLOUR(0.0f, 0.0f, 0.0f, 0.0f))
                };

                do_dropdown(input, "gradient direction", 200.0f)
                {
                    if (do_button(input, "north", 200.0f)) { global_editor_selected_entity->gradient = teleport_gradients[ORIENT_N]; }
                    if (do_button(input, "east", 200.0f))  { global_editor_selected_entity->gradient = teleport_gradients[ORIENT_E]; }
                    if (do_button(input, "south", 200.0f)) { global_editor_selected_entity->gradient = teleport_gradients[ORIENT_S]; }
                    if (do_button(input, "west", 200.0f))  { global_editor_selected_entity->gradient = teleport_gradients[ORIENT_W]; }
                }
                do_line_break();

                do_label("teleport to: ", "teleport to: ", 150.0f);
                do_label( "teleport destination", global_editor_selected_entity->level_transport, 150.0f);
            }
        }
    }

    static I8 level_path[PATH_ENTRY_BUFFER_SIZE];

    if (editor_flags & EDITOR_FLAG_LOAD_LEVEL)
    {
        do_window(input,
                  "open map",
                  input->window_width / 2,
                  input->window_height / 2,
                  512.0f)
        {
            do_text_entry(input, "map path", level_path, 32);
            
            do_line_break();
            
            if (do_button(input, "open", 100.0f))
            {
                save_map();

                I32 status = load_map(gl, level_path);
                if (status == -1)
                {
                    fprintf(stderr, "could not open file '%s'\n", level_path);
                    fprintf(stderr, "create a new map?\n", level_path);

                    editor_flags &= !EDITOR_FLAG_LOAD_LEVEL;
                    editor_flags |= EDITOR_FLAG_NEW_LEVEL;
                }
                else if (status == 0)
                {
                    fprintf(stderr, "failure while loading map '%s'\n", level_path);
                    editor_flags = 0;
                }
                else
                {
                    global_editor_selected_entity = NULL;
                    editor_flags = 0;
                }
            }

            if (do_button(input, "cancel", 100.0f))
            {
                editor_flags = 0;
            }
        }
    }

    if (editor_flags & EDITOR_FLAG_NEW_LEVEL)
    {
        do_window(input,
                  "new map",
                  input->window_width / 2,
                  input->window_height / 2,
                  512.0f)
        {
            static I8 tilemap_width_str[NUMBER_ENTRY_BUFFER_SIZE];
            static I8 tilemap_height_str[NUMBER_ENTRY_BUFFER_SIZE];
            do_text_entry(input, "map path", level_path, 32);

            do_line_break();

            do_label("tmw", "tilemap width: ", 100.0f);
            do_text_entry(input, "tmw e", tilemap_width_str, 8);
            I32 width = atoi(tilemap_width_str);

            do_line_break();

            do_label("tmh", "tilemap height: ", 100.0f);
            do_text_entry(input, "tmh e", tilemap_height_str, 8);
            I32 height = atoi(tilemap_height_str);

            do_line_break();

            if (do_button(input, "create new map", 200.0f))
            {
                create_new_map(gl, width, height);
                strncpy(global_map.path, level_path, sizeof(global_map.path));
                global_editor_selected_entity = NULL;
                editor_flags = 0;
            }

            if (do_button(input, "cancel", 200.0f))
            {
                editor_flags = 0;
            }
        }
    }

    if (editor_flags & EDITOR_FLAG_LOAD_TEXTURE)
    {
        do_window(input,
                  "load texture",
                  input->window_width / 2,
                  input->window_height / 2,
                  512.0f)
        {
            static I8 path[PATH_ENTRY_BUFFER_SIZE];
            do_text_entry(input, "texture path", path, 32);
            
            do_line_break();

            if (do_button(input, "cancel", 100.0f))
            {
                editor_flags = 0;
            }
            
            if (do_button(input, "open", 100.0f))
            {
                Asset *texture = asset_from_path(path);
                load_texture(gl, texture);
                assert(texture->loaded);

                if (global_editor_selected_entity)
                {
                    global_editor_selected_entity->texture = texture;
                }

                editor_flags = 0;
            }
        }
    }

    if (editor_flags & EDITOR_FLAG_CREATE_TELEPORT)
    {
        do_window(input,
                  "teleport creation",
                  input->window_width / 2,
                  input->window_height / 2,
                  512.0f)
        {
            static I32 orient = 0;
            do_dropdown(input, "orientation", 200.0f)
            {
                if (do_button(input, "north", 200.0f)) { orient = ORIENT_N; }
                if (do_button(input, "east", 200.0f)) { orient = ORIENT_E; }
                if (do_button(input, "south", 200.0f)) { orient = ORIENT_S; }
                if (do_button(input, "west", 200.0f)) { orient = ORIENT_W; }
            }

            do_line_break();

            static I8 path[PATH_ENTRY_BUFFER_SIZE];
            do_text_entry(input, "texture path", path, 32);
            
            do_line_break();

            if (do_button(input, "cancel", 100.0f))
            {
                editor_flags = 0;
            }
            
            if (do_button(input, "create", 100.0f))
            {
                create_teleport(RECTANGLE(global_camera_x,
                                          global_camera_y,
                                          64.0f, 64.0f),
                                 orient, path);

                editor_flags = 0;
            }
        }
    }
}

