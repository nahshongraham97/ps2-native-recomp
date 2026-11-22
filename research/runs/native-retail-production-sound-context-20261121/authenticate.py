"""Retain measured LF/CRLF hashes and authenticate the actual committed blobs."""
from pathlib import Path
import hashlib
import json
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
clean = fork.parent/'ps2-native-recomp-clean-context-21'
authored = json.loads((record/'validation-01/SOURCE_FILES.json').read_text())
measured = json.loads((record/'clean-validation-02/SOURCE_FILES.json').read_text())
assert authored.keys() == measured.keys()
receipt = json.loads((record/'clean-validation-02/VERIFICATION.json').read_text())
commit = 'c0e5d1cf155c8f88660e2afba77179118f375ee6'
assert receipt['clean_source'] and receipt['source_commit'] == commit
assert len(receipt['clients']) == 4 and len(receipt['remaining_undefined_symbols']) == 39

def git(root, *args):
    return subprocess.check_output(['git', '-c', 'core.longpaths=true', *args], cwd=root, text=True).strip()

def digest(data):
    return hashlib.sha256(data).hexdigest()

scopes = ['runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt']
blobs = {}
for line in git(fork, 'ls-tree', '-r', commit, '--', *scopes).splitlines():
    metadata, path = line.split('\t', 1)
    blobs[path] = metadata.split()[-1]
different = {}
for path, expected in authored.items():
    canonical_bytes, clean_bytes = (fork/path).read_bytes(), (clean/path).read_bytes()
    assert digest(canonical_bytes) == expected and digest(clean_bytes) == measured[path]
    oid = hashlib.sha1(b'blob '+str(len(clean_bytes)).encode()+b'\0'+clean_bytes).hexdigest()
    assert oid == blobs[path], path
    if canonical_bytes != clean_bytes:
        assert b'\0' not in canonical_bytes
        assert canonical_bytes.replace(b'\r\n', b'\n') == clean_bytes, path
        different[path] = dict(canonical_sha256=expected, clean_sha256=measured[path], committed_blob=oid)
trees = {scope: git(fork, 'rev-parse', commit+':'+scope) for scope in scopes}
assert all(git(clean, 'rev-parse', 'HEAD:'+scope) == oid for scope, oid in trees.items())
changed = ['runtime/include/mka/retail_current_sound_required.hpp',
           'runtime/src/native/retail_sound_bank_main.cpp', 'tests/test_retail_reset_sound.cpp']
assert all(authored[p] == measured[p] for p in changed)
result = dict(status='PASS_IDENTICAL_COMMITTED_SOURCE_BLOBS', clean_commit=commit,
    canonical_manifest_sha256=digest((record/'validation-01/SOURCE_FILES.json').read_bytes()),
    clean_manifest_sha256=digest((record/'clean-validation-02/SOURCE_FILES.json').read_bytes()),
    checked_source_files=len(authored), raw_newline_differences=different,
    changed_source_bytes_identical=True, source_trees=trees)
publication = dict(clean_source_commit=commit, published_source_commit=commit,
    published_ref='origin/main', source_trees=trees,
    clean_receipt='clean-validation-02/VERIFICATION.json',
    clean_receipt_sha256=digest((record/'clean-validation-02/VERIFICATION.json').read_bytes()),
    source_blob_identity_receipt='CROSS_CHECKOUT_IDENTITY.json', game_run=False,
    physical_endpoints_opened=False, frames=[], audio=[], acceptance='OPEN')
for name, value in [('CROSS_CHECKOUT_IDENTITY.json', result), ('SOURCE_PUBLICATION.json', publication),
    ('IDENTITY_FAILURE.json', dict(reason='Raw-hash equality incorrectly included unchanged checkout LF/CRLF differences',
         changed_implementation_bytes=False, measured_records_rewritten=False,
         differing_unchanged_source_files=len(different), component_results='four clean clients passed separately'))]:
    output = record/name
    assert not output.exists()
    output.write_text(json.dumps(value, indent=2)+'\n', newline='\n')
print('PASS', len(authored), 'committed source blobs;', len(different), 'distinct unchanged LF/CRLF pairs retained')
