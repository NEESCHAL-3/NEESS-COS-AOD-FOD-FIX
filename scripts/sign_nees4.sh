#!/usr/bin/env bash
set -euo pipefail

KEY="${NEES_PRIVATE_KEY:-$HOME/Downloads/NEES-RODIN-AUTH/nees_rodin_private.pem}"
AOD="${AOD_BIN:-$HOME/Downloads/nees-rodin-aodd/target/aarch64-linux-android/release/nees-rodin-aodd}"
FOD="${FOD_BIN:-$HOME/android/rodin-oplus-fp-compat/native/build/librodin_fp_compat.so}"
WORK="${NEES_SIGN_WORK:-$HOME/Downloads/NEES4-SIGNING}"

mkdir -p "$WORK"

test -f "$KEY" || { echo "Missing private key"; exit 2; }
test -f "$AOD" || { echo "Missing AOD binary"; exit 3; }
test -f "$FOD" || { echo "Missing FOD binary"; exit 4; }

adb exec-out 'su -M -c "cat /system/build.prop"' > "$WORK/system.build.prop.live"

SENSOR_SHA="$(
  adb shell 'su -M -c "sha256sum /vendor/lib64/hw/sensors.mt6899.so"' |
  tr -d '\r' | awk '{print $1}'
)"
AOD_SHA="$(sha256sum "$AOD" | awk '{print $1}')"
FOD_SHA="$(sha256sum "$FOD" | awk '{print $1}')"

export WORK SENSOR_SHA AOD_SHA FOD_SHA

python3 - <<'PY'
from pathlib import Path
import hashlib, os

work = Path(os.environ["WORK"])
raw = (work / "system.build.prop.live").read_text()

auth = next(
    (line.split("=", 1)[1].strip()
     for line in raw.splitlines()
     if line.startswith("ro.nees.aod.auth=")),
    None,
)

if not auth:
    raise SystemExit("ro.nees.aod.auth missing")

parts = auth.split(".", 2)
if len(parts) != 3 or len(parts[1]) != 64:
    raise SystemExit("current auth / ROM_ID malformed")

rom_id = parts[1]

canonical = "".join(
    line + "\n"
    for line in raw.splitlines()
    if not line.startswith("ro.nees.aod.auth=")
)

system_hash = hashlib.sha256(canonical.encode()).hexdigest()

manifest = (
    "NEES_RODIN_SHARED_V4\n"
    f"ROM_ID={rom_id}\n"
    f"SYSTEM_BUILD_PROP_SHA256={system_hash}\n"
    f"SENSOR_HAL_SHA256={os.environ['SENSOR_SHA']}\n"
    f"AOD_DAEMON_SHA256={os.environ['AOD_SHA']}\n"
    f"FOD_COMPAT_SHA256={os.environ['FOD_SHA']}"
)

(work / "manifest.txt").write_bytes(manifest.encode())
(work / "rom_id.txt").write_text(rom_id)

print(manifest)
PY

openssl pkeyutl \
  -sign -rawin \
  -inkey "$KEY" \
  -in "$WORK/manifest.txt" \
  -out "$WORK/signature.bin"

openssl pkey \
  -in "$KEY" \
  -pubout \
  -out "$WORK/public.pem" >/dev/null

openssl pkeyutl \
  -verify -pubin \
  -inkey "$WORK/public.pem" \
  -rawin \
  -in "$WORK/manifest.txt" \
  -sigfile "$WORK/signature.bin"

ROM_ID="$(cat "$WORK/rom_id.txt")"
SIG="$(base64 -w0 "$WORK/signature.bin")"

printf '%s' "NEES4.${ROM_ID}.${SIG}" > "$WORK/auth.txt"

echo
echo "AOD_DAEMON_SHA256=$AOD_SHA"
echo "FOD_COMPAT_SHA256=$FOD_SHA"
echo "SENSOR_HAL_SHA256=$SENSOR_SHA"
echo "AUTH_FILE=$WORK/auth.txt"
