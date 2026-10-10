#!/bin/sh
# Tier 3: build the applet harness (real HemisphereApplet/HSUtils/HSIOFrame +
# weegfx, stubbed hardware) and run it with the given script arguments.
set -eu
HOST="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HOST/.." && pwd)"
SRC="$ROOT/firmware/software/src"
B="$HOST/build/sim"; GEN="$HOST/build/gen"; OUT="$HOST/build/out"
mkdir -p "$B" "$GEN" "$OUT"
"$HOST/gen.sh"  # patched copies, see gen.sh

FLAGS="-O1 -std=c++17 -w -I$HOST/stub -I$GEN -I$SRC -I$SRC/util -I$SRC/src/drivers -I$ROOT -I$HOST/render -include oc_host_prelude.h"
SRCS="$HOST/sim/sim_main.cpp $HOST/sim/host_runtime.cpp $HOST/sim/host_firmware.cpp $GEN/weegfx.cpp
      $SRC/HemisphereApplet.cpp $SRC/HSUtils.cpp $SRC/HSIOFrame.cpp
      $SRC/OC_strings.cpp $SRC/OC_scales.cpp $SRC/braids_quantizer.cpp $SRC/bjorklund.cpp"
# Rebuild only when a source or stub is newer than the binary.
if [ ! -x "$B/sim" ] || [ -n "$(find $SRCS $HOST/stub $HOST/render -newer "$B/sim" 2>/dev/null | head -1)" ]; then
  echo "building $B/sim" >&2
  ${CXX:-g++} $FLAGS $SRCS -o "$B/sim"
fi
cd "$OUT"
exec "$B/sim" --out "$OUT" "$@"
