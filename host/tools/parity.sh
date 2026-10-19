#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ralf Traunsteiner
# Parity: C++ core (likembe_dump) vs browser lab engine (likembe_engine.js) event streams.
# Non-zero exit on any mismatch. Run via `task host:parity`.
set -u
HOST="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="$(cd "$HOST/.." && pwd)"
OUT="$HOST/build/parity"
mkdir -p "$OUT"
if ! command -v node >/dev/null 2>&1 && [ -s "${NVM_DIR:-$HOME/.nvm}/nvm.sh" ]; then
  . "${NVM_DIR:-$HOME/.nvm}/nvm.sh" >/dev/null 2>&1   # nvm installs are lazy-loaded in interactive shells only
fi
command -v node >/dev/null 2>&1 || { echo "node not found (needed for the JS side)"; exit 2; }
${CXX:-g++} -O2 -std=c++17 -Wall -Wextra -Werror -I"$ROOT" "$HOST/tools/likembe_dump.cpp" -o "$OUT/likembe_dump" || exit 1
fail=0
while read -r args; do
  [ -z "$args" ] && continue
  # shellcheck disable=SC2086
  "$OUT/likembe_dump" $args > "$OUT/cpp.txt"
  # shellcheck disable=SC2086
  node "$HOST/tools/likembe_lab_parity.mjs" $args > "$OUT/js.txt"
  n=$(wc -l < "$OUT/cpp.txt" | tr -d ' ')
  if diff -q "$OUT/cpp.txt" "$OUT/js.txt" >/dev/null; then echo "OK   $args ($n events)"
  else echo "FAIL $args"; diff "$OUT/cpp.txt" "$OUT/js.txt" | head -5; fail=1; fi
done <<LIST
--seed 1 --part 0 --bars 64
--seed 1 --part 1 --bars 64
--seed 4242 --part 2 --len 12 --bars 64
--seed 777 --part 3 --oct 2 --root 9 --bars 64
--seed 31337 --part 3 --len 12 --var 100 --pass 1 --bars 96
--seed 9 --part 0 --var 100 --pass 1 --dens 40 --bars 96
--seed 65535 --part 1 --var 100 --pass 1 --oct -2 --len 12 --bars 96
--seed 14572 --part 2 --var 75 --pass 2 --dens 100 --bars 128
--seed 0 --part 0 --dens 0 --var 50 --pass 1 --len 12 --bars 64
--seed 5 --part 1 --len 16 --var 60 --pass 2 --bars 96
--seed 6 --part 0 --len 24 --var 100 --pass 1 --bars 96
--seed 7 --part 3 --len 24 --var 50 --pass 3 --bars 96
--seed 8 --part 2 --len 32 --var 100 --pass 1 --bars 96
--seed 10 --part 3 --len 32 --var 75 --pass 2 --bars 96
--seed 11 --part 1 --len 7 --var 100 --pass 1 --bars 64
--seed 12 --part 0 --len 2 --var 100 --pass 1 --bars 64
LIST
[ $fail -eq 0 ] && echo "PARITY OK" || echo "PARITY FAILED"
exit $fail
