internal void
do_level_descriptor_editor_ui(PlatformState *input,
LevelDescriptor *x)
{
ui_do_text_entry(input, s8_literal("gen music_path entry 41"), x->music_path, NULL, 64);
ui_do_label(s8_literal("gen music_path label 18467"), s8_literal("music path:"), 100.0f);
_ui_do_line_break(false);
ui_do_text_entry(input, s8_literal("gen fg_path entry 6334"), x->fg_path, NULL, 64);
ui_do_label(s8_literal("gen fg_path label 26500"), s8_literal("fg path:"), 100.0f);
_ui_do_line_break(false);
ui_do_text_entry(input, s8_literal("gen bg_path entry 19169"), x->bg_path, NULL, 64);
ui_do_label(s8_literal("gen bg_path label 15724"), s8_literal("bg path:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen player_scale slider 11478"), 350.000000f, 750.000000f, 1.000000f, 524.000000f, &x->player_scale);
ui_do_label(s8_literal("gen player_scale label 29358"), s8_literal("player scale:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen floor_gradient slider 26962"), -1.500000f, 1.500000f, 0.010000f, 524.000000f, &x->floor_gradient);
ui_do_label(s8_literal("gen floor_gradient label 24464"), s8_literal("floor gradient:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen exposure slider 5705"), 0.100000f, 3.000000f, 0.020000f, 524.000000f, &x->exposure);
ui_do_label(s8_literal("gen exposure label 28145"), s8_literal("exposure:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen player_spawn_x slider 23281"), 0.000000f, 1920.000000f, 1.000000f, 524.000000f, &x->player_spawn_x);
ui_do_label(s8_literal("gen player_spawn_x label 16827"), s8_literal("player spawn x:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen player_spawn_y slider 9961"), 0.000000f, 1080.000000f, 1.000000f, 524.000000f, &x->player_spawn_y);
ui_do_label(s8_literal("gen player_spawn_y label 491"), s8_literal("player spawn y:"), 100.0f);
_ui_do_line_break(false);
_ui_begin_dropdown(input, s8_literal("gen kind 2995"), s8_literal("kind"), -1.0f);
if (ui_do_button(input, s8_literal("gen LEVEL_KIND_world 11942"), s8_literal("LEVEL_KIND_world"), -1.0f)) { x->kind = LEVEL_KIND_world; }
if (ui_do_button(input, s8_literal("gen LEVEL_KIND_memory 4827"), s8_literal("LEVEL_KIND_memory"), -1.0f)) { x->kind = LEVEL_KIND_memory; }
_ui_pop_insertion_point();
_ui_do_line_break(false);
}

internal void
serialise_level_descriptor(LevelDescriptor *x,
PlatformFile *file)
{
platform_write_to_file_f(file, x->music_path, 64 * sizeof(x->music_path[0]));
platform_write_to_file_f(file, x->fg_path, 64 * sizeof(x->fg_path[0]));
platform_write_to_file_f(file, x->bg_path, 64 * sizeof(x->bg_path[0]));
platform_write_to_file_f(file, &(x->player_scale), sizeof(x->player_scale));
platform_write_to_file_f(file, &(x->floor_gradient), sizeof(x->floor_gradient));
platform_write_to_file_f(file, &(x->exposure), sizeof(x->exposure));
platform_write_to_file_f(file, &(x->player_spawn_x), sizeof(x->player_spawn_x));
platform_write_to_file_f(file, &(x->player_spawn_y), sizeof(x->player_spawn_y));
platform_write_to_file_f(file, &(x->kind), sizeof(x->kind));
}

