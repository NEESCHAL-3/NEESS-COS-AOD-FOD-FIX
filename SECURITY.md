# Security

## Private signing key

Current local development path:

```text
~/Downloads/NEES-RODIN-AUTH/nees_rodin_private.pem
```

Never commit, upload, paste, or publish this file.

Use `scripts/backup_private_key.sh` to make a separate AES-256 encrypted backup locally.

## Public key

The public key may be embedded in source and published. The private key must remain private.

## NEES4 trust model

NEES4 is a signed build-binding mechanism, not unbreakable DRM against an attacker who has full root and can replace code.

The signature binds:

- canonical `/system/build.prop`
- sensor HAL
- AOD daemon
- FOD compatibility library

`sys.nees4.authorized=1` is only a volatile runtime handoff after crypto verification.

## Before every push

```bash
git status --short
git grep -n "BEGIN .*PRIVATE KEY" || true
find . -type f \( -name '*.pem' -o -name '*.key' -o -name '*.p12' -o -name '*.pfx' \) -print
```
