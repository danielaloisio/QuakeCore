/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2026 M3t4l

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <SDL2/SDL.h>
#include "quakedef.h"

static SDL_AudioDeviceID audio_dev  = 0;
static int               snd_inited = 0;

#define SND_BUFFER_SIZE (1 << 16)

static unsigned char     dma_buffer[SND_BUFFER_SIZE];
static volatile unsigned int dma_read_pos = 0;

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int pos = dma_read_pos & (SND_BUFFER_SIZE - 1);

    if (pos + len <= SND_BUFFER_SIZE) {
        memcpy(stream, dma_buffer + pos, len);
    } else {
        int first = SND_BUFFER_SIZE - pos;
        memcpy(stream, dma_buffer + pos, first);
        memcpy(stream + first, dma_buffer, len - first);
    }
    dma_read_pos += len;
}

qboolean SNDDMA_Init(void)
{
    SDL_AudioSpec want, have;

    snd_inited = 0;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        Con_Printf("SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
        return false;
    }

    memset(&want, 0, sizeof(want));
    want.freq     = 22050;
    want.format   = AUDIO_S16LSB;
    want.channels = 2;
    want.samples  = 512;
    want.callback = sdl_audio_callback;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_dev == 0) {
        Con_Printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    shm = &sn;
    memset((void *)shm, 0, sizeof(*shm));

    shm->splitbuffer      = false;
    shm->samplebits       = (have.format == AUDIO_S16LSB) ? 16 : 8;
    shm->speed            = have.freq;
    shm->channels         = have.channels;
    shm->samples          = SND_BUFFER_SIZE / (shm->samplebits / 8);
    shm->samplepos        = 0;
    shm->submission_chunk = have.samples;
    shm->buffer           = dma_buffer;
    shm->soundalive       = true;
    shm->gamealive        = true;

    SDL_PauseAudioDevice(audio_dev, 0);

    snd_inited = 1;
    Con_Printf("SDL audio: %d Hz, %d ch, %d-bit\n",
               have.freq, have.channels, shm->samplebits);
    return true;
}

int SNDDMA_GetDMAPos(void)
{
    if (!snd_inited) return 0;
    int bytes_per_sample = shm->samplebits / 8;
    shm->samplepos = (int)((dma_read_pos / (unsigned int)bytes_per_sample) % (unsigned int)shm->samples);
    return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
    if (audio_dev) {
        SDL_PauseAudioDevice(audio_dev, 1);
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    snd_inited = 0;
}

void SNDDMA_Submit(void)
{
    (void)0;
}
