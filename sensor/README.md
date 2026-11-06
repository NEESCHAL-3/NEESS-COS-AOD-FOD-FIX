# Rodin Sensor Compatibility

The wrapper at `/vendor/lib64/hw/sensors.mt6899.so` provides unified hardware sensor compatibility for POCO X7 Pro (Rodin) running ColorOS:

1. **Rise to Wake**: Bridges Xiaomi Rodin's pickup sensor (type `33171036`) to the ColorOS tilt detector (type `22`). When ColorOS enables tilt, the wrapper batches and activates Xiaomi pickup, then sends a tilt event with value `0` on a pickup event with value `1`, matching `ScreenOffGestureService`.
2. **AOD & Screen-Off Biometrics**: Exposes an Oplus-compatible synthetic sensor (type `65611`) for AOD and screen-off fingerprint behavior. Leaves FOD, AOD, significant motion, and hand sensor activation and events with the native HAL.
3. **Front Light Sensor & Auto-Brightness Data Stream**: Intercepts Xiaomi raw front photodiode events (`als_raw` type `33171111`, handle `0x5e`), reads the real-time OLED backlight level from `/sys/class/mi_display/disp-DSI-0/backlight`, subtracts optical screen leakage (`k_cali`), and synthesizes calibrated `android.sensor.light` (type `5`, handle `5`) lux events for `DisplayPowerController`.

Build with `./sensor/build.sh` using Android NDK r30. The tested build is installed directly on the Rodin ColorOS vendor partition.

SHA-256 of the tested `sensors.mt6899.so`:
`8dfccb1bb2f6bd1f27c72df94f5a31d5260c187ea9a5af7630158e1747d23953`

---

## Companion Compatibility Repositories

- **ColorOS Display & Brightness Mapping ([Coloros-display-autobrightness-fix-Rodin](https://github.com/NEESCHAL-3/Coloros-display-autobrightness-fix-Rodin))**:
  Provides the physical DisplayDeviceConfig XML (`display_id_4627039422300187648.xml`), 2.0-nit night brightness floor adjustment (`display_brightness_config_default.xml`), and ColorOS lux-to-nit spline curves. Pairs with this repository's sensor HAL (`sensors.mt6899.so`) to deliver calibrated, instant auto-brightness.
