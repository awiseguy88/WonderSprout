# Voice clips (embedded WAVE resources)

These WAV files are embedded into `wondersprout.exe` as Win32 `WAVE` resources and played offline via `PlaySoundW` (with mute respect and SAPI fallback).

## Current state (as embedded)
All three clips are **real Kokoro Heart (`af_heart`) natural woman's voice** samples (24 kHz, 16-bit mono PCM WAV), embedded in `wondersprout.exe` (no network at runtime).

| File | Resource ID | Intended line | Current content |
|------|-------------|---------------|-----------------|
| `welcome.wav` | 101 | *"Hi, I’m Sprout. Want to play?"* | Heart sample (5.0 s) |
| `tap_seeds.wav` | 102 | *"Tap each seed. We’ll count together."* | Heart sample (10.8 s) |
| `retry_look.wav` | 103 | *"Let’s look again. Can you find the circle?"* | Heart sample (10.3 s) |

**To ship the exact lines:** generate the three lines with Kokoro **Heart (`af_heart`) @ 0.9×**, export as **24 kHz, 16-bit mono PCM WAV**, overwrite the files in this folder (same filenames), and rebuild (`bash build/build_windows.sh`) — no code changes required. The current embedded clips are official Heart samples (Apache 2.0 licensed) that confirm the woman's voice is present; they can be replaced with the exact lines above. See [assets/voice/README.md](assets/voice/README.md) for the full replacement workflow and licensing note.

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
