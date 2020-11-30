/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Lucerna

  Author  : Tom Thornton
  Updated : 30 Nov 2020
  License : MIT, at end of file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <immintrin.h>

#include "lucerna.h"

#include "arena.c"
#include "audio.c"
#include "asset_manager.c"
#include "math.c"

MemoryArena global_permanent_memory;
internal MemoryArena global_frame_memory;

Font *global_ui_font;

#include "renderer.c"
#include "gui.c"

Texture global_test_texture;
Texture global_father_texture;
Texture global_particle_texture;

AudioSource global_test_source;
AudioSource global_test_source_2;

#define PARTICLE_LIFETIME 200
typedef struct
{
    F32 x, y;
    F32 x_vel, y_vel;
    U32 life;
} Particle;

#define PARTICLE_COUNT 200
typedef struct
{
    Particle particle_pool[PARTICLE_COUNT];
    Texture texture;
    F32 r, g, b;
} ParticleSystem;

void
create_particle(Particle *particle,
                F32 x,
                F32 y)
{
    particle->life = 0;
    particle->x = x;
    particle->y = y;

    particle->x_vel = (F32)((rand() % 10) - 5) / 4.0f;
    particle->y_vel = (F32)((rand() % 10) - 5) / 4.0f;
}

void
initialise_particle_system(ParticleSystem *system,
                           F32 x, F32 y,
                           Colour colour,
                           Texture texture)
{
    system->r = colour.r;
    system->g = colour.g;
    system->b = colour.b;
    system->texture = texture;

    I32 i;
    for (i = 0;
         i < PARTICLE_COUNT;
         ++i)
    {
        create_particle(&(system->particle_pool[i]), x, y);
        system->particle_pool[i].life = rand() % PARTICLE_LIFETIME;
    }
}

void
do_particle_system(ParticleSystem *system,
                   F32 x, F32 y)
{
    I32 i;
    for (i = 0;
         i < PARTICLE_COUNT;
         ++i)
    {
        Particle *particle = &(system->particle_pool[i]);

        if (particle->life > PARTICLE_LIFETIME)
        {
            create_particle(&(system->particle_pool[i]),
                            x, y);
        }

        F32 x = (F32)particle->life / (F32)PARTICLE_LIFETIME;
        F32 y = 3.0f * exp((-((x - 0.3f) * (x - 0.3f))) / 0.04);

        draw_texture(RECTANGLE(particle->x,
                               particle->y,
                               64.0f + 64.0f * y,
                               64.0f + 64.0f * y),
                     COLOUR(system->r,
                            system->g,
                            system->b,
                            y / 3.0f),
                     system->texture);

        particle->x += particle->x_vel;
        particle->y += particle->y_vel;

        ++particle->life;
    }
}

ParticleSystem global_smoke_particles;

void
game_init(OpenGLFunctions *gl)
{
    initialise_arena_with_new_memory(&global_permanent_memory, ONE_GB);
    initialise_arena_with_new_memory(&global_frame_memory, ONE_MB);
    initialise_arena_with_new_memory(&global_asset_memory, ONE_GB);

    initialise_renderer(gl);

    global_test_texture = load_texture(gl, TEXTURE_PATH("bg.png"));
    global_ui_font = load_font(gl, FONT_PATH("mononoki.ttf"), 18);

    global_father_texture = load_texture(gl, TEXTURE_PATH("father.png"));

    global_particle_texture = load_texture(gl, TEXTURE_PATH("particle.png"));

    initialise_ui();

    global_test_source = load_wav(SOUND_PATH("untitled.wav"));
    global_test_source_2 = load_wav(SOUND_PATH("testSound.wav"));
    set_audio_source_looping(&global_test_source_2, true);

    initialise_particle_system(&global_smoke_particles,
                               0.0f, 0.0f,
                               COLOUR(0.463f, 0.906f, 0.596f, 1.0f),
                               global_particle_texture);
}

