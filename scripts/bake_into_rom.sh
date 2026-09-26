#!/bin/bash
set -e

if [ -z "$1" ]; then
  echo "Usage: $0 <path_to_unpacked_rom_root>"
  echo "Example: $0 /home/neeschal/rom_unpacked"
  exit 1
fi

ROM_ROOT="$1"

if [ ! -d "$ROM_ROOT" ]; then
  echo "Error: Directory '$ROM_ROOT' does not exist."
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Detect system directory (handle system or system/system SAR layout)
SYS_DIR=""
if [ -d "$ROM_ROOT/system/system/bin" ]; then
  SYS_DIR="$ROM_ROOT/system/system"
elif [ -d "$ROM_ROOT/system/bin" ]; then
  SYS_DIR="$ROM_ROOT/system"
else
  echo "Error: Cannot locate system/bin under '$ROM_ROOT'"
  exit 1
fi

# Detect vendor directory
VEN_DIR=""
if [ -d "$ROM_ROOT/vendor/lib64" ]; then
  VEN_DIR="$ROM_ROOT/vendor"
elif [ -d "$ROM_ROOT/system/vendor/lib64" ]; then
  VEN_DIR="$ROM_ROOT/system/vendor"
else
  echo "Error: Cannot locate vendor/lib64 under '$ROM_ROOT'"
  exit 1
fi

echo "Detected System Partition: $SYS_DIR"
echo "Detected Vendor Partition: $VEN_DIR"

echo "-> [1/5] Installing nees_aodd to $SYS_DIR/bin/"
cp -f "$SCRIPT_DIR/system/bin/nees_aodd" "$SYS_DIR/bin/nees_aodd"
chmod 755 "$SYS_DIR/bin/nees_aodd"

echo "-> [2/5] Installing nees_aodd.rc to $SYS_DIR/etc/init/"
mkdir -p "$SYS_DIR/etc/init"
cp -f "$SCRIPT_DIR/system/etc/init/nees_aodd.rc" "$SYS_DIR/etc/init/nees_aodd.rc"
chmod 644 "$SYS_DIR/etc/init/nees_aodd.rc"

echo "-> [3/5] Installing librodin_fp_compat.so to $VEN_DIR/lib64/"
cp -f "$SCRIPT_DIR/vendor/lib64/librodin_fp_compat.so" "$VEN_DIR/lib64/librodin_fp_compat.so"
chmod 644 "$VEN_DIR/lib64/librodin_fp_compat.so"

echo "-> [4/5] Installing zz_rodin_fp_compat.rc to $VEN_DIR/etc/init/"
mkdir -p "$VEN_DIR/etc/init"
cp -f "$SCRIPT_DIR/vendor/etc/init/zz_rodin_fp_compat.rc" "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc"
chmod 644 "$VEN_DIR/etc/init/zz_rodin_fp_compat.rc"

echo "-> [5/5] Injecting SELinux CIL & build.prop..."
CIL_TARGET="$VEN_DIR/etc/selinux/vendor_sepolicy.cil"
if [ -f "$CIL_TARGET" ]; then
  if ! grep -q "typepermissive hal_fingerprint_default" "$CIL_TARGET"; then
    cat "$SCRIPT_DIR/vendor/etc/selinux/vendor_sepolicy.cil.append" >> "$CIL_TARGET"
    echo "   Appended SELinux CIL rules to $CIL_TARGET"
  else
    echo "   SELinux rules already present in $CIL_TARGET"
  fi
else
  echo "   Notice: $CIL_TARGET not found. Please append vendor_sepolicy.cil.append manually to your sepolicy."
fi

PROP_TARGET="$SYS_DIR/build.prop"
if [ -f "$PROP_TARGET" ]; then
  if ! grep -q "ro.nees.fod.compat" "$PROP_TARGET"; then
    cat "$SCRIPT_DIR/system/build.prop.append" >> "$PROP_TARGET"
    echo "   Appended properties to $PROP_TARGET"
  else
    echo "   Properties already present in $PROP_TARGET"
  fi
fi

echo "=== SUCCESS: ROM Baked with FOD & AOD fixes successfully! ==="
