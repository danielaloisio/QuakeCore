#include <SDL2/SDL.h>
#include "quakedef.h"

static SDL_AudioDeviceID audio_dev  = 0;
static int               snd_inited = 0;

#define SND_BUFFER_SIZE (1 << 16)

static unsigned char dma_buffer[SND_BUFFER_SIZE];
static volatile int  write_pos = 0;

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int avail = write_pos - shm->samplepos;
    if (avail < 0) avail = 0;
    if (avail > len) avail = len;

    int read_byte = (shm->samplepos * (shm->samplebits / 8)) & (SND_BUFFER_SIZE - 1);

    if (read_byte + avail <= SND_BUFFER_SIZE) {
        memcpy(stream, dma_buffer + read_byte, avail);
    } else {
        int first = SND_BUFFER_SIZE - read_byte;
        memcpy(stream, dma_buffer + read_byte, first);
        memcpy(stream + first, dma_buffer, avail - first);
    }
    if (avail < len)
        memset(stream + avail, 0, len - avail);
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
    /* SDL callback advances consumption; we track write_pos separately.
       The engine needs a play cursor — expose write_pos as proxy. */
    shm->samplepos = (write_pos / (shm->samplebits / 8)) % shm->samples;
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
    if (!snd_inited) return;
    write_pos = paintedtime * shm->channels * (shm->samplebits / 8);
}
