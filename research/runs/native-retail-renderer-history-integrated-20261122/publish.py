"""Publish scoped, measured native renderer/history source and immutable evidence."""
from pathlib import Path
import argparse
import hashlib
import json
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
runtime = set(json.loads((record / 'BEFORE.json').read_text())['pins'])
frozen = {}
for name in ['CANDIDATE_EVIDENCE.json', 'DELEGATE_REVIEWS.json']:
    for group in json.loads((record / name).read_text()).values():
        for path, digest in group['files'].items():
            assert path not in frozen or frozen[path] == digest
            frozen[path] = digest
for path, digest in frozen.items():
    assert sha(fork / path) == digest, path

transfer = json.loads((fork / 'research/runs/parallel-renderer-transfer-review-20261122/AUTHENTICATION.json').read_text())
audio = json.loads((fork / 'research/runs/parallel-control-history-review-20261122/REVIEW.json').read_text())
assert transfer['candidate'] == 'feb05096e43fb0ae37bac6407d2567569ddb03e0'
assert transfer['source_files'] == 856 and transfer['required_names'] == 60
assert audio['status'] == 'PASS_BOUNDED_HISTORY_REVIEW_NO_CONCRETE_FINDING'
assert audio['candidate'] == 'c9f3db8c71bc1b3d18594329c8478ed59798e327'
assert audio['corpus_rows'] == 26576 and audio['source_files'] == 853

validation = 'validation-01' if phase == 'source' else 'clean-validation-01'
receipt = json.loads((record / validation / 'VERIFICATION.json').read_text())
assert receipt['status'] == 'PASS_INTEGRATED_NATIVE_RENDERER_CORE_AND_CONTROL_HISTORY'
assert len(receipt['clients']) == 24 and len(receipt['remaining_undefined_symbols']) == 59
if phase == 'source':
    pins = json.loads((record / validation / 'SOURCE_FILES.json').read_text())
    assert len(pins) == 861
    for path, digest in pins.items():
        assert sha(fork / path) == digest, path
else:
    assert receipt['clean_source']
    assert json.loads((record / 'CROSS_CHECKOUT_IDENTITY.json').read_text())['checked_source_files'] == 861

owned = {p.relative_to(fork).as_posix() for p in record.rglob('*')
         if p.is_file() and p.suffix in ('.json', '.py', '.md', '.gz')}
staged_source = (record / 'STAGED_SOURCE.json').relative_to(fork).as_posix()
owned.add(staged_source)
approved = runtime | set(frozen) | owned
if phase == 'source':
    (record / 'STAGED_SOURCE.json').write_text(json.dumps({'approved': sorted(approved)}, indent=2) + '\n', newline='\n')
assert set(git('diff', '--cached', '--name-only').splitlines()).issubset(approved)
assert set(git('diff', '--name-only').splitlines()).issubset(approved)
for path in runtime | owned:
    if not path.endswith('.gz'):
        assert b'\r' not in (fork / path).read_bytes(), path
subprocess.run(['git', 'diff', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
manual = 'research/runs/parallel-normal-control-history-20261122/manual-42.png'
assert manual in frozen and sha(fork / manual) == frozen[manual]
subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '-f', '--', manual], cwd=fork, check=True)
subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '--pathspec-from-file=-', '--pathspec-file-nul'], input=b''.join(path.encode('utf-8') + b'\0' for path in sorted(approved)), cwd=fork, check=True)
staged = set(git('diff', '--cached', '--name-only').splitlines())
assert staged and staged.issubset(approved)
subprocess.run(['git', 'diff', '--cached', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
frozen_check = subprocess.run(['git', '-c', 'core.whitespace=cr-at-eol', 'diff', '--cached', '--check'], cwd=fork, capture_output=True, text=True)
if frozen_check.returncode:
    classification = json.loads((record / 'FROZEN_WHITESPACE.json').read_text())
    assert classification['path'] in frozen and frozen[classification['path']] == classification['sha256']
    assert frozen_check.returncode == 2 and frozen_check.stdout == classification['expected_check_output'] and not frozen_check.stderr, frozen_check.stdout

for path, digest in frozen.items():
    assert hashlib.sha256(subprocess.check_output(['git', 'show', ':' + path], cwd=fork)).hexdigest() == digest, path
message = ('Connect native renderer core and retained sound history' if phase == 'source'
           else 'Record clean integrated renderer and sound history verification')
subprocess.run(['git', 'commit', '--quiet', '-m', message], cwd=fork, check=True)
head = git('rev-parse', 'HEAD')
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
assert head == git('rev-parse', 'origin/main') == git('ls-remote', 'origin', 'refs/heads/main').split()[0]
output = record / ('PUBLISHED-' + phase + '.json')
assert not output.exists()
output.write_text(json.dumps(dict(commit=head, phase=phase, staged_paths=sorted(staged), remote_main=head), indent=2) + '\n', newline='\n')
print(json.dumps(dict(commit=head, phase=phase, files=len(staged))))
