# Voice clips (embedded WAVE resources)

These WAV files are embedded into `wondersprout.exe` as Win32 `WAVE` resources and played offline via `PlaySoundW` (with mute respect and SAPI fallback).

## Current state: development placeholders
The committed `.wav` files are **valid silent placeholders** (24 kHz, 16-bit mono PCM) so the resource pipeline builds and runs end-to-end in any environment. Replace them with real voice exports to ship Sprout's voice.

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
