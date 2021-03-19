#define DIALOGUE_FADE_OUT_SPEED 2.0f

typedef struct
{
 F64 start_time;
 B32 playing;
 F32 fade_out_transition;
 
 S8List *dialogue;
 F32 x, y;
 Colour colour;
} DialogueState;

internal void
play_dialogue(DialogueState *dialogue_state,
              S8List *dialogue,
              F32 x, F32 y,
              Colour colour)
{
 dialogue_state->playing = true;
 dialogue_state->start_time = global_time;
 dialogue_state->dialogue = dialogue;
 dialogue_state->x = x;
 dialogue_state->y = y;
 dialogue_state->colour = colour;
}

internal void
do_dialogue(DialogueState *dialogue_state,
            F64 frametime_in_s)
{
 const F64 seconds_per_character = 0.2;
 
 if (dialogue_state->playing)
 {
  S8 string_to_draw = dialogue_state->dialogue->string;
  string_to_draw.len = min_u((global_time - dialogue_state->start_time) / seconds_per_character,
                             dialogue_state->dialogue->string.len);
  
  if (string_to_draw.len >= dialogue_state->dialogue->string.len)
  {
   if (dialogue_state->fade_out_transition < 1.0f)
   {
    dialogue_state->fade_out_transition += DIALOGUE_FADE_OUT_SPEED * frametime_in_s;
   }
   else
   {
    if (dialogue_state->dialogue->next)
    {
     dialogue_state->dialogue = dialogue_state->dialogue->next;
     dialogue_state->fade_out_transition = 1.0f;
     dialogue_state->start_time = global_time;
    }
    else
    {
     memset(dialogue_state, 0, sizeof(*dialogue_state));
     dialogue_state->fade_out_transition = 1.0f;
    }
   }
  }
  else
  {
   dialogue_state->fade_out_transition = 0.0f;
  }
  
  draw_text(global_normal_font,
            dialogue_state->x,
            dialogue_state->y,
            -1,
            colour_literal(dialogue_state->colour.r,
                           dialogue_state->colour.g,
                           dialogue_state->colour.b,
                           dialogue_state->colour.a * (1.0f - dialogue_state->fade_out_transition)),
            string_to_draw,
            UI_SORT_DEPTH - 1,
            global_projection_matrix);
 }
}