"""Authenticate original profile/role reset words and existing shared aliases."""
from pathlib import Path
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
phoff = struct.unpack_from('<I', raw, 28)[0]
entsize, count = struct.unpack_from('<HH', raw, 42)
segments = []
gp = None
for index in range(count):
    kind, offset, address, _, size, memory, flags, align = struct.unpack_from('<8I', raw, phoff + index * entsize)
    if kind == 1:
        segments.append(dict(offset=offset, address=address, file_size=size, memory_size=memory))
    if kind == 0x70000000:
        gp = struct.unpack_from('<I', raw, offset + 20)[0]
shoff = struct.unpack_from('<I', raw, 32)[0]
shentsize, shcount = struct.unpack_from('<HH', raw, 46)
for index in range(shcount):
    section = struct.unpack_from('<10I', raw, shoff + index * shentsize)
    if section[1] == 0x70000006:
        assert section[5] == 24
        gp = struct.unpack_from('<I', raw, section[4] + 20)[0]
assert gp == 0x6540f0

def original(address):
    segment = next(s for s in segments if s['address'] <= address and address + 4 <= s['address'] + s['file_size'])
    offset = segment['offset'] + address - segment['address']
    return raw[offset:offset + 4]

functions = {}
for path in (record / 'source-01').glob('*.asm'):
    words = {}
    for line in path.read_text().splitlines():
        address, code, instruction = line.split('\t', 2)
        address, code = int(address, 16), bytes.fromhex(code)
        assert len(code) == 4 and original(address) == code
        assert 'UNDECODED' not in instruction
        words[address] = code
    omitted = {f'{a:08x}': original(a).hex() for a in range(min(words), max(words) + 4, 4) if a not in words}
    functions[path.stem] = dict(begin=min(words), end=max(words) + 4, listed_words=len(words), omitted=omitted,
                                listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
assert functions['002817b0']['listed_words'] == 30 and not functions['002817b0']['omitted']
assert functions['00281830']['listed_words'] == 14 and not functions['00281830']['omitted']
births = {}
for player, displacement in [(0, -0x565c), (1, -0x5660)]:
    address = gp + displacement
    segment = next(s for s in segments if s['address'] + s['file_size'] <= address and address + 4 <= s['address'] + s['memory_size'])
    births[str(player)] = dict(address=address, displacement=displacement, loader_zero=True)
receipt = dict(elf_sha256=digest, language='r5900:LE:32:default', gp=gp, segments=segments,
               functions=functions, listed_words=sum(f['listed_words'] for f in functions.values()),
               implemented_words=44, mapping_word_births=births,
               scope='Complete2817B0/281830 source and existing mapping-word aliases; support functions are producer/consumer evidence, not claimed ported.')
path = record / 'SOURCE_JOIN.json'
assert not path.exists()
path.write_text(json.dumps(receipt, indent=2) + '\n', newline='\n')
print(json.dumps(receipt, indent=2))
