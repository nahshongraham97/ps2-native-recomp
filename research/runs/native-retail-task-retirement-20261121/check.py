"""Compile and execute the affected native task/startup CPU clients, never the game."""
import ctypes
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
F = H.parents[2]
P = F.parents[2]
C = P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s = importlib.util.spec_from_file_location('task_cpu_receipts',
    F/'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(s)
s.loader.exec_module(c)
c.OUT = H/(sys.argv[1] if len(sys.argv)>1 else 'validation-01')


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous = F/'research/runs/native-retail-profile-effects-20261121/validation-01/verification.json'
        c.require(c.sha(previous) == 'f9ed93e4e439510a0b9ae3667ef2bdf0d615f7b9f80a481cbdebe05de5a10515',
                  'exact preceding profile result')
        prior = load(previous)
        c.require(prior['schema'] == 'native_retail_profile_effects_connected_verification_v1' and
                  isinstance(prior['sources'],dict) and isinstance(prior['executed_clients'],list),
                  'actual preceding schema')
        changed = ['cmake/native_game.cmake',
            'runtime/include/mka/retail_task_registry.hpp','runtime/src/native/retail_task_registry.cpp',
            'runtime/include/mka/retail_task_execution.hpp','runtime/src/native/retail_task_execution.cpp',
            'tests/test_retail_task_registry.cpp','tests/test_retail_task_execution.cpp',
            'tests/test_retail_initial_task.cpp']
        added = ['runtime/include/mka/retail_profile_task_retirement.hpp',
            'runtime/src/native/retail_profile_task_retirement.cpp','tests/test_retail_profile_task_retirement.cpp',
            'tests/test_retail_task_class_birth.cpp','tests/test_retail_stock_task_execution.cpp']
        allowed = {str(F/name) for name in changed}
        sources = dict(prior['sources'])
        changes = {}
        for path,pin in list(sources.items()):
            now = c.sha(path)
            if now != pin:
                c.require(path in allowed,'unexpected prior source change: '+path)
                changes[path] = dict(previous_sha256=pin,current_sha256=now)
                sources[path] = now
        sources.update({str(F/name):c.sha(F/name) for name in changed+added})
        sources[str(Path(__file__))] = c.sha(__file__)
        author_path = C/'native-retail-task-class-execution-77/author01/IMPLEMENTATION.json'
        author = load(author_path)
        c.require(author['schema'] == 'native_retail_task_class_birth_77_implementation_v1' and
                  c.sha(author_path) == '3dd27644d5037d8b84498a6e0c60e1fab824917fa7a9132d23919c8eea811699',
                  'frozen actual birth producer')
        c.require(all(c.sha(F/name)==pin for name,pin in author['after_sha256'].items()),
                  'class author bytes unchanged')
        stock_path = C/'native-retail-task-class-execution-77/stock-test01/FIXTURE.json'
        stock = load(stock_path)
        c.require(stock['schema']=='native_retail_stock_execution_77_fixture_v1' and
                  c.sha(stock_path)=='d2d00fce212922a6277a2b618da5953a12597069cc7bdf6995de05dce7d7bd0c' and
                  c.sha(F/'tests/test_retail_stock_task_execution.cpp')==stock['test_sha256'],
                  'independent stock fixture handoff bytes')
        evidence = {str(author_path):c.sha(author_path),str(stock_path):c.sha(stock_path)}
        for scope in ['native-retail-task-class-execution-77/source-agent01',
                      'native-retail-task-class-execution-77/source-agent02',
                      'native-retail-profile-task-retirement-75/source-agent01',
                      'native-retail-profile-task-retirement-75/source-agent02',
                      'native-retail-profile-task-retirement-75/root01']:
            for path in (C/scope).glob('*.json'):
                evidence[str(path)] = c.sha(path)
        protected = prior['protected_inputs']
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable inputs before')
        elf = (P/'research/inputs/SLUS_214.10').read_bytes()
        phoff = struct.unpack_from('<I',elf,28)[0]
        size,count = struct.unpack_from('<HH',elf,42)
        segments = [struct.unpack_from('<8I',elf,phoff+i*size) for i in range(count)]
        def image(address,length):
            seg = [x for x in segments if x[0]==1 and x[2]<=address and address+length<=x[2]+x[4]]
            c.require(len(seg)==1,'unique original file-backed range')
            x=seg[0];at=x[1]+address-x[2]
            return elf[at:at+length]
        raw_path=C/'native-retail-task-class-execution-77/source-agent01/RAW_SOURCE.json'
        c.require(c.sha(raw_path)==author['source77_raw_sha256'],'frozen raw authority')
        joins=0
        for row in load(raw_path)['ranges']:
            at=int(row['address'],16);data=image(at,row['length'])
            c.require(hashlib.sha256(data).hexdigest()==row['sha256'],'full original class/executor span')
            for i,value in enumerate(row['words_le32']):
                address=int(value['address'],16) if isinstance(value,dict) else at+i*4
                expected=value['word'] if isinstance(value,dict) else value
                c.require(struct.unpack('<I',image(address,4))[0]==int(expected,16),'original class word')
                joins+=1
        clients = {r['target']:[] for r in prior['executed_clients']}
        modules=P/'research/inputs/modules'
        for name in ['test_native_retail_profile_transition_child','test_native_retail_profile_aggregate_main',
                     'test_native_retail_game_records_main','test_native_retail_profile_preferences',
                     'test_native_retail_profile_resource_ids']:
            clients[name]=[modules]
        for name in ['test_native_retail_profile_task_retirement','test_native_retail_task_class_birth',
                     'test_native_retail_stock_task_execution','test_native_retail_task_registry',
                     'test_native_retail_task_execution','test_native_retail_task_domain',
                     'test_native_retail_initial_game_tasks','test_native_retail_initial_task',
                     'test_native_retail_observer_list','test_native_retail_input_task_argument',
                     'test_native_typed_retail']:
            clients[name]=[]
        content_root=c.OUT/'initial-task-content'
        content_root.mkdir()
        clients['test_native_retail_initial_task']=[content_root]
        c.save('inputs.json',dict(sources=sources,evidence=evidence,protected_inputs=protected,
            explicit_prior_source_changes=changes,targets=list(clients),original_class_word_joins=joins))
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*clients,'-j2'])
        runs=[]
        for name,args in clients.items():
            r=c.execute('run-'+name,[c.BUILD/(name+'.exe'),*args])
            c.require(not r['stderr'],'fixture emitted failure: '+name)
            c.require(not r['stdout'] or 'PASS' in r['stdout'] or 'passed' in r['stdout'],
                      'actual fixture success assertions: '+name)
            runs.append(dict(target=name,sha256=r['executable_sha256'],receipt='run-'+name+'.json'))
        imports={}
        for name in clients:
            r=c.execute('symbols-'+name,[c.BIN/'llvm-nm.exe','-C',c.BUILD/(name+'.exe')])
            c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b',r['stdout']),
                      'no guest/device/compiler runtime symbols: '+name)
            imports[name]=sorted(set(re.findall(r'\bSDL_[A-Za-z0-9_]+',r['stdout'])))
        for label,target,expected in [
            ('profile-required-effects','test_native_profile_required_effects',prior['required_profile_effects']),
            ('entry-required-effects','test_native_startup_missing_bindings',prior['remaining_integration_symbols'])]:
            r=c.execute(label,[c.CMAKE,'--build',c.BUILD,'--target',target,'-j2'],expected=None)
            missing=re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
            c.require(len(missing)==len(expected) and all(any('::'+n+'(' in x for x in missing) for n in expected)
                      and not (c.BUILD/(target+'.exe')).exists(),'exact named production requirements: '+target)
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable inputs after')
        result=dict(schema='native_retail_task_classes_retirement_connected_verification_v1',
            status='PASS_NATIVE_TASK_CLASS_EXECUTION_RETIREMENT_CPU_CHECKS',sources=sources,evidence=evidence,
            protected_inputs=protected,executed_clients=runs,original_class_word_joins=joins,native_sdl_linkage=imports,
            explicit_prior_source_changes=changes,prior_path=str(previous),prior_sha256=c.sha(previous),
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            registry_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_task_registry.a'),
            session_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_game_session.a'),
            remaining_integration_symbols=prior['remaining_integration_symbols'],required_profile_effects=prior['required_profile_effects'],
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['CPU/file component evidence only; no full menu/combat/game acceptance.',
                    'Actual18B270 scene producer and five profile children remain unimplemented.',
                    'Nonnull task callback producers and special EE floats remain named unsupported paths.'])
        c.save('verification.json',result)
        print(json.dumps(dict(status=result['status'],fresh_clients=len(runs),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',type=type(error).__name__,message=str(error)))
        raise


if __name__=='__main__':
    main()
