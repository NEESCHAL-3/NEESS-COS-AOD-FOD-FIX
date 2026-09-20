# ColorOS AOD Framework Patch

This repo contains the source-level AOD modification only, not the OEM `oplus-services.jar`.

Patch: `patches/OplusFeatureAOD.property-gated.smali`

The tested change gates the Rodin DOZE behavior with:
`persist.sys.rodin.aod_keep_doze=1`

No SystemUI patch is required.
