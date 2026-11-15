#include "audio_winmm.h"
#include <mmsystem.h>
#include <math.h>

#pragma comment(lib, "winmm.lib")

#define SAMPLE_RATE 44100
#define CHANNELS 1

static HWAVEOUT g_hwo = NULL;
static int g_available = 0;
static volatile LONG g_playing = 0;

static WAVEFORMATEX g_fmt;

int audio_init(void) {
    if (g_available) return 0;
    g_fmt.wFormatTag = WAVE_FORMAT_PCM;
    g_fmt.nChannels = CHANNELS;
    g_fmt.nSamplesPerSec = SAMPLE_RATE;
    g_fmt.nAvgBytesPerSec = SAMPLE_RATE * CHANNELS * sizeof(short);
    g_fmt.nBlockAlign = CHANNELS * sizeof(short);
    g_fmt.wBitsPerSample = 16 * CHANNELS;
    g_fmt.cbSize = 0;

    MMRESULT mm = waveOutOpen(&g_hwo, WAVE_MAPPER, &g_fmt, 0, 0, CALLBACK_NULL);
    if (mm != MMSYSERR_NOERROR) {
        g_hwo = NULL;
        g_available = 0;
        return -1;
    }
    g_available = 1;
    return 0;
}

void audio_shutdown(void) {
    if (!g_available || !g_hwo) return;
    waveOutReset(g_hwo);
    waveOutClose(g_hwo);
    g_hwo = NULL;
    g_available = 0;
}

int audio_available(void) {
    return g_available;
}

static short float_to_pcm(float s) {
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    return (short)(s * 32767.0f);
}

void audio_play(const ToneBuffer *b) {
    if (!g_available || !g_hwo || !b || b->n <= 0 || !b->samples) return;
    if (InterlockedExchange(&g_playing, 1)) return; /* one tone at a time */

    /* Convert float [-1,1] to 16-bit PCM. */
    short *pcm = (short *)malloc((size_t)b->n * sizeof(short));
    if (!pcm) { InterlockedExchange(&g_playing, 0); return; }
    for (int i = 0; i < b->n; i++) pcm[i] = float_to_pcm(b->samples[i]);

    WAVEHDR hdr;
    ZeroMemory(&hdr, sizeof(hdr));
    hdr.lpData = (LPSTR)pcm;
    hdr.dwBufferLength = (DWORD)(b->n * sizeof(short));
    hdr.dwFlags = 0;

    if (waveOutPrepareHeader(g_hwo, &hdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
        waveOutWrite(g_hwo, &hdr, sizeof(WAVEHDR));
        /* Wait for playback to finish (bounded). Tones are < 1s. */
        DWORD maxWait = (DWORD)((float)b->n / (float)SAMPLE_RATE * 1000.0f) + 150;
        DWORD elapsed = 0;
        while (elapsed < maxWait) {
            MMTIME mmtime;
            mmtime.wType = TIME_BYTES;
            if (waveOutGetPosition(g_hwo, &mmtime, sizeof(mmtime)) == MMSYSERR_NOERROR) {
                if (mmtime.u.cbOffset >= hdr.dwBufferLength) break;
            }
            Sleep(10);
            elapsed += 10;
        }
        waveOutUnprepareHeader(g_hwo, &hdr, sizeof(WAVEHDR));
    }

    free(pcm);
    InterlockedExchange(&g_playing, 0);
}
