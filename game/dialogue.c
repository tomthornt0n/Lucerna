typedef struct
{
 F64 start_time;
 S8List *dialogue;
 B32 playing;
} DialogueState;

internal void
play_dialogue(DialogueState *dialogue_state,
              S8List *dialogue)
{
 dialogue_state->playing = true;
 dialogue_state->start_time = global_time;
 dialogue_state->dialogue = dialogue;
}

internal void
do_dialogue(DialogueState *dialogue_state)
{
 const F64 seconds_per_character = 0.2;
 
 if (dialogue_state->playing)
 {
  S8 string_to_draw = dialogue_state->dialogue->string;
  string_to_draw.len = (global_time - dialogue_state->start_time) / seconds_per_character;
  
  if (string_to_draw.len <= dialogue_state->dialogue->string.len)
  {
   ui_draw_text(global_normal_font,
                (global_renderer_window_w >> 1) - 250,
                global_renderer_window_h - 400,
                500,
                WHITE,
                string_to_draw);
  }
  else
  {
   if (dialogue_state->dialogue->next)
   {
    play_dialogue(dialogue_state, dialogue_state->dialogue->next);
   }
   else
   {
    memset(dialogue_state, 0, sizeof(*dialogue_state));
   }
  }
 }
}