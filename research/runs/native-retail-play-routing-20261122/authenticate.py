"""Authenticate clean measured bytes against each committed source blob."""
from pathlib import Path
import hashlib
import json
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
clean = fork.parent / 'clean-route-22'
commit = json.loads((record / 'CLEAN_CONFIGURE.json').read_text())['source_commit']
authored = json.loads((record / 'validation-03/SOURCE_FILES.json').read_text())
measured = json.loads((record / 'clean-validation-01/SOURCE_FILES.json').read_text())
receipt_path = record / 'clean-validation-01/VERIFICATION.json'
receipt = json.loads(receipt_path.read_text())
assert authored.keys() == measured.keys()
assert receipt['clean_source'] and receipt['source_commit'] == commit
assert receipt['status'] == 'PASS_NATIVE_RETAIL_DIRECT_WORKER_COMPONENTS'
assert len(receipt['clients']) == 25 and len(receipt['remaining_undefined_symbols']) == 37


def git(root, *args):
    return subprocess.check_output(['git', '-c', 'core.longpaths=true', *args], cwd=root, text=True).strip()


def digest(data):
    return hashlib.sha256(data).hexdigest()


scopes = ['runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt']
blobs = {}
for line in git(fork, 'ls-tree', '-r', commit, '--', *scopes).splitlines():
    metadata, path = line.split('\t', 1)
    blobs[path] = metadata.split()[-1]
assert authored.keys() == blobs.keys()
differences = {}
for path, expected in authored.items():
    canonical_bytes, clean_bytes = (fork / path).read_bytes(), (clean / path).read_bytes()
    assert digest(canonical_bytes) == expected and digest(clean_bytes) == measured[path]
    oid = hashlib.sha1(b'blob ' + str(len(clean_bytes)).encode() + b'\0' + clean_bytes).hexdigest()
    assert oid == blobs[path], path
    if canonical_bytes != clean_bytes:
        assert b'\0' not in canonical_bytes
        assert canonical_bytes.replace(b'\r\n', b'\n') == clean_bytes, path
        differences[path] = dict(canonical_sha256=expected, clean_sha256=measured[path], committed_blob=oid)

changed = json.loads((record / 'STAGED_SOURCE.json').read_text())['approved']
changed = [p for p in changed if p in authored]
assert all(authored[p] == measured[p] for p in changed)
trees = {scope: git(fork, 'rev-parse', commit + ':' + scope) for scope in scopes}
assert all(git(clean, 'rev-parse', 'HEAD:' + scope) == oid for scope, oid in trees.items())
assert git(fork, 'rev-parse', 'origin/main') == commit
result = dict(status='PASS_IDENTICAL_COMMITTED_SOURCE_BLOBS', clean_commit=commit,
    canonical_manifest_sha256=digest((record / 'validation-03/SOURCE_FILES.json').read_bytes()),
    clean_manifest_sha256=digest((record / 'clean-validation-01/SOURCE_FILES.json').read_bytes()),
    checked_source_files=len(authored), raw_newline_differences=differences,
    changed_source_bytes_identical=True, source_trees=trees)
publication = dict(clean_source_commit=commit, published_source_commit=commit,
    published_ref='origin/main', source_trees=trees, clean_receipt='clean-validation-01/VERIFICATION.json',
    clean_receipt_sha256=digest(receipt_path.read_bytes()),
    source_blob_identity_receipt='CROSS_CHECKOUT_IDENTITY.json', game_run=False,
    physical_endpoints_opened=False, frames=[], audio=[], acceptance='OPEN')
for name, value in [('CROSS_CHECKOUT_IDENTITY.json', result), ('SOURCE_PUBLICATION.json', publication)]:
    path = record / name
    assert not path.exists()
    path.write_text(json.dumps(value, indent=2) + '\n', newline='\n')
print('PASS', len(authored), 'source blobs;', len(differences), 'unchanged raw newline pairs retained')
