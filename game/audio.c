/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 23 Dec 2020
  License : N/A
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define SOURCE_HAS_FLAG(source, flag) (source->flags & flag)

/* NOTE(tbt): be careful of arguments with side effects */
#define CLAMP(a, x, y) (a < x ? x : a > y ? y : a)

enum
{
    SOURCE_FLAG_NO_FLAGS = 0,

    SOURCE_FLAG_ACTIVE  = 1 << 0,
    SOURCE_FLAG_REWIND  = 1 << 1,
    SOURCE_FLAG_LOOPING = 1 << 2
};

typedef struct AudioSource AudioSource;
struct AudioSource
{
    AudioSource *next;
    U8 *stream;
    U32 stream_size;
    U32 playhead;
    U8  flags;
    F32 l_gain;
    F32 r_gain;

    F32 level;
    F32 pan;
};

AudioSource *global_playing_sources;

internal void
process_source(AudioSource *source,
               I16 *buffer,
               U32 buffer_size)
{
    I32 i;
    I32 mix;
    I16 *source_buffer = (I16 *)(source->stream + source->playhead);

    if (SOURCE_HAS_FLAG(source, SOURCE_FLAG_REWIND))
    {
        source->playhead = 0;
        source->flags &= ~SOURCE_FLAG_REWIND;
    }

    for (i = 0;
         i < (buffer_size >> 1);
         ++i)
    {
        /* left channel */
        mix = (I32)buffer[i];
        mix += source_buffer[i] * source->l_gain;
        buffer[i] = (I16)CLAMP(mix, -32768, 32767);

        ++i;

        /* right channel */
        mix = (I32)buffer[i];
        mix += source_buffer[i] * source->r_gain;
        buffer[i] = (I16)CLAMP(mix, -32768, 32767);

        if ((i << 2) + source->playhead > source->stream_size)
        {
            break;
        }
    }

    source->playhead += buffer_size;

    if (source->playhead >= source->stream_size)
    {
        source->playhead = 0;

        if (!SOURCE_HAS_FLAG(source, SOURCE_FLAG_LOOPING))
        {
            source->flags &= ~SOURCE_FLAG_ACTIVE;
        }
    }
}

void
game_audio_callback(void *buffer,
                    U32 buffer_size)
{
    AudioSource **indirect;

    memset(buffer, 0, buffer_size);

    platform_get_audio_lock();

    indirect = &global_playing_sources;
    while (*indirect)
    {
        process_source(*indirect, buffer, buffer_size);

        if (!SOURCE_HAS_FLAG((*indirect), SOURCE_FLAG_ACTIVE))
        /* NOTE(tbt): Remove source from list if it is not playing */
        {
            *indirect = (*indirect)->next;
        }
        else
        {
            indirect = &((*indirect)->next);
        }
    }
    platform_release_audio_lock();
}

internal void
play_audio_source(AudioSource *source)
{
    platform_get_audio_lock();
    if (!SOURCE_HAS_FLAG(source, SOURCE_FLAG_ACTIVE))
    {
        source->next = global_playing_sources;
        global_playing_sources = source;
        source->flags |= SOURCE_FLAG_ACTIVE;
    }
    platform_release_audio_lock();
}

internal void
pause_audio_source(AudioSource *source)
{
    platform_get_audio_lock();
    source->flags &= ~SOURCE_FLAG_ACTIVE;
    platform_release_audio_lock();
}

internal void
stop_audio_source(AudioSource *source)
{
    platform_get_audio_lock();
    source->flags |= SOURCE_FLAG_REWIND;
    source->flags &= ~SOURCE_FLAG_ACTIVE;
    platform_release_audio_lock();
}

internal void
set_audio_source_looping(AudioSource *source,
                         B32 looping)
{
    platform_get_audio_lock();
    if (looping)
    {
        source->flags |= SOURCE_FLAG_LOOPING;
    }
    else
    {
        source->flags &= ~SOURCE_FLAG_LOOPING;
    }
    platform_release_audio_lock();
}

internal void
recalculate_audio_source_gain(AudioSource *source)
{
    platform_get_audio_lock();
    source->l_gain = (1.0f - source->pan) * source->level;
    source->r_gain = source->pan * source->level;
    platform_release_audio_lock();
}

internal void
set_audio_source_level(AudioSource *source,
                       F32 level)
{
    source->level = level;
    recalculate_audio_source_gain(source);
}

internal void
set_audio_source_pan(AudioSource *source,
                     F32 pan)
{
    source->pan = CLAMP(pan, 0.0f, 1.0f);
    recalculate_audio_source_gain(source);
}

