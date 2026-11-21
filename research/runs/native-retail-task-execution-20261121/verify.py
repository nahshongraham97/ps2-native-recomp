"""Bounded native task continuation/cleanup checks; no game or devices."""
import importlib.util, json, re
from pathlib import Path
F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
OUT = H / 'validation-01'
D = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-task-dispatch-27'
spec = importlib.util.spec_from_file_location('checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = OUT
if __name__ == '__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(exist_ok=False)
    try:
        manifest_path = D / 'source-agent01/IMPLEMENTATION.json'
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        c.save('schema-preflight.json', {k: dict(type=type(v).__name__, size=len(v) if isinstance(v, (dict, list)) else None)
            for k, v in manifest.items()})
        c.require(manifest['schema'] == 'native_retail_task_execution_implementation_v1' and
                  manifest['status'] == 'SOURCE_FROZEN_R3_FIXTURE_ONLY_NOT_BUILT_OR_RUN', 'actual author source freeze schema')
        prior_path = F / 'research/runs/native-typed-retail-consumer-20261121/validation-01/verification.json'
        c.require(c.sha(prior_path) == 'c4258d699bb82e0b61926ffd2f2b7fb3448f258907c882699d8c069375f8e577', 'exact prior published component')
        prior = json.loads(prior_path.read_text(encoding='utf-8'))
        source = dict(prior['sources'])
        for path, digest in manifest['files_sha256'].items():
            c.require(c.sha(F / path) == digest, 'exact frozen task source')
            source[str(F / path)] = digest
        source[str(F / 'cmake/native_typed.cmake')] = c.sha(F / 'cmake/native_typed.cmake')
        source[str(Path(__file__).resolve())] = c.sha(__file__)
        evidence = {str(D / 'source-agent01' / n): h for n, h in manifest['source_evidence_sha256'].items()}
        evidence[str(manifest_path)] = c.sha(manifest_path)
        c.require(all(c.sha(p) == h for p, h in source.items()), 'all current source pins')
        c.require(all(c.sha(p) == h for p, h in evidence.items()), 'all frozen source evidence pins')
        targets = ['test_native_retail_task_registry', 'test_native_retail_task_execution']
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *targets, '-j2'])
        fixtures = []
        for target in targets:
            receipt = c.execute(target, [c.BUILD / (target + '.exe')])
            c.require('reused_from' not in receipt and not receipt['stderr'], 'fresh affected native client')
            if target.endswith('registry'):
                c.require(re.fullmatch(r'PASS native retail ordered task registry: [1-9][0-9]* publication allocation cuts; actual task producers/dispatch/main gate open\n', receipt['stdout']), 'registry assertions')
            else:
                c.require(receipt['stdout'].strip() == 'PASS native retail typed task control: immediate continuation, retained wait, direct cleanup; original bodies and Main dispatch open', 'typed task control assertions')
            symbols = c.execute(target + '-symbols', [c.BIN / 'llvm-nm.exe', '-C', c.BUILD / (target + '.exe')])
            c.require('mka::retail::NativeRetailTaskRegistry::publish_task(' in symbols['stdout'], 'same actual task registry')
            if target.endswith('execution'):
                c.require('mka::retail::NativeRetailTaskExecution::invoke_virtual_10_initial(' in symbols['stdout'], 'actual typed execution implementation')
            c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_', symbols['stdout']), 'no guest/device/compiler dependency')
            fixtures.append(dict(target=target, sha256=receipt['executable_sha256']))
        unchanged = []
        for row in [prior['executed_client']] + prior['unchanged_prior_binary_identities']:
            if row['target'] in targets:
                continue
            c.require(c.sha(c.BUILD / (row['target'] + '.exe')) == row['sha256'], 'unchanged earlier binary identity')
            unchanged.append(row)
        c.require(len(unchanged) == 40, '40 unchanged current prior binaries, not new executions')
        main_archive = c.BUILD / 'libmka_native_retail_startup.a'
        c.require(c.sha(main_archive) == prior['main_archive_sha256'], 'unchanged complete main archive; task dispatcher still unbound')
        negative = c.execute('incomplete-entry-link', [c.CMAKE, '--build', c.BUILD, '--target',
            'test_native_startup_missing_bindings', '-j2'], expected=None)
        c.require(all('main_binding::' + n + '(' in negative['stdout'] + negative['stderr'] for n in prior['remaining_integration_symbols'])
            and not (c.BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'entry refuses all39 actual integrations')
        protected = json.loads((F / 'research/runs/native-typed-retail-20261121/validation-02/verification.json').read_text(encoding='utf-8'))['protected_inputs']
        c.require(all(c.sha(p) == h for p, h in source.items()), 'source stable through checks')
        c.require(all(c.sha(p) == h for p, h in evidence.items()), 'source evidence stable through checks')
        c.require(all(c.sha(p) == h for p, h in protected.items()), 'protected original/unrelated inputs')
        result = dict(schema='native_retail_task_execution_verification_v1', status='PASS_BOUNDED_NATIVE_TASK_EXECUTION',
            sources=source, source_evidence=evidence, executed_clients=fixtures, unchanged_prior_binary_identities=unchanged,
            registry_archive_sha256=c.sha(c.BUILD / 'libmka_native_retail_task_registry.a'),
            main_archive_sha256=c.sha(main_archive), protected_inputs=protected,
            remaining_integration_symbols=prior['remaining_integration_symbols'], complete_game_entry_linkable=False,
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=manifest['unbound'] + ['Source initial virtual+10 branch only; readiness/list dispatch remains unbound.',
                'CPU test bodies are explicit fixtures, not implementations of original18E100/18DB60.',
                'Known main transfer leaves unresolved same-body continuation pending; registry lookup/publication remain usable.',
                'Pending nested native resource retirement after nonlocal transfer/full reset remains open.',
                '40 previous binaries are unchanged identities, not reexecutions.'])
        c.save('verification.json', result)
        print(json.dumps({k: result[k] for k in ['status', 'game_executed', 'complete_game_entry_linkable']}))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
