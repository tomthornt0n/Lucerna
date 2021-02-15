internal void
process_source(AudioSource *source,
               I16 *buffer,
               U32 buffer_size)
{
    I32 i;
    I32 mix;
    I16 *source_buffer = source->buffer + source->playhead;

    if (source->flags & SOURCE_FLAG_REWIND)
    {
        source->playhead = 0;
        source->flags &= ~SOURCE_FLAG_REWIND;
    }

    for (i = 0;
         i < (buffer_size >> 1) &&
         source->playhead < source->buffer_size >> 1;
         ++i)
    {
        // NOTE(tbt): mix left channel
        mix = buffer[i];
        mix += source_buffer[i] * source->l_gain;
        buffer[i] = (I16)clamp_f(mix, -32768, 32767);
        ++source->playhead;

        ++i;

        // NOTE(tbt): mix right channel
        mix = (I32)buffer[i];
        mix += source_buffer[i] * source->r_gain;
        buffer[i] = (I16)clamp_f(mix, -32768, 32767);
        ++source->playhead;
    }

    if (source->playhead >= source->buffer_size >> 1)
    {
        source->playhead = 0;

        if (!(source->flags & SOURCE_FLAG_LOOPING))
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

        if (!(*indirect)->flags & SOURCE_FLAG_ACTIVE)
        // NOTE(tbt): Remove source from list if it is not playing
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
play_audio_source(Asset *source)
{
    if (!source) { return; }

    load_audio(source);
    assert(source->loaded);

    platform_get_audio_lock();
    if (!(source->audio.flags & SOURCE_FLAG_ACTIVE))
    {
        source->audio.next = global_playing_sources;
        global_playing_sources = &source->audio;
        source->audio.flags |= SOURCE_FLAG_ACTIVE;
    }
    platform_release_audio_lock();
}

internal void
pause_audio_source(Asset *source)
{
    if (!source) { return; }

    platform_get_audio_lock();
    source->audio.flags &= ~SOURCE_FLAG_ACTIVE;
    platform_release_audio_lock();
}

internal void
stop_audio_source(Asset *source)
{
    if (!source) { return; }

    platform_get_audio_lock();
    source->audio.flags |= SOURCE_FLAG_REWIND;
    source->audio.flags &= ~SOURCE_FLAG_ACTIVE;
    platform_release_audio_lock();
}

internal void
set_audio_source_looping(Asset *source,
                         B32 looping)
{
    if (!source) { return; }

    platform_get_audio_lock();
    if (looping)
    {
        source->audio.flags |= SOURCE_FLAG_LOOPING;
    }
    else
    {
        source->audio.flags &= ~SOURCE_FLAG_LOOPING;
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
set_audio_source_level(Asset *source,
                       F32 level)
{
    if (!source) { return; }

    source->audio.level = level;
    recalculate_audio_source_gain(&source->audio);
}

internal void
set_audio_source_pan(Asset *source,
                     F32 pan)
{
    if (!source) { return; }

    // NOTE(tbt): clamp between far-left(0.0) and far-right(1.0) -
    //            0.5 is central
    source->audio.pan = clamp_f(pan, 0.0f, 1.0f);
    recalculate_audio_source_gain(&source->audio);
}

