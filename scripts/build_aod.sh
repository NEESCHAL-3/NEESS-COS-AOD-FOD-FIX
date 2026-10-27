#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${AOD_SRC:-$ROOT/aod}"
NDK="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/30.0.16248370}"
LINKER="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang"
test -x "$LINKER"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$LINKER"
export CC_aarch64_linux_android="$LINKER"
cargo test --locked
cargo build --release --target aarch64-linux-android --locked
BIN="$PWD/target/aarch64-linux-android/release/nees-rodin-aodd"
file "$BIN"
sha256sum "$BIN"
