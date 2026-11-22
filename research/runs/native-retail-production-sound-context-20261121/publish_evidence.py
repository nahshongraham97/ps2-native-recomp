"""Publish only this task's authenticated evidence after clean verification."""
from pathlib import Path
import hashlib
import json
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
prefix = str(record.relative_to(fork)).replace('\\', '/')+'/'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=fork, text=True).strip()

assert git('branch', '--show-current') == 'main'
assert git('rev-parse', 'HEAD') == 'c0e5d1cf155c8f88660e2afba77179118f375ee6'
assert not git('diff', '--cached', '--name-only')
assert all(p.startswith(prefix) for p in git('diff', '--name-only').splitlines())
assert hashlib.sha256((parent/'research/doc-audit-20261118/inventory.json').read_bytes()).hexdigest() == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
identity = json.loads((record/'CROSS_CHECKOUT_IDENTITY.json').read_text())
assert identity['status'] == 'PASS_IDENTICAL_COMMITTED_SOURCE_BLOBS'
receipt = json.loads((record/'clean-validation-02/VERIFICATION.json').read_text())
assert receipt['clean_source'] and len(receipt['clients']) == 4
assert len(receipt['remaining_undefined_symbols']) == 39
raw = {r['original'] for r in json.loads((record/'ARCHIVES.json').read_text())}
approved = sorted({str(p.relative_to(fork)).replace('\\', '/') for p in record.rglob('*') if p.is_file()
    and str(p.relative_to(record)).replace('\\', '/') not in raw
    and not any(part in ('negative-content', 'empty-content', '__pycache__') for part in p.parts)} |
    {prefix+'EVIDENCE_STAGING_PLAN.json'})
(record/'EVIDENCE_STAGING_PLAN.json').write_text(json.dumps(dict(approved_paths=approved), indent=2)+'\n', newline='\n')
subprocess.run(['git', 'add', '--force', '--', *approved], cwd=fork, check=True)
actual = git('diff', '--cached', '--name-only').splitlines()
assert actual and all(p in approved for p in actual)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=fork, check=True)
subprocess.run(['git', 'commit', '-m', 'Record clean native sound frame verification and source publication'], cwd=fork, check=True)
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
assert git('rev-parse', 'HEAD') == git('rev-parse', 'origin/main')
assert all(git('rev-parse', 'HEAD:'+scope) == oid for scope, oid in identity['source_trees'].items())
print('PUBLISHED_EVIDENCE', git('rev-parse', 'HEAD'))
