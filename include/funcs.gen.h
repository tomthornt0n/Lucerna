internal void
do_entity_editor_ui(PlatformState *input,
Entity *x)
{
_ui_begin_window(input, s8_literal("gen Entity editor"), s8_literal("entity editor"), 32.0f, 32.0f, 800.0f);
ui_do_line_break();
_ui_begin_dropdown(input, s8_literal("gen flags 18467"), s8_literal("flags"), 150.0f);
ui_do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_teleport 6334"), s8_literal("ENTITY_FLAG_teleport"), &x->flags, ENTITY_FLAG_teleport, 150.0f);
ui_do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_fade_out 26500"), s8_literal("ENTITY_FLAG_fade_out"), &x->flags, ENTITY_FLAG_fade_out, 150.0f);
_ui_pop_insertion_point();
ui_do_line_break();
ui_do_label(s8_literal("gen teleport_to_y label 19169"), s8_literal("teleport to y:"), 100.0f);
ui_do_slider_f(input, s8_literal("gen teleport_to_y slider 15724"), 0.000000f, 1080.000000f, 1.000000f, 200.0f, &x->teleport_to_y);
ui_do_line_break();
ui_do_label(s8_literal("gen teleport_to_x label 11478"), s8_literal("teleport to x:"), 100.0f);
ui_do_slider_f(input, s8_literal("gen teleport_to_x slider 29358"), 0.000000f, 1920.000000f, 1.000000f, 200.0f, &x->teleport_to_x);
ui_do_line_break();
ui_do_toggle_button(input, s8_literal("gen teleport_to_default_spawn 26962"), s8_literal("teleport to default spawn"), 150.0f, &x->teleport_to_default_spawn);
ui_do_line_break();
ui_do_label(s8_literal("gen teleport_to_level label 24464"), s8_literal("teleport to level:"), 100.0f);
ui_do_text_entry(input, s8_literal("gen teleport_to_level entry 5705"), x->teleport_to_level, NULL, 64);
ui_do_line_break();
_ui_begin_dropdown(input, s8_literal("gen fade_out_direction 28145"), s8_literal("fade out direction"), 150.0f);
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_w 23281"), s8_literal("FADE_OUT_DIR_w"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_w; }
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_s 16827"), s8_literal("FADE_OUT_DIR_s"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_s; }
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_e 9961"), s8_literal("FADE_OUT_DIR_e"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_e; }
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_n 491"), s8_literal("FADE_OUT_DIR_n"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_n; }
_ui_pop_insertion_point();
ui_do_line_break();
_ui_pop_insertion_point();
}

internal void
serialise_entity(Entity *x,
PlatformFile *file)
{
platform_append_to_file(file, &(x->flags), sizeof(x->flags));
platform_append_to_file(file, &(x->teleport_to_y), sizeof(x->teleport_to_y));
platform_append_to_file(file, &(x->teleport_to_x), sizeof(x->teleport_to_x));
platform_append_to_file(file, &(x->teleport_to_default_spawn), sizeof(x->teleport_to_default_spawn));
platform_append_to_file(file, x->teleport_to_level, 64 * sizeof(x->teleport_to_level[0]));
platform_append_to_file(file, &(x->fade_out_direction), sizeof(x->fade_out_direction));
platform_append_to_file(file, &(x->bounds.x), sizeof(x->bounds.x));
platform_append_to_file(file, &(x->bounds.y), sizeof(x->bounds.y));
platform_append_to_file(file, &(x->bounds.w), sizeof(x->bounds.w));
platform_append_to_file(file, &(x->bounds.h), sizeof(x->bounds.h));
}

internal void
deserialise_entity(Entity *x,
U8 **read_pointer)
{
x->flags = *((U64 *)(*read_pointer));
*read_pointer += sizeof(x->flags);
x->teleport_to_y = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_to_y);
x->teleport_to_x = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_to_x);
x->teleport_to_default_spawn = *((B32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_to_default_spawn);
memcpy(x->teleport_to_level, *read_pointer, 64 * sizeof(x->teleport_to_level[0]));
*read_pointer += 64 * sizeof(x->teleport_to_level[0]);x->fade_out_direction = *((I32 *)(*read_pointer));
*read_pointer += sizeof(x->fade_out_direction);
x->bounds.x = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
x->bounds.y = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
x->bounds.w = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
x->bounds.h = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
}

