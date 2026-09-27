#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: RRO_STORE_PASS=... RRO_KEY_PASS=... $0 <android.jar> <keystore> <output.apk>" >&2
  exit 2
fi

: "${RRO_STORE_PASS:?Set RRO_STORE_PASS for the signing keystore}"
: "${RRO_KEY_PASS:=$RRO_STORE_PASS}"

ANDROID_JAR="$1"
KEYSTORE="$2"
OUTPUT="$3"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf -- "$BUILD_DIR"' EXIT
AAPT2_BIN="${AAPT2:-aapt2}"

"$AAPT2_BIN" compile --dir "$SOURCE_DIR/res" -o "$BUILD_DIR/resources.zip"
"$AAPT2_BIN" link \
  -o "$BUILD_DIR/unsigned.apk" \
  --manifest "$SOURCE_DIR/AndroidManifest.xml" \
  -I "$ANDROID_JAR" \
  --auto-add-overlay \
  "$BUILD_DIR/resources.zip"
zipalign -f 4 "$BUILD_DIR/unsigned.apk" "$BUILD_DIR/aligned.apk"

mkdir -p "$(dirname "$OUTPUT")"
SIGN_ARGS=(--ks "$KEYSTORE" --ks-pass env:RRO_STORE_PASS --key-pass env:RRO_KEY_PASS)
if [ -n "${RRO_KEY_ALIAS:-}" ]; then
  SIGN_ARGS+=(--ks-key-alias "$RRO_KEY_ALIAS")
fi
apksigner sign "${SIGN_ARGS[@]}" --out "$OUTPUT" "$BUILD_DIR/aligned.apk"
apksigner verify "$OUTPUT"
echo "Built $OUTPUT"
