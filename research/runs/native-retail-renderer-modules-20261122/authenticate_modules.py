"""Read-only authentication of matrix source and latest corrected Core results."""
from pathlib import Path
import gzip
import hashlib
import json
import struct
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
matrix_record = record.parent / 'parallel-renderer-plugin-0402-20261122'
lease_record = record.parent / 'parallel-renderer-core-callout-lifetime-20261122'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def git(*args, input=None):
    return subprocess.check_output(['git', *args], cwd=fork, input=input)

final = json.loads((lease_record / 'final/VERIFICATION.json').read_text())
assert final['status'] == 'PASS_CORE_OPERATION_SESSION_LEASE'
manifest_path = next(p for p in lease_record.rglob('SOURCE_FILES.json') if sha(p.read_bytes()) == final['source_manifest_sha256'])
manifest = json.loads(manifest_path.read_text())
assert len(manifest) == final['source_files'] == 860
commit = '3acc3a23f2b184c8d42995d0409963218e94fe6c'
objects = {}
for row in git('ls-tree', '-r', commit, '--', 'runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt').decode().splitlines():
    fields, path = row.split('\t', 1)
    objects[path] = fields.split()[-1]
assert objects.keys() == manifest.keys()
batch = git('cat-file', '--batch', input=''.join(oid + '\n' for oid in objects.values()).encode())
cursor = 0
newline_pairs = 0
for path, oid in objects.items():
    end = batch.index(b'\n', cursor)
    header = batch[cursor:end].decode().split()
    assert header[0] == oid and header[1] == 'blob'
    size = int(header[2]); data = batch[end + 1:end + 1 + size]
    cursor = end + 2 + size
    if sha(data) != manifest[path]:
        assert b'\r' not in data and sha(data.replace(b'\n', b'\r\n')) == manifest[path], path
        newline_pairs += 1
assert cursor == len(batch)

pins = json.loads((lease_record / 'final/PINS.json').read_text())
for path, digest in pins.items():
    assert sha(Path(path).read_bytes()) == digest, path
clients = {}
for name, log in [('core', lease_record / 'corrected-02/core.log.gz'), ('matrix', lease_record / 'final/matrix.log.gz')]:
    row = final[name]
    assert row['exit'] == 0 and sha(Path(row['argv'][0]).read_bytes()) == row['executable_sha256']
    assert sha(gzip.decompress(log.read_bytes())) == row['log_sha256']
    clients[name] = row
negative = json.loads((lease_record / 'final/negative.json').read_text())
assert negative['exit'] != 0 and len(final['required_names']) == len(set(final['required_names'])) == 61
import re
missing = re.findall(r'undefined symbol: ([^\r\n]+)', gzip.decompress((lease_record / 'final/negative.log.gz').read_bytes()).decode())
assert sorted(missing) == sorted(final['required_names'])

archives = 0
for directory, raw_key, hash_key in [(matrix_record, 'path', 'sha256'), (lease_record, 'raw', 'raw_sha256')]:
    rows = json.loads((directory / 'ARCHIVES.json').read_text())
    assert isinstance(rows, list)
    for row in rows:
        packed = (directory / row['archive']).read_bytes()
        assert sha(packed) == row['archive_sha256'] and sha(gzip.decompress(packed)) == row[hash_key]
        archives += 1

elf = (parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10').read_bytes()
source = json.loads((matrix_record / 'SOURCE_JOIN.json').read_text())
assert sha(elf) == source['elf_sha256']
words = 0
for name, row in source['functions'].items():
    listing = (matrix_record / name).read_bytes()
    assert sha(listing) in (row['sha256'], row['lf_sha256'])
    mapping = row['mapping']; original = bytes.fromhex(mapping['bytes'])
    assert original == elf[mapping['file_offset']:mapping['file_offset'] + len(original)]
    count = 0
    for line in listing.decode().splitlines():
        if not line: continue
        address, code, instruction = line.split('\t', 2)
        relative = int(address, 16) - int(row['begin'], 16)
        assert bytes.fromhex(code) == original[relative:relative + 4]
        count += 1
    assert count == row['words']; words += count
assert words == 779 and len(source['functions']) == 12
xbe = (parent / 'private-inputs/xbox-mka-20261121/default.xbe').read_bytes()
donor = json.loads((matrix_record / 'XBOX_JOIN.json').read_text())
assert sha(xbe) == donor['xbe_sha256'] and len(donor['functions']) == 3
for name, row in donor['functions'].items():
    lines = [line for line in (matrix_record / name).read_text().splitlines() if line]
    assert len(lines) == row['instructions'] == len(row['mapped_instructions'])
    for line, (address, size, offset) in zip(lines, row['mapped_instructions']):
        fields = line.split('\t', 2)
        assert int(fields[0], 16) == address and bytes.fromhex(fields[1]) == xbe[offset:offset + size]

result = dict(status='PASS_MATRIX_SOURCE_AND_LATEST_CORE_LEASE_RESULTS', matrix_candidate='37a3a82e980e38c4702cb1aecd2529dadba06363',
    corrected_candidate=commit, committed_source_files=len(manifest), reconstructed_raw_newline_pairs=newline_pairs,
    authenticated_pins=len(pins), lossless_archives=archives, original_words=words, original_bodies=12, donor_bodies=3,
    latest_clients=clients, historical_required_names=61,
    superseded_matrix_binary='Old37 matrix52547305 has frozen source/hash/log but no retained binary copy; latest3acc fresh matrix supersedes. No old current-binary authentication claimed.',
    scope='Read-only original/source/archive/current-binary/log authentication; no test reruns. Root combined source verification is separate. Math and full engine/game remain OPEN.')
output = record / 'MATRIX_SCOPE_REVIEW.json'
assert not output.exists()
output.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
print('PASS latest860 source blobs,779 original words,3 donor bodies,2 corrected clients;', archives, 'archives')
