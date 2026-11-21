"""Targeted connected-record/input build; no game or physical endpoints."""
import importlib.util
import json
import re
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec = importlib.util.spec_from_file_location('checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = H / (c.sys.argv[1] if len(c.sys.argv) > 1 else 'validation-01')

if __name__ == '__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        prior_path = F / 'research/runs/native-retail-production-initialization-20261121/validation-01/verification.json'
        prior = json.loads(prior_path.read_text(encoding='utf-8'))
        c.require(prior['status'] == 'PASS_CONNECTED_PRODUCTION_INITIALIZATION', 'prior production checkpoint')
        sources = dict(prior['sources'])
        changed = ['cmake/native_game.cmake', 'cmake/native_typed.cmake', 'recomp/native_startup_codegen.cpp',
            'runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main_state.cpp',
            'runtime/include/mka/retail_game_session.hpp', 'runtime/src/native/retail_game_session.cpp',
            'runtime/src/native/retail_game_session_startup.cpp',
            'runtime/include/mka/retail_game_controls.hpp', 'runtime/src/native/retail_game_controls.cpp',
            'runtime/include/mka/retail_input_roles.hpp', 'runtime/src/native/retail_input_roles.cpp',
            'tests/test_retail_game_session.cpp', 'tests/test_retail_startup_initialization.cpp',
            'runtime/include/mka/retail_game_resource_registry.hpp', 'runtime/src/native/retail_game_resource_registry.cpp',
            'tests/test_retail_game_resource_registry.cpp',
            'runtime/include/mka/retail_game_info_resources.hpp', 'runtime/src/native/retail_game_info_resources.cpp',
            'runtime/include/mka/retail_game_record_values.hpp', 'runtime/include/mka/retail_game_record_reset.hpp',
            'runtime/src/native/retail_game_record_reset.cpp',
            'runtime/include/mka/retail_section_workspace.hpp', 'runtime/src/native/retail_section_workspace.cpp',
            'runtime/src/native/retail_section_workspace_main.cpp', 'tests/test_retail_section_workspace.cpp',
            'runtime/include/mka/retail_task_domain.hpp', 'runtime/src/native/retail_task_domain.cpp',
            'tests/test_retail_task_domain.cpp']
        sources.update({str(F / n): c.sha(F / n) for n in changed})
        sources[str(Path(__file__).resolve())] = c.sha(__file__)
        evidence_names = ['native-retail-game-record-init-34/source-agent01/IMPLEMENTATION.json',
            'native-retail-game-record-init-34/global-values01/IMPLEMENTATION.json',
            'native-retail-game-record-init-34/global-values01/CONTROL_ALIAS_CORRECTION.json',
            'native-retail-game-record-init-34/reset01/SOURCE.json',
            'native-retail-game-record-init-34/reset01/SESSION_ALIAS_CORRECTION.json',
            'native-retail-loaded-sections-35/source-agent01/IMPLEMENTATION.json',
            'native-retail-task-domain-36/source-agent01/SOURCE.json']
        evidence = {str(C / n): c.sha(C / n) for n in evidence_names}
        protected = prior['protected_inputs']
        def stable():
            for group in [sources, evidence, protected]:
                c.require(all(c.sha(p) == h for p, h in group.items()), 'stable source/evidence/protected inputs')
        stable()
        c.save('source-preflight.json', dict(sources=sources, evidence=evidence,
            protected_inputs=protected, changed_or_added=changed))
        targets = ['test_native_retail_startup_initialization', 'test_native_retail_section_workspace',
            'test_native_retail_game_resource_registry', 'test_native_retail_task_domain',
            'test_native_retail_game_session', 'test_native_retail_input_poll', 'test_native_retail_input_selection']
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *targets, '-j2'])
        clients = []
        for target in targets:
            args = [P / 'research/inputs/modules'] if target == targets[0] else []
            receipt = c.execute(target, [c.BUILD / (target + '.exe'), *args])
            c.require('PASS' in receipt['stdout'] and not receipt['stderr'] and 'reused_from' not in receipt,
                      'fresh targeted native assertions: ' + target)
            clients.append(dict(target=target, sha256=receipt['executable_sha256']))
        link = c.execute('remaining-entry-link', [c.CMAKE, '--build', c.BUILD, '--target',
            'test_native_startup_missing_bindings', '-j2'], expected=None)
        missing = sorted(set(re.findall(r'undefined symbol: mka::retail::main_binding::(\w+)\(', link['stdout'] + link['stderr'])))
        c.require(missing == sorted(set(prior['remaining_integration_symbols']) - {'initialize_retail_loaded_sections'})
                  and not (c.BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'exact remaining real entry calls')
        stable()
        c.save('verification.json', dict(schema='native_retail_records_workspace_checks_v1',
            status='PASS_RECORDS_WORKSPACE_AND_AFFECTED_INPUT', sources=sources, evidence=evidence,
            protected_inputs=protected, changed_or_added=changed, executed_clients=clients,
            remaining_integration_symbols=missing, startup_archive_sha256=c.sha(c.BUILD / 'libmka_native_retail_startup.a'),
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=['Seven directly affected clients only; broad regression deferred.',
                '185EE0 actual child executes on real owners; full1303F0/130350 remains unbound.',
                'Native registry test-only resource values are not retail data producers.',
                'Game pool/actual task creation and dispatch/lifetime, native input/output and full-game acceptance remain open.']))
        print(json.dumps(dict(status='PASS_RECORDS_WORKSPACE_AND_AFFECTED_INPUT',
            fresh_clients=len(clients), remaining_integrations=len(missing), game_executed=False)))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
