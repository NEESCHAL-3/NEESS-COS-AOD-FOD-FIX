#!/usr/bin/env python3
"""Bridge ColorOS's existing DT2W node request after its settings/proximity checks."""
import argparse
from pathlib import Path
import re
import shutil
import os

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("smali_root",type=Path)
    parser.add_argument("helper_smali",type=Path,help="Compiled RodinTouchBridge.smali from the included Java source")
    parser.add_argument("--dry-run",action="store_true")
    args=parser.parse_args()
    suffix="com/android/server/wm/ColorTouchNodeManagerService.smali"
    found=[p for p in args.smali_root.rglob("ColorTouchNodeManagerService.smali") if p.as_posix().endswith(suffix)]
    if len(found)!=1:raise SystemExit("Expected exactly one ColorTouchNodeManagerService; no files changed")
    p=found[0];text=p.read_text()
    signature=".method private synthetic lambda$writeNodeFileByDevice$1(Ljava/lang/String;ILjava/lang/String;I)V"
    start=text.find(signature)
    if start<0:raise SystemExit("Unsupported ColorOS method layout; no files changed")
    end=text.index(".end method",start)
    body=text[start:end]
    backend_patched = "RodinTouchBridge;->writeNodeFile" in body
    if not args.helper_smali.is_file():raise SystemExit("Missing compiled helper; no files changed")
    if 'Lcom/android/server/wm/RodinTouchBridge;' not in args.helper_smali.read_text():raise SystemExit("Wrong helper class")
    companion = args.helper_smali.parent / 'RodinAodTapBridge.smali'
    if 'RodinAodTapBridge;->initialize' in args.helper_smali.read_text() and not companion.is_file():
        raise SystemExit('Missing compiled RodinAodTapBridge companion; no files changed')
    if not re.search(r"\.registers\s+11\b",body) or 'const/4 v2, -0x2' not in body:
        raise SystemExit("Unsupported ColorOS register layout; no files changed")
    native="    invoke-direct {p0}, Lcom/android/server/wm/ColorTouchNodeManagerService;->getOplusTouchService()Lvendor/oplus/hardware/touch/IOplusTouch;"
    if body.count(native)!=1:raise SystemExit("Unexpected native backend count; no files changed")
    done=re.search(r"if-eqz v3, (:[A-Za-z0-9_]+)",body[body.index(native):])
    if not done:raise SystemExit("Missing original result path; no files changed")
    injection="""    invoke-static {p4, p2, v1}, Lcom/android/server/wm/RodinTouchBridge;->writeNodeFile(IILjava/lang/String;)I
    move-result v3
    const v4, -0x80000000
    if-eq v3, v4, :rodin_original_touch_backend
    move v2, v3
    goto DONE
    :rodin_original_touch_backend
""".replace("DONE",done[1])
    updated = text if backend_patched else text[:start]+body.replace(native,injection+native,1)+text[end:]
    context = "    iput-object p1, p0, Lcom/android/server/wm/ColorTouchNodeManagerService;->mContext:Landroid/content/Context;"
    if updated.count(context) != 1:
        raise SystemExit("Missing unique setContext hook; no files changed")
    initialize = "    invoke-static {p1}, Lcom/android/server/wm/RodinTouchBridge;->initialize(Landroid/content/Context;)V"
    if initialize not in updated:
        updated = updated.replace(context, context + "\n\n" + initialize, 1)
    refresh = p.parent / "RodinRefreshBridge.smali"
    refresh_update = None
    if refresh.exists():
        value = refresh.read_text()
        hook = "    invoke-static {p0}, Lcom/android/server/wm/RodinTouchBridge;->initialize(Landroid/content/Context;)V"
        if hook not in value:
            method = value.index(".method public static declared-synchronized updateSelectedMode(")
            registers = value.index("    .registers ", method)
            line = value.index("\n", registers)
            refresh_update = value[:line+1] + "\n" + hook + "\n" + value[line+1:]
    if args.dry_run:
        print("Would patch DT2W backend and copy helper:",p)
        return
    backup=p.with_suffix(p.suffix+'.pre-rodin-dt2w')
    if not backup.exists():shutil.copy2(p,backup)
    temporary=p.with_suffix(p.suffix+'.rodin-new');temporary.write_text(updated);shutil.copystat(p,temporary);os.replace(temporary,p)
    for pattern in ("RodinTouchBridge*.smali", "RodinAodTapBridge*.smali"):
        for helper in args.helper_smali.parent.glob(pattern):
            shutil.copy2(helper, p.parent / helper.name)
    if refresh_update is not None:
        backup = refresh.with_suffix(refresh.suffix + '.pre-rodin-dt2w')
        if not backup.exists(): shutil.copy2(refresh, backup)
        temporary = refresh.with_suffix(refresh.suffix + '.rodin-new')
        temporary.write_text(refresh_update); shutil.copystat(refresh, temporary); os.replace(temporary, refresh)
    print("Patched DT2W backend:",p)

if __name__=='__main__':main()
