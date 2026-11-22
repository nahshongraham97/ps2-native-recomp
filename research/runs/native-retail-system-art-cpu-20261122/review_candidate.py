"""Read-only authentication of the committed CPU-data candidate and inputs."""
from pathlib import Path
import gzip
import hashlib
import json
import struct
import subprocess

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
CANDIDATE = 'dbdce79b98c873021cf7575ce7eb6211d5b13118'
PREFIX = 'research/runs/parallel-native-font-zero-20261122/'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def blob(path):
    return subprocess.check_output(['git', 'show', CANDIDATE + ':' + path], cwd=ROOT)


def data(path):
    return json.loads(blob(PREFIX + path))


def batch(paths):
    query = ''.join(CANDIDATE + ':' + p + '\n' for p in paths).encode()
    raw = subprocess.check_output(['git', 'cat-file', '--batch'], input=query, cwd=ROOT)
    position = 0
    result = {}
    for path in paths:
        end = raw.index(b'\n', position)
        header = raw[position:end].split()
        assert len(header) == 3 and header[1] == b'blob', (path, header)
        size = int(header[2])
        position = end + 1
        result[path] = raw[position:position + size]
        position += size
        assert raw[position:position + 1] == b'\n'
        position += 1
    assert position == len(raw)
    return result


delivery = data('DELIVERY.json')
verification = data('validation-03/VERIFICATION.json')
source_pins = data('validation-03/COMMITTED_SOURCE_FILES.json')
raw_pins = data('validation-03/SOURCE_FILES.json')
assert sha(blob(PREFIX + 'DELIVERY.json')) == '45841f074557cda543302920088d8a83721f4ce910864a3b075e3cedb4cbeae8'
assert sha(blob(PREFIX + 'validation-03/VERIFICATION.json')) == delivery['verification_sha256']
assert verification['status'] == 'PASS_PARTIAL_SYSTEM_ART_CPU_DATA_ONLY'
assert len(source_pins) == verification['source_files'] == 867
assert isinstance(delivery['artifact_pins'], dict) and len(delivery['artifact_pins']) == 175
assert isinstance(data('DESCRIPTORS.json'), list)
sources = batch(list(source_pins))
for path, expected in source_pins.items():
    assert sha(sources[path]) == expected, path
    original = (ROOT.parent / 'ps2-native-recomp-font-zero' / path).read_bytes()
    assert sha(original) == raw_pins[path], path
    assert original.replace(b'\r\n', b'\n') == sources[path].replace(b'\r\n', b'\n'), path
artifacts = batch([PREFIX + p for p in delivery['artifact_pins']])
for path, expected in delivery['artifact_pins'].items():
    assert sha(artifacts[PREFIX + path]) == expected, path

elf = (PARENT / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10').read_bytes()
join = data('SOURCE_JOIN.json')
assert sha(elf) == join['ELF_sha256'] == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
phoff = struct.unpack_from('<I', elf, 28)[0]
entry_size, count = struct.unpack_from('<HH', elf, 42)
segments = [struct.unpack_from('<8I', elf, phoff + i * entry_size) for i in range(count)]


def original(address, size):
    segment = next(s for s in segments if s[0] == 1 and s[2] <= address and address + size <= s[2] + s[4])
    offset = segment[1] + address - segment[2]
    return elf[offset:offset + size]


total_words = 0
for path, span in join['complete_listings'].items():
    # FullFontListing fills the exact function span, unlike partial Ghidra bodies.
    path = 'source-full/' + path
    raw = artifacts.get(PREFIX + path)
    if raw is None:
        raw = blob(PREFIX + path)
    if path.endswith('.gz'):
        raw = gzip.decompress(raw)
    assert sha(raw) == span['sha256'], path
    words = {}
    for line in raw.decode().splitlines():
        fields = line.split('\t', 2)
        if len(fields) != 3:
            continue
        address = int(fields[0], 16)
        code = bytes.fromhex(fields[1])
        assert len(code) == 4 and original(address, 4) == code, (path, fields)
        assert 'UNDECODED' not in fields[2] and '??' not in fields[2], fields
        words[address] = code
    assert words, path
    assert sorted(words) == list(range(min(words), max(words) + 4, 4)), path
    total_words += len(words)
assert len(join['complete_listings']) == 22 and total_words == 2980

asset = data('ASSET_SOURCE.json')
asset_bytes = Path(asset['path']).read_bytes()
assert sha(asset_bytes) == asset['sha256'] == verification['original_asset_sha256']
assert len(asset['records']) == 170 and len(asset['variants']) == 11
offsets = [0x800, 0x61800, 0xc2800, 0x123800, 0x184800]
for entry in asset['records']:
    offset = offsets[entry['locale']] + entry['offset']
    assert sha(asset_bytes[offset:offset + entry['length']]) == entry['sha256'], entry

binary_pins = {}
for name, receipt in verification['clients'].items():
    receipt = receipt.get('exact_unchanged02', receipt)
    assert receipt['exit'] == 0
    executable = Path(receipt['argv'][0])
    assert sha(executable.read_bytes()) == receipt['executable_sha256'], name
    validation = 'validation-02' if name in ('all-atlases', 'atlas-existing') else 'validation-03'
    log = artifacts[PREFIX + validation + '/' + name + '.log']
    assert sha(log) == receipt['log_sha256'], name
    binary_pins[name] = dict(path=str(executable), sha256=receipt['executable_sha256'])

result = dict(status='PASS_BOUNDED_COMMITTED_SYSTEM_ART_CPU_CANDIDATE',
              candidate=CANDIDATE, source_blobs=867, artifact_blobs=175,
              complete_ee_bodies=22, original_words=total_words,
              original_asset_sha256=asset['sha256'], original_records=170,
              retained_current_binaries=binary_pins, runtime_review='CPU data only; no Main binding closed.',
              source_loaded=False, converted=False, cache_published=False, gpu_realized=False,
              game_run=False, frames=[], audio=[], acceptance='OPEN')
(RECORD / 'CANDIDATE_REVIEW.json').write_text(json.dumps(result, indent=2) + '\n', newline='\n')
print(json.dumps({k: result[k] for k in ['status', 'source_blobs', 'artifact_blobs', 'complete_ee_bodies', 'original_words']}))
