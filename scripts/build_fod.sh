#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${FOD_SRC:-$ROOT/fod}"
NDK="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/30.0.16248370}"
cmake -S "$SRC/native" -B "$SRC/native/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-35 -DCMAKE_BUILD_TYPE=Release
cmake --build "$SRC/native/build" -j"$(nproc)"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror "$SRC/native/tests/input_policy_test.cpp" -o "$SRC/native/build/input-policy-tests"
timeout 3 "$SRC/native/build/input-policy-tests"
file "$SRC/native/build/librodin_fp_compat.so"
sha256sum "$SRC/native/build/librodin_fp_compat.so"
