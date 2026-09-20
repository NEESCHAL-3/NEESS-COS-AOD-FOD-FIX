# Complete Rodin ColorOS AOD + FOD Package

Included source-side components:

- `aod/` - AOD daemon + NEES4 verifier
- `fod/` - fingerprint + DisplayPanelFeature compatibility
- `sensor/` - Xiaomi pickup 33171036 -> Oplus tilt 65611 compatibility HAL
- `framework/` - property-gated OplusFeatureAOD patch
- `signing/` - developer self-signing workflow

No SystemUI patch is required.

Known-good tested hashes:

- AOD: `3e48477816c5025027c5eace3e4e874ad81650aa105a4e4579ee3f6082329a1a`
- FOD: `e0e74932acc4eb79345343aa77f40c965a92606098e2f3b11229e4b348aeaffa`
- Sensor HAL: `4f21537899e0ad9bd6de3722d77172c63f82c0a1ce831f2f88468588b68f6cb8`
