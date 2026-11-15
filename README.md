# WonderSprout

**WonderSprout** is a 100% free, offline learning app for kids ages 2–5 — inspired by the *structure* of ABCmouse (learning path, activity areas, rewards) but rebuilt from scratch with all-original content.

- **Single file:** `wondersprout.exe` — a native Windows x64 app with **no runtime dependencies** (no .NET, no redistributables, no browser, no installer). Uses only Windows system libraries.
- **100% free & offline:** no accounts, no ads, no tracking, no in-app purchases, and **no network access** at all.
- **Ages 2–5 foundations:** Letters & phonics cues, Numbers & counting, Shapes & Colors, and Music (synthesized tones/songs).
- **Learning Path + Free Play:** a guided stepping-stone path plus freely playable activity areas.
- **Rewards:** stars and stickers for motivation (no currency). Progress is saved locally in `%APPDATA%\WonderSprout\progress.dat` on the device only.

## Download / Releases

Prebuilt releases are produced automatically by GitHub Actions. Push a tag like `v1.0.0` (or use the workflow dispatch) and the CI will build and attach `wondersprout.exe` to the release. You can also download the build artifact from the workflow run.

- **Workflow:** [`.github/workflows/build.yml`](.github/workflows/build.yml) builds the Windows `.exe` (MinGW-w64 cross-compile on Ubuntu) and runs the Linux core tests.

## Build it yourself (Windows `.exe`)

Requires **MinGW-w64** (`x86_64-w64-mingw32-gcc`). On Ubuntu/Debian:

```bash
sudo apt-get install gcc-mingw-w64-x86-64
chmod +x build/build_windows.sh
./build/build_windows.sh
# -> dist/wondersprout.exe
```

On Windows with MSYS2/MinGW-w64 installed, you can run the equivalent compile command from `build/build_windows.sh` (adjust the compiler path if needed).

The resulting executable is statically linked against libgcc and links only Windows system DLLs (`USER32`, `GDI32`, `WINMM`, `SHELL32`, etc.) — nothing extra to install.

## Run core tests (Linux)

The pure logic (curriculum, progress, storage, audio synthesis) is platform-independent C with no OS headers and is tested natively on Linux:

```bash
chmod +x build/test_core.sh
./build/test_core.sh
# or: cd build && make test
```

## Project structure

```
wondersprout/
  src/
    core/                 # pure C (no OS deps): curriculum, progress, storage, audio_synth
    platform_win32/       # Win32 GUI: main, render/input/state, WinMM audio playback
    test/
      test_core.c         # Linux-runnable unit tests for core
  build/
    build_windows.sh      # MinGW-w64 cross-compile -> dist/wondersprout.exe
    test_core.sh          # native gcc build + run of core tests
    Makefile
  .github/workflows/build.yml  # CI: build exe + run tests, attach exe to releases
  dist/                        # build output (gitignored)
```

## Privacy & safety

- Runs entirely offline. The app makes **zero network requests**.
- No accounts, no ads, no tracking, no external links.
- Progress is stored locally at `%APPDATA%\WonderSprout\progress.dat`. If the file is missing or corrupt, the app falls back to a fresh in-memory state so kids can always play.
- All visuals are drawn with GDI and all sounds are synthesized in code — no external media, no third-party assets.

## Notes

- `dist/wondersprout.exe` can be produced locally with MinGW-w64 (`build/build_windows.sh`) or via the included GitHub Actions workflow. Core logic is verified by the Linux test suite (`11/11` tests passing).
- All content is original; no ABCmouse branding, characters, or media are used.
