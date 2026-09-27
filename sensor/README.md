# Rodin Sensor Compatibility

The wrapper at `/vendor/lib64/hw/sensors.mt6899.so` bridges Xiaomi Rodin's pickup sensor (type `33171036`) to the ColorOS tilt detector (type `22`) for Rise to wake. It also exposes an Oplus compatible synthetic sensor (type `65611`) for AOD and screen-off fingerprint behavior.

When ColorOS enables tilt, the wrapper batches and activates Xiaomi pickup, then sends a tilt event with value `0` on a pickup event with value `1`. This matches the value expected by `ScreenOffGestureService`. The wrapper leaves FOD, AOD, significant motion, and hand sensor activation and events with the native HAL. The synthetic sensor uses Xiaomi pickup only, so its state does not disable other sensors.

Build with `./sensor/build.sh` using Android NDK r30. The tested build is installed directly on the Rodin ColorOS 17 vendor partition. SHA-256 of the tested `sensors.mt6899.so`:

`f9e74b38b50cd06f1764e95fb4de79852a3e1301a61024d564111afbbca3d14f`

Tested with `oplus_customize_gesture_wake_up_arouse=1`: three gentle lock-and-lift cycles woke the screen, and the screen-off fingerprint hint remained functional. A factory reset does not remove the vendor HAL; replacing or reflashing vendor does.
