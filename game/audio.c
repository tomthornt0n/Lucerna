
// TODO(tbt): store the default volume in the level descriptor instead of as a `#define`
#define DEFAULT_AUDIO_MASTER_LEVEL 1.0f
internal F32 global_audio_master_level = DEFAULT_AUDIO_MASTER_LEVEL;

internal void
_process_audio_source(AudioSource *source,
                      I16 *buffer,
                      U32 buffer_size)
{
 I32 mix;
 
 if (source->rewind)
 {
  source->playhead = 0;
  source->rewind = false;
 }
 
 if (NULL != source->buffer)
 {
  I32 samples_to_process = min_i(buffer_size >> 1,
                                 (source->buffer_size >> 1) - source->playhead);
  
  for (I32 i = 0;
       i < samples_to_process;
       ++i)
  {
   // NOTE(tbt): mix left channel
   mix = buffer[i];
   mix += source->buffer[source->playhead + i] * source->l_gain * global_audio_master_level;
   buffer[i] = (I16)clamp_f(mix, -32768, 32767);
   
   ++i;
   
   // NOTE(tbt): mix right channel
   mix = (I32)buffer[i];
   mix += source->buffer[source->playhead + i] * source->r_gain * global_audio_master_level;
   buffer[i] = (I16)clamp_f(mix, -32768, 32767);
  }
  
  source->playhead += samples_to_process;
  
  if (source->playhead >= source->buffer_size >> 1)
  {
   source->playhead = 0;
   
   if (!source->is_looping)
   {
    source->is_active = false;
   }
  }
 }
 else
 {
  source->is_active = false;
 }
}

internal void
_recalculate_audio_source_gain(AudioSource *source)
{
 platform_audio_critical_section
 {
  F32 pan = (source->pan + 1.0f) * 0.5f;
  source->l_gain = (1.0f - pan) * source->level;
  source->r_gain = pan * source->level;
 }
}

void
game_audio_callback(void *buffer,
                    U32 buffer_size)
{
 AudioSource **indirect;
 
 memset(buffer, 0, buffer_size);
 
 platform_audio_critical_section
 {
  indirect = &global_playing_audio_sources;
  while (*indirect)
  {
   if ((*indirect)->is_active)
   {
    _process_audio_source(*indirect, buffer, buffer_size);
   }
   
   if (!(*indirect)->is_active)
   {
    *indirect = (*indirect)->next;
   }
   else
   {
    indirect = &((*indirect)->next);
   }
  }
 }
}

//~

internal void
play_audio_source(AudioSource *source)
{
 if (!source) { return; }
 
 
 platform_audio_critical_section
 {
  if (!source->is_active)
  {
   source->next = global_playing_audio_sources;
   global_playing_audio_sources = source;
   source->is_active = true;
  }
 }
}

internal void
pause_audio_source(AudioSource *source)
{
 if (!source) { return; }
 
 platform_audio_critical_section
 {
  source->is_active = false;
 }
}

internal void
stop_audio_source(AudioSource *source)
{
 if (!source) { return; }
 
 platform_audio_critical_section
 {
  source->rewind = true;
  source->is_active = false;
 }
}

internal void
set_audio_source_looping(AudioSource *source,
                         B32 looping)
{
 if (!source) { return; }
 
 platform_audio_critical_section
 {
  source->is_looping = looping;
 }
}

internal void
set_audio_source_level(AudioSource *source,
                       F32 level)
{
 if (!source) { return; }
 
 source->level = level;
 _recalculate_audio_source_gain(source);
}

internal void
set_audio_source_pan(AudioSource *source,
                     F32 pan)
{
 if (!source) { return; }
 
 source->pan = clamp_f(pan, -1.0f, 1.0f);
 _recalculate_audio_source_gain(source);
}

internal void
set_audio_master_level(F32 level)
{
 platform_audio_critical_section
 {
  global_audio_master_level = level;
 }
}