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

The current shared-mode runtime continues after a signature-mismatch notice;
`--verify-only` is the strict signed-manifest check. The volatile
`sys.nees4.authorized` handoff is therefore not proof of crypto verification
in this mode. Vendor biometric authentication and lockout remain separate.
See [the update guide](docs/FOD-AOD-UPDATE.md#signature-notices).

## Before every push

```bash
git status --short
git grep -n "BEGIN .*PRIVATE KEY" || true
find . -type f \( -name '*.pem' -o -name '*.key' -o -name '*.p12' -o -name '*.pfx' \) -print
```
