# Installation & SELinux File Contexts Guide

## Installed Paths, Permissions & SELinux Labels

When installing manually or integrating into an unpacked ROM, every file must have the correct ownership, permissions, and SELinux context (`chcon`):

| File Path in ROM | Permissions | Owner | SELinux Context | Description |
|---|---|---|---|---|
| `/system/bin/nees_aodd` | `0755` (`rwxr-xr-x`) | `root:root` | `u:object_r:shell_exec:s0` | Native AOD backlight & SOFOD daemon executable |
| `/system/etc/init/nees_aodd.rc` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:system_file:s0` | Daemon init service definition (`seclabel u:r:shell:s0`) |
| `/vendor/lib64/librodin_fp_compat.so` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_file:s0` | Fingerprint & panel feature 217 compat shim |
| `/vendor/etc/init/zz_rodin_fp_compat.rc` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_configs_file:s0` | Hardware hook & Goodix/FocalTech permissions |
| `/vendor/etc/selinux/vendor_sepolicy.cil` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_sepolicy_file:s0` | Treble vendor CIL policy (with per-domain permissive) |

---

## File Contexts for Image Repackers (`erofs` / `ext4` / `e2fsdroid`)

If you are packing partition images using `mkfs.erofs` or `e2fsdroid`, add these entries to your partition's `file_contexts`:

### 1. `plat_file_contexts` (System Partition)
```text
/system/bin/nees_aodd                   u:object_r:shell_exec:s0
/system/etc/init/nees_aodd\.rc          u:object_r:system_file:s0
```

### 2. `vendor_file_contexts` (Vendor Partition)
```text
/vendor/lib64/librodin_fp_compat\.so    u:object_r:vendor_file:s0
/vendor/etc/init/zz_rodin_fp_compat\.rc u:object_r:vendor_configs_file:s0
```

---

## Init Services

### AOD Daemon (`system/etc/init/nees_aodd.rc`)
```rc
service nees_aodd /system/bin/nees_aodd
    class late_start
    user root
    group root system log
    seclabel u:r:shell:s0
```

### FOD Preload Hook (`vendor/etc/init/zz_rodin_fp_compat.rc`)
```rc
service mfp-daemon /vendor/bin/hw/mfp-daemon
    override
    interface aidl android.hardware.biometrics.fingerprint.IFingerprint/default
    class late_start
    user system
    group system drmrpc diag input uhid
    seclabel u:r:hal_fingerprint_default:s0
    socket fpsensor_socket stream 0777 system system
    setenv LD_PRELOAD /vendor/lib64/librodin_fp_compat.so

on boot
    # Goodix touchscreen FOD node
    chown system system /sys/devices/platform/goodix_ts.0/fod_enable
    chmod 0660 /sys/devices/platform/goodix_ts.0/fod_enable

    # FocalTech touchscreen FOD node
    chown system system /sys/devices/platform/focaltech_ts.0/fod_enable
    chmod 0660 /sys/devices/platform/focaltech_ts.0/fod_enable

    # Xiaomi generic touch devices
    chown system system /sys/devices/virtual/touch/touch_dev/fod_enable
    chmod 0660 /sys/devices/virtual/touch/touch_dev/fod_enable

    chown system system /sys/class/touch/touch_dev/fod_enable
    chmod 0660 /sys/class/touch/touch_dev/fod_enable

    # Xiaomi display feature node & symlink
    mkdir /dev/mi_display 0755 system system
    symlink /dev/disp_feature /dev/mi_display/disp_feature
    chown system system /dev/disp_feature
    chmod 0660 /dev/disp_feature
```

---

## Build Properties

### System Properties (`system/build.prop`)

```properties
sys.nees4.authorized=1
ro.nees.fod.compat=1
persist.sys.rodin.aod_keep_doze=1
ro.oplus.aod.fod.support=true
```

### Vendor Properties (Append to `vendor.prop` or `vendor/build.prop`)

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

## Post-Boot Verification

Run in ADB shell:
```bash
# Verify SELinux global state (Must return Enforcing)
getenforce

# Verify file labels
ls -lZ /system/bin/nees_aodd
ls -lZ /vendor/lib64/librodin_fp_compat.so

# Verify services
getprop init.svc.nees_aodd
getprop init.svc.mfp-daemon
```

## Custom fingerprint icons

Run `scripts/build_fod.sh` before baking. This builds the native, one-shot
`/system/bin/rodin_fod_icon_prepare` and installs it with
`/system/etc/init/rodin.fod-icon.rc`. The binary uses `rodin_fod_icon_exec`
and mode 0755; the RC uses `system_file` and mode 0644. Both are root:root.
The bake step adds `sepolicy/system_ext_icon.cil.append` to system_ext policy:
this defines a dedicated domain and executable type. Init transitions into
`rodin_fod_icon`; the utility runs as system with only CHOWN and FOWNER
capabilities to assign the icon files. It reads PackageManager’s UID map and
has no access to private app data. Recompile the combined ROM policy and
apply the matching file context when packing. The post-fs-data action also
restores that context before executing it.

ColorOS SystemUI saves icons directly to `/data/oplus/os/<userId>custom_icon.png`.
The user ID is a filename prefix; do not insert another slash. A port's performance
init rule can reset `/data/oplus/os` to system:system 0770, causing EACCES even
though Settings accepts the icon. The bake step changes that conflicting rule to
0771 where present. The initializer also restores 0771 and prepares each existing
user's exact icon file using the SystemUI app ID from PackageManager’s
`/data/system/packages.list`. It does not truncate images or make the
directory writable by other apps.

The initializer runs before framework startup and retries after boot completes,
so a first boot can resolve PackageManager’s newly created UID entry. After
creating a new Android user, reboot before using custom icons for that user.
The existing ColorOS `platform_app` access to `os_data_file` is required; no
permissive domain or extra SystemUI data-access grant is installed by this fix.
After installation, select the custom icon again to retry the previously failed
copy, then check it on the lock screen and after reboot. This storage fix is
independent of Goodix/Jiiov fingerprint sensors and Goodix/FocalTech touch panels.

Check the automatic boot run with `adb shell logcat -d -s RodinFodIcon`.
The completion line must report `failures=0`; `init.svc.rodin_fod_icon_prepare`
should be `stopped` after the one-shot completes.
