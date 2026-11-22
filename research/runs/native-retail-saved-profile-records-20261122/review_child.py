"""Authenticate the corrected committed child value, original words and run."""
from pathlib import Path
import gzip
import hashlib
import json
import struct
import subprocess

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
candidate = '534f9fcf10f8309044c3d38f0a69754a0bcfddf3'
prefix = 'research/runs/parallel-saved-profile-child-20261122/'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def blob(path):
    return subprocess.check_output(['git', 'show', candidate + ':' + path], cwd=fork)


files = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', candidate, '--', prefix], cwd=fork, text=True).splitlines()
query = ''.join(candidate + ':' + path + '\n' for path in files).encode()
packed = subprocess.check_output(['git', 'cat-file', '--batch'], cwd=fork, input=query)
position, artifacts = 0, {}
for path in files:
    end = packed.index(b'\n', position)
    header = packed[position:end].split()
    assert len(header) == 3 and header[1] == b'blob'
    size, position = int(header[2]), end + 1
    artifacts[path[len(prefix):]] = packed[position:position + size]
    position += size
    assert packed[position:position + 1] == b'\n'
    position += 1
assert position == len(packed)
manifest = json.loads(artifacts['MANIFEST.json'])
for path, digest in manifest.items():
    assert sha(artifacts[path]) == digest, path
receipt = json.loads(artifacts['validation02/VERIFICATION.json'])
assert receipt['status'] == 'PASS_TYPED_CHILD_ZERO_BSS_AND_SOURCE_RESET_DEFAULTS'
assert receipt['compile_exit'] == receipt['run_exit'] == 0
assert len(receipt['source_sha256']) == 9
for path, digest in receipt['source_sha256'].items():
    raw = gzip.decompress(artifacts['validation02/source-snapshot/' + path.replace('/', '__') + '.gz'])
    assert sha(raw) == digest, path
    assert raw.replace(b'\r\n', b'\n') == blob(path).replace(b'\r\n', b'\n'), path
binary = gzip.decompress(artifacts['validation02/child.exe.gz'])
assert sha(binary) == receipt['binary_sha256']
raw_binary_path = Path(receipt['run_argv'][0])
raw_binary_present = raw_binary_path.is_file()
if raw_binary_present:
    assert sha(raw_binary_path.read_bytes()) == receipt['binary_sha256']
assert sha(artifacts['validation02/run.log']) == receipt['run_log_sha256']
assert sha(artifacts['validation02/compile.log']) == receipt['compile_log_sha256']
assert sha(Path(receipt['compiler']['path']).read_bytes()) == receipt['compiler']['sha256']
elf = (parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10').read_bytes()
join = json.loads(artifacts['SOURCE_JOIN.json'])
assert sha(elf) == join['ELF_sha256'] == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
phoff = struct.unpack_from('<I', elf, 28)[0]
entry_size, count = struct.unpack_from('<HH', elf, 42)
segments = [struct.unpack_from('<8I', elf, phoff + i * entry_size) for i in range(count)]


def original(address, size):
    segment = next(s for s in segments if s[0] == 1 and s[2] <= address and address + size <= s[2] + s[4])
    offset = segment[1] + address - segment[2]
    return elf[offset:offset + size]


listed, omitted = 0, {}
for path, span in join['functions'].items():
    listing = artifacts[path]
    assert sha(listing) == span['listing_sha256']
    begin, end = int(span['begin'], 16), int(span['end_exclusive'], 16)
    assert sha(original(begin, end - begin)) == span['span_sha256']
    words = {}
    for line in listing.decode().splitlines():
        address, code, instruction = line.split('\t', 2)
        address, code = int(address, 16), bytes.fromhex(code)
        assert len(code) == 4 and original(address, 4) == code
        assert 'UNDECODED' not in instruction
        words[address] = code
    missing = {hex(a): original(a, 4).hex() for a in range(begin, end, 4) if a not in words}
    assert missing == {key: item['bytes'] for key, item in span['omitted'].items()}, path
    for key, value in missing.items():
        if value != '00000000':
            omitted[key] = value
            assert span['omitted'][key]['decode']
    assert len(words) == span['listed_words']
    listed += len(words)
assert len(join['functions']) == 60 and listed == 7456
assert set(omitted) == {'0x245e84', '0x4eea2c'}
result = dict(status='PASS_CORRECTED_CHILD_VALUE_SOURCE_AND_COMPONENT', candidate=candidate,
              artifact_blobs=len(artifacts), measured_source_files=9, source_views=60,
              listed_words=listed, decoded_nonzero_omissions=omitted,
              binary_sha256=receipt['binary_sha256'], raw_original_binary_present=raw_binary_present,
              exact_executable_retained_compressed=True,
              source_correction='245E84 is an unreachable duplicateLW; the candidate receipt role wording is superseded by root SOURCE_JOIN.json control-flow proof. The original word/actual LW decode remain correct.',
              auxiliary_path_limitation='Older readOnly private-project commands reused another private session settings/cache/temp; no master project used. Kept as a limitation, not exclusive isolation proof.',
              source_defaults_only=True, loaded_import='REQUIRED_REFUSAL',
              parent_main_binding=False, game_run=False, frames=[], audio=[], acceptance='OPEN')
(record / 'CHILD_REVIEW.json').write_text(json.dumps(result, indent=2) + '\n', newline='\n')
print(json.dumps({key: result[key] for key in ['status', 'artifact_blobs', 'source_views', 'listed_words']}))
