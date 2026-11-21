"""Publish exact native HUD scope; keep local history if remote disappears."""
import ast, hashlib, json, pathlib, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'hud-health-geometry'
helper = F / 'research/runs/native-service-runtime-20261120/publish.py'
tree = ast.parse(helper.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in {'sha', 'need', 'git', 'names'}]
exec(compile(tree, str(helper), 'exec'))
helper = F / 'research/runs/native-wave-allocation-20261120/publish.py'
tree = ast.parse(helper.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'commit_sync']
exec(compile(tree, str(helper), 'exec'))


def remote_probe(root, expected):
    result = subprocess.run(['git', 'ls-remote', '--heads', 'origin', 'main'], cwd=root, capture_output=True, text=True)
    words = result.stdout.split()
    if result.returncode or not words or words[0] != expected:
        return dict(available=False, synchronization='pending', exit=result.returncode, stdout=result.stdout,
                    stderr=result.stderr, reason='remote unavailable, main absent, or remote advanced; local work retained')
    return dict(available=True, before_commit=expected)


base_f = '8ed1181a687d473adff359f724886523a59ba260'
base_p = 'a02e13b41f2d61c2e4684a8edf7e85977d8010d8'
for root, expected in [(F, base_f), (P, base_p)]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == expected, 'expected authored local base')
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty initial index')
vf = RUN / 'validation-01/verification.json'
v = json.loads(vf.read_bytes())
need(v['status'] == 'PASS_NATIVE_HUD_PAIRED_GEOMETRY_AND_SAVED_FULL_HEALTH' and
     len(v['source_after_sha256']) == 1127 and len(v['fixtures']) == 1 and
     v['captured_frames'] == 3 and v['captured_vertices'] == 24 and
     v['primary_words_rejoined'] == 2378 and v['unchanged_prior_pins'] == 1004, 'actual saved component schema')
ledger = json.loads((R / 'RESULT_DOC_VERSIONS.json').read_bytes())
need(ledger['status'] == 'HUD_DRAFT_NOTES_RECONCILED_WITH_EXECUTION_RESULT' and len(ledger['versions']) == 2,
     'actual post-result note version schema')
snapshots = {}
for item in ledger['versions']:
    need(sha(item['path']) == item['current_sha256'] and sha(item['archived_path']) == item['previous_sha256'],
         'exact current and measured note bytes')
    snapshots[(item['path'], item['previous_sha256'])] = item['archived_path']
resolved = []
for name, digest in v['source_after_sha256'].items():
    if sha(name) == digest:
        continue
    need((name, digest) in snapshots and sha(snapshots[(name, digest)]) == digest, 'only exact post-result note resolution permitted')
    resolved.append(dict(original_path=name, expected_sha256=digest, retained_path=snapshots[(name, digest)]))
need(len(resolved) == 2, 'two note versions and1125 unchanged measured inputs')
for stem, status in [('CODE_REVIEW', 'SOURCE_REVIEW_NATIVE_HUD_HEALTH_GEOMETRY_VERIFIED'),
                     ('EXECUTION_REVIEW', 'SAVED_NATIVE_HUD_HEALTH_GEOMETRY_EXECUTION_VERIFIED')]:
    d = json.loads((R / (stem + '.json')).read_bytes())
    need(d['status'] == status and isinstance(d['input_sha256'], dict), 'actual independent review schema')
    for name, digest in d['input_sha256'].items():
        need(sha(name) == digest, 'review pins exact current or explicit archived input')
preserved = {'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
             'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for name, digest in preserved.items(): need(sha(F / name) == digest, 'preserved IOP source')
need(sha(P / 'research/doc-audit-20261118/inventory.json') == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6', 'preserved broad inventory')
fork_files = {F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/hud_health_geometry.hpp',
    'runtime/src/native/hud_health_geometry.cpp', 'tests/test_hud_health_geometry.cpp']}
fork_files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
parent_files = {P / n for n in ['PORT_STATUS.md', 'ROADMAP.md']}
parent_files.add(S / 'CLASS_CALLER_SOURCE.md')
parent_files.update(q for q in R.rglob('*') if q.is_file())
parent_files.update(S / (stem + ext) for stem in ['TASK_ASSOCIATION_SOURCE', 'TASK_BODY_SOURCE'] for ext in ['.md', '.json'])
for folder in ['words-task-association', 'words-task-body']:
    parent_files.update(q for q in (S / folder).rglob('*') if q.is_file())
for root, files in [(F, fork_files), (P, parent_files)]:
    for q in files:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(x == x.rstrip(b' \t') for x in b.splitlines()), 'authored LF/EOF: ' + str(q))
    allowed = {q.relative_to(root).as_posix() for q in files}
    need(names(root, 'diff', '--name-only', '-z') - allowed <=
         ({'research/doc-audit-20261118/inventory.json'} if root == P else set()), 'exact authored tracked change scope')
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(allowed)], cwd=root, check=True)
remote_f, remote_p = remote_probe(F, base_f), remote_probe(P, base_p)
head, sync_f = commit_sync(F, fork_files, 'Implement native paired health HUD geometry with captured numeric verification', remote_f)
prefix = ('Published fork `' if sync_f['synchronization'] == 'verified' else 'Local fork `') + head + '`: '
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name
    text = q.read_text(encoding='utf-8')
    marker = 'Native health HUD position component now passes its isolated CPU check:'
    need(text.count(marker) == 1, 'single current HUD result')
    text = text.replace(marker, prefix + 'native health HUD position component passes its isolated CPU check:')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(text.count(marker) == 1, 'single current checkpoint identity')
        text = text.replace(marker, '| Current native source checkpoint | ' + prefix +
            'native paired HUD position ownership;1 CPU client/3groups and24 saved full-health vertices pass (1127 measured pins,1004 prior unchanged,2378 words). Independent source/results reviews verify the component. Full task/resource/audio/GPU/startup/game acceptance OPEN. Prior published fork ', 1)
    q.write_bytes(text.encode('utf-8'))
receipt = R / 'PUBLICATION.json'
receipt.open('x', encoding='utf-8', newline='\n').write(json.dumps(dict(status='NATIVE_HUD_COMPONENT_LOCAL_CHECKPOINT',
    fork_commit=head, fork_sync=sync_f, verification_sha256=sha(vf), measured_pins=1127,
    unchanged_prior_pins=1004, original_words=2378, saved_frames=3, saved_vertices=24,
    post_result_exact_note_resolutions=resolved, test_binary_sha256=v['fixtures'][0]['executed_target_sha256'],
    game_acceptance='OPEN', owner_action='none'), indent=2) + '\n')
parent_files.add(receipt)
phead, sync_p = commit_sync(P, parent_files, 'Record native HUD evidence, task priority correction and current work state', remote_p)
for name, digest in v['source_after_sha256'].items():
    need(sha(name) == digest or ((name, digest) in snapshots and sha(snapshots[(name, digest)]) == digest), 'saved measured version still exact after commit')
print(json.dumps(dict(status='NATIVE_HUD_COMPONENT_COMMITTED', fork_commit=head, parent_commit=phead,
    fork_sync=sync_f, parent_sync=sync_p, game_acceptance='OPEN', owner_action='none')), flush=True)
