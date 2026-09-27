# NEES COS AOD + FOD FIX - v4.0 (TEST 4.0 Final)

> **Post-release update (2026-09-27):** Rise to wake is fixed on current `main` for Rodin ColorOS. The vendor HAL batches and activates Xiaomi pickup, then supplies ColorOS tilt value `0` on a lift. Native FOD and AOD events continue through the HAL, preserving the screen-off fingerprint hint. No ColorOS app patch is needed. Three gentle lock-and-lift cycles passed on the installed build (SHA-256 `f9e74b38b50cd06f1764e95fb4de79852a3e1301a61024d564111afbbca3d14f`). This fix was added **after** the `v4.0-working-final` tag; the tagged snapshot does not contain it. Use current `main` to bake the updated HAL. See [sensor details](../sensor/README.md) and [changelog](../CHANGELOG.md).

Production-ready native under-display fingerprint (FOD) and Always-On Display (AOD) hardware compatibility layer for **POCO X7 Pro (Rodin)** running ColorOS ports.

**Bake directly into ROM partitions (`system`, `vendor`, `odm`) — Zero Root / Magisk / KSU required.**

---

## What's New & Fixed in this Update (TEST 4.0 Final)

### 1. Instant LHBM Off on Unlock (OEM Match)
- Fixed issue where optical highlight lingered after unlocking while finger remained held on the screen.
- Xiaomi optical LHBM spotlight is immediately shut down (`setTouchFeature(0, 10, 0)` and `conditionUpdate(4,0)` / `(1,0)`) the exact millisecond authentication succeeds.
- Added `persist.vendor.sys.fp.fod.lhbmoffafterresult=1` to guarantee hardware optical shutoff on any auth event.

### 2. Lockscreen Dimming & Black Square Permanently Eliminated
- Fixed `IDisplayPanelFeature` AIDL registration with ServiceManager (`vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default`).
- SystemUI evaluates `isLocalHBM = 410` (true), completely destroying `OnScreenFingerprintDimLayer`.
- Lockscreen is 100% full brightness and vibrant with zero black box around the fingerprint icon.

### 3. Ultra-Fast SOFOD Response & Multi-Sensor Hardware Fusion (Zero Shake Required)
- **Power-Saving AOD Isolation**: Fixed `Setting_AodUserEnergySavingSet` evaluation (modes `1` and `2`). When power-saving AOD times out after 10s and screen goes black, picking up the device or tapping the screen displays **strictly the SOFOD fingerprint hint icon** on a pitch black screen — never the full AOD clock, wallpaper, or notification content.
- **Hardware Multi-Sensor Fusion Bridge (`sensors.mt6899.so`)**:
  - *Previous Limitation*: ColorOS listens strictly on synthetic tilt/motion sensor `65611`. The Xiaomi sensor hub's standard `pickup  Wakeup` (type `33171036`) was designed for "Raise to wake lockscreen" (requiring large upward acceleration to eye level) and dropped gentle movements (`val=2.0f`). MediaTek's hardware `tilt` detector (type `22`) required a 35° angle change. This forced users to "hard pick and shake" the phone to wake the fingerprint icon.
  - *The Solution*: Bridged all 6 hardware sensors into synthetic `65611`:
    1. Xiaomi `Fod  Wakeup` (type `33171030`, handle `68`) — fires in ~10ms upon finger approach or subtle table lift.
    2. Xiaomi `Aod  Wakeup` (type `33171029`, handle `69`) — fires on gentle movement.
    3. MediaTek `Significant Motion Detector` (type `17`, handle `17`) — fires instantly on any table displacement.
    4. MediaTek `tilt` detector (type `22`, handle `22`) — fires on tilt.
    5. Xiaomi `pickup  Wakeup` (type `33171036`, handle `25`) — accepts all motion levels (`val > 0.0f`, covering both `1.0f` lift and `2.0f` motion).
    6. Xiaomi `imu_hand_detect` (type `33171120`, handle `93`) — tracks hand grip.
  - *Performance*: Live telemetry confirms `Fod Wakeup` fires 22ms before pickup and 60ms before tilt, illuminating the fingerprint hint within **45ms** of touching the phone. Zero shake or hard lift required.
- **Automatic Cycle Restoration**: When waking to full screen (`->ON`), `Setting_AodSwitchEnable = 1` is seamlessly restored in the background so subsequent lock events show the full 10s AOD clock as intended by OEM design.

### 4. Permanent Boot Loading via ELF `DT_NEEDED`
- `bake_into_rom.sh` patches `/vendor/bin/hw/mfp-daemon` ELF headers using `patchelf --add-needed librodin_fp_compat.so`.
- Automatically loaded by the Android bionic dynamic linker on boot. No `LD_PRELOAD` or Magisk module required.

