#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$HOME/Downloads/NEESS-COS-AOD-FOD-FIX-export}"
cd "$ROOT"

if grep -RniE 'BEGIN (RSA |EC |OPENSSH |)?PRIVATE KEY' . --exclude-dir=.git 2>/dev/null; then
  echo "ERROR: private key material detected"
  exit 73
fi

if find . -type f \( -name '*.pem' -o -name '*.key' -o -name '*.p12' -o -name '*.pfx' \) | grep -q .; then
  echo "ERROR: secret-looking key file detected"
  exit 74
fi

git init
git add .
git status --short

echo
echo "Review staged files, then commit manually:"
echo 'git commit -m "Initial Rodin ColorOS AOD/FOD compatibility source"'
