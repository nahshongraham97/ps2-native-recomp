"""Targeted integration compile and existing clients; never opens the game."""
import importlib.util
import json
from datetime import datetime, timezone
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
D = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-initial-task-29'
spec = importlib.util.spec_from_file_location('native_checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = H / 'validation-01'

if __name__ == '__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        manifest_path = D / 'source-agent01/IMPLEMENTATION.json'
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        c.require(manifest['schema'] == 'native_retail_initial_task_implementation_v1'
                  and manifest['status'] == 'SOURCE_FROZEN_R3_NOT_BUILT_OR_RUN'
                  and isinstance(manifest['files_sha256'], dict)
                  and isinstance(manifest['evidence_sha256'], dict), 'R3 source manifest schema')
        reviewed = D / 'review-interface-correction01/FINAL.json'
        review = json.loads(reviewed.read_text(encoding='utf-8'))
        c.require(review['status'] == 'R3_CORRECTED_NO_CONFIRMED_INTERFACE_DEFECT_PREBUILD'
                  and review['reviewed_sha256']['source-agent01/IMPLEMENTATION.json'] == c.sha(manifest_path),
                  'exact reviewed source freeze')
        pins = {str(F / n): h for n, h in manifest['files_sha256'].items()}
        evidence = {str(D / 'source-agent01' / n): h for n, h in manifest['evidence_sha256'].items()}
        evidence.update({str(manifest_path): c.sha(manifest_path), str(reviewed): c.sha(reviewed)})
        for n in ['cmake/native_game.cmake', 'cmake/native_typed.cmake',
                  'tests/test_retail_initial_task_missing_children.cpp']:
            pins[str(F / n)] = c.sha(F / n)
        pins[str(Path(__file__).resolve())] = c.sha(__file__)
        previous_path = F / 'research/runs/native-retail-task-execution-20261121/validation-01/verification.json'
        c.require(c.sha(previous_path) == '78bd0c530e2a7689db2dcf05e4488a53498f4b6891f94af9ba19bf5ab65bb0c0',
                  'exact prior task execution receipt')
        previous = json.loads(previous_path.read_text(encoding='utf-8'))
        protected = previous['protected_inputs']
        def stable():
            for group in [pins, evidence, protected]:
                c.require(all(c.sha(n) == h for n, h in group.items()), 'frozen/protected source identity')
        stable()
        c.save('source-preflight.json', dict(files=pins, evidence=evidence, protected_inputs=protected))
        targets = ['test_native_retail_initial_task', 'test_native_retail_game_session']
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *targets, '-j2'])
        root = c.OUT / 'content-root'
        root.mkdir()
        clients = []
        for target in targets:
            args = [root] if target == 'test_native_retail_initial_task' else []
            receipt = c.execute(target, [c.BUILD / (target + '.exe'), *args])
            c.require('reused_from' not in receipt and not receipt['stderr'] and receipt['stdout'].startswith('PASS '),
                      'fresh targeted native assertions: ' + target)
            clients.append(dict(target=target, sha256=receipt['executable_sha256'], receipt=target + '.json'))
        children = ['run_reset_18d970', 'begin_intro_18deb0', 'begin_prompt_18dc10', 'transition_1225d0',
                    'begin_menu_18d450', 'begin_menu_18d870', 'begin_menu_18d8e0', 'begin_menu_18d700',
                    'begin_menu_18d800', 'begin_menu_18d660', 'begin_menu_18d770']
        negative = c.execute('missing-production-children', [c.CMAKE, '--build', c.BUILD, '--target',
            'test_native_retail_initial_task_missing_children', '-j2'], expected=None)
        c.require(all('initial_task_required::' + n + '(' in negative['stdout'] + negative['stderr'] for n in children)
                  and not (c.BUILD / 'test_native_retail_initial_task_missing_children.exe').exists(),
                  'all actual production child definitions required')
        stable()
        result = dict(schema='native_retail_initial_task_targeted_checks_v1', status='PASS_TARGETED_INITIAL_TASK',
            utc=datetime.now(timezone.utc).isoformat(), files=pins, evidence=evidence, protected_inputs=protected,
            executed_clients=clients, required_production_children=children,
            prior_task_verification_sha256=c.sha(previous_path),
            parent_archive_sha256=c.sha(c.BUILD / 'libmka_native_retail_initial_task.a'),
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=manifest['unbound'] + ['Only parent/session clients freshly run; broad regression deferred.',
                'Test-only child procedures are not retail child implementations.',
                'Full-entry gate was not rerun; prior 39 missing integrations is a dated observation.'])
        c.save('verification.json', result)
        print(json.dumps(dict(status=result['status'], executed_clients=len(clients),
                             required_production_children=len(children), game_executed=False)))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
