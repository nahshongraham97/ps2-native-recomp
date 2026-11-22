"""Freeze expected curve data from original MSL data/constructor words."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess

record=Path(__file__).resolve().parent
fork=record.parents[2]
parent=fork.parents[2]
module=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/modules/msl.irx'
ee=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10'
msl=module.read_bytes()
retail=ee.read_bytes()
assert hashlib.sha256(msl).hexdigest()=='f62dfbcedd39bc92cb8b9c02bb685b956a10357f9181d7f8c2801d8eaff07af7'
assert hashlib.sha256(retail).hexdigest()=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'

def mapped(image,address,size):
    offset=struct.unpack_from('<I',image,28)[0]
    stride,count=struct.unpack_from('<HH',image,42)
    for i in range(count):
        kind,at,vma,_,filesz,_,_,_=struct.unpack_from('<8I',image,offset+i*stride)
        if kind==1 and vma<=address and address+size<=vma+filesz:
            start=at+address-vma
            return image[start:start+size],start
    raise AssertionError(hex(address))

primary,po=mapped(retail,0x59b5f8,12)
secondary,so=mapped(retail,0x59b610,16)
assert struct.unpack('<3I',primary)==(12,1,3)
assert struct.unpack('<4I',secondary)==(16,0,0x00400000,0x5f54a0)
assert struct.unpack_from('<I',primary,4)[0]|struct.unpack_from('<I',secondary,4)[0]==1

listing=json.loads((record.parent/'native-retail-sound-context-20261121/source-01/LISTING.json').read_text())
rows=[i for i in listing['instructions'] if 0x57ac<=int(i['original_vma'],16)<0x583c]
for i in rows:
    data,_=mapped(msl,int(i['original_vma'],16),4)
    assert struct.unpack('<I',data)[0]==int(i['word'],16) and i['instruction'] is not None
words={int(i['original_vma'],16):int(i['word'],16) for i in rows}
for at,word in {0x57b8:0x00831825,0x57d0:0x30420001,0x57d4:0x10400019,
                0x57e4:0x240500c7,0x57ec:0x00061380,0x57f0:0x00461023,
                0x57f4:0x0045001a,0x57f8:0x00a61023,0x5800:0x00002012,
                0x580c:0xa4440000,0x5810:0x28c200c8,0x5814:0x1440fff5,
                0x581c:0x24050063,0x5828:0x00052040,0x5830:0x24a5ffff,
                0x5834:0x04a1fffc,0x5838:0xa0440000}.items():assert words[at]==word

gain,go=mapped(msl,0x102ac,400)
pan,pa=mapped(msl,0x1043c,100)
initial_gain=list(struct.unpack('<200H',gain))
initial_pan=list(pan)
baseline=subprocess.check_output(['git','show','19f3062159f3c7b813b337ba3b7161af7185028e:runtime/src/native/audio_controls.cpp'],cwd=fork,text=True)
for symbol,values in [('gain_curve',initial_gain),('pan_attenuation',initial_pan)]:
    match=re.search(symbol+r'\[\d+\]\s*=\s*\{([^}]*)\}',baseline,re.S)
    assert match and list(map(int,re.findall(r'\d+',match[1])))==values

# Exact original positive signed-DIV domain: (i<<14)-i fits32 bits.
# SH uses reversed index199-i; BGEZ's ordinary delay-slot SB also writes index0.
linear_gain=[None]*200
for i in range(200):linear_gain[199-i]=((i<<14)-i)//199
linear_pan=[2*i for i in range(100)]
assert (linear_gain[0],linear_gain[99],linear_gain[199],linear_pan[99])==(16383,8232,0,198)
assert initial_gain[99]==54
data=dict(initial_gain=initial_gain,initial_pan=initial_pan,linear_gain=linear_gain,linear_pan=linear_pan)
fixture=fork/'tests/fixtures/retail_sound_curve_data.hpp'
fixture.parent.mkdir(exist_ok=True)
lines=['#pragma once','#include <array>','#include <cstdint>','namespace sound_curve_fixture {']
for name,values in data.items():
    kind='std::uint8_t' if name.endswith('pan') else 'std::uint16_t'
    lines.append(f'inline constexpr std::array<{kind},{len(values)}> {name}{{')
    for i in range(0,len(values),10):lines.append('    '+','.join(map(str,values[i:i+10]))+',')
    lines.append('};')
lines.append('}')
fixture.write_text('\n'.join(lines)+'\n',newline='\n')
result=dict(module_sha256=hashlib.sha256(msl).hexdigest(),ee_sha256=hashlib.sha256(retail).hexdigest(),
            language=listing['language'],primary_file_offset=po,primary_bytes=primary.hex(),
            secondary_file_offset=so,secondary_bytes=secondary.hex(),combined_curve_flags=1,
            gain_file_offset=go,initial_gain_bytes=gain.hex(),pan_file_offset=pa,initial_pan_bytes=pan.hex(),
            constructor_words=rows,expected_tables=data,fixture_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),
            scope='Original source/data expectations; no runtime instruction/device interpreter or audio/game run')
(record/'SOURCE.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print('PASS original curve inputs/words; initial99=54, startup99=8232; expected600 entries frozen')
