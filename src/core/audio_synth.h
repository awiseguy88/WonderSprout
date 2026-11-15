#ifndef WONDERSPROUT_AUDIO_SYNTH_H
#define WONDERSPROUT_AUDIO_SYNTH_H

typedef struct {
    float *samples;   /* n samples, range [-1, 1] */
    int n;
    int sampleRate;
} ToneBuffer;

void tone_buffer_free(ToneBuffer *b);

/* Sine tone with attack/release envelope so there are no clicks. */
ToneBuffer synth_tone(float freqHz, float durSec, int sampleRate);
/* Simultaneous frequencies (a chord). */
ToneBuffer synth_chord(const float *freqs, int nFreqs, float durSec, int sampleRate);
/* A sequence of (freq, durationSec) notes played back to back. */
ToneBuffer synth_notes(const float *freqs, const float *durSecs, int nNotes, int sampleRate);
/* Short UI clicks / feedback. */
ToneBuffer synth_click(int sampleRate);
ToneBuffer synth_star(int sampleRate);
ToneBuffer synth_error(int sampleRate);

#endif
