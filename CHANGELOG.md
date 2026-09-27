# Changelog

## 2026-09-27 - Jiiov FOD field report

- One user confirmed working FOD on a Jiiov device with the current build.
- No Jiiov-specific source change was needed; broader Jiiov hardware coverage remains unverified.

## 2026-09-27 - Rise to wake on Rodin ColorOS

- Route ColorOS tilt sensor requests to Xiaomi pickup with the required batch-before-activate sequence.
- Deliver pickup events as tilt value `0` so `ScreenOffGestureService` wakes the display on gentle lifts.
- Preserve native FOD, AOD, significant motion, and hand sensor requests and events, keeping the screen-off fingerprint hint functional.
- Update the tracked `vendor/lib64/hw/sensors.mt6899.so` used by `bake_into_rom.sh` to the tested build.
- Tested three consecutive gentle lock-and-lift cycles on the installed vendor HAL.
- Installed sensor HAL SHA-256: `f9e74b38b50cd06f1764e95fb4de79852a3e1301a61024d564111afbbca3d14f`.

## NEES4 V2 - final tested development state

### Added

- shared NEES4 signed authorization
- volatile `sys.nees4.authorized` handoff
- asynchronous DisplayPanelFeature registration
- unauthorized FOD pass-through behavior
- pending physical FOD DOWN replay
- physical UP cleanup while listener is disabled

### Fixed

- immediate-lock FOD dead state
- stale physical-down/session state
- post-unlock over-brightness
- all-day AOD capability negotiation
- startup failure caused by fork/exec verification inside `mfp-daemon`

### Tested hashes

```text
AOD_DAEMON_SHA256=3e48477816c5025027c5eace3e4e874ad81650aa105a4e4579ee3f6082329a1a
FOD_COMPAT_SHA256=e0e74932acc4eb79345343aa77f40c965a92606098e2f3b11229e4b348aeaffa
SENSOR_HAL_SHA256=4f21537899e0ad9bd6de3722d77172c63f82c0a1ce831f2f88468588b68f6cb8
SYSTEM_BUILD_PROP_SHA256=428572644483493dbacd16a278666a2b3e5be4e4756c80fbad4e07128706169e
```
