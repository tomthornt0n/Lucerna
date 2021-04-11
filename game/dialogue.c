internal F32 DIALOGUE_FADE_OUT_SPEED = 2.0f;

typedef struct
{
 F64 time_playing;
 U32 characters_showing;
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
 dialogue_state->time_playing = 0.0;
 dialogue_state->dialogue = dialogue;
 dialogue_state->x = x;
 dialogue_state->y = y;
 dialogue_state->colour = colour;
}

internal void
do_dialogue(DialogueState *dialogue_state,
            F64 frametime_in_s)
{
 if (dialogue_state->playing)
 {
  dialogue_state->time_playing += frametime_in_s;
  
  dialogue_state->characters_showing = dialogue_state->time_playing / global_current_locale_config.dialogue_seconds_per_character;
  
  S8 string_to_draw;
  string_to_draw.buffer = dialogue_state->dialogue->string.buffer;
  string_to_draw.size = 0;
  
  for(U32 i = 0;
      i < dialogue_state->characters_showing;
      ++i)
  {
   UTF8Consume consume = consume_utf8_from_string(dialogue_state->dialogue->string,
                                                  string_to_draw.size);
   string_to_draw.size += consume.advance;
  }
  
  if (string_to_draw.size > dialogue_state->dialogue->string.size)
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
     dialogue_state->time_playing = 0.0;
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
  
  draw_s8(global_current_locale_config.normal_font,
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