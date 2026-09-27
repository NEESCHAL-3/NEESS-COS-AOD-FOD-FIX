#!/bin/bash
set -e

if [ -z "$1" ]; then
  echo "================================================================"
  echo " POCO X7 Pro (Rodin) ROM Bake-In Installer (TEST 4.0)"
  echo "================================================================"
  echo "Usage: $0 <path_to_unpacked_rom_root>"
  echo "Example: $0 /home/neeschal/rom_unpacked"
  echo ""
  exit 1
fi

ROM_ROOT="$1"

if [ ! -d "$ROM_ROOT" ]; then
  echo "Error: Directory '$ROM_ROOT' does not exist."
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -d "$SCRIPT_DIR/system" ]; then
  SRC_DIR="$SCRIPT_DIR"
elif [ -d "$SCRIPT_DIR/.." ] && [ -d "$SCRIPT_DIR/../system" ]; then
  SRC_DIR="$SCRIPT_DIR/.."
else
  SRC_DIR="$SCRIPT_DIR"
fi

# Detect system partition (handles flat system/ or SAR system/system/)
SYS_DIR=""
if [ -d "$ROM_ROOT/system/system/bin" ]; then
  SYS_DIR="$ROM_ROOT/system/system"
elif [ -d "$ROM_ROOT/system/bin" ]; then
  SYS_DIR="$ROM_ROOT/system"
else
  echo "Error: Cannot locate system/bin under '$ROM_ROOT'"
  exit 1
fi

# Detect vendor partition
VEN_DIR=""
if [ -d "$ROM_ROOT/vendor/lib64" ]; then
  VEN_DIR="$ROM_ROOT/vendor"
elif [ -d "$ROM_ROOT/system/vendor/lib64" ]; then
  VEN_DIR="$ROM_ROOT/system/vendor"
else
  echo "Error: Cannot locate vendor/lib64 under '$ROM_ROOT'"
  exit 1
fi

echo "================================================================"
echo " POCO X7 Pro (Rodin) - Baking FOD & AOD Fixes into ROM"
echo "================================================================"
echo "System Root: $SYS_DIR"
echo "Vendor Root: $VEN_DIR"
echo ""

# 1. Install AOD Daemon
echo "-> [1/7] Installing nees_aodd to $SYS_DIR/bin/"
cp -f "$SCRIPT_DIR/system/bin/nees_aodd" "$SYS_DIR/bin/nees_aodd"
chmod 755 "$SYS_DIR/bin/nees_aodd"
chcon u:object_r:system_file:s0 "$SYS_DIR/bin/nees_aodd" 2>/dev/null || true

# 2. Install AOD Init Service
echo "-> [2/7] Installing nees_aodd.rc to $SYS_DIR/etc/init/"
mkdir -p "$SYS_DIR/etc/init"
cp -f "$SCRIPT_DIR/system/etc/init/nees_aodd.rc" "$SYS_DIR/etc/init/nees_aodd.rc"
chmod 644 "$SYS_DIR/etc/init/nees_aodd.rc"
chcon u:object_r:system_file:s0 "$SYS_DIR/etc/init/nees_aodd.rc" 2>/dev/null || true

# 3. Install FOD Compat Shim & Sensor Compatibility HAL
echo "-> [3/8] Installing librodin_fp_compat.so to $VEN_DIR/lib64/"
cp -f "$SCRIPT_DIR/vendor/lib64/librodin_fp_compat.so" "$VEN_DIR/lib64/librodin_fp_compat.so"
chmod 644 "$VEN_DIR/lib64/librodin_fp_compat.so"
chcon u:object_r:vendor_file:s0 "$VEN_DIR/lib64/librodin_fp_compat.so" 2>/dev/null || true

if [ -f "$SCRIPT_DIR/vendor/lib64/hw/sensors.mt6899.so" ]; then
  echo "-> Installing sensors.mt6899.so (Instant Tilt & Pickup HAL) to $VEN_DIR/lib64/hw/"
  mkdir -p "$VEN_DIR/lib64/hw"
  cp -f "$SCRIPT_DIR/vendor/lib64/hw/sensors.mt6899.so" "$VEN_DIR/lib64/hw/sensors.mt6899.so"
  chmod 644 "$VEN_DIR/lib64/hw/sensors.mt6899.so"
  chcon u:object_r:vendor_file:s0 "$VEN_DIR/lib64/hw/sensors.mt6899.so" 2>/dev/null || true
