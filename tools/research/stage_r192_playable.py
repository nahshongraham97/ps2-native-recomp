"""Stage local runtime assets and one R192 launcher, retaining the existing card."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil

W=Path(__file__).resolve().parents[2]
LAUNCH_SHA='7460acd35e90b6e838e820e8f105fc010e6ec8b59c08a07419c55e51e3b41f76'
ISO_SHA='48e395b56fe6888d9013d36b4b04d2a1decf2f52259063936a8e9f53e824966d'
ELF_SHA='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'


def digest(path):
    value=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(8<<20),b''):value.update(block)
    return value.hexdigest()


def require(ok,why):
    if not ok:raise ValueError(why)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-fork',type=Path,required=True)
    ap.add_argument('--build-root',type=Path,required=True)
    args=ap.parse_args()
    fork=args.source_fork.resolve();build=args.build_root.resolve()
    require(build.is_relative_to((W/'build').resolve()),'owned build required')
    parent=fork.parents[2];old=fork/'build/r192-hooks'
    require((old/'game/disc/armageddon.ps2').is_file(),'existing persistent card missing; never create a replacement')
    require((build/'out/ps2run.exe').is_file(),'compiled candidate missing')
    copies=[]
    sources=[
        (parent/'external/forks/ps2-native-recomp-backup-20261110/games/mkarmageddon/disc.iso',build/'game/disc/disc.iso',ISO_SHA),
        (old/'game/disc/SLUS_214.10',build/'game/disc/SLUS_214.10',ELF_SHA),
        (old/'game/disc/SYSTEM.CNF',build/'game/disc/SYSTEM.CNF',None),
        (old/'out/armageddon-keys.ini',build/'out/armageddon-keys.ini',None),
    ]
    for source,destination,expected in sources:
        before=source.stat();source_sha=digest(source)
        require(expected is None or source_sha==expected,'original asset identity mismatch: '+str(source))
        destination.parent.mkdir(parents=True,exist_ok=True)
        if not destination.exists():shutil.copyfile(source,destination)
        require(digest(destination)==source_sha,'staged asset differs; inspect rather than overwrite: '+str(destination))
        after=source.stat()
        require((before.st_size,before.st_mtime_ns)==(after.st_size,after.st_mtime_ns),'original changed during staging')
        copies.append(dict(source=str(source),destination=str(destination),sha256=source_sha,size=after.st_size))
    launcher=old/'out/launch_r192_hooks.bat'
    require(digest(launcher)==LAUNCH_SHA,'original launcher identity changed')
    text=launcher.read_text(encoding='utf8')
    text=text.replace('R192 Lineage Build (D3D12 + All Hooks)','R192 Compiled Build (D3D12 + All Hooks, No EE Live)')
    # All executable/data/module paths remain inside this candidate. The existing
    # card and its quick-state directory intentionally retain their original home.
    old_iso='set "ISO_PATH=%PARENT_REPO%\\external\\forks\\ps2-native-recomp-backup-20261110\\games\\mkarmageddon\\disc.iso"'
    require(old_iso in text,'original ISO seam')
    text=text.replace(old_iso,'set "ISO_PATH=%GAME_DIR%\\disc\\disc.iso"\n'
        'set "CARD_PATH=%PARENT_REPO%\\external\\forks\\ps2-native-recomp\\build\\r192-hooks\\game\\disc\\armageddon.ps2"\n'
        'for %%I in ("%CARD_PATH%") do set "QUICK_DIR=%%~dpI"\n'
        'if "%QUICK_DIR:~-1%"=="\\" set "QUICK_DIR=%QUICK_DIR:~0,-1%"')
    text=text.replace('%GAME_DIR%\\disc\\armageddon.ps2','%CARD_PATH%')
    text=text.replace("'%GAME_DIR%\\disc'","'%QUICK_DIR%'")
    text='\n'.join(line for line in text.splitlines() if 'COMPILER_BIN' not in line)+'\n'
    text=text.replace('set "PS2_KEYS=%OUT_DIR%\\armageddon-keys.ini"',
                      'set "PATH=%OUT_DIR%;%PATH%"\nset "PS2_KEYS=%OUT_DIR%\\armageddon-keys.ini"')
    text=text.replace('set "PS2_EE_LIVE=%GAME_DIR%\\ee-live"','set "PS2_EE_LIVE=0"')
    text=text.replace('set "PS2_VU_LIVE=%GAME_DIR%\\vu1-live"','set "PS2_VU_LIVE=0"')
    text=text.replace('set "PS2_VU_RECORD=%GAME_DIR%\\vu1"','set "PS2_VU_RECORD=%R192_DIR%\\logs\\vu1-uncompiled"')
    text=text.replace('set "MKA_INTERCEPT_COMBAT=1"',
        'if not exist "%R192_DIR%\\logs\\vu1-uncompiled" mkdir "%R192_DIR%\\logs\\vu1-uncompiled"\n'
        'if not exist "%R192_DIR%\\logs\\missing-code" mkdir "%R192_DIR%\\logs\\missing-code"\n'
        'set "PS2_MISS_DUMP_DIR=%R192_DIR%\\logs\\missing-code"\n'
        'set "MKA_INTERCEPT_COMBAT=1"')
    text=text.replace('echo [r192-hooks] Instant launch (D3D12 + guest audio + all 8 register_hook hooks + 3 --wrap wrappers + observers + tracer + profiler + PS2_NATIVE_VERTEX)',
                      'echo [r192-hooks] Compiled R192: D3D12, original hooks/settings, existing card, no EE/VU live compiler')
    require('ps2-native-recomp-backup' not in text and 'COMPILER_BIN' not in text,'external code/asset/compiler dependency survived')
    for setting in ['MKA_INTERCEPT_COMBAT=1','MKA_TRACE_MODE_INPUT=1','PS2_OBSERVER_HOOKS=1','PS2_PROFILE_SUBSYSTEMS=1','PS2_NATIVE_VERTEX=1','PS2_EE_LIVE=0','PS2_VU_LIVE=0']:
        require(setting in text,'required launcher setting missing: '+setting)
    target=build/'out/launch_r192_hooks.bat'
    target.write_bytes(text.replace('\n','\r\n').encode('utf8'))
    receipt=dict(status='STAGED_NOT_LAUNCHED',binary_sha256=digest(build/'out/ps2run.exe'),
                 launcher=str(target),launcher_sha256=digest(target),copies=copies,
                 persistent_card=str(old/'game/disc/armageddon.ps2'),
                 card_policy='Existing user card mounted directly. No card read, copy, reset, creation or write by staging.',
                 quick_state_directory=str(old/'game/disc'),original_launcher_unchanged=digest(launcher)==LAUNCH_SHA,
                 runtime_external_paths=['existing persistent card and existing quick states only; standard Windows libraries'],
                 tool_sha256=digest(Path(__file__)))
    (build/'RUNTIME_STAGE.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(receipt,indent=2))


if __name__=='__main__':main()
