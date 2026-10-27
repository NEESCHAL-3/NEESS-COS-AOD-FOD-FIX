#!/usr/bin/env python3
"""Connect real Rodin single-tap wake keys to the native ColorOS AOD callback."""
import argparse
from pathlib import Path
import os
import re
import shutil

ROUTE_FIELD = '.field public static final RODIN_TAP_AOD_ROUTE_VERSION:I = 0x1'

def unique(root, suffix):
    matches=[p for p in root.rglob(Path(suffix).name) if p.as_posix().endswith(suffix)]
    if len(matches)!=1:
        raise SystemExit('Expected exactly one '+suffix+'; no files changed')
    return matches[0]

def method(text, signature):
    if text.count(signature)!=1:
        raise SystemExit('Unsupported method layout: '+signature+'; no files changed')
    start=text.index(signature);end=text.index('.end method',start)
    return start,end,text[start:end]

def prepare(root, helpers):
    policy=unique(root,'com/android/server/policy/PhoneWindowManagerExtImpl.smali')
    gestures=unique(root,'com/android/server/oplus/input/exinputservice/screenoffgesture/OplusScreenOffGesturePolicy.smali')
    touch=unique(root,'com/android/server/wm/RodinTouchBridge.smali')
    for filename in ['RodinAodTapBridge.smali','RodinTouchBridge.smali']:
        if not (helpers/filename).is_file():raise SystemExit('Missing compiled helper '+filename+'; no files changed')
    if '->sendNativeMode(II)I' not in (helpers/'RodinAodTapBridge.smali').read_text():
        raise SystemExit('Wrong single-tap helper; no files changed')
    text=policy.read_text()
    start,end,body=method(text,'.method public overrideInterceptKeyBeforeQueueing(Landroid/view/KeyEvent;I)I')
    if not re.search(r'\.registers\s+23\b',body) or 'move-object/from16 v3, p1' not in body:
        raise SystemExit('Unsupported key policy registers; no files changed')
    if 'RodinAodTapBridge;->handleKey' not in body:
        anchor='    iget-object v0, v1, Lcom/android/server/policy/PhoneWindowManagerExtImpl;->mBase:Lcom/android/server/policy/PhoneWindowManager;'
        if anchor not in body or not 0<body.index(anchor)<600:raise SystemExit('Unsupported key policy entry; no files changed')
        hook='''    invoke-static {v3}, Lcom/android/server/wm/RodinAodTapBridge;->handleKey(Landroid/view/KeyEvent;)Z
    move-result v0
    if-eqz v0, :rodin_original_key_policy
    const/4 v0, 0x0
    return v0
    :rodin_original_key_policy

'''
        body=body.replace(anchor,hook+anchor,1)
        text=text[:start]+body+text[end:]
    if 'RODIN_TAP_AOD_ROUTE_VERSION' not in text:
        pos=text.index('# direct methods')
        text=text[:pos]+ROUTE_FIELD+'\n\n'+text[pos:]
    elif ROUTE_FIELD not in text:
        raise SystemExit('Unsupported existing route version; no files changed')
    other=gestures.read_text()
    start,end,body=method(other,'.method public setGestureState(IZ)V')
    if 'RodinAodTapBridge;->refresh()V' not in body:
        pos=body.rfind('    return-void')
        if pos<0:raise SystemExit('Unsupported gesture policy return; no files changed')
        body=body[:pos]+'    invoke-static {}, Lcom/android/server/wm/RodinAodTapBridge;->refresh()V\n\n'+body[pos:]
        other=other[:start]+body+other[end:]
    return {policy:text,gestures:other},touch.parent

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('smali_root',type=Path,help='Directory containing the disassembled DEX trees')
    parser.add_argument('helper_dir',type=Path,help='Directory containing compiled RodinTouchBridge and RodinAodTapBridge smali')
    parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args()
    changes,destination=prepare(args.smali_root,args.helper_dir)
    if args.dry_run:
        print('Validated key policy, gesture policy and both helpers; no files changed')
        return
    for path,value in changes.items():
        backup=path.with_suffix(path.suffix+'.pre-rodin-tap-aod')
        if not backup.exists():shutil.copy2(path,backup)
        temporary=path.with_suffix(path.suffix+'.rodin-new')
        temporary.write_text(value);shutil.copystat(path,temporary);os.replace(temporary,path)
    for pattern in ['RodinTouchBridge*.smali','RodinAodTapBridge*.smali']:
        for helper in args.helper_dir.glob(pattern):shutil.copy2(helper,destination/helper.name)
    print('Installed native single-tap routing; other key paths retain their original policy')

if __name__=='__main__':main()
