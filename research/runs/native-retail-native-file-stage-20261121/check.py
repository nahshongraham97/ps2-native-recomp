"""Current native file/package route; no private file transport or game run."""
import importlib.util,json,re
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s=importlib.util.spec_from_file_location('checks',F/'research/runs/native-typed-retail-20261121/verify.py')
c=importlib.util.module_from_spec(s);s.loader.exec_module(c)
c.OUT=H/'validation-01'
if __name__=='__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3);c.OUT.mkdir(exist_ok=False)
    try:
        prior=json.loads((F/'research/runs/native-retail-connected-initialization-20261121/validation-03/verification.json').read_text())
        original=json.loads((F/'research/runs/native-connected-main-dependencies-20261121/validation-03/test_native_retail_package.json').read_text())
        c.require(original['exit']==0 and len(original['argv'])==3,'existing package scenario arguments')
        paths=['runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main.cpp',
            'runtime/src/native/retail_file_startup.cpp','runtime/src/native/retail_package.cpp',
            'runtime/src/native/retail_file_paths.cpp','runtime/src/native/content_volume.cpp',
            'runtime/src/native/native_file.cpp','tests/test_retail_package.cpp',
            'tests/test_retail_file_startup.cpp','cmake/native_game.cmake']
        sources={str(F/n):c.sha(F/n) for n in paths}
        sources[str(Path(__file__).resolve())]=c.sha(__file__)
        evidence={str(C/'native-retail-native-file-stage-46/SOURCE_JOIN.json'):c.sha(C/'native-retail-native-file-stage-46/SOURCE_JOIN.json'),
            original['argv'][2]:c.sha(original['argv'][2])}
        def stable():
            for group in [sources,evidence,prior['protected_inputs']]:
                c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable native file/source/protected bytes')
        stable();c.save('source-preflight.json',dict(sources=sources,evidence=evidence))
        targets=['test_native_retail_file_startup','test_native_retail_package']
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*targets,'-j2'])
        directory=c.OUT/'disposable-file-content';directory.mkdir()
        one=c.execute(targets[0],[c.BUILD/(targets[0]+'.exe'),directory])
        two=c.execute(targets[1],[c.BUILD/(targets[1]+'.exe'),*original['argv'][1:]])
        c.require(all('PASS' in r['stdout'] and not r['stderr'] for r in [one,two]),'actual file/package route clients')
        link=c.execute('actual-entry-link',[c.CMAKE,'--build',c.BUILD,'--target','test_native_startup_missing_bindings','-j2'],expected=None)
        missing=sorted(set(re.findall(r'undefined symbol: mka::retail::main_binding::(\w+)\(',link['stdout']+link['stderr'])))
        c.require(missing==sorted(set(prior['remaining_integration_symbols'])-{'initialize_retail_graphics_stage'}),
            'removed private backend call; exact other entry integrations preserved')
        stable()
        c.save('verification.json',dict(status='PASS_NATIVE_FILE_TRANSPORT_REMOVAL',sources=sources,evidence=evidence,
            executed_clients=[dict(target=n,sha256=r['executable_sha256']) for n,r in zip(targets,[one,two])],
            remaining_integration_symbols=missing,startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            game_executed=False,physical_endpoints_opened=False,
            limit='Native file/package route verified; full startup, input/render/audio/game acceptance remain open.'))
        print(json.dumps(dict(status='PASS_NATIVE_FILE_TRANSPORT_REMOVAL',current_clients=2,remaining_Main_integrations=len(missing))))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',error=type(error).__name__,message=str(error)));raise
