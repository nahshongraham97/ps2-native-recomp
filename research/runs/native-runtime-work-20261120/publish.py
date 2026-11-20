"""Publish the measured native runtime transaction and its scoped current docs."""
import ast, hashlib, json, pathlib, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
R = P / 'research/runs/native-movie-ingress-20261119'
tree = ast.parse((F / 'research/runs/native-service-runtime-20261120/publish.py').read_text())
defs = [n for n in tree.body if isinstance(n, ast.FunctionDef)
    and n.name in {'sha', 'need', 'git', 'names', 'publish'}]
assert len(defs) == 5
exec(compile(ast.Module(body=defs, type_ignores=[]), 'checked-publish-helpers', 'exec'), globals())


def validate():
    v = json.loads((RUN / 'validation-01/verification.json').read_bytes())
    need(v['status'] == 'PASS_NATIVE_PRIVATE_RUNTIME_WORK_AND_SERVICE_RETIREMENT'
        and len(v['fixtures']) == 27 and len(v['source_after_sha256']) == 567
        and v['unchanged_prior_pins'] == 535, 'actual runtime-work evidence schema')
    for name, digest in v['source_after_sha256'].items():
        need(sha(name) == digest, 'current measured input ' + name)
    for stem, status in [
        ('NATIVE_RUNTIME_WORK_SOURCE_REVIEW_20261120', 'SOURCE_REVIEW_NATIVE_PRIVATE_RUNTIME_WORK_VERIFIED'),
        ('NATIVE_RUNTIME_WORK_EXECUTION_REVIEW_20261120', 'SAVED_NATIVE_RUNTIME_WORK_EXECUTION_VERIFIED'),
        ('NATIVE_RUNTIME_WORK_CLOSURE_20261120', 'NATIVE_RUNTIME_WORK_SOURCE_AND_SAVED_RESULTS_VERIFIED')]:
        d = json.loads((R / (stem + '.json')).read_bytes())
        need(d['status'] == status and isinstance(d['input_sha256'], dict), 'actual review schema')
        for name, digest in d['input_sha256'].items():
            need(sha(name) == digest, 'current reviewed input ' + name)
    for fixture in v['fixtures']:
        need(sha(F / 'build_native_services_only_20261119-01' / (fixture['target'] + '.exe'))
            == fixture['executed_target_sha256'], 'current actual executed binary')
    return v


for root, base in [(F, 'a5386b625f677d29223b3e234bc4d073a875e3e6'),
                   (P, '585044e6dc0d2ac799d8cb2f851ceeeac1c9f8f6')]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == base,
        'expected local base')
    need(git(root, 'ls-remote', '--heads', 'origin', 'main').split()[0] == base, 'expected remote base')
protected = {
    'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory = P / 'research/doc-audit-20261118/inventory.json'
inventory_sha = '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for name, digest in protected.items():
    need(sha(F / name) == digest, 'protected IOP')
need(sha(inventory) == inventory_sha, 'preserved inventory')
v = validate()
review = json.loads((R / 'NATIVE_RUNTIME_WORK_SOURCE_REVIEW_20261120.json').read_bytes())
files = {pathlib.Path(name) for name in review['input_sha256']}
files.update(q for q in RUN.rglob('*') if q.is_file() and q.suffix in {'.py', '.json', '.md'}
    and '__pycache__' not in q.parts and not any(part.endswith('-owned') for part in q.relative_to(RUN).parts))
for q in files:
    b = q.read_bytes()
    need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n')
        and all(line == line.rstrip(b' \t') for line in b.splitlines()), 'authored LF/EOF ' + str(q))
approved = {q.relative_to(F).as_posix() for q in files}
need(names(F, 'diff', '--name-only', '-z') - approved <= set(protected), 'only authored fork tracked edits')
head, fcount = publish(F, approved, 'Build native runtime programs privately before cancellable publication')
prefix = 'Published fork ' + chr(96) + head + chr(96) + ': '
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name
    s = q.read_text()
    marker = 'Actual private runtime-work and retained Service admission are implemented and all27 affected clients pass'
    need(s.count(marker) == 1, 'single current runtime-work result')
    s = s.replace(marker, prefix + marker[0].lower() + marker[1:])
    s = s.replace('Independent source and saved execution reviews pass; publication pending.',
        'Independent source and saved execution reviews pass; this component is published.')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(s.count(marker) == 1, 'single current source checkpoint')
        s = s.replace(marker, '| Current native source checkpoint | ' + prefix +
            'private runtime build/cancellation/commit and independent Service work/result ownership;27 affected clients pass ' +
            '(567 stable pins,535 untouched prior), source/saved-result reviews complete. Native retail entry, source callbacks ' +
            'and whole-game acceptance OPEN. Prior published fork ', 1)
        lines = s.splitlines()
        for i, line in enumerate(lines):
            if line.startswith('| Native sound components |'):
                lines[i] = '| Native sound components | Real private runtime/cache/copy/output ownership and Service admission;27 clients pass | Component evidence only; retail bootstrap/queue/callback/current-game sound output and physical/all-game acceptance OPEN |'
        s = '\n'.join(lines) + '\n'
    q.write_bytes(s.rstrip('\n').encode() + b'\n')
receipt = R / 'NATIVE_RUNTIME_WORK_PUBLICATION_20261120.json'
with receipt.open('x', encoding='utf-8', newline='\n') as out:
    json.dump(dict(status='NATIVE_RUNTIME_WORK_COMPONENT_PUBLISHED', fork_commit=head,
        verification_sha256=sha(RUN / 'validation-01/verification.json'), executed_fixtures=27,
        source_pins=567, untouched_prior_pins=535,
        fixture_binaries={x['component']: x['executed_target_sha256'] for x in v['fixtures']},
        protected_dirty_sha256=protected,
        limits=['Component only; native retail entry, source coordinator and full-game acceptance OPEN',
            'Offline decoder frozen; prioritize whole retail startup/memory/input/render/service integration']), out, indent=2)
    out.write('\n')
files = {P / name for name in ['PORT_STATUS.md', 'ROADMAP.md']}
files.add(receipt)
for stem in ['NATIVE_RUNTIME_WORK_PLAN_20261120', 'NATIVE_RUNTIME_WORK_IMPLEMENTATION_20261120',
             'NATIVE_RUNTIME_WORK_SOURCE_REVIEW_20261120', 'NATIVE_RUNTIME_WORK_EXECUTION_REVIEW_20261120',
             'NATIVE_RUNTIME_WORK_RESULTS_20261120', 'NATIVE_RUNTIME_WORK_CLOSURE_20261120']:
    files.update(R / (stem + ext) for ext in ['.md', '.json'] if (R / (stem + ext)).is_file())
files.update(q for q in (R / 'native-runtime-work-before-20261120').rglob('*') if q.is_file())
for q in files:
    b = q.read_bytes()
    need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n')
        and all(line == line.rstrip(b' \t') for line in b.splitlines()), 'parent LF/EOF ' + str(q))
approved = {q.relative_to(P).as_posix() for q in files}
need(names(P, 'diff', '--name-only', '-z') - approved <= {'research/doc-audit-20261118/inventory.json'},
    'only authored parent tracked edits')
phead, pcount = publish(P, approved, 'Record runtime work verification and prioritize native retail entry')
validate()
for name, digest in protected.items():
    need(sha(F / name) == digest, 'protected IOP after publication')
need(sha(inventory) == inventory_sha, 'inventory after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_RUNTIME_WORK_AND_DOCS', fork_commit=head,
    parent_commit=phead, fork_files=fcount, parent_files=pcount, fixtures=27, pins=567)), flush=True)
