"""Publish scoped renderer module/lease source and exact immutable evidence."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('phase', choices=['source', 'evidence'])
phase = parser.parse_args().phase
record = Path(__file__).resolve().parent
fork = record.parents[2]

def git(*args):
    return subprocess.check_output(['git', '-c', 'core.longpaths=true', *args], cwd=fork, text=True).strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

assert git('branch', '--show-current') == 'main'
before = json.loads((record / 'BEFORE.json').read_text())
runtime = set(before['pins'])
frozen = {}
for name in ['CANDIDATE_EVIDENCE.json', 'DELEGATE_EVIDENCE.json']:
    for group in json.loads((record / name).read_text()).values():
        for path, digest in group['files'].items():
            assert path not in frozen or frozen[path] == digest
            frozen[path] = digest
for path, digest in frozen.items():
    assert sha(fork / path) == digest, path
assert json.loads((record / 'MATRIX_SCOPE_REVIEW.json').read_text())['status'] == 'PASS_MATRIX_SOURCE_AND_LATEST_CORE_LEASE_RESULTS'
review = json.loads((fork / 'research/runs/parallel-renderer-0401-review-20261122/RESULT.json').read_text())
assert review['status'] == 'PASS_BOUNDED_INDEPENDENT_REVIEW_NO_FINDING' and review['source_files'] == 859
validation = 'validation-01' if phase == 'source' else 'clean-validation-01'
receipt = json.loads((record / validation / 'VERIFICATION.json').read_text())
assert receipt['status'] == 'PASS_NATIVE_RENDERER_MODULES_AND_CORE_CALLOUT_LEASE'
assert len(receipt['clients']) == 8 and len(receipt['remaining_undefined_symbols']) == 62
if phase == 'source':
    assert git('rev-parse', 'HEAD') == before['base']
    pins = json.loads((record / validation / 'SOURCE_FILES.json').read_text())
    assert len(pins) == 868
    for path, digest in pins.items():
        assert sha(fork / path) == digest, path
else:
    assert receipt['clean_source']
    assert json.loads((record / 'CROSS_CHECKOUT_IDENTITY.json').read_text())['checked_source_files'] == 868

owned = {p.relative_to(fork).as_posix() for p in record.rglob('*')
         if p.is_file() and p.suffix in ('.json', '.py', '.md', '.gz')}
owned.add((record / 'STAGED_SOURCE.json').relative_to(fork).as_posix())
approved = runtime | set(frozen) | owned
if phase == 'source':
    (record / 'STAGED_SOURCE.json').write_text(json.dumps({'approved': sorted(approved)}, indent=2) + '\n', newline='\n')
assert set(git('diff', '--cached', '--name-only').splitlines()).issubset(approved)
assert set(git('diff', '--name-only').splitlines()).issubset(approved)
for path in runtime | owned:
    if not path.endswith('.gz'):
        assert b'\r' not in (fork / path).read_bytes(), path
subprocess.run(['git', 'diff', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
path_bytes = b''.join(path.encode() + b'\0' for path in sorted(approved))
ignored = subprocess.run(['git', 'check-ignore', '--no-index', '-z', '--stdin'], cwd=fork, input=path_bytes, capture_output=True)
assert ignored.returncode in (0, 1) and not ignored.stderr
ignored_paths = {path.decode() for path in ignored.stdout.split(b'\0') if path}
assert ignored_paths.issubset(frozen)
if ignored_paths:
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '-f', '--pathspec-from-file=-', '--pathspec-file-nul'],
                   input=b''.join(path.encode() + b'\0' for path in sorted(ignored_paths)), cwd=fork, check=True)
subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '--pathspec-from-file=-', '--pathspec-file-nul'], input=path_bytes, cwd=fork, check=True)
staged = set(git('diff', '--cached', '--name-only').splitlines())
assert staged and staged.issubset(approved)
subprocess.run(['git', 'diff', '--cached', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
# Imported raw logs/decompiler data retain their original measured whitespace.
# Every exception is classified from the immutable candidate BEFORE staging;
# current runtime/tests/configuration/root records above retain strict checks.
check = subprocess.run(['git', '-c', 'core.whitespace=cr-at-eol', 'diff', '--cached', '--check'], cwd=fork, capture_output=True, text=True)
assert check.returncode in (0, 2) and not check.stderr
actual = []
for line in check.stdout.splitlines():
    match = re.match(r'^(research/runs/.+?):([0-9]+): (trailing whitespace\.|new blank line at EOF\.|space before tab in indent\.)$', line)
    if match:
        path, number, kind = match.groups()
        assert path in frozen
        actual.append(dict(path=path, line=int(number), kind=kind, sha256=frozen[path]))
expected = json.loads((record / 'FROZEN_WHITESPACE.json').read_text())['findings']
expected = [row for row in expected if row['path'] in staged]
assert sorted(actual, key=lambda n: (n['path'], n['line'], n['kind'])) == expected
assert check.returncode == 0 or actual
for path, digest in frozen.items():
    assert hashlib.sha256(subprocess.check_output(['git', 'show', ':' + path], cwd=fork)).hexdigest() == digest, path
message = ('Connect native renderer transform and matrix owners with safe Core callouts' if phase == 'source'
           else 'Record clean renderer modules and Core lifetime verification')
subprocess.run(['git', 'commit', '--quiet', '-m', message], cwd=fork, check=True)
head = git('rev-parse', 'HEAD')
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
assert head == git('rev-parse', 'origin/main') == git('ls-remote', 'origin', 'refs/heads/main').split()[0]
output = record / ('PUBLISHED-' + phase + '.json')
assert not output.exists()
output.write_text(json.dumps(dict(commit=head, phase=phase, staged_paths=sorted(staged), remote_main=head), indent=2) + '\n', newline='\n')
print(json.dumps(dict(commit=head, phase=phase, files=len(staged), frozen_data_whitespace=len(actual))))
