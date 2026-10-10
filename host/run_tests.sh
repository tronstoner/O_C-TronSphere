#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ralf Traunsteiner
# Compile and run every host/tests/test_*.cpp. Non-zero exit if any fails.
set -u
HOST="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HOST/.." && pwd)"
OUT="$HOST/build/tests"
mkdir -p "$OUT"
fail=0
for src in "$HOST"/tests/test_*.cpp; do
  name="$(basename "$src" .cpp)"
  echo "== $name"
  if ! ${CXX:-g++} -O2 -std=c++17 -Wall -Wextra -Werror -I"$ROOT" "$src" -o "$OUT/$name"; then
    echo "FAIL $name (compile)"; fail=1; continue
  fi
  "$OUT/$name" || fail=1
done
[ $fail -eq 0 ] && echo "ALL PASS" || echo "SOME FAILED"
exit $fail
