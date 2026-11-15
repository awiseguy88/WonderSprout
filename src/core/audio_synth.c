#include "audio_synth.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void tone_buffer_free(ToneBuffer *b) {
    if (!b) return;
    free(b->samples);
    b->samples = NULL;
    b->n = 0;
    b->sampleRate = 0;
}

/* Fill buf with a sine at freqHz, applying a short attack + release envelope. */
static void fill_sine(ToneBuffer *b, float freqHz, float startSec, float durSec, int sr, float amp) {
    int n0 = (int)(startSec * sr);
    int n1 = n0 + (int)(durSec * sr);
    if (n1 > b->n) n1 = b->n;
    if (n0 < 0) n0 = 0;
    float attack = 0.005f;     /* 5 ms attack */
    float release = 0.05f;     /* 50 ms release */
    for (int i = n0; i < n1; i++) {
        float t = (float)i / (float)sr;
        float s = 0.5f * sinf(2.0f * M_PI * freqHz * t); /* 0.5 peak to leave headroom */
        /* envelope */
        float env = 1.0f;
        float a = attack * sr;
        float r = release * sr;
        float relStart = (float)(n1 - (int)r);
        if (i - n0 < (int)a) env = (float)(i - n0) / a;
        else if (i > (int)relStart) env = (float)(n1 - i) / r;
        if (env < 0) env = 0;
        if (env > 1) env = 1;
        b->samples[i] += amp * s * env;
    }
}

ToneBuffer synth_tone(float freqHz, float durSec, int sampleRate) {
    ToneBuffer b = {NULL, 0, sampleRate};
    if (sampleRate <= 0 || durSec <= 0 || freqHz <= 0) return b;
    int n = (int)(durSec * sampleRate);
    if (n <= 0) n = 1;
    b.samples = (float *)calloc((size_t)n, sizeof(float));
    if (!b.samples) { b.n = 0; return b; }
    b.n = n;
    fill_sine(&b, freqHz, 0.0f, durSec, sampleRate, 1.0f);
    return b;
}

ToneBuffer synth_chord(const float *freqs, int nFreqs, float durSec, int sampleRate) {
    ToneBuffer b = {NULL, 0, sampleRate};
    if (sampleRate <= 0 || durSec <= 0 || nFreqs <= 0 || !freqs) return b;
    int n = (int)(durSec * sampleRate);
    if (n <= 0) n = 1;
    b.samples = (float *)calloc((size_t)n, sizeof(float));
    if (!b.samples) { b.n = 0; return b; }
    b.n = n;
    float amp = 1.0f / (float)nFreqs;
    for (int k = 0; k < nFreqs; k++) {
        fill_sine(&b, freqs[k], 0.0f, durSec, sampleRate, amp);
    }
    return b;
}

ToneBuffer synth_notes(const float *freqs, const float *durSecs, int nNotes, int sampleRate) {
    ToneBuffer b = {NULL, 0, sampleRate};
    if (sampleRate <= 0 || nNotes <= 0 || !freqs || !durSecs) return b;
    int total = 0;
    for (int i = 0; i < nNotes; i++) total += (int)(durSecs[i] * sampleRate);
    if (total <= 0) total = 1;
    b.samples = (float *)calloc((size_t)total, sizeof(float));
    if (!b.samples) { b.n = 0; return b; }
    b.n = total;
    int offset = 0;
    for (int i = 0; i < nNotes; i++) {
        int ni = (int)(durSecs[i] * sampleRate);
        if (ni <= 0) ni = 1;
        ToneBuffer note = synth_tone(freqs[i], durSecs[i], sampleRate);
        if (note.samples) {
            int copy = ni < note.n ? ni : note.n;
            memcpy(b.samples + offset, note.samples, (size_t)copy * sizeof(float));
            tone_buffer_free(&note);
        }
        offset += ni;
    }
    return b;
}

ToneBuffer synth_click(int sampleRate) {
    /* A short, soft percussive click: noise-ish via a decaying tone. */
    return synth_tone(880.0f, 0.04f, sampleRate);
}

ToneBuffer synth_star(int sampleRate) {
    /* Arpeggiated major-ish chord: C E G. */
    float freqs[3] = {523.25f, 659.25f, 783.99f};
    float durs[3] = {0.12f, 0.12f, 0.35f};
    return synth_notes(freqs, durs, 3, sampleRate);
}

ToneBuffer synth_error(int sampleRate) {
    /* Low descending buzz. */
    float freqs[2] = {180.0f, 140.0f};
    float durs[2] = {0.15f, 0.15f};
    return synth_notes(freqs, durs, 2, sampleRate);
}
