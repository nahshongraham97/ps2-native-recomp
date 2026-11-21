"""Publish the measured records/workspace batch with explicit authored paths."""
import importlib.util
import json
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C / 'native-retail-records-workspace-38'
spec = importlib.util.spec_from_file_location('publisher', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
pub = h.pub

if __name__ == '__main__':
    vp = H / 'validation-03/verification.json'
    v = json.loads(vp.read_text(encoding='utf-8'))
    pub.need(v['schema'] == 'native_retail_records_workspace_checks_v1'
        and v['status'] == 'PASS_RECORDS_WORKSPACE_AND_AFFECTED_INPUT'
        and isinstance(v['sources'], dict) and isinstance(v['evidence'], dict)
        and isinstance(v['changed_or_added'], list) and len(v['executed_clients']) == 7
        and len(v['remaining_integration_symbols']) == 35
        and not v['game_executed'] and not v['physical_endpoints_opened'], 'actual measured schema/scope')
    for group in [v['sources'], v['evidence'], v['protected_inputs']]:
        for path, digest in group.items():
            pub.need(pub.sha(path) == digest, 'stable measured source/evidence: ' + path)
    build = F / 'build_native_services_only_20261119-01'
    pub.need(pub.sha(build / 'libmka_native_retail_startup.a') == v['startup_archive_sha256'], 'measured startup archive')
    for row in v['executed_clients']:
        r = json.loads((H / 'validation-03' / (row['target'] + '.json')).read_text(encoding='utf-8'))
        pub.need(r['exit'] == 0 and 'reused_from' not in r and
                 r['executable_sha256'] == row['sha256'] == pub.sha(build / (row['target'] + '.exe')),
                 'actual freshly run binary')
    bases = {F: 'aabb2cc7f5f1dc1d224ef1e4ac86e0b27de2b293', P: 'f352c413895fba48ddb4044f75544ea179cf743b'}
    for root, base in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == base and pub.git(root, 'branch', '--show-current') == 'main', 'exact base')
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    readme = F / 'README.md'
    text = readme.read_text(encoding='utf-8')
    text += '\nRecords/workspace checkpoint, 2026-11-21: actual section Main initialization and185EE0 child/shared Session data compile; seven directly affected CPU/file clients pass after two preserved corrections. Complete entry still refuses35 Main integrations. Full game/physical input/render/audio acceptance remains open. See research/runs/native-retail-records-workspace-20261121/validation-03/verification.json and parent current status.\n'
    readme.write_text(text, encoding='utf-8', newline='\n')
    forks = {F / n for n in v['changed_or_added']} | {readme, H / 'check.py', Path(__file__).resolve()}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    fh, fs = h.commit_sync(F, sorted(forks), 'Connect native section and game-record owners with shared input flags', fr)
    current = ('Published records/workspace checkpoint fork ' + fh + ': actual19C4C0 section initialization resolves and185EE0 child executes on real native owners in targeted integration. Seven directly affected clients pass freshly after the preserved registry-type and record-growth corrections. Actual full-entry link still refuses35 Main integrations; no game/executable/endpoints/frames/audio or full-game acceptance. Shared Controls/reset/Session fields have one owner; source initial sizes are not host limits. See research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-records-workspace-38/RESULTS.md. Next close1303F0/130350 memory/profile/reset/pool/task source sequence, actual task creator/full dispatch, then native physical input/render/sound. Broad regressions deferred; owner no action.')
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        path = P / name
        lines = path.read_text(encoding='utf-8').splitlines()
        positions = [i for i, line in enumerate(lines) if line.startswith('Published production-initialization checkpoint fork ')]
        pub.need(len(positions) == 1, 'one current production checkpoint')
        lines[positions[0]] = current
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    for rel in ['native-retail-game-record-init-34/ROOT_INTEGRATION.md',
                'native-retail-loaded-sections-35/ROOT_INTEGRATION.md', 'native-retail-task-domain-36/ROOT_INTEGRATION.md']:
        path = C / rel
        text = path.read_text(encoding='utf-8').replace('Current source is unbuilt.',
            'Targeted compilation and the affected clients pass in native-retail-records-workspace-38/RESULTS.md. This is component evidence; whole-game acceptance remains open.')
        path.write_text(text, encoding='utf-8', newline='\n')
    path = C / 'native-retail-game-record-init-34/reset01/ROOT_INTEGRATION.md'
    text = path.read_text(encoding='utf-8').replace('Source is not built yet;',
        'The source now compiles and the actual child passes targeted integration (work38);')
    path.write_text(text, encoding='utf-8', newline='\n')
    record = dict(schema='native_records_workspace_publication_v1', fork_commit=fh, fork=fs,
        parent_commit='enclosing commit', verification_sha256=pub.sha(vp), remaining_integrations=35,
        scope='Seven affected clients, no complete game or endpoint acceptance')
    with (D / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    parents = {P / 'PORT_STATUS.md', P / 'ROADMAP.md'}
    for rel in ['native-retail-game-record-init-34', 'native-retail-loaded-sections-35',
                'native-retail-task-domain-36', 'native-retail-records-workspace-38']:
        parents.update(p for p in (C / rel).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    ph, ps = h.commit_sync(P, sorted(parents), 'Record verified native records and section startup progress', pr)
    for path, digest in v['protected_inputs'].items():
        pub.need(pub.sha(path) == digest, 'protected inputs through publication')
    print(json.dumps(dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps)))
