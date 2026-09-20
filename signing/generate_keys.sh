#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$ROOT/keys}"
mkdir -p "$OUT"; chmod 700 "$OUT"
PRIV="$OUT/nees_private.pem"
PUB="$OUT/nees_public.pem"
RAW="$OUT/nees_public.raw"
HEX="$OUT/nees_public.hex"
[ ! -e "$PRIV" ] || { echo "Refusing to overwrite $PRIV"; exit 2; }
openssl genpkey -algorithm Ed25519 -out "$PRIV"
openssl pkey -in "$PRIV" -pubout -out "$PUB"
openssl pkey -in "$PRIV" -pubout -outform DER | tail -c 32 > "$RAW"
python3 - "$RAW" "$HEX" <<'PY'
from pathlib import Path
import sys
raw=Path(sys.argv[1]).read_bytes()
if len(raw)!=32: raise SystemExit(f"expected 32 bytes, got {len(raw)}")
Path(sys.argv[2]).write_text(raw.hex()+"\n")
PY
chmod 600 "$PRIV"; chmod 644 "$PUB" "$RAW" "$HEX"
echo "Generated developer keypair. KEEP $PRIV SECRET."
