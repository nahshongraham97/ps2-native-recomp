"""Join the bounded startup classification evidence to the original EE image."""
from pathlib import Path
import gzip
import hashlib
import json
import struct

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
image = parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10'
raw = image.read_bytes()
digest = hashlib.sha256(raw).hexdigest()
assert digest == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
assert raw[:6] == b'\x7fELF\x01\x01'
phoff = struct.unpack_from('<I', raw, 28)[0]
phsize, phnum = struct.unpack_from('<HH', raw, 42)
segments = []
for index in range(phnum):
    kind, offset, vma, physical, file_size, memory_size, flags, align = struct.unpack_from('<8I', raw, phoff + index * phsize)
    if kind == 1:
        segments.append(dict(offset=offset, vma=vma, file_size=file_size, memory_size=memory_size))

def extract(address, size):
    selected = [s for s in segments if s['vma'] <= address and address + size <= s['vma'] + s['file_size']]
    assert len(selected) == 1, (hex(address), size)
    segment = selected[0]
    offset = segment['offset'] + address - segment['vma']
    return dict(address=f'{address:08x}', size=size, file_offset=offset,
                bytes=raw[offset:offset+size].hex(), segment=segment)

capture = json.loads((record / 'GHIDRA.json').read_text())
assert capture['program']['sha256'] == digest
assert capture['program']['language_id'] == 'r5900:LE:32:default'
joins = {}
for key, listing in capture['functions'].items():
    assert not listing['has_more'] and listing['total'] == len(listing['items'])
    rows = listing['items']
    begin, end = int(key, 16), int(rows[-1]['address'], 16) + 4
    assert [int(row['address'], 16) for row in rows] == list(range(begin, end, 4))
    evidence = extract(begin, end - begin)
    assert ''.join(row['bytes'] for row in rows).lower() == evidence['bytes']
    joins[key] = dict(**evidence, instruction_count=len(rows), complete_contiguous_span=True)

data = {name: extract(address, size) for name, address, size in [
    ('online_allocation_label', 0x6348c0, 16),
    ('online_init_messages', 0x633100, 272),
    ('base_vtable', 0x64a200, 80),
    ('intermediate_vtable', 0x64a2a0, 80),
    ('concrete_vtable', 0x64a650, 100),
]}
assert bytes.fromhex(data['online_allocation_label']['bytes']).startswith(b'Online\0')
messages = bytes.fromhex(data['online_init_messages']['bytes'])
assert b'cOnline::NetRuntime Init Error:' in messages
assert b'cOnline::NetLobby Init Error:' in messages
assert b'After Online Initialize!' in messages
vtable = bytes.fromhex(data['concrete_vtable']['bytes'])
slots = {f'{slot:02x}': f'{struct.unpack_from("<I", vtable, slot)[0]:08x}' for slot in (0x40, 0x48)}
assert slots == {'40': '00343ee0', '48': '00343e80'}
value = dict(original_image=str(image), sha256=digest, language=capture['program']['language_id'],
             functions=joins, data=data, concrete_slots=slots,
             evidence_level='source-derived classification; no native online implementation',
             unresolved=['Native online owners/services and all required child contracts',
                         'Five configuration word meanings and live producer',
                         'Source native call results/error-policy transformation',
                         'Xbox equivalent not established by this bounded classification'])
output = record / 'SOURCE_JOIN.json'
assert not output.exists()
output.write_text(json.dumps(value, indent=2)+'\n', newline='\n')
orientation = record / 'ORIENTATION.json'
with (record / 'ORIENTATION.json.gz').open('xb') as stream:
    stream.write(gzip.compress(orientation.read_bytes(), mtime=0))
assert gzip.decompress((record / 'ORIENTATION.json.gz').read_bytes()) == orientation.read_bytes()
print(json.dumps(dict(functions=len(joins), instructions=sum(j['instruction_count'] for j in joins.values()),
                      concrete_slots=slots, evidence_level=value['evidence_level'])))
