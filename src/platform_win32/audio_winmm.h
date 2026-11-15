#ifndef AUDIO_WINMM_H
#define AUDIO_WINMM_H

#include <windows.h>
#include "core/audio_synth.h"

/* Initialize the waveOut device. Call once at startup. Returns 0 on success. */
int audio_init(void);

/* Play a synthesized tone buffer (blocking briefly until it finishes, so cues
   don't overlap). No-op if audio device unavailable or muted. */
void audio_play(const ToneBuffer *b);

/* Shut down the waveOut device. Call at shutdown. */
void audio_shutdown(void);

/* Whether audio is currently available/enabled. */
int audio_available(void);

#endif
