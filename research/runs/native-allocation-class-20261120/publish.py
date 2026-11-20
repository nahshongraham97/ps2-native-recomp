"""Publish verified native payload/profile components, preserving local continuity."""
import ast, hashlib, json, pathlib, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'allocation-class'
for path, selected in [
    (F / 'research/runs/native-service-runtime-20261120/publish.py', {'sha', 'need', 'git', 'names'}),
    (F / 'research/runs/native-wave-allocation-20261120/publish.py', {'remote_state', 'commit_sync'})]:
    tree = ast.parse(path.read_text(encoding='utf-8'))
    tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in selected]
    exec(compile(tree, str(path), 'exec'))

base_f = '84e5af891e77a2900d0f951dd6867fc3fa323c16'
base_p = '275371c5a07bdbae15b9b99cb6fbff750f7776ec'
for root, expected in [(F, base_f), (P, base_p)]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == expected, 'expected local base')
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty initial index')
vf = RUN / 'validation-01/verification.json'; v = json.loads(vf.read_bytes())
need(v['status'] == 'PASS_NATIVE_DYNAMIC_ALLOCATION_CLASS_AND_RETAIL_PROFILE_SELECTION' and
     len(v['source_after_sha256']) == 923 and len(v['fixtures']) == 2 and v['unchanged_prior_pins'] == 834 and
     v['primary_words_rejoined'] == 2376, 'actual completed certificate')
for n, h in v['source_after_sha256'].items(): need(sha(n) == h, 'current measured input')
for stem, status in [('SOURCE_REVIEW', 'SOURCE_REVIEW_NATIVE_ALLOCATION_CLASS_AND_PROFILE_VERIFIED'),
                     ('EXECUTION_REVIEW', 'SAVED_NATIVE_ALLOCATION_CLASS_AND_PROFILE_EXECUTION_VERIFIED')]:
    d = json.loads((R / (stem + '.json')).read_bytes())
    need(d['status'] == status and isinstance(d['input_sha256'], dict), 'actual review schema')
    for n, h in d['input_sha256'].items(): need(sha(n) == h, 'current review pin')
preserved = {'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
             'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for n, h in preserved.items(): need(sha(F / n) == h, 'preserved IOP bytes')
need(sha(P / 'research/doc-audit-20261118/inventory.json') == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6', 'preserved broad inventory')
fork_files = {F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/allocation_class.hpp',
    'runtime/src/native/allocation_class.cpp', 'tests/test_allocation_class.cpp', 'runtime/include/mka/allocation_profile.hpp',
    'runtime/src/native/allocation_profile.cpp', 'tests/test_allocation_profile.cpp']}
fork_files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts and
    not any(part.endswith('-owned') for part in q.relative_to(RUN).parts))
parent_files = {P / n for n in ['PORT_STATUS.md', 'ROADMAP.md']}
parent_files.update(q for q in R.rglob('*') if q.is_file())
parent_files.update(S / (stem + ext) for stem in ['PROFILE_MEMORY_SOURCE', 'FIXED_CLASS_SOURCE'] for ext in ['.md', '.json'])
for name in ['words-profile-memory', 'words-fixed-class']:
    parent_files.update(q for q in (S / name).rglob('*') if q.is_file())
for root, files in [(F, fork_files), (P, parent_files)]:
    for q in files:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in b.splitlines()), 'authored/snapshot LF/EOF: ' + str(q))
    allowed = {q.relative_to(root).as_posix() for q in files}
    need(names(root, 'diff', '--name-only', '-z') - allowed <=
         ({'research/doc-audit-20261118/inventory.json'} if root == P else set()), 'authored-only tracked modifications')
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(allowed)], cwd=root, check=True)
remote_f, remote_p = remote_state(F, base_f), remote_state(P, base_p)
head, sync_f = commit_sync(F, fork_files, 'Implement dynamic native allocation classes and original profile selection', remote_f)
prefix = ('Published fork `' if sync_f['synchronization'] == 'verified' else 'Local fork `') + head + '`: '
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name; text = q.read_text(encoding='utf-8')
    marker = 'Two native CPU clients pass: 923 stable pins'
    need(text.count(marker) == 1, 'single current class result')
    text = text.replace(marker, prefix + 'two native CPU clients pass: 923 stable pins')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(text.count(marker) == 1, 'single current checkpoint row')
        text = text.replace(marker, '| Current native source checkpoint | ' + prefix +
            'dynamic native class backing and 20-selector/13-class original profile data;2 clients pass (923 stable pins,834 prior unchanged,2376 source words), independent source/execution review verified. Typed caller/callback/reset/main integration and full-game acceptance OPEN. Prior published fork ', 1)
    q.write_bytes(text.encode('utf-8'))
receipt = R / 'PUBLICATION.json'
receipt.open('x', encoding='utf-8', newline='\n').write(json.dumps(dict(status='NATIVE_ALLOCATION_CLASS_LOCAL_CHECKPOINT',
    fork_commit=head, fork_sync=sync_f, verification_sha256=sha(vf), source_pins=923, untouched_prior_pins=834,
    primary_words_rejoined=2376, independent_profile_oracle_sha256=v['independent_profile_oracle_sha256'],
    fixture_binaries={x['component']: x['executed_target_sha256'] for x in v['fixtures']},
    game_acceptance='OPEN', owner_action='none'), indent=2) + '\n')
parent_files.add(receipt)
phead, sync_p = commit_sync(P, parent_files, 'Record verified native class storage and source-derived game profile behavior', remote_p)
for n, h in v['source_after_sha256'].items(): need(sha(n) == h, 'measured source stable after commit')
print(json.dumps(dict(status='NATIVE_ALLOCATION_CLASS_COMPONENT_COMMITTED', fork_commit=head, parent_commit=phead,
    fork_sync=sync_f, parent_sync=sync_p, game_acceptance='OPEN')), flush=True)
