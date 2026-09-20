#!/usr/bin/env bash
set -euo pipefail
adb shell 'su -M -c "
echo ===== SERVICES =====
echo AOD_state=$(getprop init.svc.nees_aodd)
echo AOD_pid=$(pidof nees_aodd)
echo MFP_state=$(getprop init.svc.mfp-daemon)
echo MFP_pid=$(pidof mfp-daemon)

echo
echo ===== AUTH =====
echo runtime_auth=$(getprop sys.nees4.authorized)
/system/bin/nees_aodd --verify-only
echo verify_rc=$?

echo
echo ===== HASHES =====
sha256sum /system/bin/nees_aodd
sha256sum /vendor/lib64/librodin_fp_compat.so
sha256sum /vendor/lib64/hw/sensors.mt6899.so

echo
echo ===== PANEL SERVICE =====
service list | grep -i displaypanelfeature || true

echo
echo ===== STARTUP LOGS =====
logcat -b all -d -v time | grep -Ei \
\"V4 shared authorization OK|NEES4 runtime authorization published|NEES4 runtime authorization OK|authorization timeout|OplusPanel registered\" \
| tail -100
"'