void
game_update_and_render(OpenGLFunctions *gl,
                       PlatformState *input)
{
    static U32 previous_width = 0, previous_height = 0;
    static F32 camera_x = 0.0f, camera_y = 0.0f;
    static F32 rect_x = 0.0f, rect_y = 0.0f;

    if (input->window_width != previous_width ||
        input->window_height != previous_height)
    {
        set_renderer_window_size(gl,
                                 input->window_width,
                                 input->window_height);
    }

    if (input->is_key_pressed[KEY_A])
    {
        camera_x -= 8.0f;
        set_camera_position(camera_x, camera_y);
    }
    else if (input->is_key_pressed[KEY_D])
    {
        camera_x += 8.0f;
        set_camera_position(camera_x, camera_y);
    }

    if (input->is_key_pressed[KEY_W])
    {
        camera_y -= 8.0f;
        set_camera_position(camera_x, camera_y);
    }
    else if (input->is_key_pressed[KEY_S])
    {
        camera_y += 8.0f;
        set_camera_position(camera_x, camera_y);
    }

    if (input->is_key_pressed[KEY_LEFT])
    {
        rect_x -= 8.0f;
    }
    else if (input->is_key_pressed[KEY_RIGHT])
    {
        rect_x += 8.0f;
    }

    if (input->is_key_pressed[KEY_UP])
    {
        rect_y -= 8.0f;
    }
    else if (input->is_key_pressed[KEY_DOWN])
    {
        rect_y += 8.0f;
    }

    ui_prepare();

    F32 bg_size = (F32)(input->window_width > input->window_height ?
                        input->window_width :
                        input->window_height);

    draw_texture(RECTANGLE(camera_x, camera_y, bg_size, bg_size),
                 COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                 global_test_texture);


    I32 grid_size = 100;
    F32 colour_step = 1.0f / (F32)grid_size;
    I32 x, y;
    for (x = 0;
         x < grid_size;
         ++x)
    {
        for (y = 0;
             y < grid_size;
             ++y)
        {
            fill_rectangle(RECTANGLE(x * 34, y * 34, 32, 32),
                           COLOUR(x * colour_step,
                                  y * colour_step,
                                  1.0f,
                                  1.0f));
        }
    }

    draw_text(global_ui_font,
              -256.0f,
              -256.0f,
              0,
              COLOUR(0.0f, 0.0f, 1.0f, 1.0f),
              "this is some text\nin the world!");


    static B32 dad = false, sound = false, smoke = false;

    if (do_toggle_button(input, "show menu", 96.0f, 64.0f, 0, "menu"))
    {
        dad = do_button(input, "dad", 96.0f, 96.0f, 0, "dad");
        sound = do_toggle_button(input, "sound", 96.0f, 128.0f, 0, "sound");
        smoke = do_toggle_button(input, "smoke", 96.0f, 160.0f, 0, "smoke");
    }

    if (smoke)
    {
        do_particle_system(&global_smoke_particles, rect_x, rect_y);
    }

    if (dad)
    {
        draw_texture(RECTANGLE(rect_x,
                               rect_y,
                               128.0f,
                               128.0f),
                     COLOUR(1.0f, 1.0f, 1.0f, 1.0f),
                     global_father_texture);
    }

    if (sound)
    {
        play_audio_source(&global_test_source);
    }
    else
    {
        stop_audio_source(&global_test_source);
    }

    if (input->is_key_pressed[KEY_SPACE])
    {
        play_audio_source(&global_test_source_2);
    }
    else if (input->is_key_pressed[KEY_RETURN])
    {
        pause_audio_source(&global_test_source_2);
    }

    ui_finish(input);
    process_render_queue(gl);

    previous_width = input->window_width;
    previous_height = input->window_height;
}

void
game_cleanup(OpenGLFunctions *gl)
{
    free(global_permanent_memory.buffer);
    free(global_frame_memory.buffer);
    free(global_asset_memory.buffer);
}

/*
MIT License

Copyright (c) 2020 Tom Thornton

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

