#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ralf Traunsteiner
# Tier 3: build the applet harness (real HemisphereApplet/HSUtils/HSIOFrame +
# weegfx, stubbed hardware) and run it with the given script arguments.
# Usage: run_sim.sh [script args]        scripted harness (bin: build/sim/sim)
#        run_sim.sh                      no args: run the scenarios below, exit 1 on any FAIL
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
FW_SRCS="$HOST/sim/sim_core.cpp $HOST/sim/host_runtime.cpp $HOST/sim/host_firmware.cpp $HOST/sim/host_clocksetup.cpp $GEN/weegfx.cpp
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

# Scenarios (docs/HOST_SIM.md "Scenarios"). LK = Likembe left, Dens 100 % and
# Var 0 % (every step is a note, no mutation), so output 0 (Out A gate) rises
# once per engine step. Cursor ends on Var (main list index 1).
LK="--left Likembe --press --encoder 3 --press --encoder 1 --press --encoder -2 --press --quiet"
scenarios() {
  fail=0
  run() {
    name="$1"; shift; echo "== $name"
    if out=$("$0" "$@"); then rc=0; else rc=1; fail=1; fi
    printf '%s\n' "$out" | grep -E '^(PASS|FAIL)' || true
    [ "$rc" = 0 ] || echo "   -> FAILED: $name"
  }
  # Src Int: internal clock 120 BPM, x4 on the hemisphere's first input -> 8 steps/s.
  run "likembe_src_int_x4" $LK --int-bpm 120 --mult 1:x4 --segment --seconds 0.5 --expect-rises 0 0 \
      --segment --int-start --seconds 1.99 --expect-rises 0 16
  # Src TR1, Div 3 (Clock page: Src TR1, Div 3): 7 TR1 pulses -> steps on pulses 1, 4, 7.
  run "likembe_src_tr1_div3" $LK --encoder 7 --press --press --encoder 1 --press --encoder 1 --press \
      --encoder 2 --press --segment --trig 1@0.1 --trig 1@0.2 --trig 1@0.3 --trig 1@0.4 \
      --trig 1@0.5 --trig 1@0.6 --trig 1@0.7 --seconds 1 --expect-rises 0 3
  # Src TR1 = physical jack: internal clock runs with x4 on TR1's slot, yet only the
  # scripted TR1 pulses (every 500 ms) step the engine.
  run "likembe_src_tr1_jack_override" $LK --encoder 7 --press --press --encoder 1 --press \
      --int-bpm 120 --mult 1:x4 --int-start --segment --trig 1@0.25 --trig 1@0.75 --trig 1@1.25 \
      --trig 1@1.75 --seconds 2 --expect-rises 0 4
  # Clock start always restarts (Rst Off): start, stop, start -> second run replays the first.
  run "likembe_rst_clockstart" $LK --int-bpm 120 --mult 1:x4 --segment --int-start --seconds 1.3 --int-stop \
      --seconds 0.5 --expect-rises 0 11 --segment --int-start --seconds 1.3 --expect-same 0 1 2 10
  # Rst TR2 (Roll Off): a TR2 pulse mid-run restarts at bar start -> replays the first run.
  run "likembe_rst_tr2" $LK --encoder 7 --press --encoder 2 --press --encoder -2 --press --encoder 1 \
      --press --encoder 2 --press --int-bpm 120 --mult 1:x4 --segment --int-start --seconds 1.3 \
      --expect-rises 0 11 --segment --trig 2@1.3 --seconds 1.3 --expect-same 0 1 2 10
  # External sync: armed internal clock follows TR1 at 150 BPM, PPQN 4; x2 -> 5 steps/s.
  run "likembe_ext_sync_tr1" $LK --clock 150:4 --int-bpm 120 --ppqn 4 --mult 1:x2 --int-arm \
      --seconds 1.1 --expect-bpm 149..151 --segment --seconds 1.9 --expect-rises 0 9
  [ "$fail" = 0 ] && echo "SIM SCENARIOS OK" || { echo "SIM SCENARIOS FAILED"; exit 1; }
}

if [ "$MODE" = sim ] && [ $# -eq 0 ]; then
  scenarios; exit 0
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
