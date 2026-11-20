"""Commit exact reviewed scope locally; retain work if a GitHub remote vanishes."""
import ast, hashlib, json, pathlib, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
R = P / 'research/runs/native-retail-startup-20261120/native-heap/role-accounting'
helper = F / 'research/runs/native-service-runtime-20261120/publish.py'
tree = ast.parse(helper.read_text(encoding='utf-8'))
tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in {'sha', 'need', 'git', 'names'}]
exec(compile(tree, str(helper), 'exec'))


def remote_state(root, expected):
    result = subprocess.run(['git', 'ls-remote', '--heads', 'origin', 'main'], cwd=root, capture_output=True, text=True)
    if result.returncode:
        return dict(available=False, exit=result.returncode, stderr=result.stderr, synchronization='pending')
    words = result.stdout.split()
    need(bool(words) and words[0] == expected, 'remote main changed; resolve before synchronizing')
    return dict(available=True, before_commit=expected)


def commit_sync(root, files, title, remote):
    approved = {q.relative_to(root).as_posix() for q in files}
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty initial index')
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '--', *sorted(approved)], cwd=root, check=True)
    staged = names(root, 'diff', '--cached', '--name-only', '-z')
    need(staged and staged == (names(root, 'diff', 'HEAD', '--name-only', '-z') & approved), 'exact authored staged scope')
    subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
    for name in staged: need(git(root, 'cat-file', 'blob', ':' + name, raw=True) == (root / name).read_bytes(), 'exact staged bytes')
    subprocess.run(['git', 'commit', '-m', title], cwd=root, check=True, stdout=subprocess.DEVNULL)
    head = git(root, 'rev-parse', 'HEAD')
    if not remote['available']: return head, dict(remote, local_commit=head)
    push = subprocess.run(['git', 'push', 'origin', 'main'], cwd=root, capture_output=True, text=True)
    if push.returncode:
        return head, dict(available=False, local_commit=head, exit=push.returncode, stderr=push.stderr, synchronization='pending')
    check = subprocess.run(['git', 'ls-remote', '--heads', 'origin', 'main'], cwd=root, capture_output=True, text=True)
    words = check.stdout.split()
    if check.returncode or not words or words[0] != head:
        return head, dict(available=False, local_commit=head, exit=check.returncode, stderr=check.stderr, synchronization='unverified')
    return head, dict(available=True, local_commit=head, remote_commit=head, synchronization='verified')


base_f = '55e8e56b9d883a6802d188626947803a93271af3'
base_p = '18f2e8f7646cea663205b4c0c92a1548251bf14f'
for root, expected in [(F, base_f), (P, base_p)]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == expected, 'expected local base')
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty index')
vf = RUN / 'validation-01/verification.json'; v = json.loads(vf.read_bytes())
need(v['status'] == 'PASS_NATIVE_WAVE_TRANSACTIONS_AND_ACTUAL_OWNER_ACCOUNTING' and
     len(v['fixtures']) == 3 and len(v['source_after_sha256']) == 749 and v['unchanged_prior_pins'] == 715, 'actual component certificate')
for name, digest in v['source_after_sha256'].items(): need(sha(name) == digest, 'current measured input')
for stem, status in [('SOURCE_REVIEW', 'SOURCE_REVIEW_NATIVE_WAVE_ALLOCATION_AND_HEAP_ROLES_VERIFIED'),
                     ('EXECUTION_REVIEW', 'SAVED_NATIVE_WAVE_ALLOCATION_EXECUTION_VERIFIED')]:
    report = json.loads((R / (stem + '.json')).read_bytes())
    need(report['status'] == status and isinstance(report['input_sha256'], dict), 'actual review schema')
    for name, digest in report['input_sha256'].items(): need(sha(name) == digest, 'current review pin')
