# POCO X7 Pro (Rodin) - Unpacked ROM Integration & SELinux Guide (TEST 4.0)

This package contains everything required to bake the verified working **FOD (Fingerprint on Display)** and **AOD (Always On Display)** fixes directly into an unpacked ColorOS port for the POCO X7 Pro (Rodin).

---

## Summary of Root-Cause Fixes in TEST 4.0

| Issue | Root Cause | Solution in TEST 4.0 |
|---|---|---|
| **Daemon Crash Loop / Fork Bomb** | `nees_aodd` in TEST 3.8 was running `/system/bin/settings get secure Setting_AodSwitchEnable` on **every single logcat line**, spawning 100+ Java processes per second. Android watchdog (`MBrainServer`) was repeatedly killing `nees_aodd` every 30ms, dropping panel backlight to 0. | Restored **zero-fork stream processing** from original working source. Logcat is inspected purely with string matching (`line.contains()`) with zero process spawning. |
| **Seamless & Classic AOD Cutoff (2s)** | ColorOS `SmoothTransitionController` calls `setDisplayPanelFeature` for feature `217` (`OPLUS_FEATURE_AOD_SMOOTH`). In previous builds, `librodin_fp_compat.so` returned `-1` (unsupported), prompting ColorOS to abort smooth AOD and force the screen OFF after 2s. | Feature 217 is explicitly handled and returns `STATUS_OK` (0) in `librodin_fp_compat.so`. Seamless and Classic AOD stay lit without cutting off. |
| **Screen-Off Fingerprint (SOFOD) Dead** | Daemon was missing `settings_put("Setting_AodSwitchEnable", "0")` on `DOZE->OFF`. ColorOS believed AOD was still displaying and completely suppressed the screen-off fingerprint icon. | Restored exact state machine from original source: sets `Setting_AodSwitchEnable=0` on `DOZE->OFF`, lights panel (100) on `notifyWakeUpCallback type 1`, and restores `1` on screen wake (`->ON`). |
| **SELinux Enforcing Compatibility** | Previous test stripped SEPolicy rules, causing denials when running in Enforcing mode. | Clean **Per-Domain Permissive** policy (`hal_fingerprint_default` and `shell`). The **whole ROM stays globally Enforcing** (`getenforce` returns `Enforcing`, passing Play Integrity and Banking apps) while the two hardware daemons have full hardware access. |

---

## Package Directory Structure

```text
POCO_X7_Pro_ROM_BAKE_TEST_4_0/
├── bake_into_rom.sh                         # Automated 1-command ROM injection script
├── README_ROM_BAKE.md                      # This comprehensive guide
├── system/
│   ├── bin/
│   │   └── nees_aodd                       # Zero-fork AOD backlight & SOFOD daemon
│   ├── etc/
│   │   └── init/
│   │       └── nees_aodd.rc                # Init service configuration (u:r:shell:s0)
│   └── build.prop.append                   # Properties to append to system/build.prop
├── vendor/
│   ├── lib64/
│   │   └── librodin_fp_compat.so           # FOD compat shim with feature 217 ACK
│   └── etc/
│       ├── init/
│       │   └── zz_rodin_fp_compat.rc       # mfp-daemon preload & touch node permissions
│       └── selinux/
│           └── vendor_sepolicy.cil.append  # CIL rules for vendor_sepolicy.cil
└── sepolicy/
    ├── vendor_sepolicy.cil.append          # Pre-formatted CIL rules
    └── rodin_fod_aod.te                    # Source .te format for AOSP source compilation
```

---

## Method 1: Automated Integration (Recommended)

Run `bake_into_rom.sh` and supply the path to your unpacked ROM:

```bash
chmod +x bake_into_rom.sh
./bake_into_rom.sh /path/to/unpacked_rom
```

