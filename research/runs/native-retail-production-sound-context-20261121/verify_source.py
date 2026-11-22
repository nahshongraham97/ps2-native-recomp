"""Join the inspected context dependencies to original ELF/XBE file bytes."""
from pathlib import Path
import hashlib
import json
import struct

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
assets = parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
images = {'EE': (assets / 'SLUS_214.10').read_bytes(),
          'MSL': (assets / 'modules/msl.irx').read_bytes()}
expected = {'EE': 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f',
            'MSL': 'f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7'}
for name, data in images.items():
    assert hashlib.sha256(data).hexdigest() == expected[name]

def elf_bytes(name, address, size):
    data = images[name]
    phoff = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    for i in range(count):
        kind, offset, vma, _, filesz, _, _, _ = struct.unpack_from('<8I', data, phoff+i*stride)
        if kind == 1 and vma <= address and address+size <= vma+filesz:
            at = offset+address-vma
            return data[at:at+size], at
    raise AssertionError((name, hex(address), size))

module = {}
exports = []
for path in sorted(record.glob('source-*/LISTING.json')):
    document = json.loads(path.read_text())
    assert document['language'] == 'MIPS:LE:32:default'
    for row in document['instructions']:
        address = int(row['original_vma'], 16)
        raw, _ = elf_bytes('MSL', address, 4)
        assert int.from_bytes(raw, 'little') == int(row['word'], 16)
        if row.get('instruction'):
            module[address] = row
    exports.append(dict(path=str(path.relative_to(fork)),
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), rows=len(document['instructions'])))

coverage = []
for begin, end in [(0x334, 0x3ac), (0x3784, 0x38c8), (0x5244, 0x5318),
                   (0x4ccc, 0x4d14), (0x2c6c, 0x2cb4)]:
    assert all(at in module for at in range(begin, end, 4)), (hex(begin), hex(end))
    coverage.append(dict(image='MSL', begin=hex(begin), end_exclusive=hex(end),
        full_word_coverage=True, words=(end-begin)//4))

ee = {}
for path in sorted(record.glob('EE-*.json')):
    document = json.loads(path.read_text())
    assert document['has_more'] is False
    rows = document['items']
    addresses = []
    for row in rows:
        address = int(row['address'], 16)
        raw, _ = elf_bytes('EE', address, 4)
        assert raw.hex() == row['bytes']
        addresses.append(address)
        ee[address] = row
    assert addresses == list(range(min(addresses), max(addresses)+4, 4))
    coverage.append(dict(image='EE', begin=hex(min(addresses)), end_exclusive=hex(max(addresses)+4),
        full_word_coverage=True, words=len(rows), language='r5900:LE:32:default'))

# 3152E0 BEQL's taken-only load obtains the counter directly when admission
# returns0; the admitted path reaches315314 after reservation/send/poll.
words = {at: int.from_bytes(bytes.fromhex(row['bytes']), 'little') for at, row in ee.items()}
assert words[0x3152e0] == 0x5040000d
assert words[0x3152e4] == words[0x315314] == 0x8f83abb8
assert words[0x3152f8] == 0x50400004 and words[0x3152fc] == 0x24100008
assert words[0x31531c] == 0x24630001 and words[0x315320] == 0xaf83abb8
assert words[0x314f38] == 0xaf80abb8
assert int(module[0x2c6c]['word'], 16) == 0x24030063
assert int(module[0x2c88]['word'], 16) == 0x0c000de1  # JAL3784, never46F8.

xbe_path = parent / 'private-inputs/xbox-mka-20261121/default.xbe'
xbe = xbe_path.read_bytes()
assert hashlib.sha256(xbe).hexdigest() == '78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393'
base = struct.unpack_from('<I', xbe, 0x104)[0]
count, table = struct.unpack_from('<II', xbe, 0x11c)
sections = [struct.unpack_from('<9I20s', xbe, table-base+i*56) for i in range(count)]

def xbe_bytes(address, size):
    matches = [s for s in sections if s[1] <= address and address+size <= s[1]+s[4]]
    assert len(matches) == 1
    s = matches[0]
    at = s[3]+address-s[1]
    return xbe[at:at+size]

xbox = []
for path in sorted(record.glob('XBOX-CONTEXT-*.json')):
    document = json.loads(path.read_text())
    counts = {}
    for function in document['function_candidates']:
        for row in function['instructions']:
            raw = bytes.fromhex(row['bytes_hex'])
            assert xbe_bytes(int(row['address'], 16), len(raw)) == raw
        counts[function['entry']] = len(function['instructions'])
    xbox.append(dict(path=str(path.relative_to(fork)),
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(), function_instruction_counts=counts,
        behavioral_equivalence=False))

result = dict(status='PASS_ORIGINAL_CONTEXT_DEPENDENCY_BYTE_JOIN', input_sha256=expected,
    module_exports=exports, full_coverage=coverage, xbox_exports=xbox,
    source_counter_paths=['admission0:3152E0->315318', 'reservation0:3152F8->31530C->315314',
                          'submitted99:315300->315314'],
    required_remaining=['production316320', 'status99/314F30 receiver', 'full5244/46F8 scheduling',
                        'native output configuration', 'envelope policy', 'play flags9'],
    game_run=False, physical_endpoints_opened=False, frames=[], audio=[])
output = record / 'SOURCE_JOIN.json'
assert not output.exists()
output.write_text(json.dumps(result, indent=2)+'\n', newline='\n')
print('PASS original EE/MSL/Xbox context dependency bytes; full bounded coverage',
      sum(r['words'] for r in coverage))
