# NEESS COS AOD + FOD FIX

Native AOD + under-display fingerprint compatibility for **Rodin / POCO X7 Pro ColorOS ports**, designed to avoid patching SystemUI for the core FOD/AOD path.

This repository is intended to hold the final reproducible source for:

- Rodin FOD compatibility shim (`librodin_fp_compat.so`)
- Rodin AOD helper daemon (`nees_aodd`)
- NEES4 shared authorization/signing workflow
- build/install/verification documentation
- device-side regression tests

> **Never commit the private Ed25519 signing key.**

## Current tested state

The final tested branch provides:

- FOD unlock on ColorOS without SystemUI patching
- rapid lock -> immediate FOD press race fix
- physical FOD DOWN/UP tracking even while the listener is not ready
- pending physical DOWN replay when an auth session becomes ready
- session re-arm after contact release
- Local-HBM capability reporting
- Oplus DisplayPanelFeature compatibility service
- all-day AOD capability reporting
- AOD panel brightness ownership
- FP-only wake brightness handling
- correct brightness release after fingerprint unlock
- pickup/tilt sensor compatibility for Oplus wake behavior
- shared NEES4 ROM-bound authorization for the AOD/FOD pair

## Target

- Device family: Rodin / POCO X7 Pro
- SoC: MediaTek MT6899 family
- Android userspace target: Android 35
- Real FOD hardware ownership remains with Xiaomi `mfp-daemon`

The shim intentionally lets Xiaomi choose the actual fingerprint/touch backend rather than hard-coding one vendor.

## Architecture

```text
ColorOS SystemUI / framework
        |
        v
librodin_fp_compat.so
        |
        | Oplus fingerprint/session compatibility
        | Oplus DisplayPanelFeature AIDL
        | FOD lifecycle + race handling
        v
Xiaomi mfp-daemon
        |
        +--> actual FP backend
        +--> Xiaomi Local-HBM/panel path

nees_aodd
        |
        +--> verifies signed NEES4 manifest
        +--> publishes volatile sys.nees4.authorized=1
        +--> handles AOD/FP-only panel brightness timing
```

## DisplayPanelFeature

- GET `211` -> `0x410`
  - `0x10`: Local-HBM
  - `0x400`: Local-HBM acceleration capability
- GET `217` -> `0xF`
  - direct AOD / no forced OFF-before-DOZE
  - smooth transition capability
  - panoramic/full-screen capability
  - panoramic all-day capability
- FOD display features `22` / `28`: compatibility ACK path
- SET `217`: currently unsupported by the shim

## AOD daemon

Current tested values:

```text
AOD panel = 28
FP-only   = 100
```

The daemon releases FP brightness ownership immediately when the screen reaches ON, preventing the launcher from remaining over-bright after FOD unlock.

## NEES4 shared authorization

One Ed25519-signed manifest binds the exact ROM + AOD + FOD pair:

```text
NEES_RODIN_SHARED_V4
ROM_ID=<rom id>
SYSTEM_BUILD_PROP_SHA256=<canonical system/build.prop hash>
SENSOR_HAL_SHA256=<sensor HAL hash>
AOD_DAEMON_SHA256=<nees_aodd hash>
FOD_COMPAT_SHA256=<librodin_fp_compat.so hash>
```

`SYSTEM_BUILD_PROP_SHA256` is calculated with the `ro.nees.aod.auth=` line excluded.

ROM property:

```text
ro.nees.aod.auth=NEES4.<ROM_ID>.<BASE64_ED25519_SIGNATURE>
```

Boot flow:

1. `nees_aodd` verifies the signed NEES4 manifest.
2. On success it publishes volatile `sys.nees4.authorized=1`.
3. The FOD shim waits for that runtime authorization.
4. Protected Oplus compatibility behavior activates only after authorization succeeds.

The runtime property is only a handoff. The signed manifest is the trust root.

## Final tested hashes

```text
AOD_DAEMON_SHA256=3e48477816c5025027c5eace3e4e874ad81650aa105a4e4579ee3f6082329a1a
FOD_COMPAT_SHA256=e0e74932acc4eb79345343aa77f40c965a92606098e2f3b11229e4b348aeaffa
SENSOR_HAL_SHA256=4f21537899e0ad9bd6de3722d77172c63f82c0a1ce831f2f88468588b68f6cb8
SYSTEM_BUILD_PROP_SHA256=428572644483493dbacd16a278666a2b3e5be4e4756c80fbad4e07128706169e
```

Any AOD/FOD rebuild changes hashes and requires a new NEES4 signature.

## Recommended repo layout

```text
NEESS-COS-AOD-FOD-FIX/
├── README.md
├── SECURITY.md
├── BUILDING.md
├── INSTALLATION.md
├── TESTING.md
├── CHANGELOG.md
├── .gitignore
├── fod/
│   ├── native/
│   │   ├── rodin_fp_compat.cpp
│   │   └── CMakeLists.txt
│   └── zz_rodin_fp_compat.rc
├── aod/
│   ├── Cargo.toml
│   ├── Cargo.lock
│   ├── src/main.rs
│   └── nees_aodd.rc
├── signing/
├── docs/
└── scripts/
```

## Quick start

```bash
bash scripts/export_repo.sh
bash scripts/build_fod.sh
bash scripts/build_aod.sh
bash scripts/sign_nees4.sh
bash scripts/verify_device.sh
```

## Security

Never commit:

- `nees_rodin_private.pem`
- private-key backups
- unencrypted key archives
- temporary signing directories containing private material

Inspect `git status` before every push.

## License

No open-source license is included in this kit. Add the license you actually want before making the repository public.

## Complete package

- `aod/` - AOD daemon + NEES4 verifier
- `fod/` - fingerprint + DisplayPanelFeature compatibility
- `sensor/` - pickup/tilt sensor compatibility HAL
- `framework/` - property-gated AOD framework patch
- `signing/` - developer self-signing tools

No SystemUI patch is required. The official private signing key is not stored in this repository.
