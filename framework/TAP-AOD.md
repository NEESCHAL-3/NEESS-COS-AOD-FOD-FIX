# Screen-off single tap on Rodin

This adapts the touchscreen controller's real single-tap wake key to ColorOS's
native AOD callback. It belongs in the ROM's current `oplus-services.jar`;
SystemUI and Settings APKs remain unchanged.

| Screen-off action | Result |
| --- | --- |
| Single tap, AOD enabled and within its schedule | Clock and fingerprint hint |
| Single tap, AOD disabled | Fingerprint hint only |
| Pickup while the clock is hidden | Existing native hint behavior; no forced clock |
| Double tap | Existing ColorOS DT2W preference and policy |

The helper never enables the AOD master preference. When a permitted tap
restores hidden clock content, it sets `Setting_AodSwitchEnable` and dispatches
the native gesture. The updated AOD daemon carries that confirmed state through
DOZE entry, avoiding repeated settings subprocesses on this path. Its fresh
power-state check still prevents delayed backlight writes while awake.

## Input and sensor handling

Xiaomi touch mode 11 enables the hardware AOD tap detector. The tested Goodix
touchscreen emits Linux scan code 354 (`KEY_GOTO`). The framework accepts only
that physical touchscreen key while noninteractive, rejects repeats, and
debounces tap delivery by 300 ms. Mode 10 (FOD) and mode 14 (DT2W) remain separate.
There is no input polling loop. The callback uses a bounded 1500 ms partial
wakelock, released after delivery.

ColorOS gesture 16 normally reads an Oplus coordinate node absent on this port.
The bridge instead dispatches its existing whole-display callback under the
policy's own lock. `16,0:0` is a whole-display target anchor for a positionless
wake key, not a measured coordinate. It must not be used for localized gestures
or authentication. Fingerprint matching and biometric thresholds are unchanged.

The detector requires confirmed FAR from Android proximity type 8. If Xiaomi
NonUI type 33171027 registers, it also requires a known uncovered value: stock
uses 0 for uncovered and 1/2 for pocket mode. Unknown or covered readings block
delivery. Use the real sensor values; do not substitute a fabricated near
reading or darkness-only suppression.

The HAL path does not select a fingerprint vendor. Physical single-tap testing
covered the connected Goodix touchscreen only; validate scan-code delivery and
sleep behavior on each other touch-controller/FOD pairing before shipping it.

## Integrate into the current ROM

Preserve all existing display, proximity and other JAR changes. Follow
[DT2W.md](DT2W.md) to compile both Java helpers, convert all generated classes
with d8, and disassemble them. Apply the DT2W context bootstrap first, then:

```bash
python3 framework/tools/patch_rodin_tap_aod.py /path/to/services-smali out/touch-smali/com/android/server/wm --dry-run
python3 framework/tools/patch_rodin_tap_aod.py /path/to/services-smali out/touch-smali/com/android/server/wm
```

The root must contain every disassembled services DEX tree. The patcher checks
the known method/register layouts before editing, installs the key-route hook
in `PhoneWindowManagerExtImpl`, refreshes when native gesture state changes,
and copies both helper families. A route-version field prevents the helper from
arming mode 11 when only the DT2W bootstrap has been installed. Other ROM
layouts require source review rather than bypassing the checks.

Reassemble the affected DEX entries, replace only those entries in the current
JAR, align it and run the target Android ART verifier before installation.
Build `nees_aodd` with `scripts/build_aod.sh`; install that build alongside the
framework changes. Bake the JAR into `/system/framework/oplus-services.jar`
(root:root, 0644, original label), and the daemon into `/system/bin/nees_aodd`
(root:root, 0755, `shell_exec`). Preserve the existing init service and explicit
file-context mapping. Keep SELinux Enforcing; this change adds no policy rules.
Keep originals available for rollback before rebooting.

The optional Secure setting `rodin_tap_to_aod_enabled` is 1 by default when the
complete route is installed. Set it to 0 to disable the bridge, or 1 to enable
it again. Rollback requires restoring both the original JAR and daemon, then
rebooting; disabling the setting alone only disables tap routing.

## Verify on hardware

Check startup for `Native single-tap bridge initialized` and HAL result 0.
After the timed AOD clock hides, repeat taps with AOD on, then with it off.
Verify pickup remains hint-only, normal and screen-off fingerprint unlock stay
visible, and the DT2W toggle works in both states. Test scheduled AOD separately.
Check relevant AVCs under Enforcing and measure deep sleep after leaving the
phone idle.
