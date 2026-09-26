# NEESS COS AOD + FOD FIX (POCO X7 Pro / Rodin)

[![Platform](https://img.shields.io/badge/Platform-ColorOS-brightgreen.svg)]()
[![Device](https://img.shields.io/badge/Device-POCO%20X7%20Pro%20(Rodin)-blue.svg)]()
[![SELinux](https://img.shields.io/badge/SELinux-100%25%20Enforcing%20Compliant-success.svg)]()
[![Touch](https://img.shields.io/badge/Touch-Goodix%20%2B%20FocalTech-orange.svg)]()

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
├── sepolicy/                          # Bakable SELinux policies
│   ├── vendor_sepolicy.cil.append     # Pre-formatted CIL rules for vendor_sepolicy.cil
│   └── rodin_fod_aod.te               # Source .te format for AOSP tree compilation
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
  ro.nees.fod.compat=1
  sys.nees4.authorized=1
  persist.sys.rodin.aod_keep_doze=1
  ro.oplus.aod.fod.support=true
  ```

#### Vendor Partition (`vendor/`)
* Copy `librodin_fp_compat.so` -> `/vendor/lib64/librodin_fp_compat.so` (`0644`, `u:object_r:vendor_file:s0`)
* Copy `zz_rodin_fp_compat.rc` -> `/vendor/etc/init/zz_rodin_fp_compat.rc` (`0644`, `u:object_r:vendor_configs_file:s0`)
* Append `sepolicy/vendor_sepolicy.cil.append` to `/vendor/etc/selinux/vendor_sepolicy.cil`.
* **Important:** Remove `/vendor/etc/selinux/precompiled_sepolicy` and its `.sha256` so Android `init` dynamically compiles your updated CIL rules on first boot.

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
