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
for begin,end in [(0x46f8,0x47d8),(0x5244,0x5318),(0xaa88,0xaab8),(0x3db0,0x3fcc),(0x4180,0x4220),(0x4220,0x45b0),(0x45b0,0x46a8),(0x4c50,0x4ccc)]:
    assert all(at in module for at in range(begin,end,4)), (hex(begin),hex(end))
    coverage.append(dict(begin=hex(begin),end_exclusive=hex(end),words=(end-begin)//4,full_word_coverage=True))
assert int(module[0x178]['word'],16)==0x03e00008
assert int(module[0x17c]['word'],16)==0x24000021
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

result=dict(status='PASS_ORIGINAL_SOUND_WORKER_BYTE_JOIN', input_sha256=expected,
    module_exports=exports, full_coverage=coverage, xbox_exports=xbox,
    windows_references=['https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/ne-mmdeviceapi-erole',
      'https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getdeviceperiod'],
    import_reference='https://ps2dev.github.io/ps2sdk/thbase_8h_source.html',
    unresolved=['complete source worker passes and producers','source envelope policy','flags9','full game/output acceptance'],
    game_run=False,physical_endpoints_opened=False,frames=[],audio=[])
output=record/'SOURCE_JOIN.json'
assert not output.exists()
output.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print(result['status'],sum(r['words'] for r in coverage),'full bounded words')
