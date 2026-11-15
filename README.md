# WonderSprout

**WonderSprout** is a 100% free, offline learning app for kids ages 2–5. It runs as a single portable Windows executable with no accounts, no ads, no tracking, and no network access.

## Download for Windows

- **Latest build:** download `wondersprout.exe` from the [Releases](https://github.com/awiseguy88/WonderSprout/releases) page (each release includes a `SHA256SUMS.txt` checksum).
- **Compatibility:** Windows 10/11 x64 (64-bit). No installer, no runtime dependencies — just run the `.exe`.
- **Offline & private:** all progress is stored locally in `%APPDATA%\WonderSprout\progress.dat`. Nothing is uploaded.

## One Complete Play Session

1. Open the app and press **Continue Adventure** (or **Start Adventure!**).
2. Play **three short activities** from the guided learning path (letters, numbers, shapes/colors, or music).
3. After each activity choose **Next**, **Play Again**, or **Done for today**.
4. When the session is complete, **plant a seed in My Garden** and watch it grow over future sessions.
5. Pick up right where you left off tomorrow — progress and your garden are saved automatically.

## Features

- **Guided Learning Path:** sequential, unlocked progression across 20 activities.
- **Letters & Phonics:** meet letters A–E, find-the-letter quizzes, and tap-to-trace letter paths.
- **Numbers & Counting:** count objects 1–5, "how many?" quizzes, and matching.
- **Shapes & Colors:** shape matching, color matching, and sorting (circles vs squares).
- **Music & Movement:** free-play music pads and repeat-the-melody songs (Twinkle/Scale).
- **My Garden:** earn a seed each completed session, plant sunflowers/tulips/carrots/trees, and watch them grow from seed → sprout → plant → bloom.
- **Natural voice prompts:** spoken cues and encouragement via Windows SAPI (system voices), with text always visible.
- **Accessibility:** mute audio, Reduce Motion (suppresses confetti/particles), keyboard navigation (Enter/Space, Esc, arrows for the path).
- **Rewards:** stars and stickers for progress and completed domains.

## Build from source

### Windows x64 executable (cross-compile on Linux)

Requires MinGW-w64 (`x86_64-w64-mingw32-gcc`). Example:

```bash
cd wondersprout
bash build/build_windows.sh
# or with explicit compiler:
CC=/path/to/x86_64-w64-mingw32-gcc bash build/build_windows.sh
```

Output: `dist/wondersprout.exe` (statically linked, GUI subsystem, system DLLs only).

### Core logic tests (Linux/native)

```bash
cd wondersprout/build
make clean test
```

Runs the platform-independent curriculum/progress/storage/audio-synth test suite.

## GitHub Actions

- **Build Windows exe:** cross-compiles with MinGW-w64, verifies PE/imports, uploads `wondersprout-windows-x64` artifact.
- **Test core logic:** runs native core tests on Ubuntu.
- **Release (on tags `v*`):** attaches `wondersprout.exe` and `SHA256SUMS.txt` to the GitHub Release.

## Embedded voice clips (Windows resources)

WonderSprout can embed voice prompts directly in `wondersprout.exe` as Win32 `WAVE` resources (compiled with `windres`, played offline via `PlaySoundW`, with mute respect and Windows SAPI text-to-speech as fallback).

- **Lines/assets:** `assets/voice/{welcome.wav,tap_seeds.wav,retry_look.wav}` → IDs `101/102/103` (see [voice_res.h](src/platform_win32/voice_res.h) and [voice.rc](src/platform_win32/voice.rc)).
- **Current committed clips:** development **placeholder** WAVs (valid 24 kHz, 16-bit mono PCM, silent) so the resource pipeline builds/runs end-to-end anywhere.
- **Ship voice:** generate the three lines with Kokoro **Heart (`af_heart`) @ 0.9×**, export as **24 kHz, 16-bit mono PCM WAV**, overwrite the files in `assets/voice/` (same filenames), and rebuild (`bash build/build_windows.sh`) — no code changes required. Kokoro model weights are Apache 2.0; see [assets/voice/README.md](assets/voice/README.md) for the replacement workflow and licensing note.

Prompts are triggered at launch/lesson/round state transitions (not on window repaint) so repainting cannot restart playback.

## Project structure

```
wondersprout/
  src/
    core/           # OS-independent logic: curriculum, progress, storage, audio synthesis
    platform_win32/ # Win32 GUI (GDI), WinMM audio, SAPI TTS, app state/rendering/input
    test/           # Core unit tests
  build/            # Build scripts and Makefile
  dist/             # Built executable output
  .github/workflows # CI build/test/release
```

## Privacy

- No accounts, no ads, no tracking, no analytics, no network requests.
- Progress is stored only on the local machine (`%APPDATA%\WonderSprout\progress.dat`).
- Fully playable offline.

## License

[MIT](LICENSE)
