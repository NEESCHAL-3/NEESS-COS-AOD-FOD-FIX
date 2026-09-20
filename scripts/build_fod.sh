#!/usr/bin/env bash
set -euo pipefail
cd "${FOD_SRC:-$HOME/android/rodin-oplus-fp-compat}"
cmake --build native/build -j"$(nproc)"
BIN="$PWD/native/build/librodin_fp_compat.so"
file "$BIN"
sha256sum "$BIN"
strings "$BIN" | grep -E \
'NEES4 runtime authorization OK|runtime authorization timeout|replay pending physical FOD DOWN'
