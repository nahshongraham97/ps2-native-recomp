"""Build a fresh owned reference candidate from admitted VU source outputs.

No existing archive is edited, no launcher is promoted, and no card is opened.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time
from expand_r192_vu_sources import functions

ROOT=Path(__file__).resolve().parents[2]

def require(ok,reason):
    if not ok:
        raise ValueError(reason)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base-build','vu-proof','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-base-binary-sha',required=True)
    a=p.parse_args();base=a.base_build.resolve();out=a.output.resolve()
    require(base.is_relative_to(ROOT/'build') and out.is_relative_to(ROOT/'build'),'owned_builds_required')
    require(not out.exists() and not out.is_relative_to(base),'fresh_separate_output_required')
    require(sha(base/'out/ps2run.exe')==a.expected_base_binary_sha,'reference_binary_pin')
    raw=a.vu_proof.read_bytes();proof=json.loads(raw);vu=a.vu_proof.parent/'merged'
    require(proof['status']=='SOURCE_BOUNDED_MPG_TARGET_GENERATION_PASS','source_bounded_proof_required')
    require(proof['script_sha256']==sha(Path(__file__).with_name('extract_r192_elf_vu.py')),'proof_producer_changed')
    for row in proof['generated_files']:
        require(sha(vu/row['name'])==row['sha256'],'VU_source_pin')
    old,_,_=functions(base/'generated_ee');new,_,_=functions(vu)
    rejected=json.loads((ROOT/'research/ledger/aot_coverage_inventory.json').read_text())['vu_microprograms']['rejected_captures']
    bad={name.split('_')[0].upper() for name in rejected}
    require((set(old)-set(new))<=bad,'valid_compiled_entry_dropped')
    require(not (set(new)&bad),'disproved_capture_retained')
    require(set(new)-set(old)==set(proof['additional_keys']),'unproved_entry_added')
    require(all(old[k]==new[k] for k in set(old)&set(new)),'existing_generated_body_changed')
    before={str(x):sha(x) for x in [base/'out/ps2run.exe',base/'out/libps2game.a',a.vu_proof]}
    out.mkdir(parents=True);shutil.copytree(base/'src',out/'src')
    gen=out/'generated_ee';gen.mkdir()
    for source in (base/'generated_ee').iterdir():
        if source.is_file() and not source.name.startswith('vu1_'):
            shutil.copyfile(source,gen/source.name)
    for source in vu.glob('*.cpp'):
        shutil.copyfile(source,gen/source.name)
    shutil.copytree(base/'out/aot-modules',out/'out/aot-modules')
    config=json.loads((base/'CONFIGURE.json').read_text())
    argv=[x.replace(str(base),str(out)) for x in config['argv'] if not x.startswith('-DPS2_RUNTIME_LIB=')]
    require(any(x.startswith('-DPS2_GENERATED=') for x in argv),'local_generated_library_required')
    require('-DPS2_PREBUILT_GAME_LIB=OFF' in argv,'prebuilt_game_library_forbidden')
    (out/'CONFIGURE.json').write_text(json.dumps(dict(config,argv=argv),indent=2)+'\n')
    env=dict(os.environ);env['PATH']=str(Path(config['compiler']).parent)+os.pathsep+env['PATH']
    commands=[]
    for name,command in [('configure',argv),('compile',[argv[0],'--build',str(out/'out'),'--target','ps2run','--parallel','4'])]:
        start=time.monotonic()
        with (out/(name+'.log')).open('wb') as log:
            run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=2400)
        commands.append({'name':name,'argv':command,'exit_code':run.returncode,'seconds':time.monotonic()-start})
        require(run.returncode==0,'candidate_'+name+'_failed')
        print(json.dumps(commands[-1]),flush=True)
    require(before=={x:sha(Path(x)) for x in before},'reference_changed')
    source_hashes={str(x.relative_to(out)):sha(x) for folder in ['src','generated_ee'] for x in (out/folder).rglob('*') if x.is_file()}
    result={'status':'COMPILED_SOURCE_ADMITTED_VU_REFERENCE_CANDIDATE','binary_sha256':sha(out/'out/ps2run.exe'),
            'VU_keys':len(new),'excluded_disproved_keys':sorted(set(old)-set(new)),
            'additional_source_bounded_keys':proof['additional_keys'],'source_hashes':source_hashes,
            'commands':commands,'reference_pins':before,'script_sha256':sha(Path(__file__)),
            'scope':'Fresh local game AND runtime archives. No launcher/card/protected writes or native credit. Remaining EE/IOP/VU domains and whole-game/native acceptance stay OPEN.'}
    record=out/'RESULT.json';record.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'result':str(record),'sha256':sha(record),'binary_sha256':result['binary_sha256'],'VU_keys':len(new)}))

if __name__=='__main__':
    main()
