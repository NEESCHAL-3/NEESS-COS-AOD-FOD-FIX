# POCO X7 Pro (Rodin) - Unpacked ROM Bake-In Guide (TEST 4.0)

This package allows you to integrate the working FOD (Fingerprint on Display) and AOD (Always on Display) fixes directly into your unpacked ColorOS ROM so it boots with working FOD & AOD out-of-the-box (no Magisk required).

---

## Method 1: Automated Script (Recommended)

Run `bake_into_rom.sh` and pass your unpacked ROM directory path:

```bash
chmod +x bake_into_rom.sh
./bake_into_rom.sh /path/to/unpacked_rom
```

The script will automatically:
1. Detect `system` and `vendor` locations (handles SAR layout).
2. Copy `nees_aodd` to `system/bin/` with permissions `0755`.
3. Copy `nees_aodd.rc` to `system/etc/init/` with permissions `0644`.
4. Copy `librodin_fp_compat.so` to `vendor/lib64/` with permissions `0644`.
5. Copy `zz_rodin_fp_compat.rc` to `vendor/etc/init/` with permissions `0644`.
6. Append the SELinux permissive & binder rules to `vendor/etc/selinux/vendor_sepolicy.cil`.
7. Append properties to `system/build.prop`.

---

## Method 2: Manual Integration

If you prefer to copy the files manually into your unpacked partitions:

### 1. System Partition (`system/`)
* **Copy Daemon:**
  `system/bin/nees_aodd` -> `[ROM]/system/bin/nees_aodd` (chmod `0755`, root:root)
* **Copy Init Service:**
  `system/etc/init/nees_aodd.rc` -> `[ROM]/system/etc/init/nees_aodd.rc` (chmod `0644`, root:root)
* **Append Properties:**
  Add the lines in `system/build.prop.append` to the end of `[ROM]/system/build.prop`:
  ```properties
  ro.nees.aod.auth=NEES4.da2be82c1aa5c6cd4ea892a1e36ad5e95ede8c4e427eae72ab0edb70f90455eb.ng4nZYoavfIg9S3JEtflMpx0+T7BeAjqIwnmwT0+MkovQdIQQYGDkeRYnIzcOWSrrLawyi9EyhimwDjFuTIoAA==
  sys.nees4.authorized=1
  ro.nees.fod.compat=1
  persist.sys.rodin.aod_keep_doze=1
  ro.oplus.aod.fod.support=true
  ```

### 2. Vendor Partition (`vendor/`)
* **Copy FOD Compat Shim:**
  `vendor/lib64/librodin_fp_compat.so` -> `[ROM]/vendor/lib64/librodin_fp_compat.so` (chmod `0644`, root:root)
* **Copy FP Hook Init Script:**
  `vendor/etc/init/zz_rodin_fp_compat.rc` -> `[ROM]/vendor/etc/init/zz_rodin_fp_compat.rc` (chmod `0644`, root:root)
* **Inject SELinux Rules:**
  Append the contents of `vendor/etc/selinux/vendor_sepolicy.cil.append` to:
  `[ROM]/vendor/etc/selinux/vendor_sepolicy.cil`

---

## What These Fixes Do in the ROM:

1. **`librodin_fp_compat.so`**: Intercepts `mfp-daemon` calls, translates Xiaomi Goodix fingerprint sensor ioctls/events into ColorOS biometrics AIDL, and returns `STATUS_OK` (0) for panel feature 217 (`OPLUS_FEATURE_AOD_SMOOTH`), fixing Seamless & Classic AOD.
2. **`nees_aodd`**: Manages the panel backlight (`/sys/devices/virtual/mi_display/disp_feature/disp-DSI-0/backlight`) during sleep, wake, and screen-off fingerprint touch. Toggles `Setting_AodSwitchEnable` between `0` and `1` so ColorOS native Screen-Off Fingerprint icon and wakeups function properly.
3. **`vendor_sepolicy.cil`**: Grants per-domain permissive mode to `hal_fingerprint_default` and `shell`, allowing hardware interaction while keeping the entire ROM globally Enforcing.
