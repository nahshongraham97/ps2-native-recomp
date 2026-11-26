"""Offline native link closure for NETR's full generated image and NTGUI.

The linked probes only register and read compiled entry metadata. They never
boot either image, execute guest code or enable online features. Do not mix
entry IDs from a different generated main image into NETR's call graph.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess

from extract_r192_elf_vu import require

ROOT=Path(__file__).resolve().parents[2]


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(4<<20),b''):digest.update(block)
    return digest.hexdigest()


def verify_pins(pins):
    for path,digest in pins.items():require(sha(Path(path))==digest,'input_changed:'+path)


def bind_pin(pins,path,digest):
    key=str(Path(path).resolve())
    require(key not in pins or pins[key]==digest,'source_identity_rebound:'+key)
    pins[key]=digest


HOST=r'''
#include "ps2/generated.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
void register_generated_code();
static const ps2::UnitInfo*registered_units{};static unsigned registered_count{};
static const ps2::ModuleInfo*registered_modules{};static unsigned registered_module_count{};
extern "C" void __real__ZN3ps214register_unitsEPKNS_8UnitInfoEj(const ps2::UnitInfo*,unsigned);
extern "C" void __real__ZN3ps216register_modulesEPKNS_10ModuleInfoEj(const ps2::ModuleInfo*,unsigned);
extern "C" void __wrap__ZN3ps214register_unitsEPKNS_8UnitInfoEj(const ps2::UnitInfo*u,unsigned n){
 if(registered_units)throw std::runtime_error("duplicate_unit_registration");registered_units=u;registered_count=n;__real__ZN3ps214register_unitsEPKNS_8UnitInfoEj(u,n);}
extern "C" void __wrap__ZN3ps216register_modulesEPKNS_10ModuleInfoEj(const ps2::ModuleInfo*m,unsigned n){
 if(registered_modules)throw std::runtime_error("duplicate_module_registration");registered_modules=m;registered_module_count=n;__real__ZN3ps216register_modulesEPKNS_10ModuleInfoEj(m,n);}
static unsigned entries(const ps2::UnitInfo*u,unsigned count){
 unsigned total=0;
 for(unsigned i=0;i<count;++i){if(!u[i].function||!u[i].entries||!u[i].size||(u[i].size&3))throw std::runtime_error("compiled_unit_metadata_missing");
  for(unsigned j=0;j<u[i].size/4;++j)if(u[i].entries[j])++total;}
 return total;
}
int main(int argc,char**){try{
 register_generated_code(); // Registry metadata only: no boot/call/dispatch.
 if(registered_count!=EXPECTED_UNITS||registered_module_count!=EXPECTED_MODULES)throw std::runtime_error("compiled_registration_denominator_changed");
 const auto measured=entries(registered_units,registered_count);
 if(measured!=EXPECTED_ENTRIES||(argc>1&&measured==EXPECTED_ENTRIES))throw std::runtime_error("compiled_main_entry_denominator_changed");
 std::printf("MAIN units=%u entries=%u\n",EXPECTED_UNITS,measured);
#if EXPECTED_MODULES
 for(unsigned i=0;i<EXPECTED_MODULES;++i){const auto&m=registered_modules[i];
  if(!m.name||!m.units||!m.count||!m.checks||!m.check_count)throw std::runtime_error("compiled_module_metadata_missing");
  std::printf("MODULE %s units=%u entries=%u\n",m.name,m.count,entries(m.units,m.count));}
#endif
 std::puts("PASS offline_static_image_link_and_metadata guest_instructions=0 boot=0 native_credit=0");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 1;}}
'''


def map_count(folder, prefix):
    total=units=0
    for path in sorted(folder.glob(prefix+'unit_*.cpp')):
        match=re.search(r'const std::uint16_t \w+\[(\d+)\] = \{(.*?)\};',path.read_text(),re.S)
        require(match is not None,'original_entry_array_missing:'+path.name)
        values=[int(x) for x in re.findall(r'\d+',match[2])]
        require(len(values)==int(match[1]),'original_entry_array_extent:'+path.name)
        total+=sum(bool(x) for x in values);units+=1
    return units,total


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['native-proof','runtime-root','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-proof-sha',required=True)
    p.add_argument('--workers',type=int,default=2)
    p.add_argument('--reuse-link-proof',type=Path)
    p.add_argument('--expected-reuse-sha')
    a=p.parse_args();out=a.output.resolve();n=a.runtime_root.resolve()
    require(out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    require(1<=a.workers<=4,'bounded_offline_workers_required')
    require(sha(a.native_proof)==a.expected_proof_sha,'native_body_proof_changed')
    proof=json.loads(a.native_proof.read_text())
    require(proof['status']=='OFFLINE_DISC_IMAGE_GENERATION_PASS' and proof['native_compilation_all_pass'],'completed_body_compilation_required')
    for path,digest in proof['input_pins'].items():require(sha(Path(path))==digest,'body_input_changed:'+path)
    for rows in proof['generated_files'].values():
        for pin in rows:require(sha(Path(pin['path']))==pin['sha256'],'body_source_changed')
    for row in proof['native_objects']:
        require(sha(Path(row['source']))==row['source_sha256'] and sha(Path(row['object']))==row['object_sha256'],'native_body_object_changed')
    require(sha(a.compiler)=='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe','compiler_pin')
    runtime=n/'out/libps2runtime.a'
    require(sha(runtime)=='2f7d85506da2298aea415a343b099e2f280e6349ac2604ac55cc4c3a126e08ec','original_runtime_pin')
    folders={name:Path(rows[0]['path']).parent for name,rows in proof['generated_files'].items()}
    reused={name:[row for row in proof['native_objects'] if row['image']==name] for name in folders}
    support=[runtime,n/'src/third_party/sdl2/lib/libSDL2.dll.a',n/'src/third_party/wgpu-native/lib/libwgpu_native.a']
    inputs=[Path(__file__),a.native_proof,a.compiler,*support,*(n/'src/runtime/include/ps2').glob('*.hpp')]
    pins={str(x.resolve()):sha(x) for x in inputs}
    for rows in proof['generated_files'].values():
        for pin in rows:pins[str(Path(pin['path']).resolve())]=pin['sha256']
    for row in proof['native_objects']:
        for key in ('source','object'):pins[str(Path(row[key]).resolve())]=row[key+'_sha256']
    cached={}
    require(bool(a.reuse_link_proof)==bool(a.expected_reuse_sha),'reuse_receipt_and_SHA_pair_required')
    if a.reuse_link_proof:
        require(a.reuse_link_proof.resolve().is_relative_to(ROOT/'build/oracle-telemetry') and sha(a.reuse_link_proof)==a.expected_reuse_sha,'reuse_link_proof_changed')
        old=json.loads(a.reuse_link_proof.read_text());require(old['status']=='PASS_OFFLINE_STATIC_IMAGE_LINK_CLOSURE','successful_previous_link_required')
        # The authoring tool changed to add these guards. Compiler/ABI/runtime
        # and every other measured support input must remain identical.
        for path,digest in old['source_pins'].items():
            if Path(path).resolve()!=Path(__file__).resolve():require(sha(Path(path))==digest,'reuse_support_input_changed:'+path)
        for row in old['compiled_objects']:
            source=Path(row['source']).resolve()
            if source.parent in folders.values():cached[source]=row
        pins[str(a.reuse_link_proof.resolve())]=a.expected_reuse_sha
    out.mkdir(parents=True);commands=[];objects=[]
    def space():require(shutil.disk_usage(out).free>(12<<30)+(128<<20),'disk_reserve_required')
    def compile_one(image,source,extra=()):
        space();obj=out/image/(source.stem+'.obj')
        source_digest=sha(source);expected=pins.get(str(source.resolve()))
        require(expected is None or expected==source_digest,'source_changed_before_compile:'+str(source))
        argv=[str(a.compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-DNDEBUG','-I'+str(n/'src/runtime/include'),'-I'+str(folders[image]),*extra,'-c',str(source),'-o',str(obj)]
        prior=cached.get(source.resolve())
        if prior:
            require(prior['exit']==0 and prior['argv'][:-2]==argv[:-2] and source_digest==prior['source_sha256'] and sha(Path(prior['object']))==prior['object_sha256'],'reused_compile_input_or_flags_changed')
            return {**prior,'verified_reuse_proof':str(a.reuse_link_proof),'verified_reuse_proof_sha256':a.expected_reuse_sha}
        cp=subprocess.run(argv,capture_output=True,timeout=240);log=obj.with_suffix('.log');log.write_bytes(cp.stdout+cp.stderr)
        require(sha(source)==source_digest,'source_changed_during_compile:'+str(source))
        return {'image':image,'source':str(source),'source_sha256':source_digest,'object':str(obj),'object_sha256':sha(obj) if obj.exists() else None,'argv':argv,'exit':cp.returncode,'log_sha256':sha(log)}
    jobs=[];expectations={}
    for name,folder in folders.items():
        (out/name).mkdir()
        main=map_count(folder,'');modules={}
        for prefix in sorted({m[1] for p in folder.glob('m_*unit_*.cpp') if (m:=re.match(r'm_(.+)_unit_',p.name))}):
            modules[prefix]=map_count(folder,'m_'+prefix+'_')
        expectations[name]={'main_units':main[0],'main_entries':main[1],'modules':modules}
        # Only exact measured body objects are reused. All other main/module
        # bodies and the image's OWN table are compiled from this same graph.
        reused_sources={Path(row['source']).resolve() for row in reused[name]}
        jobs += [(name,path) for path in sorted(folder.glob('*.cpp')) if path.resolve() not in reused_sources]
    (out/'CONFIGURED.json').write_text(json.dumps({'source_pins':pins,'expectations':expectations,'jobs':len(jobs),'reused_objects':{k:len(v) for k,v in reused.items()}},indent=2)+'\n',newline='\n')
    with ThreadPoolExecutor(max_workers=a.workers) as pool:
        future=[pool.submit(compile_one,*job) for job in jobs]
        for index,f in enumerate(as_completed(future),1):
            row=f.result();objects.append(row)
            if index%20==0 or row['exit']:print(json.dumps({'validated_objects':index,'total':len(jobs),'source':Path(row['source']).name,'verified_reuse':'verified_reuse_proof' in row,'exit':row['exit']}),flush=True)
    (out/'COMPILED.json').write_text(json.dumps(objects,indent=2)+'\n',newline='\n')
    require(all(row['exit']==0 for row in objects),'additional_generated_object_compile_refused')
    cases=[]
    env=dict(os.environ);env['PATH']=str(n/'out')+os.pathsep+str(a.compiler.parent)+os.pathsep+env.get('PATH','')
    for name,folder in folders.items():
        expected=expectations[name];host=out/name/'metadata_probe.cpp';host.write_text(HOST,encoding='utf8',newline='\n')
        defines=[f'-DEXPECTED_UNITS={expected["main_units"]}u',f'-DEXPECTED_ENTRIES={expected["main_entries"]}u',f'-DEXPECTED_MODULES={len(expected["modules"])}']
        built=compile_one(name,host,defines);objects.append(built);require(built['exit']==0,'metadata_probe_compile_refused:'+name)
        exe=out/name/'offline-link-probe.exe'
        body_objects=[row['object'] for row in objects if row['image']==name]+[row['object'] for row in reused[name]]
        argv=[str(a.compiler),'-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-Wl,--wrap=_ZN3ps214register_unitsEPKNS_8UnitInfoEj','-Wl,--wrap=_ZN3ps216register_modulesEPKNS_10ModuleInfoEj',*body_objects,*map(str,support),'-lbcrypt','-lwinmm',*['-l'+s for s in ['ws2_32','userenv','ntdll','synchronization','pthread','m','kernel32','user32','gdi32','winspool','shell32','ole32','oleaut32','uuid','comdlg32','advapi32']],'-o',str(exe)]
        # Exact argv uses a response file solely to stay below Windows limits.
        rsp=out/name/'link.rsp';rsp.write_text('\n'.join('"'+x.replace('\\','/').replace('"','\\"')+'"' for x in argv[1:])+'\n',newline='\n')
        cp=subprocess.run([str(a.compiler),'@'+str(rsp)],capture_output=True,timeout=240);(out/name/'link.log').write_bytes(cp.stdout+cp.stderr)
        commands.append({'image':name,'argv':argv,'exit':cp.returncode,'response_sha256':sha(rsp)})
        if cp.returncode:cases.append({'image':name,'status':'LINK_REFUSED','log_sha256':sha(out/name/'link.log')});continue
        cp=subprocess.run([str(exe)],capture_output=True,env=env,timeout=60);(out/name/'run.log').write_bytes(cp.stdout+cp.stderr)
        observed_main=re.findall(rb'MAIN units=(\d+) entries=(\d+)',cp.stdout)
        observed_modules={m[0].decode():(int(m[1]),int(m[2])) for m in re.findall(rb'MODULE (\w+) units=(\d+) entries=(\d+)',cp.stdout)}
        exact=observed_main==[(str(expected['main_units']).encode(),str(expected['main_entries']).encode())] and observed_modules==expected['modules']
        plant=subprocess.run([str(exe),'--planted-count'],capture_output=True,env=env,timeout=60);(out/name/'count-plant.log').write_bytes(plant.stdout+plant.stderr)
        passed=cp.returncode==0 and exact and plant.returncode==1 and b'compiled_main_entry_denominator_changed' in plant.stderr
        cases.append({'image':name,'status':'PASS' if passed else 'METADATA_REFUSED','binary_sha256':sha(exe),'actual':cp.stdout.decode(errors='replace'),'exit':cp.returncode,'metadata_exact':exact,'count_plant_exit':plant.returncode})
    # Guard every source and object actually used, not just the small support
    # set. Reused and newly compiled generated objects are both immutable.
    for row in objects:
        bind_pin(pins,row['source'],row['source_sha256'])
        bind_pin(pins,row['object'],row['object_sha256'])
    verify_pins(pins)
    controls=[];directory=out/'final-hash-controls';directory.mkdir()
    for name in ('source','reused-object','new-object'):
        good=directory/(name+'-authentic.bin');bad=directory/(name+'-changed.bin');good.write_bytes(b'original');bad.write_bytes(b'changed')
        digest=sha(good);verify_pins({str(good):digest});refused=False
        try:verify_pins({str(bad):digest})
        except ValueError as error:refused=str(error)=='input_changed:'+str(bad)
        require(refused,'changed_final_input_accepted:'+name);controls.append({'name':name,'status':'REJECTED','expected_sha256':digest,'changed_sha256':sha(bad)})
    rejected=False
    try:bind_pin({str(good.resolve()):digest},good,sha(bad))
    except ValueError as error:rejected=str(error)=='source_identity_rebound:'+str(good.resolve())
    require(rejected,'source_identity_rebinding_accepted');controls.append({'name':'overwrite_original_source_identity','status':'REJECTED'})
    result={'status':'PASS_OFFLINE_STATIC_IMAGE_LINK_CLOSURE' if all(c['status']=='PASS' for c in cases) else 'OFFLINE_STATIC_IMAGE_LINK_HAS_REFUSALS','source_pins':pins,'compiled_objects':objects,'reused_objects':reused,'verified_previous_compilations':sum('verified_reuse_proof' in row for row in objects),'final_full_input_controls':controls,'link_commands':commands,'cases':cases,'expectations':expectations,'limits':['Metadata registration/reading only, zero guest instructions or boot.','No online activation, game integration, NTGUI execution, differential equivalence or native credit.','NETR uses its own matching full generated main/module graph; no mixing shifted entry IDs with the installed S4 image.','Any unresolved native symbol/link/metadata mismatch remains an explicit refusal.']}
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
    print(json.dumps({'status':result['status'],'cases':cases,'result_sha256':sha(target)}))
    return 0 if all(c['status']=='PASS' for c in cases) else 2


if __name__=='__main__':raise SystemExit(main())
