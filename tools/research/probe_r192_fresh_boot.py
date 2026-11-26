"""Bounded headless diagnostic from the original ELF, without a saved state.

Only an owned companion-card copy is mounted. No gameplay/boot-completion,
hardware/audio correctness, universal code coverage or native credit follows.
"""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import time

import replay_r192_aot_sessions as common

ROOT=Path(__file__).resolve().parents[2]


def assess(text,code,timed_out,frames,misses,changes):
    objections=[]
    if code:objections.append('nonzero_exit')
    if timed_out:objections.append('timeout')
    if re.search(r'^\[state\] loaded frame ',text,re.M):objections.append('unexpected_state_load')
    stops=re.findall(r'^\[ee\] stopped after (\d+) frames, (\d+) cycles: (.*)$',text,re.M)
    if len(stops)!=1 or int(stops[0][0])!=frames or stops[0][2].strip()!='frame limit reached':objections.append('fresh_frame_budget_not_measured')
    for name,pattern in {
        'code_miss':r'missing code at|untranslated code|\[(?:vu[01]|iop)[^\]]*aot-(?:miss|stop)\]|bad[_ -]?entry',
        'runtime_compiler':r'\[(?:ee|vu)-live\b|\btranslating code\b|\bcompiling\b',
        'window_or_audio_endpoint':r'\[window\]|\[audio\] sound device',
        'named_failure':r'\bREFUSED\b|IOP_AOT_REFUSED|\bUNIMPLEMENTED\b',
    }.items():
        if re.search(pattern,text,re.I):objections.append(name)
    ee=re.findall(r'^\[sealed-ee-aot\] registered (\d+) modules from (\d+) sealed precompiled DLLs; no runtime compiler\s*$',text,re.M)
    vu=re.findall(r'^\[sealed-vu-aot\] registered (\d+) native entries from (\d+) offline DLLs; no runtime compiler\s*$',text,re.M)
    if ee!=[('3569','3569')]:objections.append('EE_registration_not_exact')
    if vu!=[('65026','99')]:objections.append('VU_registration_not_exact')
    if misses.strip():objections.append('miss_file_not_empty')
    if changes:objections.append('input_changed')
    return {'status':'PASS' if not objections else 'REFUSED','objections':objections,'stop_summaries':stops,'EE_registration':ee,'VU_registration':vu}


