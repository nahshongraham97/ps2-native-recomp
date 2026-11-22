"""Join the newly inspected R5900 bodies to original PT_LOAD bytes."""
from pathlib import Path
import hashlib
import json
import struct

record = Path(__file__).resolve().parent
fork = record.parents[2]
image = fork.parents[2] / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10'
raw = image.read_bytes()
sha = hashlib.sha256(raw).hexdigest()
assert sha == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
phoff = struct.unpack_from('<I', raw, 28)[0]
ents, count = struct.unpack_from('<HH', raw, 42)
segments = [struct.unpack_from('<8I', raw, phoff+i*ents) for i in range(count)]

def mapped(address, size):
    for kind, offset, vma, _, filesz, _, _, _ in segments:
        if kind == 1 and vma <= address and address+size <= vma+filesz:
            at = offset + address-vma
            return at, raw[at:at+size]
    raise AssertionError(hex(address))

joins = []
for start, end in [(0x12d7e0,0x12d82c),(0x12c030,0x12c128),
                   (0x12c1d0,0x12c2f8),(0x12c300,0x12c434)]:
    listing = json.loads((record / f'{start:08x}.json').read_text())
    items = listing['items']
    assert not listing['has_more']
    assert [int(i['address'],16) for i in items] == list(range(start,end,4))
    at, body = mapped(start,end-start)
    assert body == bytes.fromhex(''.join(i['bytes'] for i in items))
    joins.append(dict(start=hex(start),end_exclusive=hex(end),offset=at,
                      words=len(items),sha256=hashlib.sha256(body).hexdigest(),
                      exact_original_words=body.hex(),missing_words=[]))

# Read actual ELF .reginfo; do not infer gp from a guessed global address.
shoff = struct.unpack_from('<I',raw,32)[0]
shents, shcount = struct.unpack_from('<HH',raw,46)
sections=[struct.unpack_from('<10I',raw,shoff+i*shents) for i in range(shcount)]
reginfo=[s for s in sections if s[1] == 0x70000006]
assert len(reginfo) == 1 and reginfo[0][5] == 24
section = reginfo[0]
gp = struct.unpack_from('<I',raw,section[4]+20)[0]
assert gp == 0x6540f0
result=dict(elf_sha256=sha,language='r5900:LE:32:default',processor='MIPS-R5900',
            joins=joins,reginfo_offset=section[4],reginfo_bytes=raw[section[4]:section[4]+24].hex(),
            gp=hex(gp),reference_table=hex((0x66 << 16)-0x4640),
            reference_count=hex(gp-0x61cc),auxiliary_word=hex(gp-0x61d4),
            current_context=hex(gp-0x61d0),
            scope='source evidence; only12D7E0 preparation proposed, no gameplay claim')
(record / 'SOURCE_JOIN.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print('PASS exact PT_LOAD joins:',sum(j['words'] for j in joins),'words; gp',hex(gp))