The script automatically:
1. Detects `system` and `vendor` partition roots (including SAR `system/system/` layout).
2. Copies `nees_aodd` to `system/bin/` (`0755`).
3. Copies `nees_aodd.rc` to `system/etc/init/` (`0644`).
4. Copies `librodin_fp_compat.so` to `vendor/lib64/` (`0644`).
5. Copies `zz_rodin_fp_compat.rc` to `vendor/etc/init/` (`0644`).
6. Appends CIL rules to `vendor/etc/selinux/vendor_sepolicy.cil`.
7. **Handles `precompiled_sepolicy`:** Backs up and removes any stale `precompiled_sepolicy` so Android `init` dynamically compiles your new CIL rules at boot.
8. Appends properties to `system/build.prop`.

---

## Method 2: Manual Integration

### Step 1: System Partition (`system/`)
1. **Copy AOD Daemon:**
   `system/bin/nees_aodd` -> `[ROM]/system/bin/nees_aodd`
   * Permissions: `0755` (`rwxr-xr-x`, root:root)
   * SELinux context: `u:object_r:system_file:s0`
2. **Copy Daemon Init Service:**
   `system/etc/init/nees_aodd.rc` -> `[ROM]/system/etc/init/nees_aodd.rc`
   * Permissions: `0644` (`rw-r--r--`, root:root)
   * SELinux context: `u:object_r:system_file:s0`
3. **Append Properties:**
   Add these lines to `[ROM]/system/build.prop`:
   ```properties
   ro.nees.aod.auth=NEES4.da2be82c1aa5c6cd4ea892a1e36ad5e95ede8c4e427eae72ab0edb70f90455eb.ng4nZYoavfIg9S3JEtflMpx0+T7BeAjqIwnmwT0+MkovQdIQQYGDkeRYnIzcOWSrrLawyi9EyhimwDjFuTIoAA==
   sys.nees4.authorized=1
   ro.nees.fod.compat=1
   persist.sys.rodin.aod_keep_doze=1
   ro.oplus.aod.fod.support=true
   ```

---

### Step 2: Vendor Partition (`vendor/`)
1. **Copy FOD Compat Shim:**
   `vendor/lib64/librodin_fp_compat.so` -> `[ROM]/vendor/lib64/librodin_fp_compat.so`
   * Permissions: `0644` (`rw-r--r--`, root:root)
   * SELinux context: `u:object_r:vendor_file:s0`
2. **Copy Hardware Hook Init Script:**
   `vendor/etc/init/zz_rodin_fp_compat.rc` -> `[ROM]/vendor/etc/init/zz_rodin_fp_compat.rc`
   * Permissions: `0644` (`rw-r--r--`, root:root)
   * SELinux context: `u:object_r:vendor_configs_file:s0`
3. **Append Vendor Properties:**
   Append these lines to `vendor.prop` (or `[ROM]/vendor/build.prop`):

```properties
# AIDL Fingerprint HAL
vendor.fingerprint.aidl.support=1

# Rodin Oplus FOD UI gate
persist.vendor.fingerprint.type=udfps_optical
persist.vendor.fingerprint.sensor_type=optical
persist.vendor.fingerprint.fod.enable=true
persist.vendor.fingerprint.animation=true
persist.vendor.fingerprint.sensor_location=504,2332,105
persist.vendor.fp.vendor=goodix

# Oplus optical fingerprint support
ro.oplus.biometrics.fingerprint.optical=true
ro.oplus.aod.support=true
ro.oplus.aod.fod.support=true
ro.vendor.fod.animation.support=true
persist.sys.fingerprint.animation=1
persist.sys.fp.fod.anim=1
persist.sys.fp.fod.screenoff=true

# Rodin Xiaomi FOD core
ro.hardware.fp.fod=true
ro.hardware.fp.tddi=true
ro.hardware.fp.fod.location=low
ro.hardware.fp.fod.touch.ctl.version=2.0
ro.hardware.fp.halworkmode=true
ro.hardware.fp.mievent=true
ro.hardware.fp.onetrack.period=3600000

# Rodin Goodix FOD values
persist.vendor.sys.fp.vendor=goodix_fod
persist.vendor.sys.fp.module=ofilm
persist.vendor.sys.fp.fod.optimize=true
persist.vendor.sys.fp.expolevel=0x88
persist.vendor.sys.fp.fod.location.X_Y=504,2332
persist.vendor.sys.fp.fod.size.width_height=210,210
persist.vendor.sys.fp.fp_anti_mistouch=true
persist.vendor.sys.fp.heartbeat=true

# Low brightness FOD thresholds
ro.hardware.fp.fod.lowlight.lux.threshold=3
ro.hardware.fp.fod.lowlight.brightness.threshold=411
```

