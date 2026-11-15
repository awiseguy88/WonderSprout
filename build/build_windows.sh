#!/usr/bin/env bash
set -euo pipefail
# Build WonderSprout for Windows x64 using MinGW-w64.
# Requires: x86_64-w64-mingw32-gcc in PATH.
# Produces: dist/wondersprout.exe (single, static, no external DLL deps beyond Windows system libs)

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT/dist"
mkdir -p "$OUT_DIR"

CC="${CC:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"

SRC_CORE=(
  "$ROOT/src/core/curriculum.c"
  "$ROOT/src/core/progress.c"
  "$ROOT/src/core/storage.c"
  "$ROOT/src/core/audio_synth.c"
)
SRC_WIN=(
  "$ROOT/src/platform_win32/main.c"
  "$ROOT/src/platform_win32/app_state.c"
  "$ROOT/src/platform_win32/audio_winmm.c"
  "$ROOT/src/platform_win32/tts_sapi.c"
)

CFLAGS=(
  -std=c11 -O2 -Wall -Wextra
  -I"$ROOT/src"
  -municode -mwindows
  -DUNICODE -D_UNICODE
)
LDFLAGS=(
  -static -static-libgcc
  -luser32 -lgdi32 -lwinmm -lshell32 -lole32 -luuid -lkernel32 -lsapi -lm
)

echo "==> Compiling embedded voice resources with $WINDRES"
(
  cd "$ROOT"
  "$WINDRES" --include-dir=src/platform_win32 -O coff src/platform_win32/voice.rc "$OUT_DIR/voice.o"
)

echo "==> Building WonderSprout (Windows x64) with $CC"
"$CC" "${CFLAGS[@]}" -o "$OUT_DIR/wondersprout.exe" "${SRC_CORE[@]}" "${SRC_WIN[@]}" "$OUT_DIR/voice.o" "${LDFLAGS[@]}"

echo "==> Output: $OUT_DIR/wondersprout.exe"
if command -v file >/dev/null 2>&1; then
  file "$OUT_DIR/wondersprout.exe"
fi
if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
  echo "==> Imported DLLs (should be Windows system only):"
  x86_64-w64-mingw32-objdump -p "$OUT_DIR/wondersprout.exe" | grep 'DLL Name' || true
fi
echo "==> Done."
