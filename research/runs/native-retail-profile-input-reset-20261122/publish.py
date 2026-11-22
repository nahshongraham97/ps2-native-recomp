"""Publish only measured profile/input reset source and its pinned evidence."""
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
before = json.loads((record / 'BEFORE.json').read_text())
runtime = set(before['pins'])
frozen = json.loads((record / 'DELEGATE_REVIEW.json').read_text())['files']
review = json.loads((fork / 'research/runs/parallel-profile-input-reset-review-20261122/AUTHENTICATION.json').read_text())
assert review['base'] == before['base'] and review['canonical_uncommitted']
assert review['all_source_files'] == 862 and review['required_names'] == 59
assert review['listed_words_total'] == 318 and review['implemented_words_included'] == 44
assert set(review['scoped_source_sha256']) == runtime
for path, digest in review['scoped_source_sha256'].items():
    assert sha(fork / path) == digest, path
for path, digest in frozen.items():
    assert sha(fork / path) == digest, path

validation = 'validation-01' if phase == 'source' else 'clean-validation-01'
receipt = json.loads((record / validation / 'VERIFICATION.json').read_text())
assert receipt['status'] == 'PASS_NATIVE_SHARED_PROFILE_INPUT_RESET_DEPENDENCY'
assert len(receipt['clients']) == 9 and len(receipt['remaining_undefined_symbols']) == 59
if phase == 'source':
    assert git('rev-parse', 'HEAD') == before['base']
    pins = json.loads((record / validation / 'SOURCE_FILES.json').read_text())
    assert len(pins) == 862
    for path, digest in pins.items():
        assert sha(fork / path) == digest, path
else:
    assert receipt['clean_source']
    assert json.loads((record / 'CROSS_CHECKOUT_IDENTITY.json').read_text())['checked_source_files'] == 862

owned = {p.relative_to(fork).as_posix() for p in record.rglob('*')
         if p.is_file() and p.suffix in ('.json', '.py', '.md', '.gz', '.asm', '.tsv', '.txt')}
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
subprocess.run(['git', '-c', 'core.autocrlf=false', 'add', '--pathspec-from-file=-', '--pathspec-file-nul'],
               input=b''.join(path.encode('utf-8') + b'\0' for path in sorted(approved)), cwd=fork, check=True)
staged = set(git('diff', '--cached', '--name-only').splitlines())
assert staged and staged.issubset(approved)
subprocess.run(['git', 'diff', '--cached', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
subprocess.run(['git', '-c', 'core.whitespace=cr-at-eol', 'diff', '--cached', '--check'], cwd=fork, check=True)
for path, digest in frozen.items():
    assert hashlib.sha256(subprocess.check_output(['git', 'show', ':' + path], cwd=fork)).hexdigest() == digest, path
message = ('Retain actual profile input role controls across reset' if phase == 'source'
           else 'Record clean native profile input reset dependency checks')
subprocess.run(['git', 'commit', '--quiet', '-m', message], cwd=fork, check=True)
head = git('rev-parse', 'HEAD')
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
assert head == git('rev-parse', 'origin/main') == git('ls-remote', 'origin', 'refs/heads/main').split()[0]
path = record / ('PUBLISHED-' + phase + '.json')
assert not path.exists()
path.write_text(json.dumps(dict(commit=head, phase=phase, staged_paths=sorted(staged), remote_main=head), indent=2) + '\n', newline='\n')
print(json.dumps(dict(commit=head, phase=phase, files=len(staged))))