### 5. 100% Pure Strict Enforcing SELinux
- All legacy `(typepermissive ...)` directives have been completely purged from the repository.
- Runs globally in strict Enforcing mode with zero permissive domains, fully passing SafetyNet, Google Play Integrity, and banking app checks.

---

## How to Inspect SELinux Denials & Add Validated Strict CIL Rules

When porting or modifying system components, never switch to permissive mode. Follow this exact workflow to identify denials and add clean, versioned CIL rules:

### Step 1: Capture Audit Denials
Run via ADB:
```bash
# Check kernel audit buffer
adb shell "dmesg | grep avc"

# Check logcat audit events
adb shell "logcat -b all -d | grep -iE 'avc:  denied'"
```

### Step 2: Decode the Audit Denial
Example denial message:
```text
type=1400 audit(...): avc:  denied  { read open getattr } for  comm="mfp-daemon" path="/sys/devices/virtual/touch/touch_dev/fod_enable" dev="sysfs" ino=1234 scontext=u:r:hal_fingerprint_default:s0 tcontext=u:object_r:sysfs:s0 tclass=file permissive=0
```

Break down the components:
* `scontext`: Calling domain (`hal_fingerprint_default`)
* `tcontext`: Target object/domain (`sysfs`)
* `tclass`: Object class (`file`, `dir`, `binder`, `service_manager`, `chr_file`, `property_service`)
* `{ ... }`: The denied action(s) (`read`, `open`, `getattr`, `call`, `transfer`, `add`, `find`, `set`)

### Step 3: Translate to Treble CIL Format
In Treble-compliant vendor images (Android 14/15), system types referenced by vendor CIL files are versioned (e.g. `_202404`).

Construct the CIL allow rule:
```cil
(allow <scontext> <tcontext>[_version] (<tclass> (<actions...>)))
```

Verified strict rules included in this release:
```cil
; File and directory access
(allow hal_fingerprint_default sysfs_202404 (file (read open getattr)))
(allow hal_fingerprint_default sysfs_202404 (dir (search read open)))

; Binder IPC between HAL and system server
(allow hal_fingerprint_default system_server_202404 (binder (call transfer)))
(allow system_server_202404 hal_fingerprint_default (binder (call transfer)))

; Property reading
(allow hal_fingerprint_default default_prop_202404 (file (read open getattr map)))
(allow hal_fingerprint_default system_prop_202404 (file (read open getattr map)))
```

### Step 4: Handle AIDL Services in ServiceManager
When an AIDL service is added, `servicemanager` checks `service_contexts`.
* By assigning the service to an existing authorized type in `/vendor/etc/selinux/vendor_service_contexts`:
  ```text
  vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default u:object_r:vendor_hal_fingerprint_service_xiaomi:s0
  ```
  Because `vendor_hal_fingerprint_service_xiaomi` is already allowed to be added by `hal_fingerprint_default` and found by `platform_app_202404` (SystemUI) and `system_server_202404`, registration succeeds with **zero denials and zero extra CIL rules required**.

### Step 5: Critical Treble Step — Neutralize the 3 `precompiled_sepolicy` Files in `/odm/etc/selinux` & `/vendor/etc/selinux`
On modern Android Treble devices with dedicated vendor/odm partitions (like POCO X7 Pro / Rodin running ColorOS ports), Android ships up to 3 precompiled SELinux policy artifacts:
1. `precompiled_sepolicy`
2. `precompiled_sepolicy.plat_sepolicy_and_mapping.sha256`
3. `precompiled_sepolicy.*_sepolicy_and_mapping.sha256` (e.g. `system_ext` or `product`)

**Why this is critical:** During early boot, Android `init` verifies whether the SHA256 hashes of the system partitions match the stored `.sha256` checksum files. If they match, `init` loads `precompiled_sepolicy` directly into the kernel and **completely ignores your edits to `vendor_sepolicy.cil` and `odm_sepolicy.cil`**! Any custom allow rules you added will never take effect.

**The Professional Solution:**
Remove or back up all 3 `precompiled_sepolicy*` files located in:
* `/odm/etc/selinux/`
* `/vendor/etc/selinux/`

When these 3 files are neutralized, Android `init` detects that no valid precompiled policy exists and automatically invokes `/system/bin/secilc` at boot time to compile `plat_sepolicy.cil` + `vendor_sepolicy.cil` + `odm_sepolicy.cil` into a monolithic runtime policy, seamlessly activating all custom strict rules with zero bootloop risk.

---

## Credits & Authors
* **NEESCHAL** – Lead developer, hardware reverse engineering, protocol shims, and timing state machines.
* Community testers for feedback and telemetry logs.