internal void
deserialise_level_descriptor(LevelDescriptor *x,
U8 **read_pointer)
{
memcpy(x->music_path, *read_pointer, 64 * sizeof(x->music_path[0]));
*read_pointer += 64 * sizeof(x->music_path[0]);
memcpy(x->fg_path, *read_pointer, 64 * sizeof(x->fg_path[0]));
*read_pointer += 64 * sizeof(x->fg_path[0]);
memcpy(x->bg_path, *read_pointer, 64 * sizeof(x->bg_path[0]));
*read_pointer += 64 * sizeof(x->bg_path[0]);
x->player_scale = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->player_scale);
x->floor_gradient = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->floor_gradient);
x->exposure = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->exposure);
x->player_spawn_x = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->player_spawn_x);
x->player_spawn_y = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->player_spawn_y);
x->kind = *((I32 *)(*read_pointer));
*read_pointer += sizeof(x->kind);
}

internal void
do_entity_editor_ui(PlatformState *input,
Entity *x)
{
_ui_begin_dropdown(input, s8_literal("gen flags 5436"), s8_literal("flags"), -1.0f);
ui_do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_trigger_dialogue 32391"), s8_literal("ENTITY_FLAG_trigger_dialogue"), &x->flags, ENTITY_FLAG_trigger_dialogue, -1.0f);
ui_do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_teleport 14604"), s8_literal("ENTITY_FLAG_teleport"), &x->flags, ENTITY_FLAG_teleport, -1.0f);
ui_do_bit_toggle_button(input, s8_literal("gen ENTITY_FLAG_fade_out 3902"), s8_literal("ENTITY_FLAG_fade_out"), &x->flags, ENTITY_FLAG_fade_out, -1.0f);
_ui_pop_insertion_point();
_ui_do_line_break(true);
ui_do_slider_f(input, s8_literal("gen teleport_to_x slider 153"), 0.000000f, 1920.000000f, 1.000000f, 524.000000f, &x->teleport_to_x);
ui_do_label(s8_literal("gen teleport_to_x label 292"), s8_literal("teleport to x:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen teleport_to_y slider 12382"), 0.000000f, 1080.000000f, 1.000000f, 524.000000f, &x->teleport_to_y);
ui_do_label(s8_literal("gen teleport_to_y label 17421"), s8_literal("teleport to y:"), 100.0f);
_ui_do_line_break(false);
ui_do_toggle_button(input, s8_literal("gen teleport_do_not_persist_exposure 18716"), s8_literal("teleport do not persist exposure"), -1.0f, &x->teleport_do_not_persist_exposure);
_ui_do_line_break(false);
ui_do_toggle_button(input, s8_literal("gen teleport_to_non_default_spawn 19718"), s8_literal("teleport to non default spawn"), -1.0f, &x->teleport_to_non_default_spawn);
_ui_do_line_break(false);
ui_do_text_entry(input, s8_literal("gen teleport_to_level entry 19895"), x->teleport_to_level, NULL, 64);
ui_do_label(s8_literal("gen teleport_to_level label 5447"), s8_literal("teleport to level:"), 100.0f);
_ui_do_line_break(true);
_ui_begin_dropdown(input, s8_literal("gen fade_out_direction 21726"), s8_literal("fade out direction"), -1.0f);
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_w 14771"), s8_literal("FADE_OUT_DIR_w"), -1.0f)) { x->fade_out_direction = FADE_OUT_DIR_w; }
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_s 11538"), s8_literal("FADE_OUT_DIR_s"), -1.0f)) { x->fade_out_direction = FADE_OUT_DIR_s; }
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_e 1869"), s8_literal("FADE_OUT_DIR_e"), -1.0f)) { x->fade_out_direction = FADE_OUT_DIR_e; }
if (ui_do_button(input, s8_literal("gen FADE_OUT_DIR_n 19912"), s8_literal("FADE_OUT_DIR_n"), -1.0f)) { x->fade_out_direction = FADE_OUT_DIR_n; }
_ui_pop_insertion_point();
_ui_do_line_break(true);
ui_do_text_entry(input, s8_literal("gen dialogue_path entry 25667"), x->dialogue_path, NULL, 64);
ui_do_label(s8_literal("gen dialogue_path label 26299"), s8_literal("dialogue path:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen dialogue_y slider 17035"), 0.000000f, 1080.000000f, 1.000000f, 524.000000f, &x->dialogue_y);
ui_do_label(s8_literal("gen dialogue_y label 9894"), s8_literal("dialogue y:"), 100.0f);
_ui_do_line_break(false);
ui_do_slider_f(input, s8_literal("gen dialogue_x slider 28703"), 0.000000f, 1920.000000f, 1.000000f, 524.000000f, &x->dialogue_x);
ui_do_label(s8_literal("gen dialogue_x label 23811"), s8_literal("dialogue x:"), 100.0f);
_ui_do_line_break(false);
ui_do_toggle_button(input, s8_literal("gen repeat_dialogue 31322"), s8_literal("repeat dialogue"), -1.0f, &x->repeat_dialogue);
_ui_do_line_break(false);
}