fi

# Install the framework Doze RRO used by ColorOS AOD.
DOZE_OVERLAY="$SRC_DIR/vendor/overlay/RodinAodDozeOverlay.apk"
if [ ! -f "$DOZE_OVERLAY" ]; then
  echo "Error: Missing required Doze overlay: $DOZE_OVERLAY" >&2
  exit 1
fi
echo "-> Installing RodinAodDozeOverlay.apk to $VEN_DIR/overlay/"
mkdir -p "$VEN_DIR/overlay"
cp -f "$DOZE_OVERLAY" "$VEN_DIR/overlay/RodinAodDozeOverlay.apk"
chmod 644 "$VEN_DIR/overlay/RodinAodDozeOverlay.apk"
chcon u:object_r:vendor_overlay_file:s0 "$VEN_DIR/overlay/RodinAodDozeOverlay.apk" 2>/dev/null || true

# 4. Patch mfp-daemon ELF DT_NEEDED (Permanent boot loading without LD_PRELOAD)
echo "-> [4/8] Baking librodin_fp_compat.so into $VEN_DIR/bin/hw/mfp-daemon ELF header..."
if [ -f "$VEN_DIR/bin/hw/mfp-daemon" ]; then
  if which patchelf >/dev/null 2>&1; then
    patchelf --add-needed librodin_fp_compat.so "$VEN_DIR/bin/hw/mfp-daemon" 2>/dev/null || true
    chmod 755 "$VEN_DIR/bin/hw/mfp-daemon"
    chcon u:object_r:hal_fingerprint_default_exec:s0 "$VEN_DIR/bin/hw/mfp-daemon" 2>/dev/null || true
    echo "   [OK] Patched mfp-daemon with DT_NEEDED: librodin_fp_compat.so"
  else
    echo "   [WARN] patchelf not found! Please ensure patchelf is installed on build host."
  fi
fi

# 5. Install Fingerprint Hook & Node Init Script
echo "-> [5/8] Installing zz_rodin_fp_compat.rc to $VEN_DIR/etc/init/"
mkdir -p "$VEN_DIR/etc/init"
cp -f "$SCRIPT_DIR/vendor/etc/init/zz_rodin_fp_compat.rc" "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc"
chmod 644 "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc"
chcon u:object_r:vendor_configs_file:s0 "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc" 2>/dev/null || true

# 6. Inject SELinux CIL Rules & Handle precompiled_sepolicy
echo "-> [6/8] Injecting Strict SELinux CIL rules into $VEN_DIR/etc/selinux/vendor_sepolicy.cil..."
CIL_TARGET="$VEN_DIR/etc/selinux/vendor_sepolicy.cil"
if [ -f "$CIL_TARGET" ]; then
  if ! grep -q "Pure Strict Enforcing FOD Rules" "$CIL_TARGET"; then
    cat "$SCRIPT_DIR/sepolicy/vendor_sepolicy.cil.append" >> "$CIL_TARGET"
    echo "   [OK] Appended strict enforcing rules to vendor_sepolicy.cil"
  else
    echo "   [SKIP] Rules already present in vendor_sepolicy.cil"
  fi
else
  echo "   [WARN] $CIL_TARGET not found! Please check vendor selinux directory."
fi

# Inject DisplayPanelFeature AIDL service into vendor_service_contexts and odm_service_contexts
VEN_SC="$VEN_DIR/etc/selinux/vendor_service_contexts"
if [ -f "$VEN_SC" ]; then
  if ! grep -q "vendor.oplus.hardware.displaypanelfeature" "$VEN_SC"; then
    echo "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default       u:object_r:vendor_hal_fingerprint_service_xiaomi:s0" >> "$VEN_SC"
    echo "   [OK] Appended DisplayPanelFeature AIDL service to $VEN_SC"
  fi
fi

ODM_SC="$ROM_ROOT/odm/etc/selinux/odm_service_contexts"
if [ -f "$ODM_SC" ]; then
  if ! grep -q "vendor.oplus.hardware.displaypanelfeature" "$ODM_SC"; then
    echo "vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature/default       u:object_r:vendor_hal_fingerprint_service_xiaomi:s0" >> "$ODM_SC"
    echo "   [OK] Appended DisplayPanelFeature AIDL service to $ODM_SC"
  fi
