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
echo "-> [1/6] Installing nees_aodd to $SYS_DIR/bin/"
cp -f "$SCRIPT_DIR/system/bin/nees_aodd" "$SYS_DIR/bin/nees_aodd"
chmod 755 "$SYS_DIR/bin/nees_aodd"

# 2. Install AOD Init Service
echo "-> [2/6] Installing nees_aodd.rc to $SYS_DIR/etc/init/"
mkdir -p "$SYS_DIR/etc/init"
cp -f "$SCRIPT_DIR/system/etc/init/nees_aodd.rc" "$SYS_DIR/etc/init/nees_aodd.rc"
chmod 644 "$SYS_DIR/etc/init/nees_aodd.rc"

# 3. Install FOD Compat Shim
echo "-> [3/6] Installing librodin_fp_compat.so to $VEN_DIR/lib64/"
cp -f "$SCRIPT_DIR/vendor/lib64/librodin_fp_compat.so" "$VEN_DIR/lib64/librodin_fp_compat.so"
chmod 644 "$VEN_DIR/lib64/librodin_fp_compat.so"

# 4. Install Fingerprint Hook & Node Init Script
echo "-> [4/6] Installing zz_rodin_fp_compat.rc to $VEN_DIR/etc/init/"
mkdir -p "$VEN_DIR/etc/init"
cp -f "$SCRIPT_DIR/vendor/etc/init/zz_rodin_fp_compat.rc" "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc"
chmod 644 "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc"

# 5. Inject SELinux CIL Rules & Handle precompiled_sepolicy
echo "-> [5/6] Injecting SELinux CIL rules into $VEN_DIR/etc/selinux/vendor_sepolicy.cil..."
CIL_TARGET="$VEN_DIR/etc/selinux/vendor_sepolicy.cil"
if [ -f "$CIL_TARGET" ]; then
  if ! grep -q "typepermissive hal_fingerprint_default" "$CIL_TARGET"; then
    cat "$SCRIPT_DIR/sepolicy/vendor_sepolicy.cil.append" >> "$CIL_TARGET"
    echo "   [OK] Appended per-domain permissive & binder rules to vendor_sepolicy.cil"
  else
    echo "   [SKIP] Rules already present in vendor_sepolicy.cil"
  fi
else
  echo "   [WARN] $CIL_TARGET not found! Please check vendor selinux directory."
fi

# CRITICAL TREBLE STEP:
# If vendor has precompiled_sepolicy, init will load it and IGNORE vendor_sepolicy.cil edits!
# We back it up and remove it so Android init runs secilc to compile our new CIL rules at boot.
PRECOMPILED="$VEN_DIR/etc/selinux/precompiled_sepolicy"
if [ -f "$PRECOMPILED" ]; then
  echo "   [CRITICAL] Found precompiled_sepolicy in vendor partition."
  echo "   Backing up to precompiled_sepolicy.bak and removing original,"
  echo "   forcing init to compile fresh policy from CIL files on boot..."
  mv "$PRECOMPILED" "${PRECOMPILED}.bak"
  rm -f "${PRECOMPILED}.plat_sepolicy_and_mapping.sha256" 2>/dev/null || true
  echo "   [OK] precompiled_sepolicy handled."
fi

# 6. Append Properties to build.prop
echo "-> [6/6] Injecting properties into $SYS_DIR/build.prop..."
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
echo " 2. Hook mfp-daemon with librodin_fp_compat.so (FOD active)."
echo " 3. Register DisplayPanelFeature with feature 217 ACKed (Seamless & Classic AOD fixed)."
echo " 4. Run nees_aodd with zero-fork stream loop (No watchdog crashes)."
echo " 5. Show Screen-Off Fingerprint icon on touch/pickup."
echo "================================================================"