preserved = {'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
             'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for name, digest in preserved.items(): need(sha(F / name) == digest, 'preserved unrelated IOP bytes')
need(sha(P / 'research/doc-audit-20261118/inventory.json') == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6', 'preserved broad inventory')
fork_files = {F / name for name in ['cmake/native_game.cmake', 'runtime/include/mka/native_heap.hpp',
    'runtime/src/native/native_heap.cpp', 'tests/test_native_heap_owner.cpp', 'tests/test_native_heap_inflight.cpp',
    'runtime/include/mka/wave_allocation.hpp', 'runtime/src/native/wave_allocation.cpp', 'tests/test_wave_allocation.cpp']}
fork_files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
parent_files = {P / name for name in ['PORT_STATUS.md', 'ROADMAP.md', 'REMOTE_WORK_CONTINUITY.md']}
parent_files.update(q for q in R.rglob('*') if q.is_file())
for root, files in [(F, fork_files), (P, parent_files)]:
    for q in files:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in b.splitlines()), 'authored/snapshot LF/EOF: ' + str(q))
    allowed = {q.relative_to(root).as_posix() for q in files}
    need(names(root, 'diff', '--name-only', '-z') - allowed <=
         ({'research/doc-audit-20261118/inventory.json'} if root == P else set()), 'authored-only modifications')
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(allowed)], cwd=root, check=True)
remote_f, remote_p = remote_state(F, base_f), remote_state(P, base_p)
head, sync_f = commit_sync(F, fork_files, 'Implement source-ordered Wave allocation with actual native owner accounting', remote_f)
prefix = ('Published fork `' if sync_f['synchronization'] == 'verified' else 'Local fork `') + head + '`: '
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name; text = q.read_text(encoding='utf-8')
    marker = 'Source-bound Wave transaction/role accounting now passes3 actual native clients'
    need(text.count(marker) == 1, 'single current transaction result')
    text = text.replace(marker, prefix + 'source-bound Wave transaction/role accounting passes3 actual native clients')
    text = text.replace('independent saved-results review is pending.', 'independent saved-results review verifies all three executions and exact binary/tool/source joins.')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(text.count(marker) == 1, 'single current checkpoint row')
        text = text.replace(marker, '| Current native source checkpoint | ' + prefix +
            'Wave primary/alternate/overflow/terminal native transactions, actual/requested roles and owner-sticky status;3 clients pass (749 stable pins,715 untouched prior), source/execution reviews verified. Actual startup/overlay producer and full-game acceptance OPEN. Prior published fork ', 1)
    text += '\nLocal work continues if GitHub becomes unavailable; commits and synchronization outcomes are recorded separately. See [REMOTE_WORK_CONTINUITY.md](REMOTE_WORK_CONTINUITY.md). No owner launch is requested.\n'
    q.write_bytes(text.encode('utf-8'))
q = R / 'RESULTS.md'; text = q.read_text(encoding='utf-8').replace('Independent saved-execution review is pending.',
    'Independent saved-execution review verifies749 pins,715 unchanged inputs, all three exact saved executions, corrected source snapshots and native symbol/linkage joins. No blocking contradiction was found.')
q.write_bytes(text.encode('utf-8'))
receipt = R / 'PUBLICATION.json'
receipt.open('x', encoding='utf-8', newline='\n').write(json.dumps(dict(status='NATIVE_WAVE_COMPONENT_LOCAL_CHECKPOINT',
    fork_commit=head, fork_sync=sync_f, verification_sha256=sha(vf), source_pins=749, untouched_prior_pins=715,
    fixture_binaries={x['component']: x['executed_target_sha256'] for x in v['fixtures']}, game_acceptance='OPEN'), indent=2) + '\n')
parent_files.add(receipt)
phead, sync_p = commit_sync(P, parent_files, 'Record Wave ownership verification and local work continuity', remote_p)
for name, digest in v['source_after_sha256'].items(): need(sha(name) == digest, 'measured inputs stable after commit')
print(json.dumps(dict(status='NATIVE_WAVE_COMPONENT_COMMITTED', fork_commit=head, parent_commit=phead,
    fork_sync=sync_f, parent_sync=sync_p, game_acceptance='OPEN')), flush=True)