internal void
serialise_entity(Entity *x,
PlatformFile *file)
{
platform_write_to_file_f(file, &(x->flags), sizeof(x->flags));
platform_write_to_file_f(file, &(x->teleport_to_x), sizeof(x->teleport_to_x));
platform_write_to_file_f(file, &(x->teleport_to_y), sizeof(x->teleport_to_y));
platform_write_to_file_f(file, &(x->teleport_do_not_persist_exposure), sizeof(x->teleport_do_not_persist_exposure));
platform_write_to_file_f(file, &(x->teleport_to_non_default_spawn), sizeof(x->teleport_to_non_default_spawn));
platform_write_to_file_f(file, x->teleport_to_level, 64 * sizeof(x->teleport_to_level[0]));
platform_write_to_file_f(file, &(x->fade_out_direction), sizeof(x->fade_out_direction));
platform_write_to_file_f(file, x->dialogue_path, 64 * sizeof(x->dialogue_path[0]));
platform_write_to_file_f(file, &(x->dialogue_y), sizeof(x->dialogue_y));
platform_write_to_file_f(file, &(x->dialogue_x), sizeof(x->dialogue_x));
platform_write_to_file_f(file, &(x->repeat_dialogue), sizeof(x->repeat_dialogue));
platform_write_to_file_f(file, &(x->triggers), sizeof(x->triggers));
platform_write_to_file_f(file, &(x->bounds.x), sizeof(x->bounds.x));
platform_write_to_file_f(file, &(x->bounds.y), sizeof(x->bounds.y));
platform_write_to_file_f(file, &(x->bounds.w), sizeof(x->bounds.w));
platform_write_to_file_f(file, &(x->bounds.h), sizeof(x->bounds.h));
}

internal void
deserialise_entity(Entity *x,
U8 **read_pointer)
{
x->flags = *((U64 *)(*read_pointer));
*read_pointer += sizeof(x->flags);
x->teleport_to_x = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_to_x);
x->teleport_to_y = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_to_y);
x->teleport_do_not_persist_exposure = *((B32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_do_not_persist_exposure);
x->teleport_to_non_default_spawn = *((B32 *)(*read_pointer));
*read_pointer += sizeof(x->teleport_to_non_default_spawn);
memcpy(x->teleport_to_level, *read_pointer, 64 * sizeof(x->teleport_to_level[0]));
*read_pointer += 64 * sizeof(x->teleport_to_level[0]);
x->fade_out_direction = *((I32 *)(*read_pointer));
*read_pointer += sizeof(x->fade_out_direction);
memcpy(x->dialogue_path, *read_pointer, 64 * sizeof(x->dialogue_path[0]));
*read_pointer += 64 * sizeof(x->dialogue_path[0]);
x->dialogue_y = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->dialogue_y);
x->dialogue_x = *((F32 *)(*read_pointer));
*read_pointer += sizeof(x->dialogue_x);
x->repeat_dialogue = *((B32 *)(*read_pointer));
*read_pointer += sizeof(x->repeat_dialogue);
x->triggers = *((U8 *)(*read_pointer));
*read_pointer += sizeof(x->triggers);
x->bounds.x = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
x->bounds.y = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
x->bounds.w = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
x->bounds.h = *((F32 *)(*read_pointer));
*read_pointer += sizeof(F32);
}

