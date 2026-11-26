"""Bounded actual v8 cold-loader/start/IRQ execution through fresh code leases.

Reuses the frozen70-image object/catalogue build. No game/card/GUI operation,
original writes, bulk generation, SDK registration-history or native credit.
"""
from __future__ import annotations
import sys
sys.dont_write_bytecode=True
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
import generate_r192_iop_aot as common
import build_r192_iop_v8_candidate as builder
from verify_r192_loader_owner_corpus import Compression

W,need,sha=common.W,common.require,common.file_sha
OWNER_SHA='6bb83e2f228e13e6d85ce1f03d475a825a8ba506bf0da7177d8f37f9a5b90900'


def reserve(extra=0):need(shutil.disk_usage(W).free>(12<<30)+(8<<20)+extra,'12GiB_execution_proof_reserve')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('owner-proof','image','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.owner_proof=a.owner_proof.resolve();a.image=a.image.resolve();a.output=a.output.resolve()
    need(sha(a.owner_proof)==OWNER_SHA,'published_owner_proof_changed')
    need(a.output.is_relative_to(W/'build/oracle-telemetry') and not a.output.exists(),'fresh_owned_output_required')
    old=json.loads(a.owner_proof.read_text());need(old['catalogue']['all_original_source_images']==70,'original_domain_not70')
    catalogue=a.owner_proof.parent/'original-sources.inc';need(sha(catalogue)==old['catalogue']['generated_include_sha256'],'original_source_catalogue_changed')
    fixture=W/'tests/test_r192_fresh_execution_owner.cpp';header=W/'runtime/src/iop/r192_fresh_execution_owner.hpp'
    public_prefix=W/'tests/test_r192_iop_v8_start.cpp';old_fixture=W/'tests/test_r192_loader_code_owner.cpp'
    expected={x['path']:x['sha256'] for x in old['source_pins']}
    build=list(old['build_command']);used={catalogue,a.image,public_prefix}
    for item in build:
        f=Path(item)
        if f.is_file() and f.suffix.lower() in ('.obj','.a','.cpp','.hpp','.h'):used.add(f.resolve())
        if item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='):used.add(Path(item.split('=',1)[1].strip('"')).resolve())
    for name in ('r192_loader_code_owner.hpp','r192_static_metadata_guard.hpp','r192_pic.hpp','r192_aot.hpp'):
        used.add(W/'runtime/src/iop'/name)
    pins=[{'path':str(f),'sha256':sha(f)} for f in sorted(used,key=str)]
    for pin in pins:
        previous=expected.get(pin['path'])
        if previous:need(previous==pin['sha256'],'frozen_build_input_changed:'+pin['path'])
    pins += [{'path':str(f),'sha256':sha(f)} for f in (Path(__file__).resolve(),fixture,header,a.owner_proof,catalogue,Path(builder.__file__))]
    compiler=Path(build[0]);need(str(compiler) in expected and sha(compiler)==expected[str(compiler)],'compiler_changed')
    pins.append({'path':str(compiler),'sha256':sha(compiler)})
    # The source catalogue is pinned by the published70-image receipt. C++
    # independently repeats this membership check immediately before loading.
    original_digests=set(re.findall(r'^\{"([0-9a-f]{64})",',catalogue.read_text(),re.M))
    need(len(original_digests)==70 and sha(a.image) in original_digests,'actual_original_IRX_not_in_pinned70')
    reserve(128<<20);a.output.mkdir(parents=True);archive=a.output/'source-archive';archive.mkdir()
    for f in (Path(__file__).resolve(),fixture,header):(archive/f.name).write_bytes(f.read_bytes())
    prefix_text=public_prefix.read_text().split('int main(',1)[0]
    seam='extern "C" void __real__ZN3ps23iop7executeERNS0_3CpuERx'
    need(prefix_text.count(seam)==1,'actual_execute_wrapper_seam_changed')
    prefix_text=prefix_text.replace(seam,'namespace { std::function<void(Cpu &, std::int64_t &)> execution_override; }\n'+seam)
    before='    __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);'
    need(prefix_text.count(before)==1,'actual_execute_body_seam_changed')
    prefix_text=prefix_text.replace(before,'    if (execution_override) execution_override(cpu, budget); else __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);')
    prefix=a.output/'original-prefix-with-execution-bridge.inc';prefix.write_text(prefix_text,encoding='utf-8',newline='\n')
    command=[]
    for item in build:
        if item==str(old_fixture):item=str(fixture)
        elif item.startswith('-DR192_OWNER_FIXTURE_PREFIX='):item='-DR192_OWNER_FIXTURE_PREFIX="'+prefix.as_posix()+'"'
        command.append(item)
    need(str(fixture) in command and command[-2]=='-o','frozen_component_build_layout_changed')
    exe=a.output/'fresh-owner-execution.exe';command[-1]=str(exe)
    rsp=a.output/'build.rsp';rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:])+'\n',encoding='utf-8',newline='\n')
    started=time.perf_counter();compiled=subprocess.run([str(compiler),'@'+str(rsp)],capture_output=True,text=True,timeout=240)
    (a.output/'build.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8',newline='\n')
    if compiled.returncode:
        fail={'status':'BUILD_REFUSED','build_command':command,'build_exit':compiled.returncode,'source_pins':pins}
        out=a.output/'RESULT.json';out.write_text(json.dumps(fail,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'result':str(out),'status':fail['status'],'build_exit':compiled.returncode}));return 1
    compression=Compression(a.output);compressed=[compression.compress(exe)];binary_sha=sha(exe)
    runtime=next(Path(x).parent.parent for x in command if x.endswith('libps2runtime.a'))
    env={k:v for k,v in os.environ.items() if not k.startswith(('PS2_','MKA_','SDL_'))}
    env['PATH']=str(runtime/'out')+os.pathsep+str(compiler.parent)+os.pathsep+env.get('PATH','')
    env.update({'PS2_EE_LIVE':'0','PS2_VU_LIVE':'0','SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'})
    cases=[]
    for route in ('filename','buffer'):
        runs={}
        for mode in ('reference','compiled'):
            reserve(20<<20);prefix_out=a.output/(route+'-'+mode);cmd=[str(exe),mode,str(a.image),route,str(prefix_out)]
            need(sha(exe)==binary_sha,'compiled_binary_changed_before_run')
            at=time.perf_counter();r=subprocess.run(cmd,cwd=a.output,env=env,capture_output=True,text=True,timeout=60)
            Path(str(prefix_out)+'.log').write_text(r.stdout+r.stderr,encoding='utf-8',newline='\n')
            measured=json.loads(Path(str(prefix_out)+'.json').read_text());runs[mode]={'argv':cmd,'exit_code':r.returncode,'seconds':time.perf_counter()-at,'measurement':measured}
            for f in a.output.glob(prefix_out.name+'.*'):
                if f.is_file() and f.stat().st_size>65536:compressed.append(compression.compress(f))
        comparisons={suffix:Path(str(a.output/(route+'-reference'))+suffix).exists() and Path(str(a.output/(route+'-compiled'))+suffix).exists() and
            sha(Path(str(a.output/(route+'-reference'))+suffix))==sha(Path(str(a.output/(route+'-compiled'))+suffix)) for suffix in ('.trace','.cpu','.ram','.iop-state')}
        cases.append({'route':route,'runs':runs,'comparisons':comparisons,'status':'PASS' if all(comparisons.values()) and all(x['exit_code']==0 for x in runs.values()) else 'REFUSED'})
    controls=[];expected_controls={'wrong_word':2,'wrong_image':1,'free_reuse':4,'noop_free':1,'overlap_current':2,'wrong_facts':2,'metadata_pc':1,'actual_reset':2,'restore_boundary':3}
    for mode in expected_controls:
        reserve(4<<20);prefix_out=a.output/mode;cmd=[str(exe),mode,str(a.image),'filename',str(prefix_out)]
        need(sha(exe)==binary_sha,'compiled_binary_changed_before_control')
        r=subprocess.run(cmd,cwd=a.output,env=env,capture_output=True,text=True,timeout=60)
        Path(str(prefix_out)+'.log').write_text(r.stdout+r.stderr,encoding='utf-8',newline='\n')
        measured=json.loads(Path(str(prefix_out)+'.json').read_text());controls.append({'mode':mode,'argv':cmd,'exit_code':r.returncode,'measurement':measured})
    changed=[x['path'] for x in pins if sha(Path(x['path']))!=x['sha256']]
    ok=all(x['status']=='PASS' for x in cases) and all(x['exit_code']==0 and x['measurement']['controls']==expected_controls[x['mode']] for x in controls) and not changed and sha(exe)==binary_sha
    result={'status':'FRESH_SOURCE_OWNER_EXECUTION_COMPONENT_PASS' if ok else 'REFUSED','classification':'actual frozen-v8 cold-loader/start/IRQ component; not game cold boot or SDK-complete/native credit',
        'native_credit':0,'source_pins':pins,'input_changes':changed,'build_command':command,'binary_sha256':binary_sha,'binary_unchanged_after_runs':sha(exe)==binary_sha,'elapsed_seconds':time.perf_counter()-started,
        'cases':cases,'controls':controls,'transparent_compression':compressed,
        'remaining':['Actual game/private-kernel loader/free/reset/BEFORE-full-parser wiring is not yet adopted by this component proof.',
            'Restored states retain an explicitly unadmitted old route; unique nonoverlapping restored-CODE identity is a separate proof, never fresh load or SDK history.',
            'loadcore#7 remains the original zero-return reference stub and explicitly OPEN SDK semantics, not an admitted native release.',
            'Direct guest-export routing, arbitrary callback/thread generations on reused ranges, IRQ blocking/yield/exception paths and whole-game IOP closure remain OPEN.',
            'The real source export leaf only measures actual v8 IRQ current-Cpu transport; its SDK callable ABI/registration by retail code is not inferred.',
            'Manual start comparison excludes its known pending scheduler on_exit closure from serialization, exactly as the published original start fixture.']}
    out=a.output/'RESULT.json';out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'result':str(out),'sha256':sha(out),'status':result['status'],'cases':[x['status'] for x in cases],'controls':[x['exit_code'] for x in controls]}))
    return 0 if ok else 1


if __name__=='__main__':raise SystemExit(main())
