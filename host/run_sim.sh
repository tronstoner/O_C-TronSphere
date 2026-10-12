#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ralf Traunsteiner
# Tier 3: build the applet harness (real HemisphereApplet/HSUtils/HSIOFrame +
# weegfx, stubbed hardware) and run it with the given script arguments.
# Usage: run_sim.sh [script args]        scripted harness (bin: build/sim/sim)
#        run_sim.sh --ui [ui args]       SDL2 window       (bin: build/sim/ui)
set -eu
HOST="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HOST/.." && pwd)"
SRC="$ROOT/firmware/software/src"
B="$HOST/build/sim"; GEN="$HOST/build/gen"; OUT="$HOST/build/out"
mkdir -p "$B" "$GEN" "$OUT"
"$HOST/gen.sh"  # patched copies, see gen.sh
CXX="${CXX:-g++}"

MODE=sim
if [ "${1:-}" = "--ui" ]; then MODE=ui; shift; fi

# Firmware side (upstream headers are noisy on clang: -w), built once into a
# static library shared by both front ends.
FW_FLAGS="-O1 -std=c++17 -w -I$HOST/stub -I$GEN -I$SRC -I$SRC/util -I$SRC/src/drivers -I$ROOT -include oc_host_prelude.h"
FW_SRCS="$HOST/sim/sim_core.cpp $HOST/sim/host_runtime.cpp $HOST/sim/host_firmware.cpp $GEN/weegfx.cpp
      $SRC/HemisphereApplet.cpp $SRC/HSUtils.cpp $SRC/HSIOFrame.cpp
      $SRC/OC_strings.cpp $SRC/OC_scales.cpp $SRC/braids_quantizer.cpp $SRC/bjorklund.cpp"
# Our own front ends: warnings on.
OWN_FLAGS="-O1 -std=c++17 -Wall -Wextra -I$HOST/sim -I$HOST/render"

stale() {  # stale BIN DEPS... : true if BIN missing or any dep newer
  bin="$1"; shift
  [ ! -e "$bin" ] || [ -n "$(find "$@" -newer "$bin" 2>/dev/null | head -1)" ]
}

LIB="$B/libsimcore.a"
if stale "$LIB" $FW_SRCS "$HOST/sim/sim_core.h" "$HOST/sim/host_applets.h" "$HOST/sim/applets" "$HOST/stub" "$ROOT/custom"; then
  echo "building $LIB" >&2
  rm -rf "$B/obj"; mkdir -p "$B/obj"
  for s in $FW_SRCS; do
    "$CXX" $FW_FLAGS -c "$s" -o "$B/obj/$(basename "$s" .cpp).o"
  done
  rm -f "$LIB"; ar rcs "$LIB" "$B"/obj/*.o
fi

if [ "$MODE" = sim ]; then
  if stale "$B/sim" "$LIB" "$HOST/sim/sim_main.cpp" "$HOST/render"; then
    echo "building $B/sim" >&2
    "$CXX" $OWN_FLAGS "$HOST/sim/sim_main.cpp" "$LIB" -o "$B/sim"
  fi
  cd "$OUT"
  exec "$B/sim" --out "$OUT" "$@"
fi

command -v sdl2-config >/dev/null || { echo "sdl2-config not found: brew install sdl2" >&2; exit 1; }
if stale "$B/ui" "$LIB" "$HOST/sim/ui_main.cpp" "$HOST/render"; then
  echo "building $B/ui" >&2
  # SDL headers via -isystem so -Wall stays about our code.
  SDL_CFLAGS="$(sdl2-config --cflags | sed 's/-I/-isystem /g')"
  "$CXX" $OWN_FLAGS -I"$SRC/extern" $SDL_CFLAGS "$HOST/sim/ui_main.cpp" "$LIB" $(sdl2-config --libs) -o "$B/ui"
fi
cd "$OUT"
exec "$B/ui" --out "$OUT" "$@"
