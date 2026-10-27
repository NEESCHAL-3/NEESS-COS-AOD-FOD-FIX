# Updating FOD/AOD on an existing Rodin ColorOS port

## Build and test

Use Android NDK r30, a Rust Android target, CMake, Ninja and a host C++ compiler:

```bash
rustup target add aarch64-linux-android
./scripts/build_aod.sh
./scripts/build_fod.sh
```

The AOD build runs power-state tests, including the ColorOS dump that contains
both `mWakefulness=Dozing` and `mWakefulness=3`. The FOD build runs an event-pipe
regression using the same nonblocking drain helper as production. It checks
single-event return, empty input, bounded bursts and hardware-edge precedence.

Copy the resulting artifacts into the ROM payload only after testing:

```bash
cp aod/target/aarch64-linux-android/release/nees-rodin-aodd system/bin/nees_aodd
cp fod/native/build/librodin_fp_compat.so vendor/lib64/librodin_fp_compat.so
```

## Permanent ROM installation

Back up the ROM's current files, labels and sensor HAL hash first. The AOD
binary belongs at `/system/bin/nees_aodd` (root:root, 0755, `shell_exec`); its
existing init service runs in `u:r:shell:s0`. The fingerprint library belongs
at `/vendor/lib64/librodin_fp_compat.so` (root:root, 0644, `vendor_file`).

The system file-context source must contain:

```text
/system/bin/nees_aodd -- u:object_r:shell_exec:s0
```

Do not label the daemon `system_file`: that fails the shell-domain entry-point
check under Enforcing. Preserve the init service and existing fingerprint
ELF dependency. If the dependency is already installed, updating the library
does not require adding it again. Retain the real fingerprint HAL's
authentication, enrollment, lockout and LHBM-ready behavior.

Use `./bake_into_rom.sh /path/to/unpacked-rom` when assembling an image. The
installer preserves an existing `sensors.mt6899.so` by default. Set
`NEES_INSTALL_SENSOR_HAL=1` only when intentionally installing the wrapper
after reconciling any independent sensor fixes. Regenerate compiled file
contexts during image packing if the ROM uses them.

On a writable, already-running development ROM, copy files atomically, set
ownership and mode, restore labels from the corrected file-context map, and
restart `nees_aodd` and `mfp-daemon`. Reboot once to verify the permanent init
path. A live copy alone is not sufficient evidence of a fresh-boot result.
Do not change SELinux to permissive or add broad allow rules for a mislabeled
executable.

## Signature notices

Current shared-mode runtime code logs a NEES4 signature mismatch and continues;
`--verify-only` returns a failure for a mismatch. Rebuilding either binary or
changing the bound sensor HAL/build.prop invalidates an older build signature.
This notice must not be mistaken for a daemon crash or used to explain a
SELinux entry-point denial. Use `scripts/sign_nees4.sh` with the private key
outside Git when a matching signed build manifest is required. Never publish
the private key. Shared-mode startup is not proof that the manifest verified.
The native fingerprint HAL still performs biometric authentication.

## Acceptance checks

Test each available touchscreen/FOD pairing, not merely its vendor property:

1. Normal lock-screen unlock and an app biometric prompt.
2. Screen-off unlock, quick repeat attempts, and a finger held after success.
3. Lift/release: highlight goes away, touch re-arms correctly, no stale DOWN.
4. AOD off: ordinary supported pickup/touch wake shows the native hint.
5. AOD on: clock, hint, all-day and power-saving hide still work; changing
   AOD mode without restarting the daemon is supported.
6. After display-service restart and after reboot, successful unlock leaves
   the interactive panel visible. Screen-off hint hide must not blank it.
7. Enforcing stays enabled; inspect relevant AVCs and confirm neither daemon
   is restarting. Check the sensor HAL hash is unchanged if not updating it.

An 8 ms polling interval is a fallback scheduling target, not a promise of
8 ms biometric authentication. Evdev notification wakes immediately. Keep
the post-authentication animation grace and real panel readiness intact.

For rollback, restore the saved binaries and matching labels, then restart
their services. Keep recovery access for image/JAR changes. Proximity is
documented separately in [the framework guide](../framework/PROXIMITY.md).

## Hardware backends and validation scope

The compatibility library supplements the native fingerprint HAL; it cannot
replace a missing hardware backend. Preserve the ROM's original
`/odm/lib64/hw/fingerprint.goodix_fod.so` and
`/odm/lib64/hw/fingerprint.jiiov.so`, plus their dependencies. Their absence
can leave the AIDL service registered while cleanup and authentication remain
queued. Check the daemon's native initialization log and the Xiaomi fingerprint
extension service, not only `init.svc.mfp-daemon=running`.

If either backend is missing, restore it from the matching Rodin vendor/ODM
release and verify its dependencies, ownership and file label. Do not copy
OPPO fingerprint drivers or force a sensor vendor property to hide discovery
failure. Do not delete enrollment data as a first repair step.

On the connected Goodix device, normal fingerprint unlock, screen-off/AOD
recovery, display visibility and both DT2W preference states were checked
under Enforcing. FocalTech/Jiiov combinations still require physical testing.
Gentle pickup/touch hint reliability needs further validation; the shared HAL
route alone is not proof of identical wake behavior on every panel.

DT2W integration and its source patcher are documented in
[the framework guide](../framework/DT2W.md).
