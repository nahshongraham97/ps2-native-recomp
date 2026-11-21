"""Verify the actual Main task producer and source-born dispatch connection."""
import ctypes
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

H=Path(__file__).resolve().parent
F=H.parents[2]
P=F.parents[2]
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s=importlib.util.spec_from_file_location('main_task_receipts',F/'research/runs/native-typed-retail-20261121/verify.py')
c=importlib.util.module_from_spec(s);s.loader.exec_module(c)
c.OUT=H/(sys.argv[1] if len(sys.argv)>1 else 'validation-01')


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous=F/'research/runs/native-retail-task-retirement-20261121/validation-02/verification.json'
        c.require(c.sha(previous)=='0995ee96ec310339b4bbe47ea8c881f741b8d5699f5bf469d8ce77f1fbdb662d','previous exact result')
        prior=load(previous)
        c.require(prior['schema']=='native_retail_task_classes_retirement_connected_verification_v1' and
                  isinstance(prior['sources'],dict) and isinstance(prior['executed_clients'],list),'actual prior producer')
        changed=['cmake/native_game.cmake','cmake/native_typed.cmake','runtime/include/mka/retail_task_registry.hpp',
            'runtime/src/native/retail_task_registry.cpp','runtime/include/mka/retail_task_domain.hpp',
            'runtime/src/native/retail_task_domain.cpp','tests/test_retail_initial_task.cpp']
        added=['runtime/include/mka/retail_task_script_context.hpp','runtime/src/native/retail_task_script_context.cpp',
            'runtime/src/native/retail_task_dispatch_main.cpp','runtime/src/native/retail_initial_task_main.cpp',
            'runtime/include/mka/retail_initial_task.hpp','runtime/src/native/retail_initial_task.cpp',
            'runtime/include/mka/retail_initial_task_required.hpp','tests/test_retail_task_dispatch_main.cpp']
        sources=dict(prior['sources']);changes={};allowed={str(F/n) for n in changed}
        for path,pin in list(sources.items()):
            now=c.sha(path)
            if now!=pin:
                c.require(path in allowed,'unexpected previous source change: '+path)
                changes[path]=dict(previous_sha256=pin,current_sha256=now);sources[path]=now
        sources.update({str(F/n):c.sha(F/n) for n in changed+added})
        sources[str(Path(__file__))]=c.sha(__file__)
        author_path=C/'native-retail-initial-task-main-79/author01/IMPLEMENTATION.json'
        author=load(author_path)
        c.require(c.sha(author_path)=='72cb40945660be0d05521dffb11ecf7a8a99fd8df1fcbab1793c00a5c70d1467' and
            author['schema']=='native_retail_initial_task_main_79_implementation_v1','exact actual author handoff')
        c.require(all(c.sha(F/n)==pin for n,pin in author['authored_sha256'].items()),'frozen creator source')
        evidence={str(author_path):c.sha(author_path)}
        for scope in ['native-retail-main-task-dispatch-78/source-agent01','native-retail-main-task-dispatch-78/review01',
                      'native-retail-initial-task-main-79/author01']:
            for path in (C/scope).glob('*.json'):evidence[str(path)]=c.sha(path)
        protected=prior['protected_inputs']
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable inputs before')
        elf=(P/'research/inputs/SLUS_214.10').read_bytes()
        phoff=struct.unpack_from('<I',elf,28)[0];size,count=struct.unpack_from('<HH',elf,42)
        segments=[struct.unpack_from('<8I',elf,phoff+i*size) for i in range(count)]
        def image(at,length):
            seg=[x for x in segments if x[0]==1 and x[2]<=at and at+length<=x[2]+x[4]]
            c.require(len(seg)==1,'unique original file-backed span')
            x=seg[0];offset=x[1]+at-x[2];return elf[offset:offset+length]
        joins={}
        for name,pin in [('native-retail-main-task-dispatch-78/source-agent01/RAW_SOURCE.json',
                          '34cc7ed2b67bf8c7a7e2830fbcdec99c4d87f92840e61468f82e5b36b0fcffa5'),
                         ('native-retail-initial-task-main-79/author01/RAW_SOURCE.json',author['raw_source_sha256'])]:
            path=C/name;c.require(c.sha(path)==pin,'exact original source record');total=0
            for row in load(path)['ranges']:
                at=int(row['address'],16);data=image(at,row['length'])
                c.require(hashlib.sha256(data).hexdigest()==row['sha256'],'full original source span')
                for i,value in enumerate(row['words_le32']):
                    address=int(value['address'],16) if isinstance(value,dict) else at+i*4
                    expected=value['word'] if isinstance(value,dict) else value
                    c.require(struct.unpack('<I',image(address,4))[0]==int(expected,16),'original instruction')
                    total+=1
            joins[name]=total
        clients={row['target']:[] for row in prior['executed_clients']}
        modules=P/'research/inputs/modules'
        for n in ['test_native_retail_profile_transition_child','test_native_retail_profile_aggregate_main',
                  'test_native_retail_game_records_main','test_native_retail_profile_preferences',
                  'test_native_retail_profile_resource_ids']:clients[n]=[modules]
        root=c.OUT/'content';root.mkdir()
        clients['test_native_retail_initial_task']=[root]
        clients['test_native_retail_task_dispatch_main']=[root]
        c.save('inputs.json',dict(sources=sources,evidence=evidence,protected_inputs=protected,
            explicit_prior_source_changes=changes,targets=list(clients),original_word_joins=joins))
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*clients,'-j2'])
        runs=[]
        for name,args in clients.items():
            r=c.execute('run-'+name,[c.BUILD/(name+'.exe'),*args])
            c.require(not r['stderr'] and (not r['stdout'] or 'PASS' in r['stdout'] or 'passed' in r['stdout']),
                      'actual client assertions: '+name)
            runs.append(dict(target=name,sha256=r['executable_sha256'],receipt='run-'+name+'.json'))
        for name in clients:
            r=c.execute('symbols-'+name,[c.BIN/'llvm-nm.exe','-C',c.BUILD/(name+'.exe')])
            c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b',r['stdout']),
                      'no guest/device runtime symbols: '+name)
        remaining=sorted(set(prior['remaining_integration_symbols'])-{'dispatch_retail_task_lists','create_retail_initial_mode_task'})
        children=['run_reset_18d970','begin_intro_18deb0','begin_prompt_18dc10','transition_1225d0',
            'begin_menu_18d450','begin_menu_18d870','begin_menu_18d8e0','begin_menu_18d700','begin_menu_18d800',
            'begin_menu_18d660','begin_menu_18d770']
        expected_plans=[('profile-effects','test_native_profile_required_effects',prior['required_profile_effects']),
            ('initial-children','test_native_retail_initial_task_missing_children',children),
            ('entry-effects','test_native_startup_missing_bindings',remaining+children)]
        for label,target,expected in expected_plans:
            r=c.execute(label,[c.CMAKE,'--build',c.BUILD,'--target',target,'-j2'],expected=None)
            missing=re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
            c.require(len(missing)==len(expected) and all(any('::'+n+'(' in x for x in missing) for n in expected)
                and not (c.BUILD/(target+'.exe')).exists(),'exact separate production dependencies: '+target)
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable inputs after')
        result=dict(schema='native_retail_main_task_connection_verification_v1',status='PASS_NATIVE_MAIN_TASK_PRODUCER_DISPATCH_CPU_CHECKS',
            sources=sources,evidence=evidence,protected_inputs=protected,executed_clients=runs,
            explicit_prior_source_changes=changes,prior_path=str(previous),prior_sha256=c.sha(previous),
            original_word_joins=joins,remaining_integration_symbols=remaining,required_initial_task_children=children,
            required_profile_effects=prior['required_profile_effects'],
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            registry_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_task_registry.a'),
            session_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_game_session.a'),
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['Source-born flags0 first-root dispatch only; full second-root/script/context/delta producers remain unbound.',
                    'Main publishes actual18E100; eleven real reset/intro/prompt/menu/transition children remain required.',
                    'CPU/file checks are not full boot/menu/combat/input/render/sound acceptance.'])
        c.save('verification.json',result)
        print(json.dumps(dict(status=result['status'],clients=len(runs),main=len(remaining),initial_children=len(children),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',type=type(error).__name__,message=str(error)))
        raise


if __name__=='__main__':
    main()
