"""Add paired Plan persistence to a fresh copy of the actual v8 system source.

Preserves the original typed-continuation save barrier. Never edits the supplied
source, protected builds, cards or states. Compilation produces two private
override objects only; root composes its current IOP/runtime objects at link.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth


def once(text,old,new):
    auth.need(text.count(old)==1,'checkpoint_source_seam_changed:'+old[:64]);return text.replace(old,new)


def patch(text,header):
    text=text.replace('\r\n','\n')
    auth.need(re.search(r'kStateVersion\s*=\s*8\s*;',text),'actual_v8_source_required')
    barrier='else if (mka::emitted::reference::family_pending()) throw std::runtime_error("semantic reference: live typed continuation cannot be saved in original state format");'
    auth.need(text.count(barrier)==1 and text.index(barrier)<text.index('std::ofstream out(os::path(path)'),
              'original_save_barrier_required_before_truncation')
    namespace='mka::emitted::resource_plan_checkpoint::'
    text='#include "'+header.as_posix()+'"\n'+text
    text=once(text,'        state::Writer w;','        '+namespace+'before_save(path);\n        state::Writer w;')
    text=once(text,'        machine_state(w);','        machine_state(w);\n        '+namespace+'prepare_save(path,w.out);')
    needle='        if (!out) throw std::runtime_error("cannot write " + path);'
    text=once(text,needle,needle+'\n        out.close();\n'+needle+'\n        '+namespace+'saved(path,w.out);')
    marker='bool load_state(const std::uint8_t *data, std::size_t size) {'
    start=text.index(marker);end=text.index('\n}\n',start)+2;piece=text[start:end]
    piece=once(piece,'    try {','    bool plan_restore_started=false;\n    try {\n        '+namespace+'begin_buffer_restore({data,size});\n        plan_restore_started=true;')
    piece=once(piece,'        state::Reader r(data, size);','        const auto native_payload='+namespace+'payload_for_restore({data,size});\n        state::Reader r(native_payload.data(),native_payload.size());')
    piece=once(piece,'        machine_state(r);','        machine_state(r);\n        if(r.remaining()!=0)throw std::runtime_error("native Plan checkpoint: trailing state payload");\n        '+namespace+'end_buffer_restore(true);\n        plan_restore_started=false;')
    piece=once(piece,'    } catch (const std::exception &e) {','    } catch (const std::exception &e) {\n        if(plan_restore_started)'+namespace+'end_buffer_restore(false);')
    text=text[:start]+piece+text[end:]
    marker='bool load_state(const std::string &path) {';start=text.index(marker);end=text.index('\n}\n',start)+2;piece=text[start:end]
    piece=once(piece,marker,marker+'\n    try {')
    needle='    if (!load_state(data.data(), data.size())) return false;'
    piece=once(piece,needle,'    '+namespace+'PathLoad plan_pair(path,data);\n'+needle)
    piece=piece[:-1]+'    } catch(const std::exception&e){std::fprintf(stderr,"[state] paired native Plan preflight refused: %s\\n",e.what());return false;}\n}'
    text=text[:start]+piece+text[end:]
    auth.need(text.count(barrier)==1,'save_barrier_not_preserved')
    return text


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('system-source','expected-system-sha','runtime-build','compiler','output'):p.add_argument('--'+name,required=True,type=str if name=='expected-system-sha' else Path)
    p.add_argument('--original-object',type=Path);p.add_argument('--expected-original-object-sha')
    p.add_argument('--compile',action='store_true');a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    auth.need(shutil.disk_usage(out.parent).free>(12<<30)+(16<<20),'12GiB_reserve_required')
    auth.need(auth.pin(a.system_source)['sha256']==a.expected_system_sha,'actual_system_source_changed')
    auth.need(not a.compile or a.original_object is not None and a.expected_original_object_sha,'exact_original_system_object_required_for_compiled_replacement')
    auth.need((a.original_object is None)==(a.expected_original_object_sha is None),'orphan_original_system_object_pin')
    if a.original_object:auth.need(auth.pin(a.original_object)['sha256']==a.expected_original_object_sha,'actual_original_system_object_changed')
    abi={'cpu.hpp':'2eb8c482859337add6480f0a1f0ad79ab908dda6d878d575f84f554ecf81c0fe','state.hpp':'1c2cc5173971b9645d213942171cf1a0906b754bd9ada990235f644a634fa3eb'}
    for name,sha in abi.items():auth.need(auth.pin(a.runtime_build/'src/runtime/include/ps2'/name)['sha256']==sha,'fixed_v8_serializer_ABI_changed:'+name)
    core=auth.W/'tools/research/r192_aot_modules';header=core/'reference_resource_plan_checkpoint.hpp';implementation=core/'reference_resource_plan_checkpoint.cpp'
    runtime_dependencies=[q for parent in (a.runtime_build/'src/runtime/include',a.runtime_build/'src/runtime/src') for q in parent.rglob('*') if q.is_file() and q.suffix in ('.h','.hpp','.inc')]
    inputs=[Path(__file__),Path(auth.__file__),a.system_source,a.compiler,header,implementation,core/'reference_resource_owner_epochs.hpp',core/'effective_resource_construction.hpp',core/'reference_resource_layout.hpp',core/'reference_resource_word_equality.hpp',core/'semantic_resource_pipeline.hpp',core/'semantic_vector_effects.hpp',*runtime_dependencies]
    if a.original_object:inputs.append(a.original_object)
    pins=[auth.pin(q) for q in dict.fromkeys(inputs)];text=a.system_source.read_text();changed=patch(text,header)
    controls=[]
    for name,old,new in [('missing_barrier','family_pending()','family_pending_removed()'),('wrong_state_version','kStateVersion = 8','kStateVersion = 14')]:
        auth.need(old in text,'checkpoint_control_seam_missing')
        try:patch(text.replace(old,new),header)
        except ValueError as e:controls.append(dict(name=name,status='REJECTED',reason=str(e)))
        else:raise ValueError('checkpoint_source_control_accepted')
    out.mkdir(parents=True);system=out/'system.cpp';system.write_text(changed,encoding='utf8',newline='\n');commands=[]
    for source in (system,implementation):
        obj=out/(source.stem+'.obj');argv=[str(a.compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-DNDEBUG','-DPS2_GPU','-I'+str((a.runtime_build/'src/runtime/include').resolve()),'-I'+str((a.runtime_build/'src/runtime/src').resolve()),'-c',str(source),'-o',str(obj)]
        exit_code=None
        if a.compile:
            run=subprocess.run(argv,capture_output=True,timeout=120);(out/(source.stem+'-compile.log')).write_bytes(run.stdout+run.stderr);exit_code=run.returncode;auth.need(exit_code==0,'checkpoint_private_object_compile_failed:'+source.name)
        commands.append(dict(argv=argv,exit=exit_code,object=auth.pin(obj) if a.compile else None))
    changes=[p['path'] for p in pins if auth.pin(Path(p['path']))!=p];auth.need(not changes,'checkpoint_input_changed')
    result=dict(status='PAIRED_PLAN_CHECKPOINT_PRIVATE_OBJECTS_PREPARED' if a.compile else 'PAIRED_PLAN_CHECKPOINT_SOURCE_PREPARED',source_pins=pins,input_changes=changes,patched_source=auth.pin(system),commands=commands,controls=controls,
                replacement_objects=[dict(original=auth.pin(a.original_object),replacement=auth.pin(out/'system.obj'))] if a.compile else [],extra_objects=[auth.pin(out/'reference_resource_plan_checkpoint.obj')] if a.compile else [],extra_link_arguments=['-lcrypt32','-lbcrypt'],
                link_required=['-lcrypt32','-lbcrypt','root r192_family_registry/restore_begin/restore_end/source_id/configured_save_root bridge','baseline r192_fresh_before/after_machine_restore bridge'],
                fixed_serializer_abi=abi,scope='Source-path compilation only; original v8 payload unchanged inside a NEW 32-byte required-pair envelope. Card unchanged. Save barrier retained. Actual quiescent owned save/load and pending-save refusal remain separate runtime gates.',invocation=[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],receipt=auth.pin(target))))


if __name__=='__main__':main()
