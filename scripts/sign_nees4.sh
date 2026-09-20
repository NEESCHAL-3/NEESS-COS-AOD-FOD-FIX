#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEY="${NEES_PRIVATE_KEY:-$ROOT/signing/keys/nees_private.pem}"
AOD="${AOD_BIN:-$ROOT/aod/target/aarch64-linux-android/release/nees-rodin-aodd}"
FOD="${FOD_BIN:-$ROOT/fod/native/build/librodin_fp_compat.so}"
WORK="${NEES_SIGN_WORK:-$ROOT/out/signing}"
mkdir -p "$WORK"
[ -f "$KEY" ] || { echo "Missing private key: $KEY"; exit 2; }
[ -f "$AOD" ] || { echo "Missing AOD binary: $AOD"; exit 3; }
[ -f "$FOD" ] || { echo "Missing FOD binary: $FOD"; exit 4; }
if [ -n "${SYSTEM_BUILD_PROP:-}" ]; then
  cp "$SYSTEM_BUILD_PROP" "$WORK/system.build.prop.live"
else
  adb exec-out 'su -M -c "cat /system/build.prop"' > "$WORK/system.build.prop.live"
fi
if [ -n "${SENSOR_HAL:-}" ]; then
  SENSOR_SHA="$(sha256sum "$SENSOR_HAL"|awk '{print $1}')"
else
  SENSOR_SHA="$(adb shell 'su -M -c "sha256sum /vendor/lib64/hw/sensors.mt6899.so"'|tr -d '\r'|awk '{print $1}')"
fi
AOD_SHA="$(sha256sum "$AOD"|awk '{print $1}')"
FOD_SHA="$(sha256sum "$FOD"|awk '{print $1}')"
export WORK SENSOR_SHA AOD_SHA FOD_SHA REQUESTED_ROM_ID="${ROM_ID:-}"
python3 - <<'PY'
from pathlib import Path
import hashlib,os,re
work=Path(os.environ['WORK']); raw=(work/'system.build.prop.live').read_text()
rom=os.environ.get('REQUESTED_ROM_ID','').strip()
auth=next((x.split('=',1)[1].strip() for x in raw.splitlines() if x.startswith('ro.nees.aod.auth=')),None)
if not rom and auth:
    p=auth.split('.',2)
    if len(p)==3 and re.fullmatch(r'[0-9a-fA-F]{64}',p[1]): rom=p[1].lower()
if not rom: raise SystemExit('No ROM_ID. Run ./scripts/generate_rom_id.sh and export ROM_ID=<value>.')
if not re.fullmatch(r'[0-9a-fA-F]{64}',rom): raise SystemExit('ROM_ID must be 64 hex characters')
canon=''.join(x+'\n' for x in raw.splitlines() if not x.startswith('ro.nees.aod.auth='))
sys_hash=hashlib.sha256(canon.encode()).hexdigest()
manifest=(
 'NEES_RODIN_SHARED_V4\n'
 f'ROM_ID={rom.lower()}\n'
 f'SYSTEM_BUILD_PROP_SHA256={sys_hash}\n'
 f'SENSOR_HAL_SHA256={os.environ["SENSOR_SHA"]}\n'
 f'AOD_DAEMON_SHA256={os.environ["AOD_SHA"]}\n'
 f'FOD_COMPAT_SHA256={os.environ["FOD_SHA"]}'
)
(work/'manifest.txt').write_bytes(manifest.encode()); (work/'rom_id.txt').write_text(rom.lower()+'\n')
print(manifest)
PY
openssl pkeyutl -sign -rawin -inkey "$KEY" -in "$WORK/manifest.txt" -out "$WORK/signature.bin"
openssl pkey -in "$KEY" -pubout -out "$WORK/public.pem" >/dev/null
openssl pkeyutl -verify -pubin -inkey "$WORK/public.pem" -rawin -in "$WORK/manifest.txt" -sigfile "$WORK/signature.bin"
ROM_ID_FINAL="$(tr -d '\r\n' < "$WORK/rom_id.txt")"
SIG="$(base64 -w0 "$WORK/signature.bin")"
printf '%s' "NEES4.${ROM_ID_FINAL}.${SIG}" > "$WORK/auth.txt"
echo "AUTH_FILE=$WORK/auth.txt"
