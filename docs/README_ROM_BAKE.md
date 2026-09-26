# POCO X7 Pro (Rodin) - ROM Bake-In Installer (TEST 4.0 Final)

Native ColorOS 15 AOD & Optical FOD Hardware Compatibility Layer for **POCO X7 Pro (Rodin)**.

This kit bakes all fixes directly into your unpacked ROM partitions (`system`, `vendor`, `odm`).
**No root, Magisk, or KernelSU required** on the flashed device. Works 100% out-of-the-box on clean flash with **100% Pure Strict Enforcing SELinux** (zero permissive domains).

---

## What's Included & Fixed in TEST 4.0 Final

1. **Instant LHBM Off on Unlock (OEM Match)**:
   - Immediately turns off Xiaomi optical LHBM spotlight (`setTouchFeature(0, 10, 0)`) the exact millisecond authentication succeeds.
   - Prevents the optical highlight from lingering while the user holds their finger down after unlocking.
2. **Lockscreen Dimming & Black Box Fixed**:
   - Registers `vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default` in ServiceManager.
   - Evaluates `isLocalHBM = 410` (true), completely destroying `OnScreenFingerprintDimLayer` (full brightness lockscreen, zero black box).
3. **DT_NEEDED ELF Baking**:
   - Injects `librodin_fp_compat.so` into `/vendor/bin/hw/mfp-daemon` ELF headers via `patchelf`.
   - Loaded automatically by the bionic dynamic linker at boot without `LD_PRELOAD`.
4. **100% Pure Strict Enforcing SELinux**:
   - Zero permissive domains (`typepermissive` completely removed).
   - Full compliance with Google Play Integrity / CTS / banking apps.
5. **Seamless & Classic AOD**:
   - Full-day AOD and energy-saving modes supported without watchdog crashes.

---

## How to Bake Into Your ROM

### Prerequisites on Build Host
- Linux / WSL
- `patchelf` (`sudo apt install patchelf`)

### Usage
```bash
chmod +x bake_into_rom.sh
./bake_into_rom.sh /path/to/unpacked_rom_root
```

Example:
```bash
./bake_into_rom.sh /home/neeschal/rom_work/unpacked
```

### What the Script Does Automatically:
1. Installs `nees_aodd` into `system/bin/nees_aodd` (`0755`, `u:object_r:system_file:s0`).
2. Installs `nees_aodd.rc` into `system/etc/init/nees_aodd.rc` (`0644`).
3. Installs `librodin_fp_compat.so` into `vendor/lib64/librodin_fp_compat.so` (`0644`).
4. Injects `DT_NEEDED: librodin_fp_compat.so` into `vendor/bin/hw/mfp-daemon` using `patchelf`.
5. Installs `zz_rodin_fp_compat.rc` into `vendor/etc/init/zz_rodin_fp_compat.rc` (`0644`).
6. Appends strict enforcing CIL rules to `vendor/etc/selinux/vendor_sepolicy.cil`.
7. Appends `IDisplayPanelFeature` AIDL mapping to `vendor/etc/selinux/vendor_service_contexts` and `odm/etc/selinux/odm_service_contexts`.
8. Safely renames stale `precompiled_sepolicy` so init compiles fresh CIL policy on first boot.
9. Updates `plat_file_contexts` and `vendor_file_contexts` for image repackers (`erofs`/`ext4`).
10. Appends optimized FOD/AOD properties to `system/build.prop`.

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

Check existing versioned symbols in your vendor image:
```bash
grep -E "\(type (sysfs_|system_server_|default_prop_)" /vendor/etc/selinux/vendor_sepolicy.cil
```

Construct the CIL allow rule:
```cil
(allow <scontext> <tcontext> (<tclass> (<actions...>)))
```

Examples of verified strict rules:
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
* If you assign the service to an existing authorized type:
  In `/vendor/etc/selinux/vendor_service_contexts`:
  ```text
  vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default u:object_r:vendor_hal_fingerprint_service_xiaomi:s0
  ```
  Because `vendor_hal_fingerprint_service_xiaomi` is already allowed to be added by `hal_fingerprint_default` and found by `platform_app_202404` (SystemUI) and `system_server_202404`, registration succeeds with **zero denials and zero extra CIL rules required**.

### Step 5: Handle `precompiled_sepolicy`
If your vendor partition contains `/vendor/etc/selinux/precompiled_sepolicy`, Android `init` will load that binary blob directly and **completely ignore your edits** to `vendor_sepolicy.cil`.
* Always rename or remove `precompiled_sepolicy` and `precompiled_sepolicy.plat_sepolicy_and_mapping.sha256`.
* On first boot, `init` will automatically invoke `secilc` to compile your updated `vendor_sepolicy.cil` into a fresh, unified policy.

---

After running the script, repack your `system.img`, `vendor.img`, `odm.img` (or `super.img`) and flash!
