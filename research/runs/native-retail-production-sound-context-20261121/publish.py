"""Stage only the reviewed authored scope; any failed gate prevents publication."""
from pathlib import Path
import hashlib
import json
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]

def git(*args):
    return subprocess.check_output(['git', *args], cwd=fork, text=True).strip()

assert git('branch', '--show-current') == 'main'
assert git('rev-parse', 'HEAD') == '7fa313a84f608552763706f746b66733bf904654'
assert not git('diff', '--cached', '--name-only')
assert hashlib.sha256((parent/'research/doc-audit-20261118/inventory.json').read_bytes()).hexdigest() == '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
source = ['runtime/include/mka/retail_current_sound_required.hpp',
          'runtime/src/native/retail_sound_bank_main.cpp', 'tests/test_retail_reset_sound.cpp']
assert sorted(git('diff', '--name-only').splitlines()) == sorted(source)
verification = json.loads((record/'validation-01/VERIFICATION.json').read_text())
assert verification['status'] == 'PASS_NATIVE_SOUND_FRAME_ATTEMPT_COUNTER'
assert len(verification['clients']) == 4 and len(verification['remaining_undefined_symbols']) == 39
pins = json.loads((record/'validation-01/SOURCE_FILES.json').read_text())
assert all(hashlib.sha256((fork/p).read_bytes()).hexdigest() == digest for p, digest in pins.items())
source_join = json.loads((record/'SOURCE_JOIN.json').read_text())
assert source_join['status'] == 'PASS_ORIGINAL_CONTEXT_DEPENDENCY_BYTE_JOIN'
raw = {row['original'] for row in json.loads((record/'ARCHIVES.json').read_text())}
evidence = [str(p.relative_to(fork)).replace('\\', '/') for p in record.rglob('*') if p.is_file()
    and str(p.relative_to(record)).replace('\\', '/') not in raw
    and not any(part in ('negative-content', 'empty-content', '__pycache__') for part in p.parts)]
approved = sorted(set(source+evidence+[str((record/'STAGING_PLAN.json').relative_to(fork)).replace('\\', '/')]))
(record/'STAGING_PLAN.json').write_text(json.dumps(dict(approved_paths=approved,
    excluded_original_input_binaries=True, protected_parent_inventory=True), indent=2)+'\n', newline='\n')
subprocess.run(['git', 'add', '--force', '--', *approved], cwd=fork, check=True)
assert sorted(git('diff', '--cached', '--name-only').splitlines()) == approved
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=fork, check=True)
subprocess.run(['git', 'commit', '-m', 'Correct native sound frame attempts and trace production context services'], cwd=fork, check=True)
subprocess.run(['git', 'push', 'origin', 'main'], cwd=fork, check=True)
assert git('rev-parse', 'HEAD') == git('rev-parse', 'origin/main')
print('PUBLISHED', git('rev-parse', 'HEAD'))