---

### Step 3: SELinux Integration (CRITICAL)

#### A. CIL Format (For Unpacked Treble ROMs)
Append the contents of `sepolicy/vendor_sepolicy.cil.append` to the end of `[ROM]/vendor/etc/selinux/vendor_sepolicy.cil`:

```cil
(typepermissive hal_fingerprint_default)
(typepermissive shell)
(allow hal_fingerprint_default default_android_service (service_manager (add find)))
(allow hal_fingerprint_default servicemanager (binder (call transfer)))
(allow servicemanager hal_fingerprint_default (binder (call transfer)))
(allow servicemanager hal_fingerprint_default (dir (search)))
(allow servicemanager hal_fingerprint_default (file (read open)))
(allow servicemanager hal_fingerprint_default (process (getattr)))
(allow system_server hal_fingerprint_default (binder (call transfer)))
(allow hal_fingerprint_default system_server (binder (call transfer)))
(allow platform_app hal_fingerprint_default (binder (call transfer)))
(allow hal_fingerprint_default platform_app (binder (call transfer)))
(allow system_server default_android_service (service_manager (find)))
(allow platform_app default_android_service (service_manager (find)))
(allow hal_fingerprint_default sysfs (file (read open getattr)))
(allow hal_fingerprint_default sysfs (dir (search read open)))
(allow hal_fingerprint_default sysfs_touchpanel (file (read open getattr)))
(allow hal_fingerprint_default sysfs_touch (file (read open getattr)))
(allow hal_fingerprint_default input_device (dir (search read open)))
(allow hal_fingerprint_default input_device (chr_file (read open getattr ioctl)))
(allow hal_fingerprint_default default_prop (file (read open getattr map)))
(allow hal_fingerprint_default system_prop (file (read open getattr map)))
(allow init shell (process (transition)))
(allow shell default_prop (property_service (set)))
(allow shell system_prop (property_service (set)))
(allow shell sysfs (file (read write open getattr)))
```

#### B. IMPORTANT: Handling `precompiled_sepolicy`
In Android, if `/vendor/etc/selinux/precompiled_sepolicy` exists and its checksum matches the system partition, `init` loads it directly and **ignores** modifications to `vendor_sepolicy.cil`.
To ensure `init` compiles and loads your updated CIL rules:
1. Rename or delete `[ROM]/vendor/etc/selinux/precompiled_sepolicy` (e.g. rename to `precompiled_sepolicy.bak`).
2. Remove `[ROM]/vendor/etc/selinux/precompiled_sepolicy.plat_sepolicy_and_mapping.sha256` if present.
3. On first boot, Android `init` automatically detects that `precompiled_sepolicy` is missing, executes `secilc` dynamically, compiles all `.cil` files, and loads the active policy into memory.

#### C. Source .te Format (For AOSP / Vendor Tree Compilers)
If you build the vendor image or kernel from source, use `sepolicy/rodin_fod_aod.te`.

---

## Verification After First Boot

Once booted, open an ADB shell or terminal and run:

```bash
# 1. Verify global SELinux is Enforcing
getenforce
# Expected output: Enforcing

# 2. Verify services are running
getprop init.svc.nees_aodd
# Expected: running
getprop init.svc.mfp-daemon
# Expected: running

# 3. Verify runtime authorization
getprop sys.nees4.authorized
# Expected: 1

# 4. Verify DisplayPanelFeature AIDL registration
service list | grep -i displaypanel
# Expected: vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default
```

If all 4 commands check out, your baked ROM is fully functional!

---

## License

This project is open-source software licensed under the [Apache License, Version 2.0](../LICENSE).
