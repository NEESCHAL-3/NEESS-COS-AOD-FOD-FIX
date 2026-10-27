# ColorOS AOD Framework Patch

This repo contains the source-level AOD modification only, not the OEM `oplus-services.jar`.

Patch: `patches/OplusFeatureAOD.property-gated.smali`

The tested change gates the Rodin DOZE behavior with:
`persist.sys.rodin.aod_keep_doze=1`

No SystemUI patch is required.

For the ColorOS Settings double-tap toggle, see [DT2W.md](DT2W.md).
For hardware single-tap AOD and hint routing, see [TAP-AOD.md](TAP-AOD.md).
For the separate call-proximity framework correction, see [PROXIMITY.md](PROXIMITY.md).
