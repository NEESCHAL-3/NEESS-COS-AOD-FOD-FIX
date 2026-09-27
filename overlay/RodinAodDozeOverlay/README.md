# Rodin AOD Doze framework overlay

This is source reconstructed from the installed `/vendor/overlay/RodinAodDozeOverlay.apk` on the Rodin ColorOS build. The APK is a static resource overlay for `android` (`framework-res`), package `com.rodin.aoddoze.overlay`, priority `999`.

It overrides `config_dozeComponent` with ColorOS SystemUI's `DozeService` and `config_displayLightSensorType` with `android.sensor.light`. Both mappings are active in the installed IDMAP. The source also retains `config_dozeAfterScreenOff=true` from the APK; the current framework has no resource by that name, so this value is **not mapped** and has no effect on this build.

The known-working APK in `vendor/overlay/RodinAodDozeOverlay.apk` was pulled from the phone. Its SHA-256 is `6dd1c111de906fbd9c829fa46a0811ce8dd50aeb2a7bab5726ac368aa35054f7`. The build script creates a new APK from this source; its signature and byte hash will differ from the extracted APK.

Build with Android SDK 35 and a private signing key:

```bash
RRO_STORE_PASS='your-store-password' RRO_KEY_PASS='your-key-password' \
  AAPT2=/path/to/android-sdk/build-tools/35.0.0/aapt2 \
  ./build.sh /path/to/android-35/android.jar /path/to/signing.jks /path/to/output.apk
```

Set `RRO_KEY_ALIAS` if the keystore contains multiple keys. Keep keystores and passwords out of this repository. After baking a rebuilt APK, check `cmd overlay dump com.rodin.aoddoze.overlay` and confirm both string mappings. The ROM bake script uses the extracted known-working APK by default.
