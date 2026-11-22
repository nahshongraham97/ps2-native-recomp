"""Authenticate the measured precommit source against its published tree."""
from pathlib import Path
import hashlib
import json
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
def git(*argv):
    return subprocess.check_output(['git', *argv], cwd=fork).decode().strip()
def digest(data):
    return hashlib.sha256(data).hexdigest()
assert not git('status', '--porcelain', '-uno')
commit = git('rev-parse', 'HEAD')
assert git('ls-remote', 'origin', 'refs/heads/main').split()[0] == commit
measured = json.loads((record/'validation-01/SOURCE_FILES.json').read_text())
tree = git('ls-tree', '-r', '--format=%(objectname) %(path)', commit,
           '--', 'runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt')
objects = {line.split(' ', 1)[1]: line.split(' ', 1)[0] for line in tree.splitlines()}
assert set(measured) == set(objects)
batch = subprocess.run(['git', 'cat-file', '--batch'], cwd=fork,
                       input=('\n'.join(objects[name] for name in measured)+'\n').encode(),
                       capture_output=True, check=True)
offset = 0
rows = {}
for name, raw_digest in measured.items():
    end = batch.stdout.index(b'\n', offset)
    oid, kind, size = batch.stdout[offset:end].decode().split()
    assert oid == objects[name] and kind == 'blob'
    size = int(size)
    blob = batch.stdout[end+1:end+1+size]
    offset = end+size+2
    raw = (fork/name).read_bytes()
    assert digest(raw) == raw_digest
    if raw == blob:
        identity = 'byte-identical'
    else:
        assert raw.replace(b'\r\n', b'\n') == blob, name
        identity = 'canonical-CRLF-to-committed-LF-only'
    rows[name] = dict(canonical_sha256=raw_digest, committed_sha256=digest(blob),
                      git_blob=oid, identity=identity)
assert offset == len(batch.stdout)
identity = dict(published_source_commit=commit, source_files=rows,
                note='No runtime bytes changed between the recorded compile and publication')
output = record/'PUBLISHED_SOURCE_IDENTITY.json'
assert not output.exists()
output.write_text(json.dumps(identity, indent=2)+'\n', newline='\n')
receipt = dict(published_source_commit=commit, published_ref='origin/main',
               source_identity_receipt=output.name, source_identity_sha256=digest(output.read_bytes()),
               measured_receipt='validation-01/VERIFICATION.json',
               measured_receipt_sha256=digest((record/'validation-01/VERIFICATION.json').read_bytes()),
               measured_source_files=len(rows), byte_identical=sum(v['identity']=='byte-identical' for v in rows.values()),
               newline_only_pairs=sum(v['identity']!='byte-identical' for v in rows.values()),
               observed_compile_was_precommit=True, fresh_clean_build=False,
               game_run=False, physical_endpoints_opened=False, frames=[], audio=[], acceptance='OPEN')
output = record/'SOURCE_PUBLICATION.json'
assert not output.exists()
output.write_text(json.dumps(receipt, indent=2)+'\n', newline='\n')
print(json.dumps({key: value for key, value in receipt.items() if key in (
    'published_source_commit', 'measured_source_files', 'byte_identical', 'newline_only_pairs', 'fresh_clean_build')}))
