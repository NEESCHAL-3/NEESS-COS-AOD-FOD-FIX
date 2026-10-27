#!/usr/bin/env python3
"""Keep Rodin call proximity on Android type 8, not Xiaomi offhand type 33171067."""
import argparse
import os
from pathlib import Path
import re
import shutil

CLASSES = [
    "com/android/server/display/OplusDisplayPowerControllerExtImpl",
    "com/android/server/display/feature/power/OplusProximitySensorScreenOnOff",
]
METHODS = {
    "registerPSensor(Landroid/hardware/SensorManager;Landroid/hardware/SensorEventListener;ILandroid/os/Handler;Landroid/hardware/Sensor;)Z": "Z",
    "applyOplusProximitySensorLocked(Landroid/hardware/display/DisplayManagerInternal$DisplayPowerRequest;IZZZI)Z": "Z",
    "getUseProximityForceSuspendState(I)Z": "Z",
    "isUseProximityForceSuspendStateChanged(I)Z": "Z",
    "setUseProximityForceSuspend(ZI)V": "V",
}

def patch(text, label):
    for signature, result in METHODS.items():
        pattern = re.compile(r"(^\.method[^\n]* " + re.escape(signature) + r"\n)(.*?)(^\.end method)", re.M | re.S)
        matches = list(pattern.finditer(text))
        if len(matches) != 1:
            raise ValueError(f"{label}: expected one method {signature}, found {len(matches)}; no files changed")
        body = "    .locals 1\n\n    const/4 v0, 0x0\n    return v0\n" if result == "Z" else "    .locals 0\n\n    return-void\n"
        text = pattern.sub(lambda m: m[1] + body + m[3], text, count=1)
    return text

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("smali_root", type=Path, help="Root containing the extracted oplus-services.jar smali directories")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    planned = []
    for cls in CLASSES:
        matches = [p for p in args.smali_root.rglob(cls.rsplit("/", 1)[1] + ".smali") if p.as_posix().endswith(cls + ".smali")]
        if len(matches) != 1:
            raise SystemExit(f"Expected exactly one {cls}.smali, found {len(matches)}; no files changed")
        path = matches[0]
        original = path.read_text()
        planned.append((path, original, patch(original, str(path))))
    for path, original, updated in planned:
        if original == updated:
            print(f"Already patched: {path}")
        elif args.dry_run:
            print(f"Would patch: {path}")
        else:
            backup = path.with_suffix(path.suffix + ".pre-rodin-proximity")
            if not backup.exists():
                shutil.copy2(path, backup)
            temporary = path.with_suffix(path.suffix + ".rodin-new")
            temporary.write_text(updated)
            shutil.copystat(path, temporary)
            os.replace(temporary, path)
            print(f"Patched: {path}")

if __name__ == "__main__":
    main()
