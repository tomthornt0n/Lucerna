internal void
do_entity_editor_ui(PlatformState *input,
Entity *x)
{
begin_window(input, s8_literal("gen Entity 41"), s8_literal("entity editor"), 32.0f, 32.0f, 600.0f);
do_line_break();
begin_dropdown(input, s8_literal("gen flags 18467"), s8_literal("flags"), 150.0f);
do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_teleport 6334"), s8_literal("ENTITY_FLAG_teleport"), &x->flags, ENTITY_FLAG_teleport, 150.0f);
do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_fade_out 26500"), s8_literal("ENTITY_FLAG_fade_out"), &x->flags, ENTITY_FLAG_fade_out, 150.0f);
_ui_pop_insertion_point();
do_line_break();
do_label(s8_literal("gen teleport_to label 19169"), s8_literal("teleport to:"), 100.0f);
do_text_entry(input, s8_literal("gen teleport_to entry 15724"), x->teleport_to, NULL, 64);
do_line_break();
begin_dropdown(input, s8_literal("gen fade_out_direction 11478"), s8_literal("fade out direction"), 150.0f);
if (do_button(input, s8_literal("gen FADE_OUT_DIR_w 29358"), s8_literal("FADE_OUT_DIR_w"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_w; }
if (do_button(input, s8_literal("gen FADE_OUT_DIR_s 26962"), s8_literal("FADE_OUT_DIR_s"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_s; }
if (do_button(input, s8_literal("gen FADE_OUT_DIR_e 24464"), s8_literal("FADE_OUT_DIR_e"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_e; }
if (do_button(input, s8_literal("gen FADE_OUT_DIR_n 5705"), s8_literal("FADE_OUT_DIR_n"), 150.0f)) { x->fade_out_direction = FADE_OUT_DIR_n; }
_ui_pop_insertion_point();
do_line_break();
_ui_pop_insertion_point();
}

internal void
serialise_entity(Entity *x,
PlatformFile *file)
{
platform_append_to_file(file, &(x->flags), sizeof(x->flags));
platform_append_to_file(file, x->teleport_to, 64 * sizeof(x->teleport_to[0]));
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
memcpy(x->teleport_to, *read_pointer, 64 * sizeof(x->teleport_to[0]));
*read_pointer += 64 * sizeof(x->teleport_to[0]);x->fade_out_direction = *((I32 *)(*read_pointer));
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