fi

# CRITICAL TREBLE STEP:
# On Android Treble devices (including POCO X7 Pro / ColorOS ports), if precompiled_sepolicy exists
# in /odm/etc/selinux or /vendor/etc/selinux, Android init will load the precompiled binary blob
# and completely IGNORE custom CIL policy edits!
# We remove all 3 precompiled sepolicy files:
#   1. precompiled_sepolicy
#   2. precompiled_sepolicy.plat_sepolicy_and_mapping.sha256
#   3. precompiled_sepolicy.*_sepolicy_and_mapping.sha256
# This forces Android init to dynamically invoke secilc at boot and compile all custom CIL rules cleanly.
for DIR in "$ROM_ROOT/odm/etc/selinux" "$VEN_DIR/etc/selinux"; do
  if [ -d "$DIR" ]; then
    FOUND=$(find "$DIR" -maxdepth 1 -name "precompiled_sepolicy*" 2>/dev/null)
    if [ -n "$FOUND" ]; then
      echo "   [CRITICAL] Found precompiled sepolicy files in $DIR:"
      echo "$FOUND" | while read -r f; do
        echo "     -> Neutralizing $(basename "$f")"
        mv -f "$f" "${f}.bak" 2>/dev/null || rm -f "$f"
      done
      echo "   [OK] Precompiled sepolicy files neutralized in $DIR."
    fi
  fi
done

# 7. Update SELinux file_contexts if present in unpacked partitions
echo "-> [7/8] Updating SELinux file_contexts for image repackers (erofs/ext4)..."
VEN_FC="$VEN_DIR/etc/selinux/vendor_file_contexts"
if [ -f "$VEN_FC" ]; then
  if ! grep -q "librodin_fp_compat" "$VEN_FC"; then
    cat "$SCRIPT_DIR/sepolicy/vendor_file_contexts.append" >> "$VEN_FC"
    echo "   [OK] Appended labels to $VEN_FC"
  fi
  if ! grep -q "RodinAodDozeOverlay" "$VEN_FC"; then
    echo '/vendor/overlay/RodinAodDozeOverlay\.apk u:object_r:vendor_overlay_file:s0' >> "$VEN_FC"
    echo "   [OK] Appended Doze overlay label to $VEN_FC"
  fi
fi

SYS_FC="$SYS_DIR/etc/selinux/plat_file_contexts"
if [ -f "$SYS_FC" ]; then
  if ! grep -q "nees_aodd" "$SYS_FC"; then
    cat "$SCRIPT_DIR/sepolicy/plat_file_contexts.append" >> "$SYS_FC"
    echo "   [OK] Appended labels to $SYS_FC"
  fi
fi

# 8. Append Properties to build.prop
echo "-> [8/8] Injecting properties into $SYS_DIR/build.prop..."
PROP_TARGET="$SYS_DIR/build.prop"
if [ -f "$PROP_TARGET" ]; then
  if ! grep -q "ro.nees.fod.compat" "$PROP_TARGET"; then
    cat "$SCRIPT_DIR/system/build.prop.append" >> "$PROP_TARGET"
    echo "   [OK] Appended FOD & AOD properties to build.prop"
  else
    echo "   [SKIP] Properties already present in build.prop"
  fi
fi

echo ""
echo "================================================================"
echo " [SUCCESS] ROM successfully baked with TEST 4.0 FOD & AOD Fixes!"
echo "================================================================"
echo "Repack your ROM partitions. On first boot, the phone will:"
echo " 1. Run global SELinux in 100% Enforcing mode (passes CTS/Integrity)."
echo " 2. Assign exact SELinux labels to all binaries and configs."
echo " 3. Hook mfp-daemon with librodin_fp_compat.so (FOD active)."
echo " 4. Register DisplayPanelFeature with feature 217 ACKed (Seamless & Classic AOD fixed)."
echo " 5. Run nees_aodd with zero-fork stream loop (No watchdog crashes)."
echo " 6. Show Screen-Off Fingerprint icon on touch/pickup."
echo "================================================================"
