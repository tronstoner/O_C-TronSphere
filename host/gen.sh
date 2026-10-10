#!/bin/sh
# Generate host-patched copies of the few firmware files that cannot compile
# on a 64-bit host as-is. firmware/ is never modified; copies go to build/gen.
#   weegfx.cpp:533      print(uint32_t, size_t) vs header's (uint32_t, unsigned)
#   util/util_math.h    ARM inline asm (usat, umull) -> portable C
set -eu
HOST="$(cd "$(dirname "$0")" && pwd)"
SRC="$HOST/../firmware/software/src"
GEN="$HOST/build/gen"
mkdir -p "$GEN/util"
# Write only when content changed, so run_sim.sh's rebuild check stays quiet.
put() { if ! cmp -s "$GEN/.tmp" "$1"; then mv "$GEN/.tmp" "$1"; else rm -f "$GEN/.tmp"; fi; }
sed 's/print(uint32_t value, size_t width)/print(uint32_t value, unsigned width)/' \
  "$SRC/src/drivers/weegfx.cpp" > "$GEN/.tmp"; put "$GEN/weegfx.cpp"
sed -e 's/__asm("usat %0, %1, %2".*/result = (int32_t)value < 0 ? 0 : ((int32_t)value > 65535 ? 65535 : (uint32_t)value);/' \
    -e 's/asm volatile("umull.*/{ uint64_t r_ = (uint64_t)a * b; lo = (uint32_t)r_; hi = (uint32_t)(r_ >> 32); }/' \
  "$SRC/util/util_math.h" > "$GEN/.tmp"; put "$GEN/util/util_math.h"
if grep -n 'asm' "$GEN/util/util_math.h"; then echo "gen.sh: asm left in util_math.h" >&2; exit 1; fi
