# Installation & SELinux File Contexts Guide

## Installed Paths, Permissions & SELinux Labels

When installing manually or integrating into an unpacked ROM, every file must have the correct ownership, permissions, and SELinux context (`chcon`):

| File Path in ROM | Permissions | Owner | SELinux Context | Description |
|---|---|---|---|---|
| `/system/bin/nees_aodd` | `0755` (`rwxr-xr-x`) | `root:root` | `u:object_r:system_file:s0` | Native AOD backlight & SOFOD daemon executable |
| `/system/etc/init/nees_aodd.rc` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:system_file:s0` | Daemon init service definition (`seclabel u:r:shell:s0`) |
| `/vendor/lib64/librodin_fp_compat.so` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_file:s0` | Fingerprint & panel feature 217 compat shim |
| `/vendor/etc/init/zz_rodin_fp_compat.rc` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_configs_file:s0` | Hardware hook & Goodix/FocalTech permissions |
| `/vendor/etc/selinux/vendor_sepolicy.cil` | `0644` (`rw-r--r--`) | `root:root` | `u:object_r:vendor_sepolicy_file:s0` | Treble vendor CIL policy (with per-domain permissive) |

---

## File Contexts for Image Repackers (`erofs` / `ext4` / `e2fsdroid`)

If you are packing partition images using `mkfs.erofs` or `e2fsdroid`, add these entries to your partition's `file_contexts`:

### 1. `plat_file_contexts` (System Partition)
```text
/system/bin/nees_aodd                   u:object_r:system_file:s0
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

## Build Properties (`system/build.prop`)

```properties
sys.nees4.authorized=1
ro.nees.fod.compat=1
persist.sys.rodin.aod_keep_doze=1
ro.oplus.aod.fod.support=true
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
