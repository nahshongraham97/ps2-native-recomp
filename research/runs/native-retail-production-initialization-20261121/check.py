"""Compile the connected production initialization calls; no game/endpoints."""
import importlib.util
import json
import re
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec = importlib.util.spec_from_file_location('native_checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = H / 'validation-01'

if __name__ == '__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous = json.loads((F / 'research/runs/native-retail-task-execution-20261121/validation-01/verification.json').read_text(encoding='utf-8'))
        task = json.loads((F / 'research/runs/native-retail-initial-task-20261121/validation-01/verification.json').read_text(encoding='utf-8'))
        sources = dict(previous['sources'])
        sources.update(task['files'])
        profile_path = C / 'native-retail-profile-binding-30/source-agent01/IMPLEMENTATION.json'
        overlay_path = C / 'native-retail-active-overlay-32/source-agent01/SOURCE.json'
        profile = json.loads(profile_path.read_text(encoding='utf-8'))
        overlay = json.loads(overlay_path.read_text(encoding='utf-8'))
        c.require(profile['schema'] == 'mka.native-retail-profile-main.source-implementation.v1'
                  and profile['status'] == 'SOURCE_WRITTEN_UNBUILT'
                  and isinstance(profile['source_files_sha256'], dict), 'actual profile manifest')
        c.require(overlay['schema'] == 'native_retail_active_overlay_source_implementation_v2'
                  and overlay['status'] == 'source_written_not_built_or_run'
                  and isinstance(overlay['pins'], dict), 'actual overlay manifest')
        for base, group in [(F, profile['source_files_sha256']), (P, overlay['pins'])]:
            for name, digest in group.items():
                c.require(c.sha(base / name) == digest, 'frozen production source: ' + name)
                sources[str(base / name)] = digest
        for name in ['cmake/native_game.cmake', 'recomp/native_startup_codegen.cpp',
                     'runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_post_overlay.cpp',
                     'runtime/src/native/retail_active_overlay_main.cpp', 'tests/test_retail_startup_initialization.cpp']:
            sources[str(F / name)] = c.sha(F / name)
        sources[str(Path(__file__).resolve())] = c.sha(__file__)
        evidence = {str(p): c.sha(p) for p in [profile_path, overlay_path,
            C / 'native-retail-post-overlay-33/RAW_SOURCE.json', C / 'native-retail-post-overlay-33/SOURCE_JOIN.json']}
        protected = previous['protected_inputs']
        def stable():
            for group in [sources, evidence, protected]:
                c.require(all(c.sha(p) == h for p, h in group.items()), 'current source/protected identities')
        stable()
        c.save('source-preflight.json', dict(sources=sources, evidence=evidence, protected_inputs=protected))
        targets = ['test_native_retail_startup_initialization', 'test_native_retail_active_overlay']
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *targets, '-j2'])
        clients = []
        for target in targets:
            receipt = c.execute(target, [c.BUILD / (target + '.exe'), P / 'research/inputs/modules'])
            c.require(not receipt['stderr'] and 'PASS' in receipt['stdout'] and 'reused_from' not in receipt,
                      'fresh connected production assertions')
            clients.append(dict(target=target, sha256=receipt['executable_sha256']))
        link = c.execute('remaining-entry-link', [c.CMAKE, '--build', c.BUILD, '--target',
            'test_native_startup_missing_bindings', '-j2'], expected=None)
        missing = sorted(set(re.findall(r'undefined symbol: mka::retail::main_binding::(\w+)\(', link['stdout'] + link['stderr'])))
        expected = sorted(set(previous['remaining_integration_symbols']) - {
            'reset_and_initialize_retail_profile', 'initialize_retail_active_overlay', 'initialize_retail_post_overlay_state'})
        c.require(missing == expected and not (c.BUILD / 'test_native_startup_missing_bindings.exe').exists(),
                  'actual full entry still refuses exact remaining production calls')
        stable()
        c.save('verification.json', dict(schema='native_retail_production_initialization_checks_v1',
            status='PASS_CONNECTED_PRODUCTION_INITIALIZATION', sources=sources, evidence=evidence,
            protected_inputs=protected, executed_clients=clients, remaining_integration_symbols=missing,
            startup_archive_sha256=c.sha(c.BUILD / 'libmka_native_retail_startup.a'),
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=['Targeted production initialization and lifecycle only; broad regressions deferred.',
                'Other overlay modes, Wave callback calls, later companion callpoint and complete game remain open.']))
        print(json.dumps(dict(status='PASS_CONNECTED_PRODUCTION_INITIALIZATION',
            remaining_integrations=len(missing), game_executed=False)))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
