"""Scope-check and publish only this correction and its owned evidence."""
from pathlib import Path
import argparse
import json
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('phase', choices=['source', 'evidence'])
args = parser.parse_args()
record = Path(__file__).resolve().parent
fork = record.parents[2]
def git(*argv):
    return subprocess.check_output(['git', '-c', 'core.longpaths=true', *argv], cwd=fork, text=True).strip()
assert git('branch', '--show-current') == 'main'
assert not git('diff', '--cached', '--name-only')
runtime = ['runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main.cpp']
owned = sorted(p.relative_to(fork).as_posix() for p in record.rglob('*') if p.is_file()
               and p.suffix in ('.json', '.gz', '.md', '.py') and p.name != 'ORIENTATION.json')
approved = set(runtime + owned)
assert set(git('diff', '--name-only').splitlines()).issubset(approved)
for name in approved:
    path = fork/name
    if path.suffix != '.gz':
        assert b'\r' not in path.read_bytes(), name
subprocess.run(['git', 'diff', '--check', '--', *approved], cwd=fork, check=True)
subprocess.run(['git', 'add', '--', *sorted(approved)], cwd=fork, check=True)
staged = set(git('diff', '--cached', '--name-only').splitlines())
assert staged and staged.issubset(approved)
assert 'research/doc-audit-20261118/inventory.json' not in staged
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=fork, check=True)
message = ('Correct retail startup online effect mislabeled as audio' if args.phase == 'source'
           else 'Authenticate published online-stage correction and negative link')
subprocess.run(['git', 'commit', '-m', message], cwd=fork, check=True)
commit = git('rev-parse', 'HEAD')
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
remote = git('ls-remote', 'origin', 'refs/heads/main').split()[0]
assert remote == commit == git('rev-parse', 'origin/main')
receipt = dict(phase=args.phase, commit=commit, remote_main=remote, staged_paths=sorted(staged))
output = record/('PUBLISHED-'+args.phase+'.json')
assert not output.exists()
output.write_text(json.dumps(receipt, indent=2)+'\n', newline='\n')
print(json.dumps(dict(commit=commit, files=len(staged), remote_main=remote)))
