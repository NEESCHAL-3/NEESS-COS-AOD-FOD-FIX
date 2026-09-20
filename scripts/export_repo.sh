#!/usr/bin/env bash
set -euo pipefail

OUT="${1:-$HOME/Downloads/NEESS-COS-AOD-FOD-FIX-export}"
FOD_SRC="$HOME/android/rodin-oplus-fp-compat"
AOD_SRC="$HOME/Downloads/nees-rodin-aodd"

rm -rf "$OUT"
mkdir -p "$OUT/fod/native" "$OUT/aod/src" "$OUT/docs" "$OUT/signing" "$OUT/scripts"

cp -f "$FOD_SRC/native/rodin_fp_compat.cpp" "$OUT/fod/native/"
cp -f "$FOD_SRC/native/CMakeLists.txt" "$OUT/fod/native/"
[ -f "$FOD_SRC/zz_rodin_fp_compat.rc" ] && cp -f "$FOD_SRC/zz_rodin_fp_compat.rc" "$OUT/fod/"

cp -f "$AOD_SRC/src/main.rs" "$OUT/aod/src/"
cp -f "$AOD_SRC/Cargo.toml" "$OUT/aod/"
[ -f "$AOD_SRC/Cargo.lock" ] && cp -f "$AOD_SRC/Cargo.lock" "$OUT/aod/"

if adb get-state >/dev/null 2>&1; then
  adb exec-out 'su -M -c "cat /system/etc/init/nees_aodd.rc"' > "$OUT/aod/nees_aodd.rc" || true
fi

KIT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for f in README.md SECURITY.md BUILDING.md INSTALLATION.md TESTING.md CHANGELOG.md .gitignore; do
  cp -f "$KIT_DIR/$f" "$OUT/$f"
done
cp -af "$KIT_DIR/docs/." "$OUT/docs/"
cp -af "$KIT_DIR/signing/." "$OUT/signing/"
cp -af "$KIT_DIR/scripts/." "$OUT/scripts/"

echo "===== SECRET SCAN ====="
if grep -RniE 'BEGIN (RSA |EC |OPENSSH |)?PRIVATE KEY' "$OUT" 2>/dev/null; then
  echo "ERROR: private key material detected"
  exit 73
fi

if find "$OUT" -type f \( -name '*.pem' -o -name '*.key' -o -name '*.p12' -o -name '*.pfx' \) | grep -q .; then
  echo "ERROR: secret-looking key file detected"
  exit 74
fi

echo "EXPORT_READY=$OUT"
