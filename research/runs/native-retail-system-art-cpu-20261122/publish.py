"""Publish only the reviewed CPU-data source and its measured evidence."""
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
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stage(paths, force=False):
    argv = ['git', '-c', 'core.autocrlf=false', 'add']
    if force:
        argv.append('-f')
    argv += ['--pathspec-from-file=-', '--pathspec-file-nul']
    subprocess.run(argv, cwd=fork, check=True, input=b''.join(p.encode() + b'\0' for p in sorted(paths)))


assert git('branch', '--show-current') == 'main'
before = json.loads((record / 'BEFORE.json').read_text())
runtime = set(before['paths'])
frozen = json.loads((record / 'CANDIDATE_EVIDENCE.json').read_text())['paths']
review = json.loads((record / 'CANDIDATE_REVIEW.json').read_text())
assert review['status'] == 'PASS_BOUNDED_COMMITTED_SYSTEM_ART_CPU_CANDIDATE'
assert review['source_blobs'] == 867 and review['artifact_blobs'] == 175
for path, digest in frozen.items():
    assert sha(fork / path) == digest, path
validation = 'validation-01' if phase == 'source' else 'clean-validation-01'
receipt = json.loads((record / validation / 'VERIFICATION.json').read_text())
assert receipt['status'] == 'PASS_NATIVE_SYSTEM_ART_CPU_AND_EXISTING_CONSUMERS'
assert len(receipt['clients']) == 5 and len(receipt['remaining_undefined_symbols']) == 62
if phase == 'source':
    assert git('rev-parse', 'HEAD') == before['base']
    pins = json.loads((record / validation / 'SOURCE_FILES.json').read_text())
    assert len(pins) == 873
    for path, digest in pins.items():
        assert sha(fork / path) == digest, path
else:
    assert receipt['clean_source']
    assert json.loads((record / 'CROSS_CHECKOUT_IDENTITY.json').read_text())['checked_source_files'] == 873
    assert git('rev-parse', 'HEAD') == json.loads((record / 'PUBLISHED-source.json').read_text())['commit']

owned = {p.relative_to(fork).as_posix() for p in record.rglob('*')
         if p.is_file() and p.suffix in ('.json', '.py', '.md', '.gz')}
owned.add((record / 'STAGED_SOURCE.json').relative_to(fork).as_posix())
approved = runtime | set(frozen) | owned
if phase == 'source':
    (record / 'STAGED_SOURCE.json').write_text(json.dumps(dict(approved=sorted(approved)), indent=2) + '\n', newline='\n')
assert set(git('diff', '--cached', '--name-only').splitlines()).issubset(approved)
assert set(git('diff', '--name-only').splitlines()).issubset(approved)
for path in runtime | owned:
    if not path.endswith('.gz'):
        assert b'\r' not in (fork / path).read_bytes(), path
subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
path_bytes = b''.join(p.encode() + b'\0' for p in sorted(approved))
ignored = subprocess.run(['git', 'check-ignore', '--no-index', '-z', '--stdin'], cwd=fork,
                         input=path_bytes, capture_output=True)
assert ignored.returncode in (0, 1) and not ignored.stderr
ignored_paths = {p.decode() for p in ignored.stdout.split(b'\0') if p}
assert ignored_paths.issubset(frozen)
if ignored_paths:
    stage(ignored_paths, force=True)
stage(approved)
staged = set(git('diff', '--cached', '--name-only').splitlines())
assert staged and staged.issubset(approved)
subprocess.run(['git', 'diff', '--cached', '--check', '--', *sorted(runtime | owned)], cwd=fork, check=True)
check = subprocess.run(['git', '-c', 'core.whitespace=cr-at-eol', 'diff', '--cached', '--check'],
                       cwd=fork, capture_output=True, text=True)
assert check.returncode in (0, 2) and not check.stderr
actual = []
for line in check.stdout.splitlines():
    match = re.match(r'^(research/runs/.+?):([0-9]+): (trailing whitespace\.|new blank line at EOF\.|space before tab in indent\.)$', line)
    if match:
        path, number, kind = match.groups()
        assert path in frozen
        actual.append(dict(path=path, line=int(number), kind=kind, sha256=frozen[path]))
expected = json.loads((record / 'FROZEN_WHITESPACE.json').read_text())['findings']
expected = [entry for entry in expected if entry['path'] in staged]
assert sorted(actual, key=lambda row: (row['path'], row['line'], row['kind'])) == expected
assert check.returncode == 0 or actual
for path, digest in frozen.items():
    assert hashlib.sha256(subprocess.check_output(['git', 'show', ':' + path], cwd=fork)).hexdigest() == digest, path
message = ('Prepare original localized system art through native CPU decoders' if phase == 'source'
           else 'Record clean localized system art CPU verification')
subprocess.run(['git', 'commit', '--quiet', '-m', message], cwd=fork, check=True)
commit = git('rev-parse', 'HEAD')
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
assert commit == git('rev-parse', 'origin/main') == git('ls-remote', 'origin', 'refs/heads/main').split()[0]
output = record / ('PUBLISHED-' + phase + '.json')
assert not output.exists()
output.write_text(json.dumps(dict(commit=commit, phase=phase, staged_paths=sorted(staged),
                                  remote_main=commit), indent=2) + '\n', newline='\n')
print(json.dumps(dict(commit=commit, phase=phase, files=len(staged), immutable_data_findings=len(actual))))