def self_test():
    good='[sealed-ee-aot] registered 3569 modules from 3569 sealed precompiled DLLs; no runtime compiler\n[sealed-vu-aot] registered 65026 native entries from 99 offline DLLs; no runtime compiler\n[ee] stopped after 120 frames, 12345 cycles: frame limit reached\n'
    common.require(assess(good,0,False,120,'',[])['status']=='PASS','fresh_positive_control')
    cases=[('state_load',good+'[state] loaded frame 0\n',0,False,'',[]),
           ('early_stop',good.replace('after 120','after 119'),0,False,'',[]),
           ('compiler',good+'[ee-live] translating code\n',0,False,'',[]),
           ('code_miss',good+'[ee] missing code at 008A5140\n',0,False,'',[]),
           ('IOP_refusal',good+'[iop-aot-v8] IOP_AOT_REFUSED:missing_identity\n',0,False,'',[]),
           ('audio_endpoint',good+'[audio] sound device opened\n',0,False,'',[]),
           ('wrong_VU_set',good.replace('65026','61'),0,False,'',[]),
           ('invented_VU_text',good.replace('65026 native','65026 unique native'),0,False,'',[]),
           ('nonzero_exit',good,1,False,'',[]),('timeout',good,0,True,'',[]),
           ('miss_file',good,0,False,'008A5140',[]),('changed_input',good,0,False,'',['changed'])]
    for name,text,code,timeout,misses,changed in cases:
        common.require(assess(text,code,timeout,120,misses,changed)['status']=='REFUSED','fresh_plant_accepted:'+name)
    return {'positive':True,'rejected':[row[0] for row in cases]}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['candidate','source-fork','card-copy','output']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-binary-sha',required=True)
    p.add_argument('--frames',type=int,default=120);p.add_argument('--timeout-seconds',type=int,default=180)
    a=p.parse_args();out=a.output.resolve();candidate=a.candidate.resolve();card=a.card_copy.resolve()
    common.require(out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    common.require(candidate.is_relative_to(ROOT/'build') and card.is_relative_to(ROOT/'build/oracle-telemetry') and card.suffix=='.ps2','owned_candidate_and_companion_required')
    common.require(1<=a.frames<=600 and 1<=a.timeout_seconds<=300,'bounded_boot_limits_required')
    common.require(shutil.disk_usage(out.parent).free>(12<<30)+(64<<20),'12GiB_boot_reserve_required')
    elf=ROOT/'build/r192-compiled-konquest/game/disc/SLUS_214.10'
    iso=a.source_fork.resolve().parent/'ps2-native-recomp-backup-20261110/games/mkarmageddon/disc.iso'
    pins=[common.pin(candidate,a.expected_binary_sha),common.pin(elf,common.ELF_SHA),common.pin(iso,common.ISO_SHA),common.pin(card),common.pin(Path(__file__)),common.pin(Path(common.__file__))]
    ee=candidate.parent/'aot-modules/ee/manifest.txt';pins.append(common.pin(ee,common.MANIFEST_SHA))
    lines=ee.read_text(encoding='ascii').splitlines();common.require(len(lines)==3570 and lines[0]=='R192_AOT_EE_V1 A2659FA9BD846661 1456 328 3569','EE_manifest_changed')
    for line in lines[1:]:
        row=re.fullmatch(r'([0-9a-f]{64}) ([01]) (\d+) ([A-Za-z0-9_.-]+\.dll)',line)
        common.require(row is not None and '..' not in row[4],'EE_manifest_row')
        pins.append(common.pin(ee.parent/row[4],row[1]))
    vu=candidate.parent/'aot-modules/vu/manifest.txt';pins.append(common.pin(vu));meta,rows=common.parse_vu_manifest(vu.read_text(encoding='ascii'))
    common.require(meta['programs']==65026 and meta['DLLs']==99,'complete_original_upload_VU_package_required')
    common.require({p.name for p in vu.parent.glob('*.dll')}=={r['name'] for r in rows},'VU_package_file_set_changed')
    pins += [common.pin(vu.parent/row['name'],row['sha256']) for row in rows]
    out.mkdir();program=out/'SLUS_214.10';program.write_bytes(elf.read_bytes());disposable=out/'disposable-boot-card.ps2';disposable.write_bytes(card.read_bytes());(out/'quick').mkdir()
    pins.append(common.pin(program,common.ELF_SHA))
    initial_card=common.pin(disposable,next(row['sha256'] for row in pins if Path(row['path'])==card))
    tests=self_test();command=[str(candidate),str(program),'--disc',str(iso),'--memcard',str(disposable),'--no-record','--quick-dir',str(out/'quick'),'--misses',str(out/'misses.txt'),'--frames',str(a.frames)]
    env={k:v for k,v in os.environ.items() if not k.startswith(('PS2_','MKA_','SDL_'))}
    overrides={'PS2_EE_LIVE':'0','PS2_VU_LIVE':'0','PS2_EE_LIVE_BG':'0','PS2_THREADS':'0','PS2_RECORD':'0','MKA_INTERCEPT_COMBAT':'1','MKA_TRACE_MODE_INPUT':'1','PS2_OBSERVER_HOOKS':'1','PS2_PROFILE_SUBSYSTEMS':'1','PS2_NATIVE_VERTEX':'1','PS2_MISS_DUMP_DIR':str(out/'misses'),'PS2_VU_RECORD':str(out/'vu-record')};env.update(overrides)
    configured={'command':command,'cwd':str(candidate.parent),'overrides':overrides,'source_pins':pins,'initial_disposable_card':initial_card,'frames':a.frames,'timeout_seconds':a.timeout_seconds,'self_test':tests,'state_load':False}
    (out/'CONFIGURED.json').write_text(json.dumps(configured,indent=2)+'\n',newline='\n')
    began=time.monotonic();timed_out=False
    with (out/'stdout.log').open('xb') as stdout,(out/'stderr.log').open('xb') as stderr:
        process=subprocess.Popen(command,cwd=candidate.parent,env=env,stdout=stdout,stderr=stderr,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        try:code=process.wait(timeout=a.timeout_seconds)
        except subprocess.TimeoutExpired:
            timed_out=True;process.kill();code=process.wait() # This owned child only.
    elapsed=time.monotonic()-began
    changes=[row['path'] for row in pins if common.hash_file(Path(row['path']))!=row['sha256']]
    text=(out/'stdout.log').read_text(errors='replace')+'\n'+(out/'stderr.log').read_text(errors='replace');misses=(out/'misses.txt').read_text(errors='replace') if (out/'misses.txt').exists() else ''
    result=assess(text,code,timed_out,a.frames,misses,changes)
    result.update(configured|{'exit_code':code,'timed_out':timed_out,'elapsed_seconds':elapsed,'input_changes':changes,'disposable_card_after':common.pin(disposable),'log_pins':[common.pin(out/n) for n in ['stdout.log','stderr.log']],'scope':'One fresh-ELF headless frame-budget diagnostic with copied existing companion card; no saved state, window, audio endpoint, gameplay/initialization/whole-game acceptance or native credit.'})
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps({'status':result['status'],'objections':result['objections'],'result_sha256':common.hash_file(target)}))
    return 0 if result['status']=='PASS' else 2


if __name__=='__main__':raise SystemExit(main())
