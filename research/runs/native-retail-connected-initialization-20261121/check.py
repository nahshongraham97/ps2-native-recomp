"""Connected retail initialization CPU/file checks; never launches the game."""
from pathlib import Path
import importlib.util, json, re, subprocess

F=Path(__file__).resolve().parents[3]
P=F.parents[2]
H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec=importlib.util.spec_from_file_location('checks',F/'research/runs/native-typed-retail-20261121/verify.py')
c=importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT=H/(c.sys.argv[1] if len(c.sys.argv)>1 else 'validation-01')
resume=H/c.sys.argv[2] if len(c.sys.argv)>2 else None
rebuild='--rebuild-changed' in c.sys.argv

if __name__=='__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        prior=json.loads((F/'research/runs/native-retail-records-workspace-20261121/validation-03/verification.json').read_text())
        records_path=C/'native-retail-connected-game-records-39/author01/IMPLEMENTATION_R3.json'
        tasks_path=C/'native-retail-connected-task-runtime-40/author01/INTEGRATION_FREEZE_R4.json'
        records=json.loads(records_path.read_text())
        tasks=json.loads(tasks_path.read_text())
        c.require(records['status']=='SOURCE_FROZEN_UNBUILT_R3' and isinstance(records['files_sha256'],dict),'actual records producer schema')
        c.require(tasks['schema']=='native_retail_connected_task_runtime_40_integration_freeze_r4' and
                  isinstance(tasks['owned_source_pins'],dict) and isinstance(tasks['direct_dependency_snapshot_pins'],dict),
                  'actual tasks producer schema')
        author_pins={**records['files_sha256'],**tasks['owned_source_pins']}
        correction_path=tasks_path.parent/'CORRECTION_R6.json'
        correction=json.loads(correction_path.read_text())
        c.require(correction['schema']=='native_retail_connected_task_runtime_40_compiler_correction_r6' and
            correction['before_sha256']==author_pins[correction['after_file']] and
            c.sha(tasks_path.parent/correction['before_file'])==correction['before_sha256'],
            'explicit preserved InitialTask thread-order correction')
        author_pins[correction['after_file']]=correction['after_sha256']
        c.require(all(c.sha(F/n)==pin for n,pin in author_pins.items()),'exact delegate-owned source freeze')
        context_deltas={n:dict(snapshot=pin,current=c.sha(F/n)) for n,pin in
                        tasks['direct_dependency_snapshot_pins'].items() if c.sha(F/n)!=pin}
        c.require(set(context_deltas)<={'cmake/native_game.cmake','recomp/native_startup_codegen.cpp'},
                  'only documented Root-owned target/native Wave binding edits after contextual snapshot')
        c.save('producer-preflight.json',dict(records_manifest_sha256=c.sha(records_path),
            records_schema={k:type(v).__name__ for k,v in records.items()},
            tasks_manifest_sha256=c.sha(tasks_path),tasks_schema={k:type(v).__name__ for k,v in tasks.items()},
            author_pins=author_pins,root_context_deltas=context_deltas,compiler_correction_sha256=c.sha(correction_path)))
        def names(*args):
            return [n.decode('utf-8') for n in subprocess.check_output(['git',*args],cwd=F).split(b'\0') if n]
        authored=sorted(set(names('diff','--name-only','-z','--','cmake','recomp/native_startup_codegen.cpp',
            'runtime/include/mka','runtime/src/native','tests'))|set(names('ls-files','--others','--exclude-standard','-z','--',
            'runtime/include/mka','runtime/src/native','tests')))
        sources={str(F/n):c.sha(F/n) for n in authored}
        sources[str(Path(__file__).resolve())]=c.sha(__file__)
        protected=prior['protected_inputs']
        for n in authored:
            raw=(F/n).read_bytes()
            c.require(b'\r' not in raw and raw.endswith(b'\n'),'LF source '+n)
        def stable():
            for group in [sources,protected]:
                c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable connected/protected source bytes')
        stable()
        c.save('source-preflight.json',dict(sources=sources,authored_paths=authored,protected_inputs=protected))
        targets=['test_native_retail_game_pool','test_native_retail_observer_list',
            'test_native_retail_initial_game_tasks','test_native_retail_task_registry',
            'test_native_retail_task_domain','test_native_retail_initial_task',
            'test_native_retail_text_storage','test_native_retail_game_records_main','test_native_wave_allocation']
        if resume and not rebuild:
            original_inputs=json.loads((resume/'source-preflight.json').read_text())['sources']
            c.require(all(c.sha(path)==pin for path,pin in original_inputs.items()
                          if Path(path)!=Path(__file__).resolve()),'same compiled sources; runner-only argument correction')
            built=json.loads((resume/'build.json').read_text())
            c.require(built['exit']==0,'retained successful connected compile')
            built['retained_from']=str(resume/'build.json')
            c.save('build.json',built)
        else:
            c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*targets,'-j2'])
        clients=[]
        for target in targets:
            args=[P/'research/inputs/modules'] if target=='test_native_retail_game_records_main' else []
            if target=='test_native_retail_initial_task':
                directory=c.OUT/'disposable-initial-task-content'
                directory.mkdir()
                args=[directory]
            argv=list(map(str,[c.BUILD/(target+'.exe'),*args]))
            earlier=resume/(target+'.json') if resume else None
            saved=json.loads(earlier.read_text()) if earlier and earlier.exists() else None
            if saved and saved['exit']==0 and saved['argv']==argv and saved['executable_sha256']==c.sha(argv[0]):
                receipt=dict(saved,retained_from=str(earlier),retained_receipt_sha256=c.sha(earlier))
                c.save(target+'.json',receipt)
            else:
                receipt=c.execute(target,argv)
            c.require('PASS' in receipt['stdout'] and not receipt['stderr'] and 'reused_from' not in receipt,
                'fresh directly affected connected client '+target)
            clients.append(dict(target=target,sha256=receipt['executable_sha256'],fresh_execution='retained_from' not in receipt))
        refused=c.execute('actual-entry-link',[c.CMAKE,'--build',c.BUILD,'--target',
            'test_native_startup_missing_bindings','-j2'],expected=None)
        missing=sorted(set(re.findall(r'undefined symbol: mka::retail::main_binding::(\w+)\(',refused['stdout']+refused['stderr'])))
        c.require(missing==sorted(set(prior['remaining_integration_symbols'])-
            {'initialize_retail_game_records','retire_retail_deferred_storage'}),'exact remaining actual Main integrations')
        stable()
        c.save('verification.json',dict(schema='native_connected_initialization_checks_v1',
            status='PASS_CONNECTED_RETAIL_INITIALIZATION',sources=sources,authored_paths=authored,
            protected_inputs=protected,executed_clients=clients,remaining_integration_symbols=missing,
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['Actual1303F0/130350/four task bodies/text retirement executed on native owners with original GAMER.',
                'Repeat130350 follows explicit source task cleanup; full1861B0 preceding scene cleanup remains open.',
                'BSS-empty source heads/no-actor camera branch only; live actor/scene callback algorithms remain named blockers.',
                'No full game executable, physical input/output, frames/audio or gameplay acceptance.']))
        print(json.dumps(dict(status='PASS_CONNECTED_RETAIL_INITIALIZATION',fresh_clients=sum(x['fresh_execution'] for x in clients),
            retained_clients=sum(not x['fresh_execution'] for x in clients),
            remaining_Main_integrations=len(missing),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',error=type(error).__name__,message=str(error)))
        raise
