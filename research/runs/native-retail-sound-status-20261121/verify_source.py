"""Join the new status dependencies to the unchanged original retail images."""
from pathlib import Path
import hashlib
import json
import struct

record = Path(__file__).resolve().parent
parent = record.parents[5]
image = parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/modules/msl.irx'
data = image.read_bytes()
sha = hashlib.sha256(data).hexdigest()
assert sha == 'f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7'
phoff = struct.unpack_from('<I', data, 28)[0]
stride, count = struct.unpack_from('<HH', data, 42)
segments = [struct.unpack_from('<8I', data, phoff + i * stride) for i in range(count)]


def word(at):
    candidates = [s for s in segments if s[0] == 1 and s[2] <= at and at + 4 <= s[2] + s[4]]
    segment, = candidates
    return struct.unpack_from('<I', data, segment[1] + at - segment[2])[0]


words = {}
exports = []
for path in sorted(record.glob('source-*/LISTING.json')):
    document = json.loads(path.read_text())
    assert document['language'] == 'MIPS:LE:32:default'
    for row in document['instructions']:
        at = int(row['original_vma'], 16)
        assert word(at) == int(row['word'], 16)
        if row['instruction']:
            if at in words:
                assert words[at] == row
            words[at] = row
    exports.append(dict(path=path.relative_to(record).as_posix(), sha256=hashlib.sha256(path.read_bytes()).hexdigest()))

coverage = []
for lo, hi in [(0x7148, 0x7178), (0xf318, 0xf344), (0xcd88, 0xcf04),
               (0xcf04, 0xd018), (0x6d8c, 0x6fdc), (0x6fdc, 0x7148)]:
    assert all(at in words for at in range(lo, hi, 4)), (hex(lo), hex(hi))
    coverage.append(dict(begin=hex(lo), end_exclusive=hex(hi), full_word_coverage=True, words=(hi-lo)//4))
assert word(0x714c) == 0x8c224cbc and word(0x715c) == 0x8c224cb8
assert word(0x716c) == 0x8c224cb4
assert word(0xf318) == 0x94a2000c and word(0xf32c) == 0x94a2000e
assert word(0xf320) == 0x00021400 and word(0xf324) == 0x00021482
assert word(0xced0) == 0xac6203b8 and word(0xcfe4) == 0xac6203b8

xbe = (parent / 'private-inputs/xbox-mka-20261121/default.xbe').read_bytes()
xbe_sha = hashlib.sha256(xbe).hexdigest()
assert xbe_sha == '78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393'
base = struct.unpack_from('<I', xbe, 0x104)[0]
count, table = struct.unpack_from('<II', xbe, 0x11c)
sections = [struct.unpack_from('<9I20s', xbe, table-base+i*56) for i in range(count)]
xbox_path = record / 'XBOX-CONTEXT-02.json'
document = json.loads(xbox_path.read_text())
xbox_counts = {}
for function in document['function_candidates']:
    for row in function['instructions']:
        at, raw = int(row['address'], 16), bytes.fromhex(row['bytes_hex'])
        section, = [s for s in sections if s[1] <= at and at + len(raw) <= s[1] + s[4]]
        file_offset = section[3] + at - section[1]
        assert xbe[file_offset:file_offset+len(raw)] == raw
    xbox_counts[function['entry']] = len(function['instructions'])
assert '0037bc20' in xbox_counts
result = dict(status='PASS_ORIGINAL_STATUS_DEPENDENCY_BYTE_JOIN', original_msl_sha256=sha,
    original_xbe_sha256=xbe_sha, msl_exports=exports, full_coverage=coverage,
    xbox_export=xbox_path.name, xbox_instruction_counts=xbox_counts,
    behavioral_equivalence=False, native_status_implemented=False,
    prior_full_status_words='../native-retail-production-sound-context-20261121/SOURCE_JOIN.json',
    unresolved=['Native mapping/consumers of source arena and heap counters',
                'Typed active-play snapshot and actual source live-reference retirement',
                'Complete source control/queued-role/envelope/flags9 behavior'],
    runtime_changed=False, game_run=False, physical_endpoints_opened=False, frames=[], audio=[], acceptance='OPEN')
path = record / 'SOURCE_JOIN.json'
assert not path.exists()
path.write_text(json.dumps(result, indent=2)+'\n', newline='\n')
print(result['status'], sum(row['words'] for row in coverage), 'fully covered bounded MSL words')
