#include "audio_winmm.h"
#include <mmsystem.h>
#include <math.h>

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

#define SAMPLE_RATE 44100
#define CHANNELS 1

static HWAVEOUT g_hwo = NULL;
static int g_available = 0;

static WAVEFORMATEX g_fmt;
static WAVEHDR g_hdr;
static short *g_pcm = NULL;
static int g_prepared = 0;

int audio_init(void) {
    if (g_available) return 0;
    g_fmt.wFormatTag = WAVE_FORMAT_PCM;
    g_fmt.nChannels = CHANNELS;
    g_fmt.nSamplesPerSec = SAMPLE_RATE;
    g_fmt.nAvgBytesPerSec = SAMPLE_RATE * CHANNELS * sizeof(short);
    g_fmt.nBlockAlign = CHANNELS * sizeof(short);
    g_fmt.wBitsPerSample = 16;
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

static void audio_release_buffer(void) {
    if (!g_hwo) return;
    if (g_prepared) {
        waveOutReset(g_hwo);
        waveOutUnprepareHeader(g_hwo, &g_hdr, sizeof(WAVEHDR));
        g_prepared = 0;
    }
    if (g_pcm) {
        free(g_pcm);
        g_pcm = NULL;
    }
}

void audio_shutdown(void) {
    if (!g_available || !g_hwo) return;
    audio_release_buffer();
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

/* Non-blocking: reset/stop previous tone, write new one, return immediately.
   This keeps music-pad taps responsive and avoids UI stalls. */
void audio_play(const ToneBuffer *b) {
    if (!g_available || !g_hwo || !b || b->n <= 0 || !b->samples) return;

    /* Stop and release previous buffer so we can reuse safely. */
    audio_release_buffer();

    g_pcm = (short *)malloc((size_t)b->n * sizeof(short));
    if (!g_pcm) return;
    for (int i = 0; i < b->n; i++) g_pcm[i] = float_to_pcm(b->samples[i]);

    ZeroMemory(&g_hdr, sizeof(g_hdr));
    g_hdr.lpData = (LPSTR)g_pcm;
    g_hdr.dwBufferLength = (DWORD)(b->n * sizeof(short));
    g_hdr.dwFlags = 0;

    if (waveOutPrepareHeader(g_hwo, &g_hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        free(g_pcm);
        g_pcm = NULL;
        return;
    }
    g_prepared = 1;
    if (waveOutWrite(g_hwo, &g_hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
        audio_release_buffer();
    }
}