#!/bin/sh
# Tier 2: compile real weegfx.cpp on the host and render a test frame.
set -eu
HOST="$(cd "$(dirname "$0")" && pwd)"
SRC="$HOST/../firmware/software/src"
OUT="$HOST/build/out"; GEN="$HOST/build/gen"; mkdir -p "$OUT" "$GEN" "$HOST/build/render"
"$HOST/gen.sh"  # patched weegfx.cpp copy, see gen.sh
${CXX:-g++} -O2 -std=c++17 -Wall -Wno-unused-parameter \
  -I"$HOST/stub" -I"$SRC/src" -I"$SRC/src/drivers" -I"$HOST/render" \
  "$HOST/render/render_main.cpp" "$GEN/weegfx.cpp" \
  -o "$HOST/build/render/render"
"$HOST/build/render/render" "$OUT"
