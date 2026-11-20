"""Publish only reviewed native heap component/evidence; never launch a game."""
import ast, csv, hashlib, json, pathlib, struct, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
RUN = pathlib.Path(__file__).resolve().parent
H = P / 'research/runs/native-retail-startup-20261120/native-heap'
source = F / 'research/runs/native-service-runtime-20261120/publish.py'
tree = ast.parse(source.read_text())
tree.body = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in {'sha', 'need', 'git', 'names', 'publish'}]
exec(compile(tree, str(source), 'exec'))


def check_pins(report):
    need(isinstance(report['input_sha256'], dict), 'actual report pin schema')
    for name, digest in report['input_sha256'].items(): need(sha(name) == digest, 'report pin: ' + name)


for root, expected in [(F, '8d0d59804a82fb99642438c6db1a136e68517238'), (P, '6618cd86c2570e7bae41e6af81e7bc1aa3756b00')]:
    need(git(root, 'branch', '--show-current') == 'main' and git(root, 'rev-parse', 'HEAD') == expected, 'expected base')
    need(git(root, 'ls-remote', '--heads', 'origin', 'main').split()[0] == expected, 'expected remote base')
    need(not names(root, 'diff', '--cached', '--name-only', '-z'), 'empty initial index')
vpath = RUN / 'validation-03/verification.json'; v = json.loads(vpath.read_bytes())
need(v['status'] == 'PASS_NATIVE_HEAP_OWNERSHIP_AND_REAL_HOST_LIFETIME_CUTS' and
     len(v['source_after_sha256']) == 720 and len(v['fixtures']) == 2 and v['unchanged_prior_pins'] == 650,
     'actual component certificate')
for name, digest in v['source_after_sha256'].items(): need(sha(name) == digest, 'current certificate input')
for stem, status in [('PEER_OWNER_INCLUDE_REVIEW', 'SOURCE_REVIEW_NATIVE_HEAP_OWNER_INCLUDE_ONLY_VERIFIED'),
                     ('EXECUTION_REVIEW', 'SAVED_NATIVE_HEAP_EXECUTION_VERIFIED'),
                     ('caller-review/CONSUMERS', 'WAVE_OVERLAY_KIND_AND_LIFECYCLE_QUERY_SOURCE_VERIFIED')]:
    report = json.loads((H / (stem + '.json')).read_bytes())
    need(report['status'] == status, 'actual review status')
    check_pins(report)
elf = (P / 'research/inputs/SLUS_214.10').read_bytes(); phoff, = struct.unpack_from('<I', elf, 28)
size, count = struct.unpack_from('<HH', elf, 42)
segments = [struct.unpack_from('<8I', elf, phoff + i * size) for i in range(count)]
joined = 0
for path in (H / 'caller-review/consumer-words').glob('*.csv'):
    rows = list(csv.DictReader(path.read_text().splitlines()))
    need(bool(rows) and {'va', 'file_offset', 'le32'}.issubset(rows[0]), 'inspected consumer export schema')
    for row in rows:
        va, offset, word = (int(row[k], 16) for k in ['va', 'file_offset', 'le32'])
        mapped = [s[1] + va - s[2] for s in segments if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
        need(mapped == [offset] and struct.unpack_from('<I', elf, offset)[0] == word, 'consumer original words')
        joined += 1
need(joined == 499, '499 additional actual owner-consumer words')
preserved = {'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
             'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for name, digest in preserved.items(): need(sha(F / name) == digest, 'preserved IOP source')
inventory = P / 'research/doc-audit-20261118/inventory.json'
need(sha(inventory) == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6', 'preserved broad inventory')
fork_files = {F / name for name in ['cmake/native_game.cmake', 'runtime/include/mka/native_heap.hpp',
    'runtime/src/native/native_heap.cpp', 'tests/test_native_heap_owner.cpp', 'tests/test_native_heap_inflight.cpp']}
fork_files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
parent_files = {P / name for name in ['PORT_STATUS.md', 'ROADMAP.md', '.gitattributes']}
parent_files.update(q for q in H.rglob('*') if q.is_file())
for root, files in [(F, fork_files), (P, parent_files)]:
    for q in files:
        if q.is_relative_to(H) and 'before' in q.relative_to(H).parts: continue
        data = q.read_bytes()
        need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in data.splitlines()), 'authored LF/EOF: ' + str(q))
    allowed = {q.relative_to(root).as_posix() for q in files}
    extra = names(root, 'diff', '--name-only', '-z') - allowed
    need(extra <= ({'research/doc-audit-20261118/inventory.json'} if root == P else set()), 'authored-only dirty scope')
# Complete both repositories' read-only formatting/pin preflight before first commit.
subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(q.relative_to(F).as_posix() for q in fork_files)], cwd=F, check=True)
subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(q.relative_to(P).as_posix() for q in parent_files)], cwd=P, check=True)
head, fcount = publish(F, {q.relative_to(F).as_posix() for q in fork_files},
    'Add native aligned allocation ownership and real host lifetime checks')
for name in ['PORT_STATUS.md', 'ROADMAP.md']:
    q = P / name; text = q.read_text()
    old = 'Native allocation ownership now passes2 actual native CPU clients:'
    need(text.count(old) == 1, 'single current native heap result')
    text = text.replace(old, 'Published fork `' + head + '`: native allocation ownership passes2 actual native CPU clients:')
    text = text.replace('independent saved-results review is pending.', 'independent saved-results review verifies both executions and exact source/binary/tool joins.')
    if name == 'PORT_STATUS.md':
        marker = '| Current native source checkpoint | Published fork '
        need(text.count(marker) == 1, 'single current checkpoint row')
        text = text.replace(marker, '| Current native source checkpoint | Published fork `' + head +
            '`: actual host allocation ownership;2 native clients pass (720 stable pins,650 untouched prior;765 ELF words); saved execution/ABI/native linkage independently verified. Retail Wave callback/overlay lifecycle and full-game acceptance OPEN. Prior published fork ', 1)
    q.write_bytes(text.encode())
q = H / 'RESULTS.md'; text = q.read_text()
text = text.replace('Independent saved-execution review is pending.',
    'Independent saved-execution review verifies720 pins, both exact executions, ABI/native linkage and preserved preflights. No blocking contradiction was found.')
q.write_bytes(text.encode())
receipt = H / 'PUBLICATION.json'
receipt.open('x', encoding='utf-8', newline='\n').write(json.dumps(dict(
    status='NATIVE_HEAP_COMPONENT_PUBLISHED', fork_commit=head, verification_sha256=sha(vpath),
    source_pins=720, unchanged_prior_pins=650, fixture_binaries={x['component']: x['executed_target_sha256'] for x in v['fixtures']},
    additional_consumer_words_rejoined=joined, game_acceptance='OPEN', owner_action='none'), indent=2) + '\n')
parent_files.add(receipt)
phead, pcount = publish(P, {q.relative_to(P).as_posix() for q in parent_files},
    'Record native allocation ownership and exact retail Wave source boundary')
for name, digest in v['source_after_sha256'].items(): need(sha(name) == digest, 'stable measured inputs after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_HEAP_COMPONENT', fork_commit=head, parent_commit=phead,
    fork_files=fcount, parent_files=pcount, clients=2, game_acceptance='OPEN')), flush=True)
