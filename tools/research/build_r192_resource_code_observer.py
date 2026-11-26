"""Add a read-only descriptor/code-identity diagnostic to the frozen candidate04.

The three admitted native plans and all AOT dispatch remain unchanged. This does
not install a normal build, enable runtime compilation or admit new execution.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess

import verify_r192_resource_word_equality as auth

ROOT=Path(__file__).resolve().parents[2]


def require(ok,why):
    if not ok:raise ValueError(why)


def pin(q):return dict(path=str(Path(q).resolve()),sha256=auth.digest(q))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['base-build','word-proof','runtime-root','compiler','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();base=a.base_build.resolve();out=a.output.resolve();normal=a.runtime_root.resolve()
    require(out.is_relative_to(ROOT/'build') and not out.exists(),'fresh_owned_output_required')
    require(normal==ROOT/'build/r192-compiled-konquest','frozen_actual_include_root_required')
    require(auth.digest(a.compiler)=='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe','frozen_compiler_required')
    receipt=base/'BUILD.json';require(auth.digest(receipt)=='57b220ba8816b596b44f04ee467b7d90a546bd58d95e1e7dc315ee06b6917694','frozen_base_build_changed')
    old=json.loads(receipt.read_text());require(old['status']=='BOUNDED_THREE_PLAN_RESOURCE_ADAPTER_BUILT' and not old['input_changes'],'frozen_connected_base_required')
    require(auth.digest(old['candidate'])==old['candidate_sha256']=='6bab8b9cfa9f0ae28bc027841825f1ce0ebd89fd75e046fb7e189f88de7715b5','frozen_base_PE_changed')
    proof=a.word_proof/'RESULT.json';require(auth.digest(proof)=='78bbfc99ff7f2413ce68c05ea6081b0e9244b5209585e0303f833ee7f7e65f30','word_equation_proof_changed')
    measured=json.loads(proof.read_text());require(measured['status']=='PASS_BOUNDED_DESCRIPTOR_WORD_EQUALITY' and not measured['input_changes'],'word_equations_not_measured')
    for row in old['source_pins']+old['private_sources']+old['private_objects']+old['package_pins']+measured['source_pins']:
        require(auth.digest(row['path'])==row['sha256'],'frozen_input_changed:'+row['path'])
    require(shutil.disk_usage(out.parent).free>(12<<30)+Path(old['candidate']).stat().st_size+(64<<20),'12GiB_build_reserve_required')
    core=ROOT/'tools/research/r192_aot_modules';header=core/'reference_resource_code_observer.hpp'
    inputs=[Path(__file__),Path(auth.__file__),receipt,proof,header,a.compiler,*[Path(row['path']) for row in old['source_pins']+old['private_sources']+old['private_objects']+old['package_pins']+measured['source_pins']]]
    inputs=list(dict.fromkeys(inputs));pins=[pin(q) for q in inputs]
    out.mkdir(parents=True);source=out/'source';source.mkdir();target=out/'out';target.mkdir()
    contract=source/'resource_contract.hpp';contract.write_bytes((base/'source/resource_contract.hpp').read_bytes())
    cpp=source/'reference_adapter.cpp';text=(base/'source/reference_adapter.cpp').read_text();text='#include "'+header.as_posix()+'"\n'+text
    needle='static resource_reference::Adapter adapter(resource_contract);';require(text.count(needle)==1,'base_adapter_seam_changed');text=text.replace(needle,needle+'\nstatic reference_resources::ResourceCodeObserver code_observer;')
    needle='return adapter.try_dispatch(c);';require(text.count(needle)==1,'base_dispatch_seam_changed');text=text.replace(needle,'if(adapter.try_dispatch(c))return true; code_observer.inspect(c); return false;')
    needle='adapter.reset();';require(text.count(needle)==1,'base_reset_seam_changed');text=text.replace(needle,'adapter.reset();code_observer.note_reset();')
    cpp.write_text(text,newline='\n');obj=cpp.with_suffix('.obj');commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=pin(log)));require(cp.returncode==0,'observer_'+name+'_failed')
    run([str(a.compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-DNDEBUG','-DPS2_GPU','-I'+str(normal/'src/runtime/include'),'-I'+str(normal/'src/runtime/src'),'-c',str(cpp),'-o',str(obj)],'compile')
    link=list(old['link_arguments']);replaced=0
    for i,s in enumerate(link):
        if Path(s).resolve()==(base/'source/reference_adapter.obj').resolve():link[i]=str(obj);replaced+=1
        elif s.startswith('-Wl,-Map,'):link[i]='-Wl,-Map,'+str(out/'observer-link.map')
    require(replaced==1 and link[-2]=='-o','exact_one_adapter_replacement_required');link[-1]=str(target/'ps2run.exe')
    response=out/'link.rsp';response.write_text('\n'.join('"'+x.replace('\\','/').replace('"','\\"')+'"' for x in link)+'\n',newline='\n');run([str(a.compiler),'@'+str(response)],'link')
    packages=[]
    for q in (base/'out').glob('*.dll'):os.link(q,target/q.name);packages.append(pin(target/q.name))
    for domain in ['ee','vu']:
        folder=target/'aot-modules'/domain;folder.mkdir(parents=True)
        for q in (base/'out/aot-modules'/domain).iterdir():
            require(q.is_file(),'package_directory_shape_changed');os.link(q,folder/q.name);require(auth.digest(folder/q.name)==auth.digest(q),'package_hardlink_changed');packages.append(pin(folder/q.name))
    changes=[row['path'] for row in pins if auth.digest(row['path'])!=row['sha256']];require(not changes,'observer_build_input_changed')
    result=dict(status='READ_ONLY_RESOURCE_CODE_OBSERVER_BUILT',candidate=str(target/'ps2run.exe'),candidate_sha256=auth.digest(target/'ps2run.exe'),base_receipt=pin(receipt),word_proof=pin(proof),source_pins=pins,input_changes=changes,commands=commands,link_arguments=link,package_pins=packages,private_sources=[pin(cpp),pin(contract)],private_objects=[pin(obj)],scope='Read-only diagnostic after unchanged three-plan adapter declines; no new execution/lease admission or normal/protected build installation.')
    q=out/'BUILD.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],candidate_sha256=result['candidate_sha256'],build_sha256=auth.digest(q))))


if __name__=='__main__':main()
