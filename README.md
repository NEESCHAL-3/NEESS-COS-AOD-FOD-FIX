# NEESS COS AOD + FOD FIX (POCO X7 Pro / Rodin)

[![Platform](https://img.shields.io/badge/Platform-ColorOS-brightgreen.svg)]()
[![Device](https://img.shields.io/badge/Device-POCO%20X7%20Pro%20(Rodin)-blue.svg)]()
[![SELinux](https://img.shields.io/badge/SELinux-100%25%20Enforcing%20Compliant-success.svg)]()
[![Touch](https://img.shields.io/badge/Touch-Goodix%20%2B%20FocalTech-orange.svg)]()
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

Native under-display fingerprint (FOD) and Always-On Display (AOD) hardware compatibility layer for **POCO X7 Pro (Rodin)** running ColorOS ports. 

Achieves full OEM-grade FOD unlock, seamless animations, and AOD modes **without patching ColorOS SystemUI bytecode**.

---

## Architecture Overview

```text
                  ColorOS SystemUI / Keyguard / PowerManager
                                      |
                     +----------------+----------------+
                     |                                 |
                     v                                 v
        [librodin_fp_compat.so]                  [nees_aodd]
    (Preloaded on Xiaomi mfp-daemon)      (Native Rust AOD Daemon)
                     |                                 |
   +-----------------+-----------------+               |
   |                                   |               |
   v                                   v               v
ColorOS AIDL Interfaces        Xiaomi Hardware Path   Display & Panel
* IBiometricsFingerprint       * Goodix / FocalTech   * Backlight control
* IDisplayPanelFeature           TouchFeature HAL     * Sysfs disp_feature
  - Feature 211 (LHBM)         * /dev/disp_feature    * SOFOD hint state
  - Feature 217 (Smooth AOD)   * /dev/input (FOD)     * Zero-fork stream
```

---

## Key Features & Fixes

### 1. Dual Touchscreen Backend Support: Goodix + FocalTech
The POCO X7 Pro is manufactured with panels using either **Goodix** (`goodix_ts.0`) or **FocalTech** (`focaltech_ts.0`) touch ICs.
* **Dynamic Hardware Routing:** Rather than writing to fixed sysfs nodes, `librodin_fp_compat.so` binds to Xiaomi's hardware abstraction layer `vendor.xiaomi.hw.touchfeature.ITouchFeature/default` (Transaction 9, mode 10, value 1). Xiaomi's HAL automatically routes the FOD touch enable command to whichever touch IC is active.
* **Adaptive Edge Polling:** Drivers like FocalTech do not always emit standard Linux input key events (`KEY_FOD_GESTURE_DOWN`) or sysfs notifications. The compatibility worker polls both input device events and `/sys/class/touch/touch_dev/fod_press_status` with an adaptive 15–25ms active window to ensure instantaneous touch down/up detection on both Goodix and FocalTech hardware.

### 2. Seamless & Classic AOD Fix (Panel Feature 217)
* **Root Cause of 2s Cutoff:** ColorOS `SmoothTransitionController` queries and sets panel feature `217` (`OPLUS_FEATURE_AOD_SMOOTH`). If the call returns `-1`, ColorOS aborts the smooth transition and forces the display state to `OFF` after ~2 seconds.
* **Solution:** `librodin_fp_compat.so` intercepts `SET_FEATURE` for feature `217` and returns `STATUS_OK` (0). Seamless AOD, Classic AOD, and lockscreen fade-in animations render smoothly and stay lit indefinitely.

### 3. Screen-Off Fingerprint (SOFOD) Hint Integration
* **Coordinated State Machine:** When AOD is disabled or enters energy-saving hide (`DOZE->OFF`), `nees_aodd` toggles `Setting_AodSwitchEnable` to `0`. This informs ColorOS `OnScreenFingerprintUiMech` that AOD is inactive, enabling the screen-off fingerprint icon to appear on pickup or screen tap (`notifyWakeUpCallback type 1`).
* **Brightness Coordination:** On touch or pickup callback, `nees_aodd` immediately ramps panel brightness to `100` for clear FOD icon visibility, dropping to `0` when the icon hides, and restoring `Setting_AodSwitchEnable = 1` when the screen wakes to `ON`.

### 4. Zero-Fork AOD Keepalive (Watchdog Crash Prevention)
* Previous iterations spawned `/system/bin/settings get` inside the logcat stream loop, executing 50–100 Java processes per second and triggering Android process watchdogs (`MBrainServer`) to kill `nees_aodd`.
* `nees_aodd` now operates on **pure in-memory string matching** on the logcat stream with zero subprocess forks. Panel backlight is held at 28 during `DOZE_SUSPEND` and `performAodUpdate` without system overhead.

### 5. SELinux Security: Per-Domain Permissive
* Uses **Per-Domain Permissive** (`typepermissive hal_fingerprint_default` and `typepermissive shell`).
* **The entire ROM remains 100% Enforcing globally** (`getenforce` returns `Enforcing`).
* Google Play Integrity, SafetyNet, Google Wallet, and banking applications pass without restrictions.

---

## Repository Structure

```text
NEESS-COS-AOD-FOD-FIX/
├── aod/                               # Native Rust AOD helper daemon
│   ├── Cargo.toml
│   ├── Cargo.lock
│   ├── nees_aodd.rc                   # Init service definition (u:r:shell:s0)
│   └── src/main.rs                    # Zero-fork logcat listener & backlight manager
├── fod/                               # Native C++ FOD & panel compatibility shim
│   ├── native/
│   │   ├── CMakeLists.txt
│   │   └── rodin_fp_compat.cpp        # Dual-touch, Feature 217 & AIDL hooks
│   └── zz_rodin_fp_compat.rc          # Preload & Goodix/Focaltech sysfs setup
├── sepolicy/                          # Bakable SELinux policies (Tested & Verified)
│   ├── vendor_sepolicy.cil.append     # Default CIL append (Hybrid Enforcing)
│   ├── vendor_sepolicy_hybrid.cil.append # Case 1: Full ROM Enforcing + FOD Permissive
│   └── vendor_sepolicy_strict.cil.append # Case 2: 100% Pure Strict Enforcing
├── scripts/                           # Build & packaging scripts
│   ├── bake_into_rom.sh               # 1-command unpacked ROM injection script
│   ├── build_aod.sh                   # Cargo cross-compiler script
│   ├── build_fod.sh                   # NDK CMake build script
│   └── verify_device.sh               # Device diagnostic & status checker
└── docs/                              # Detailed guides
    ├── ARCHITECTURE.md                # Deep-dive protocol & timing specs
    └── README_ROM_BAKE.md             # Complete unpacked ROM integration guide
```

---

## Building from Source

### Prerequisites
* Android NDK r27+ (or r30)
* Rust toolchain with target `aarch64-unknown-linux-musl` or `aarch64-linux-android`
* CMake 3.22+ and Ninja

### 1. Build FOD Shim (`librodin_fp_compat.so`)
```bash
cd fod/native
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-35
cmake --build build -j"$(nproc)"
```
Output: `fod/native/build/librodin_fp_compat.so`

### 2. Build AOD Daemon (`nees_aodd`)
```bash
cd aod
CARGO_TARGET_AARCH64_UNKNOWN_LINUX_MUSL_LINKER=aarch64-linux-gnu-gcc \
cargo build --release --target aarch64-unknown-linux-musl
```
Output: `aod/target/aarch64-unknown-linux-musl/release/nees-rodin-aodd`

---

## Integration into Unpacked ROM

### Option 1: Automated Script
```bash
./scripts/bake_into_rom.sh /path/to/unpacked_rom
```

### Option 2: Manual Placement

#### System Partition (`system/`)
* Copy `nees_aodd` -> `/system/bin/nees_aodd` (`0755`, `u:object_r:system_file:s0`)
* Copy `nees_aodd.rc` -> `/system/etc/init/nees_aodd.rc` (`0644`, `u:object_r:system_file:s0`)
* Append to `/system/build.prop`:

```properties
sys.nees4.authorized=1
ro.nees.fod.compat=1
persist.sys.rodin.aod_keep_doze=1
ro.oplus.aod.fod.support=true
```

#### Vendor Partition (`vendor/`)
* Copy `librodin_fp_compat.so` -> `/vendor/lib64/librodin_fp_compat.so` (`0644`, `u:object_r:vendor_file:s0`)
* Copy `zz_rodin_fp_compat.rc` -> `/vendor/etc/init/zz_rodin_fp_compat.rc` (`0644`, `u:object_r:vendor_configs_file:s0`)
* Append to `vendor.prop` (or `/vendor/build.prop`):

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

* Append `sepolicy/vendor_sepolicy.cil.append` to `/vendor/etc/selinux/vendor_sepolicy.cil`.
* **Important:** Remove `/vendor/etc/selinux/precompiled_sepolicy` and its `.sha256` so Android `init` dynamically compiles your updated CIL rules on first boot.

### SELinux File Contexts & Permissions Table

| File Path in ROM | Permissions | Owner | SELinux Context | Description |
|---|---|---|---|---|
| `/system/bin/nees_aodd` | `0755` (`rwxr-xr-x`) | `root:root` | `u:object_r:system_file:s0` | AOD & SOFOD daemon executable |
| `/system/etc/init/nees_aodd.rc` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:system_file:s0` | Init service definition (`u:r:shell:s0`) |
| `/vendor/lib64/librodin_fp_compat.so` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_file:s0` | Fingerprint & panel compat shim |
| `/vendor/etc/init/zz_rodin_fp_compat.rc` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_configs_file:s0` | Hardware hook & Goodix/FocalTech permissions |
| `/vendor/etc/selinux/vendor_sepolicy.cil` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_sepolicy_file:s0` | Treble vendor CIL policy |

#### `file_contexts` Entries for Image Repackers (`erofs` / `ext4` / `e2fsdroid`)
* **`plat_file_contexts` (System):**
  ```text
  /system/bin/nees_aodd                   u:object_r:system_file:s0
  /system/etc/init/nees_aodd\.rc          u:object_r:system_file:s0
  ```
* **`vendor_file_contexts` (Vendor):**
  ```text
  /vendor/lib64/librodin_fp_compat\.so    u:object_r:vendor_file:s0
  /vendor/etc/init/zz_rodin_fp_compat\.rc u:object_r:vendor_configs_file:s0
  ```

---

## Post-Boot Verification

Run these commands in an ADB shell:

```bash
# 1. Verify global SELinux is Enforcing
getenforce
# Expected: Enforcing

# 2. Check daemon status
getprop init.svc.nees_aodd
# Expected: running

# 3. Check fingerprint HAL
getprop init.svc.mfp-daemon
# Expected: running

# 4. Check DisplayPanelFeature AIDL
service list | grep -i displaypanel
# Expected: vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default
```

---

## Credits & Authors
* **NEESCHAL** – Lead developer, hardware reverse engineering, protocol shims, and timing state machines.
* Community testers for feedback and telemetry logs.

---

## License

This project is open-source software licensed under the [Apache License, Version 2.0](LICENSE).

```text
Copyright (c) 2026 NEESCHAL-3 and NEESS-COS-AOD-FOD-FIX Contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
