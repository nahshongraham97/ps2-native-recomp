"""Publish exact decoded-art/coverage changes; keep work if a remote is removed."""
import ast, hashlib, json, pathlib, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
R = P / 'research/runs/native-retail-startup-20261120/health-task-effects-03'
for relative, selected in [
    ('native-service-runtime-20261120/publish.py', {'sha', 'need', 'git', 'names'}),
    ('native-wave-allocation-20261120/publish.py', {'commit_sync'}),
    ('native-hud-health-geometry-20261120/publish.py', {'remote_probe'})]:
    helper = F / 'research/runs' / relative
    tree = ast.parse(helper.read_text(encoding='utf-8'))
    tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in selected]
    # This batch retains original CRLF source snapshots. Current authored files
    # still require LF below; Git must recognize the retained CR as line endings.
    if 'commit_sync' in selected:
        for node in ast.walk(tree):
            if isinstance(node, ast.List) and all(isinstance(x, ast.Constant) for x in node.elts):
                if [x.value for x in node.elts] == ['git', 'diff', '--cached', '--check']:
                    node.elts[1:1] = [ast.Constant('-c'), ast.Constant('core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol')]
        ast.fix_missing_locations(tree)
    exec(compile(tree, str(helper), 'exec'))

base_f = '26bba46290fafc2b186fdb0be2808582b449ac81'
base_p = 'a81954f8c58c1efee9722646422e165992895140'
for root, expected in [(F, base_f), (P, base_p)]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == expected, 'expected local authored base')
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty initial index')
vf = RUN / 'validation-01/verification.json'
v = json.loads(vf.read_bytes())
need(v['status'] == 'PASS_NATIVE_INDEXED_ART_AND_PRE_EFFECT_PROGRAM_COVERAGE' and len(v['source_after_sha256']) == 1320 and
     len(v['fixtures']) == 7 and v['unchanged_prior_pins'] == 1234 and v['historical_resolution_count'] == 12 and
     len(v['decoded_images']) == 2, 'actual saved component schema')
for n, h in v['source_after_sha256'].items(): need(sha(n) == h, 'exact measured input')
for relative, status in [('indexed-sprite/CODE_REVIEW.json', 'SOURCE_REVIEW_NATIVE_INDEXED_SPRITE_DATA_VERIFIED'),
                         ('program-coverage/CODE_REVIEW.json', 'SOURCE_REVIEW_NATIVE_PROGRAM_EARLY_COVERAGE_VERIFIED'),
                         ('EXECUTION_REVIEW.json', 'SAVED_NATIVE_INDEXED_ART_PROGRAM_COVERAGE_EXECUTION_VERIFIED')]:
    d = json.loads((R / relative).read_bytes())
    need(d['status'] == status and isinstance(d['input_sha256'], dict), 'actual final review schema')
    for n, h in d['input_sha256'].items(): need(sha(n) == h, 'exact current or retained review input')
for image in v['decoded_images']:
    need(pathlib.Path(image['path']).stat().st_size == image['bytes'] == 1024 and sha(image['path']) == image['sha256'], 'actual decoded source artifact')
for n, h in {'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
             'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}.items():
    need(sha(F / n) == h, 'protected IOP source')
need(sha(P / 'research/doc-audit-20261118/inventory.json') == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6', 'preserved broad inventory')
fork_files = {F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/indexed_sprite_data.hpp',
    'runtime/src/native/indexed_sprite_data.cpp', 'tests/test_indexed_sprite_data.cpp',
    'runtime/include/mka/program_executor.hpp', 'runtime/src/native/program_executor.cpp',
    'runtime/src/native/playback_owner.cpp', 'tests/test_program_executor.cpp', 'tests/test_program_output_activation.cpp']}
fork_files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts and
                  not any(x.endswith('-owned') for x in q.relative_to(RUN).parts))
parent_files = {P / n for n in ['PORT_STATUS.md', 'ROADMAP.md']}
parent_files.update(q for q in R.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
retained_raw = {}
for relative in ['before-root-integration/provenance.json', 'program-coverage/before/provenance.json',
                 'program-coverage/before-owner-integration/provenance.json']:
    d = json.loads((R / relative).read_bytes())
    for item in d['files'].values():
        q = pathlib.Path(item['archived_path'])
        need(sha(q) == item['sha256'], 'exact original raw snapshot identity')
        retained_raw[q] = item['sha256']
for root, files in [(F, fork_files), (P, parent_files)]:
    for q in files:
        if q in retained_raw:
            need(sha(q) == retained_raw[q], 'preserve original bytes including CRLF')
            continue
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(x == x.rstrip(b' \t') for x in b.splitlines()), 'authored LF/EOF: ' + str(q))
    allowed = {q.relative_to(root).as_posix() for q in files}
    need(names(root, 'diff', '--name-only', '-z') - allowed <=
         ({'research/doc-audit-20261118/inventory.json'} if root == P else set()), 'exact authored tracked change scope')
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(allowed)], cwd=root, check=True)
remote_f, remote_p = remote_probe(F, base_f), remote_probe(P, base_p)
head, sync_f = commit_sync(F, fork_files, 'Decode real indexed artwork and reject invalid sound successors before side effects', remote_f)
prefix = ('Published fork `' if sync_f['synchronization'] == 'verified' else 'Local fork `') + head + '`: '
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name
    text = q.read_text(encoding='utf-8')
    marker = 'Native indexed-art and early sound-coverage components pass seven native CPU/file clients:'
    need(text.count(marker) == 1, 'single current verified batch')
    text = text.replace(marker, prefix + 'native indexed-art and early sound-coverage components pass seven native CPU/file clients:')
    text = text.replace('publication is next. Command support and the mode1 gate remain unchanged.',
                        'the verified source batch is checkpointed. Command support and the mode1 gate remain unchanged.')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(text.count(marker) == 1, 'single current checkpoint identity')
        text = text.replace(marker, '| Current native source checkpoint | ' + prefix +
            'actual stored-channel indexed-art decode and pre-effect sound successor validation;7 native clients/2 decoded source hashes pass (1320 stable pins,1234 unchanged prior,12 historical resolutions), source/results reviews verified. Other formats/render/audio modes/full startup/game acceptance OPEN. Prior published fork ', 1)
    q.write_bytes(text.encode('utf-8'))
receipt = R / 'PUBLICATION.json'
receipt.open('x', encoding='utf-8', newline='\n').write(json.dumps(dict(status='NATIVE_INDEXED_ART_PROGRAM_COVERAGE_LOCAL_CHECKPOINT',
    fork_commit=head, fork_sync=sync_f, verification_sha256=sha(vf), measured_pins=1320, unchanged_prior_pins=1234,
    historical_resolutions=12, executable_sha256={x['component']: x['executed_target_sha256'] for x in v['fixtures']},
    decoded_stored_channels=v['decoded_images'], game_acceptance='OPEN', owner_action='none'), indent=2) + '\n')
parent_files.add(receipt)
phead, sync_p = commit_sync(P, parent_files, 'Record native art decoding and verified incumbent-preserving audio admission', remote_p)
for n, h in v['source_after_sha256'].items(): need(sha(n) == h, 'measured input stable after commit')
print(json.dumps(dict(status='NATIVE_INDEXED_ART_PROGRAM_COVERAGE_COMMITTED', fork_commit=head, parent_commit=phead,
    fork_sync=sync_f, parent_sync=sync_p, game_acceptance='OPEN', owner_action='none')), flush=True)
