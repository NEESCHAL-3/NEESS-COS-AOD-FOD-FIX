# Installation

## Installed paths

```text
/system/bin/nees_aodd
/system/etc/init/nees_aodd.rc
/vendor/lib64/librodin_fp_compat.so
/system/build.prop
```

## AOD init service

```rc
service nees_aodd /system/bin/nees_aodd
    class late_start
    user root
    group root system log
    seclabel u:r:shell:s0
```

## Tested permissions

```text
/system/bin/nees_aodd
  root:root 0755
  u:object_r:system_file:s0

/vendor/lib64/librodin_fp_compat.so
  root:root 0644
  u:object_r:vendor_file:s0
```

## Safe update rule

Never update only one member of a signed AOD/FOD pair and reboot.

Install together:

1. new `nees_aodd`
2. new `librodin_fp_compat.so`
3. matching `ro.nees.aod.auth`
4. run `nees_aodd --verify-only`
5. start daemon and confirm `sys.nees4.authorized=1`
6. reload/reboot `mfp-daemon`
7. confirm DisplayPanelFeature registration

## SELinux note

During development, service-manager AVCs were observed while the device was permissive. Add proper service labels/policy before claiming enforcing support.
