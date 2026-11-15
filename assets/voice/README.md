# Voice clips (embedded WAVE resources)

These WAV files are embedded into `wondersprout.exe` as Win32 `WAVE` resources and played offline via `PlaySoundW` (with mute respect and SAPI fallback).

## Current state (as embedded)
| File | Status |
|------|--------|
| `welcome.wav` (101) | **Real Kokoro Heart (`af_heart`) sample** — natural woman's voice (24 kHz 16-bit mono PCM). This is an official Heart sample clip; replace with a Heart 0.9× export of *"Hi, I’m Sprout. Want to play?"* for the exact line. |
| `tap_seeds.wav` (102) | **Audible synthesized placeholder** (speech-like, not Kokoro). Replace with a Heart 0.9× export of *"Tap each seed. We’ll count together."* |
| `retry_look.wav` (103) | **Audible synthesized placeholder** (speech-like, not Kokoro). Replace with a Heart 0.9× export of *"Let’s look again. Can you find the circle?"* |

All clips are valid 24 kHz, 16-bit mono PCM WAV and embedded in `wondersprout.exe` (no network at runtime).

## Target voice (recommended)
- **Voice:** Kokoro **Heart** (`af_heart`, American English female, rated A)
- **Speed:** 0.9×
- **Format:** 24 kHz, 16-bit mono PCM WAV (keep the same filenames/IDs)
- **License note:** Kokoro model weights are Apache 2.0; generate WonderSprout's own lines with the model. Preview in the official Kokoro demo, compare Heart vs Bella on the retry line, then export.

## Lines & files

| Resource ID | File            | Line |
|-------------|-----------------|------|
| 101         | `welcome.wav`   | "Hi, I’m Sprout. Want to play?" |
| 102         | `tap_seeds.wav` | "Tap each seed. We’ll count together." |
| 103         | `retry_look.wav`| "Let’s look again. Can you find the circle?" |

## Replacing placeholders
1. Generate each line as a **24 kHz, 16-bit mono PCM WAV** (e.g. via Kokoro export tooling).
2. Overwrite the corresponding file in this folder (keep filenames).
3. Rebuild: `bash build/build_windows.sh` (or CI) — `windres` embeds the WAVs into `dist/wondersprout.exe`. No code changes required.

Playback is triggered at launch/lesson/round state transitions (not on repaint) and falls back to Windows SAPI text-to-speech if a resource clip fails to play.
