#ifndef TTS_SAPI_H
#define TTS_SAPI_H

#include <stdbool.h>

/* Initialize COM and SAPI voice. Returns 0 on success. */
int tts_init(void);

/* Shutdown COM/SAPI. */
void tts_shutdown(void);

/* Speak text (wide chars). If async=true, returns immediately and queues.
   If async=false, blocks until done. Returns 0 on success. */
int tts_speak(const wchar_t *text, bool async);

/* Stop any current speech immediately. */
void tts_stop(void);

/* Set speaking rate (-10 to 10, 0 = normal). */
void tts_set_rate(int rate);

/* Check if SAPI is available/initialized. */
bool tts_available(void);

#endif