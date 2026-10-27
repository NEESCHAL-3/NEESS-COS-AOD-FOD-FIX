# Rodin DT2W controlled by ColorOS Settings

ColorOS's `ColorTouchNodeController` retains the user toggle and proximity
gating, then delegates its node-flag 1 request to `ColorTouchNodeManagerService`.
On this port the final backend expects `vendor.oplus.hardware.touch.IOplusTouch`;
Rodin instead provides Xiaomi's `ITouchFeature` HAL.

The included Java helper translates only primary-device node flag 1 values
`0` and `1` to Xiaomi `setModeValue(0, 14, value)` (transaction 9,
`Touch_Doubletap_Mode`). It checks the actual service descriptor and reports
the HAL result. Other flags and devices retain the original backend.
A system-server ContentObserver follows
`Settings.Secure.oplus_customize_gesture_double_touch`, synchronizes Android's
`double_tap_to_wake` value, and applies the real HAL mode. Boot and screen
events retry after service initialization. The existing proximity lock still
blocks enabling gestures while covered. Generic gesture writes cannot enable
DT2W when the ColorOS preference is off.
The backend patch remains after ColorOS's existing policy checks. It does not force DT2W on at boot, bypass pocket/proximity gating,
change Settings.apk, or require a root manager at runtime.

## Rebuild the ROM's current services JAR

Start from the ROM's current `oplus-services.jar`, preserving its proximity,
display and other fixes. Compile `framework/java/com/android/server/wm/RodinTouchBridge.java`
against the target Android SDK, convert it using d8, and disassemble its DEX:

```bash
mkdir -p out/touch-classes out/touch-dex
javac -source 17 -target 17 -cp "$ANDROID_JAR" -d out/touch-classes framework/java/com/android/server/wm/RodinTouchBridge.java
d8 --min-api 34 --lib "$ANDROID_JAR" --output out/touch-dex out/touch-classes/com/android/server/wm/*.class
baksmali disassemble --api 35 out/touch-dex/classes.dex -o out/touch-smali
python3 framework/tools/patch_rodin_dt2w.py /path/to/services-smali out/touch-smali/com/android/server/wm/RodinTouchBridge.smali --dry-run
python3 framework/tools/patch_rodin_dt2w.py /path/to/services-smali out/touch-smali/com/android/server/wm/RodinTouchBridge.smali
```

The patcher copies all helper classes and initializes the observer from the
touch service's `setContext`. If this ROM includes `RodinRefreshBridge`, its
existing display bootstrap also initializes the observer; initialization is
idempotent. On another port, verify the context hook actually runs by checking
the `Watching the ColorOS double-tap preference` startup message. Do not ship
without that boot-path verification.

The patcher validates the expected ColorOS method/register layout and stops
on other versions. Reassemble the affected DEX and replace only that entry
in the current JAR. Verify it with the target ART verifier before installation.
Bake it into `/system/framework/oplus-services.jar` with root:root, 0644 and
its original SELinux label, then reboot with rollback available.

## Validate the real toggle

Enable DT2W in ColorOS Settings, sleep the phone, and double-tap to wake it.
Disable DT2W in Settings, sleep it again, and confirm double-tap no longer
wakes it. Re-enable and repeat after reboot. Inspect `RodinTouchBridge` logs
for HAL result 0 and relevant AVCs under Enforcing. On Goodix, read
`/sys/devices/platform/goodix_ts.0/double_tap_enable` to cross-check state;
the bridge itself uses the HAL rather than that device-specific path.
Keep native touch-controller and proximity safeguards. A direct HAL test
alone does not establish that Settings is connected correctly.
