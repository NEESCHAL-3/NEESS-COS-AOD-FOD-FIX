#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
API="${ANDROID_API:-35}"
if [ -n "${ANDROID_NDK_HOME:-}" ]; then
  NDK="$ANDROID_NDK_HOME"
elif [ -d "$HOME/Android/Sdk/ndk/30.0.16248370" ]; then
  NDK="$HOME/Android/Sdk/ndk/30.0.16248370"
else
  echo "Set ANDROID_NDK_HOME"; exit 2
fi
CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang"
[ -x "$CC" ] || { echo "Missing compiler: $CC"; exit 3; }
mkdir -p "$ROOT/build"
"$CC" -shared -fPIC -O2 -Wall -Wextra \
  -Wl,-soname,sensors.mt6899.so \
  "$ROOT/src/sensors_rodin_oplus_compat.c" \
  -o "$ROOT/build/sensors.mt6899.so" \
  -llog -ldl -pthread
file "$ROOT/build/sensors.mt6899.so"
sha256sum "$ROOT/build/sensors.mt6899.so"
