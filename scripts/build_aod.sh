#!/usr/bin/env bash
set -euo pipefail
cd "${AOD_SRC:-$HOME/Downloads/nees-rodin-aodd}"
NDK="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/30.0.16248370}"
LINKER="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$LINKER"
export CC_aarch64_linux_android="$LINKER"
cargo build --release --target aarch64-linux-android
BIN="$PWD/target/aarch64-linux-android/release/nees-rodin-aodd"
file "$BIN"
sha256sum "$BIN"
strings "$BIN" | grep -E \
'NEES_RODIN_SHARED_V4|NEES4 runtime authorization published|V4 shared authorization OK'
