# Rodin proximity on ColorOS ports

This is a framework JAR fix. Do not replace `sensors.mt6899.so` to apply it.
Rodin exposes Elliptic Proximity as Android sensor type 8. Its Xiaomi offhand
sensor uses type 33171067, which collides with the proprietary proximity type
expected by this ColorOS framework. Using offhand for call proximity selects
the wrong sensor.

The patch disables that proprietary interception in two classes of
`oplus-services.jar`, allowing Android's `DisplayPowerProximityStateController`
to register the real type-8 sensor. It does not fabricate proximity readings.
It is independent of the touchscreen and fingerprint sensor vendor.

## Apply to the ROM's existing JAR

1. Back up the current `oplus-services.jar`, its permissions and context. Work
   from this ROM's current JAR so HDR, AOD and other patches are preserved.
2. Extract and disassemble its DEX files with a compatible smali tool. Locate
   `OplusDisplayPowerControllerExtImpl` and `OplusProximitySensorScreenOnOff`.
3. Run the included source patcher against the directory containing those
   extracted classes:

   ```bash
   python3 framework/tools/patch_rodin_proximity.py /path/to/extracted-smali --dry-run
   python3 framework/tools/patch_rodin_proximity.py /path/to/extracted-smali
   ```

4. Reassemble only the affected DEX file(s), preserving unrelated entries in
   the JAR. The patcher validates every expected method before writing and
   backs up each changed smali file. It stops if this ColorOS build differs.
5. Verify the rebuilt DEX/JAR with the target Android ART verifier before
   installing. Bake the JAR into `/system/framework/oplus-services.jar` with
   root:root ownership, 0644 permissions and its original SELinux label.
   Reboot to load the framework change. Keep a recovery rollback available.

The boolean hooks `registerPSensor`, `applyOplusProximitySensorLocked`,
`getUseProximityForceSuspendState` and
`isUseProximityForceSuspendStateChanged` return false. The proprietary
`setUseProximityForceSuspend` hook does nothing. Both classes are patched;
Android's ordinary proximity handling remains active.

## Verify on the target ROM

During a call, confirm SensorService registers Elliptic Proximity type 8 for
`DisplayPowerProximityStateController`, rather than offhand type 33171067.
Cover/uncover the sensing area and verify near turns the screen off and far
restores it. Test normal calls, speaker mode and repeated call endings, then
check for relevant SELinux denials while Enforcing. Do not mark a new ROM
verified using only the patcher's success message.

The installed Rodin sensor HAL may contain independent Rise-to-Wake/AOD fixes.
Preserve it when updating this JAR. The bake script preserves an existing
sensor HAL by default; replacing it requires `NEES_INSTALL_SENSOR_HAL=1`.
