"""Scoped checkpoint publication; leaves unrelated and next-batch work alone."""
import importlib.util
import json
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
D = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-initial-task-29'
spec = importlib.util.spec_from_file_location('publisher', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
pub = helper.pub

if __name__ == '__main__':
    vpath = H / 'validation-01/verification.json'
    v = json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(v['schema'] == 'native_retail_initial_task_targeted_checks_v1'
             and v['status'] == 'PASS_TARGETED_INITIAL_TASK'
             and isinstance(v['files'], dict) and isinstance(v['evidence'], dict)
             and [r['target'] for r in v['executed_clients']] ==
                 ['test_native_retail_initial_task', 'test_native_retail_game_session']
             and len(v['required_production_children']) == 11
             and not v['game_executed'] and not v['physical_endpoints_opened'], 'actual bounded result schema')
    for group in [v['files'], v['evidence'], v['protected_inputs']]:
        for path, digest in group.items():
            pub.need(pub.sha(path) == digest, 'unchanged measured/protected bytes: ' + path)
    build = F / 'build_native_services_only_20261119-01'
    for row in v['executed_clients']:
        receipt = json.loads((H / 'validation-01' / row['receipt']).read_text(encoding='utf-8'))
        pub.need(receipt['exit'] == 0 and 'reused_from' not in receipt
                 and receipt['executable_sha256'] == row['sha256'] == pub.sha(build / (row['target'] + '.exe')),
                 'actual fresh targeted execution')
    pub.need(pub.sha(build / 'libmka_native_retail_initial_task.a') == v['parent_archive_sha256'], 'measured parent archive')
    bases = {F: 'e1bcc21cdb8373b91d1562a41bc0a3e7152a8d12', P: 'cfe37f11f0dd75caf76088f660fec1b86be6394a'}
    for root, base in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == base and pub.git(root, 'branch', '--show-current') == 'main', 'checkpoint base')
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    readme = F / 'README.md'
    addition = '\nTargeted startup-task checkpoint, 2026-11-21: the native18E100 parent/18DB60 continuation and persistent session compile; two affected CPU clients pass. Eleven production child definitions remain required at link. Full entry/input/render/audio/game acceptance remains open. See research/runs/native-retail-initial-task-20261121/validation-01/verification.json and the parent current status.\n'
    text = readme.read_text(encoding='utf-8')
    pub.need('Targeted startup-task checkpoint, 2026-11-21:' not in text, 'new bounded checkpoint note')
    readme.write_text(text.rstrip() + '\n' + addition, encoding='utf-8', newline='\n')
    forks = {Path(p) for p in v['files']} | {readme, Path(__file__).resolve()}
    forks.update(p for p in (H / 'validation-01').iterdir() if p.is_file())
    fh, fs = helper.commit_sync(F, sorted(forks), 'Compile native retail initial task and persistent menu state', fr)
    current = ('Published startup-task checkpoint fork ' + fh + ' (work29): actual18E100 parent and18DB60 continuation compile on the existing native registry/execution path. Two targeted CPU clients pass freshly, including persistent session and invalid-child/ownership checks. All eleven real child effects remain required at production link. No game/endpoints/frames/audio or full-game acceptance; broad regression and full-entry recheck deferred. Previous39 integration count is Work27 evidence. See research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-initial-task-29/RESULTS.md. Active production work now follows entry order: complete profile/resource reset, actual overlay/startup dependencies, then task creation/full list dispatch and real input/render/sound. Owner no action.')
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        path = P / name
        lines = path.read_text(encoding='utf-8').splitlines()
        found = [i for i, line in enumerate(lines) if line.startswith('Current work29 (')]
        pub.need(len(found) == 1, 'one current Work29 paragraph')
        lines[found[0]] = current
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    plan = D / 'IMPLEMENTATION_PLAN.md'
    text = plan.read_text(encoding='utf-8')
    text = text.replace('Status: implementation frozen at R3 and independently source-reviewed; no new runtime code built yet. Build validation is next.',
                        'Status: R3 source reviewed; targeted compilation and two existing CPU clients pass. Eleven production child definitions remain unresolved; see RESULTS.md.')
    plan.write_text(text, encoding='utf-8', newline='\n')
    record = dict(schema='native_retail_initial_task_publication_v1', fork_commit=fh, fork=fs,
        parent_commit='enclosing commit', verification_sha256=pub.sha(vpath),
        scope='Two targeted CPU clients; production children still missing; full-game acceptance OPEN')
    with (D / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    parents = {P / 'PORT_STATUS.md', P / 'ROADMAP.md'}
    parents.update(p for p in D.rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    ph, ps = helper.commit_sync(P, sorted(parents), 'Record native startup task checks and connected implementation priority', pr)
    for path, digest in v['protected_inputs'].items():
        pub.need(pub.sha(path) == digest, 'protected bytes through publication')
    print(json.dumps(dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps)))
