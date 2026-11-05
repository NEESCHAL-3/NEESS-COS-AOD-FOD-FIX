# Developer Signing

The official NEES private key is not included.

Generate your own keypair:
`./signing/generate_keys.sh`

Embed your public key in the AOD verifier:
`./scripts/set_public_key.sh signing/keys/nees_public.pem`

For a new ROM ID:
`ROM_ID="$(./scripts/generate_rom_id.sh)"; export ROM_ID`

Sign:
`export NEES_PRIVATE_KEY="$PWD/signing/keys/nees_private.pem"`
`./scripts/sign_nees4.sh`

The FOD library does not carry a second Ed25519 key; it trusts the runtime authorization published by the verified AOD daemon.


After updating the AOD daemon, FOD library, sensor HAL, or system properties,
regenerate the ROM signature against the final installed files. A successful
Binder/HAL startup does not establish signature validity. Check the installed
`/system/bin/nees_aodd --verify-only` before restarting the services, and confirm
normal startup reports `V4 shared authorization OK`. A stale signature must not
be treated as a signed build merely because the daemon continues in shared mode.
