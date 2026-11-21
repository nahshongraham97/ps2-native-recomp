"""Exact native art ingress publication, retaining local work without a remote."""
import ast, hashlib, json, pathlib, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'health-task-effects-01'
A = R / 'art-section'
for relative, selected in [
    ('native-service-runtime-20261120/publish.py', {'sha', 'need', 'git', 'names'}),
    ('native-wave-allocation-20261120/publish.py', {'commit_sync'}),
    ('native-hud-health-geometry-20261120/publish.py', {'remote_probe'})]:
    helper = F / 'research/runs' / relative
    tree = ast.parse(helper.read_text(encoding='utf-8'))
    tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in selected]
    exec(compile(tree, str(helper), 'exec'))

base_f = '48e98f26c4d92e58189027ebfb8f1a49d3652b97'
base_p = '0789ffd67193f84e5b9f4be323384a67312ee033'
for root, expected in [(F, base_f), (P, base_p)]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == expected, 'expected local authored base')
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty initial index')
vf = RUN / 'validation-01/verification.json'
v = json.loads(vf.read_bytes())
need(v['status'] == 'PASS_NATIVE_ART_SECTION_DATA_FIVE_ACTUAL_VARIANTS' and len(v['source_after_sha256']) == 1240 and
     len(v['fixtures']) == 5 and v['unchanged_prior_pins'] == 1124 and v['primary_words_rejoined'] == 3197 and
     len(v['asset_ranges']) == 5, 'actual saved verification schema')
for n, h in v['source_after_sha256'].items(): need(sha(n) == h, 'exact current measured input')
for stem, status in [('CODE_REVIEW', 'SOURCE_REVIEW_NATIVE_ART_SECTION_DATA_VERIFIED'),
                     ('EXECUTION_REVIEW', 'SAVED_NATIVE_ART_SECTION_DATA_EXECUTION_VERIFIED')]:
    d = json.loads((A / (stem + '.json')).read_bytes())
    need(d['status'] == status and isinstance(d['input_sha256'], dict), 'actual independent review schema')
    for n, h in d['input_sha256'].items(): need(sha(n) == h, 'exact current or explicit archived review input')
for n, h in {'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
             'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}.items():
    need(sha(F / n) == h, 'protected IOP source')
need(sha(P / 'research/doc-audit-20261118/inventory.json') == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6', 'preserved broad inventory')
fork_files = {F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/art_section_data.hpp',
    'runtime/src/native/art_section_data.cpp', 'tests/test_art_section_data.cpp']}
fork_files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts and
                  not any(x.startswith('owned-') for x in q.relative_to(RUN).parts))
parent_files = {P / n for n in ['PORT_STATUS.md', 'ROADMAP.md']}
parent_files.update(q for q in R.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
for root, files in [(F, fork_files), (P, parent_files)]:
    for q in files:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(x == x.rstrip(b' \t') for x in b.splitlines()), 'authored LF/EOF: ' + str(q))
    allowed = {q.relative_to(root).as_posix() for q in files}
    need(names(root, 'diff', '--name-only', '-z') - allowed <=
         ({'research/doc-audit-20261118/inventory.json'} if root == P else set()), 'authored-only tracked scope')
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(allowed)], cwd=root, check=True)
remote_f, remote_p = remote_probe(F, base_f), remote_probe(P, base_p)
head, sync_f = commit_sync(F, fork_files, 'Read owned native art sections and verify five actual locale variants', remote_f)
prefix = ('Published fork `' if sync_f['synchronization'] == 'verified' else 'Local fork `') + head + '`: '
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name
    text = q.read_text(encoding='utf-8')
    marker = 'Native art-section data ingress passes its isolated native CPU/file build and five actual locale executions:'
    need(text.count(marker) == 1, 'single current art ingress result')
    text = text.replace(marker, prefix + 'native art-section data ingress passes its isolated native CPU/file build and five actual locale executions:')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(text.count(marker) == 1, 'single checkpoint identity')
        text = text.replace(marker, '| Current native source checkpoint | ' + prefix +
            'native filesystem SEC directory/sprite data;5 actual locale executions/10 encoded resource comparisons pass (1240 stable pins,1124 unchanged prior,3197 original words), independent source/results reviews verified. Texture/resource/audio/startup/game acceptance OPEN. Prior published fork ', 1)
    q.write_bytes(text.encode('utf-8'))
receipt = A / 'PUBLICATION.json'
receipt.open('x', encoding='utf-8', newline='\n').write(json.dumps(dict(status='NATIVE_ART_SECTION_COMPONENT_LOCAL_CHECKPOINT',
    fork_commit=head, fork_sync=sync_f, verification_sha256=sha(vf), measured_pins=1240, unchanged_prior_pins=1124,
    primary_words=3197, actual_locale_executions=5, actual_encoded_resources=10,
    executable_sha256=v['fixtures'][0]['executed_target_sha256'], game_acceptance='OPEN', owner_action='none'), indent=2) + '\n')
parent_files.add(receipt)
phead, sync_p = commit_sync(P, parent_files, 'Record actual retail art ingress and exact health audio implementation gaps', remote_p)
for n, h in v['source_after_sha256'].items(): need(sha(n) == h, 'measured inputs unchanged after commit')
print(json.dumps(dict(status='NATIVE_ART_SECTION_COMPONENT_COMMITTED', fork_commit=head, parent_commit=phead,
    fork_sync=sync_f, parent_sync=sync_p, game_acceptance='OPEN', owner_action='none')), flush=True)
