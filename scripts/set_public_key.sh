#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INPUT="${1:-$ROOT/signing/keys/nees_public.pem}"
RUST="$ROOT/aod/src/main.rs"
[ -f "$INPUT" ] || { echo "Key not found: $INPUT"; exit 2; }
TMP="$(mktemp)"; trap 'rm -f "$TMP"' EXIT
if grep -q 'PRIVATE KEY' "$INPUT" 2>/dev/null; then
  openssl pkey -in "$INPUT" -pubout -outform DER | tail -c 32 > "$TMP"
else
  openssl pkey -pubin -in "$INPUT" -outform DER | tail -c 32 > "$TMP"
fi
python3 - "$TMP" "$RUST" <<'PY'
from pathlib import Path
import re,sys
raw=Path(sys.argv[1]).read_bytes(); src=Path(sys.argv[2])
if len(raw)!=32: raise SystemExit(f"expected 32-byte Ed25519 key, got {len(raw)}")
rows=["    "+",".join(f"0x{x:02x}" for x in raw[i:i+8])+"," for i in range(0,32,8)]
rep="const PUBLIC_KEY: [u8; 32] = [\n"+"\n".join(rows)+"\n];"
text=src.read_text()
new,n=re.subn(r"const PUBLIC_KEY:\s*\[u8;\s*32\]\s*=\s*\[(?:.|\n)*?\];",rep,text,count=1)
if n!=1: raise SystemExit("could not locate exactly one PUBLIC_KEY array")
src.write_text(new)
print("Embedded public key:",raw.hex())
PY
echo "Rebuild nees_aodd now."
