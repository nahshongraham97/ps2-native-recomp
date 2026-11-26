"""Compile structured MPG payloads directly from the pinned retail ELF.

This extends the compiled VU corpus without requiring gameplay captures. It
does not prove all VU upload roots, indirect targets or runtime patch variants.
Only fresh owned output is written; no runtime or protected input is changed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import shutil
import re
from expand_r192_vu_sources import functions, merge

ROOT = Path(__file__).resolve().parents[2]
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'

def require(ok, reason):
    if not ok:
        raise ValueError(reason)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def payload(data, at):
    require(at % 16 == 0 and at + 16 <= len(data), 'DMA_header_bounds')
    tag, address, first, second = struct.unpack_from('<IIII', data, at)
    require(tag & 0xffff0000 in (0x60000000, 0x70000000) and address == first == 0,
            'unsupported_DMA_tag')
    require((second >> 24) & 127 == 0x4a, 'initial_MPG_required')
    end = at + 16 + (tag & 65535) * 16
    require(end <= len(data), 'DMA_payload_truncated')
    cursor, image, spans, commands = at+8, bytearray(16384), [], []
    while cursor < end:
        require(cursor+4 <= end, 'VIF_word_truncated')
        command = struct.unpack_from('<I', data, cursor)[0]
        cursor += 4
        if command == 0:
            continue
        require((command >> 24) & 127 == 0x4a, 'unsupported_VIF_command')
        count = (command >> 16) & 255 or 256
        destination, size = (command & 65535)*8, count*8
        require(destination+size <= len(image), 'VU_code_extent')
        require(cursor+size <= end, 'MPG_payload_truncated')
        image[destination:destination+size] = data[cursor:cursor+size]
        spans.append({'file_offset':cursor,'VU_offset':destination,'size':size})
        commands.append(command)
        cursor += size
    require(bool(spans), 'empty_MPG_payload')
    return image, {'file_offset':at,'file_end':end,'spans':spans,'commands':commands}

def self_test():
    base = struct.pack('<IIIIIIII', 0x60000001,0,0,0x4a020000,0x8000033c,0x400002ff,0x8000033c,0x2ff)
    image, _ = payload(base,0)
    require(len(image)==16384 and image[:16]==base[16:], 'positive_parse')
    plants = {'truncated_DMA':base[:-1],
              'MPG_count_exceeds_DMA':base[:12]+struct.pack('<I',0x4a030000)+base[16:],
              'VU_destination_overflow':base[:12]+struct.pack('<I',0x4a0207ff)+base[16:],
              'unsupported_command':base[:12]+struct.pack('<I',0x50000000)+base[16:]}
    results=[]
    for name, raw in plants.items():
        try:
            payload(raw,0)
        except ValueError as error:
            results.append({'name':name,'status':'REJECTED','reason':str(error)})
        else:
            raise ValueError('plant_accepted:'+name)
    return results

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('elf','baseline','helper','vurecomp','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    require(args.output.resolve().is_relative_to(ROOT/'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.elf)==ELF_SHA, 'ELF_pin')
    original_pins={str(p):sha(p) for p in [args.elf,args.helper,args.vurecomp]}
    data=args.elf.read_bytes()
    # Establish the real file-backed segment mapping instead of treating file offsets as VMAs.
    require(data[:6]==b'\x7fELF\x01\x01','ELF32_LE_required')
    phoff=struct.unpack_from('<I',data,28)[0]
    size,count=struct.unpack_from('<HH',data,42)
    segments=[struct.unpack_from('<IIIIIIII',data,phoff+i*size) for i in range(count)]
    args.output.mkdir(parents=True)
    inputs=args.output/'inputs';inputs.mkdir()
    admitted, rejected=[],[]
    for at in range(0,len(data)-16,16):
        tag,address,first,second=struct.unpack_from('<IIII',data,at)
        if tag & 0xffff0000 not in (0x60000000,0x70000000) or address or first or (second>>24)&127!=0x4a:
            continue
        image,row=payload(data,at)
        mapped=[s for s in segments if s[0]==1 and s[1]<=at and row['file_end']<=s[1]+s[4]]
        require(len(mapped)==1,'ambiguous_payload_segment')
        row['VMA']=mapped[0][2]+at-mapped[0][1]
        row['image_sha256']=hashlib.sha256(image).hexdigest()
        raw=args.output/(f'{at:08X}.bin');raw.write_bytes(image)
        result=subprocess.run([str(args.helper.resolve()),str(raw.resolve()),'0000'],capture_output=True,text=True)
        require(result.returncode==0,'reachable_helper_failed')
        reachable=json.loads(result.stdout)
        missing=[word['pc'] for word in reachable['words'] if not any(s['VU_offset']<=word['pc'] and
                    word['pc']+8<=s['VU_offset']+s['size'] for s in row['spans'])]
        row['reachable']=reachable;row['reachable_unwritten']=missing
        if missing:
            rejected.append(row);continue
        destination=inputs/(reachable['key']+'_0000.bin')
        require(not destination.exists() or destination.read_bytes()==image,'same_key_different_source_image')
        destination.write_bytes(image)
        admitted.append(row)
    require(bool(admitted),'zero_admitted_payloads')
    result=subprocess.run([str(args.vurecomp.resolve()),str(inputs.resolve()),str((args.output/'generated').resolve()),'--require-complete'],
                          capture_output=True,text=True)
    (args.output/'generation.log').write_text(result.stdout+result.stderr)
    require(result.returncode==0 and not result.stderr.strip(),'offline_generation_refused')
    match=re.fullmatch(r'(\d+) VU1 programs, (\d+) of them jump targets, 0 skipped\s*',result.stdout)
    require(match and int(match[1])-int(match[2])==len(list(inputs.glob('*.bin'))),'generation_denominator')
    new,prefix,new_origins=functions(args.output/'generated')
    old,old_prefix,old_origins=functions(args.baseline)
    require(prefix==old_prefix,'generated_prefix_changed')
    merged=dict(old)
    for key,body in new.items():
        merge(merged,key,body)
    out=args.output/'merged';out.mkdir()
    baseline_files=[p for p in args.baseline.glob('vu1_*.cpp') if p.name!='vu1_table.cpp']
    for path in baseline_files:
        shutil.copyfile(path,out/path.name)
    added=sorted(new.keys()-old.keys())
    if added:
        number=max(int(re.fullmatch(r'vu1_(\d+).cpp',p.name)[1]) for p in baseline_files)+1
        additional=prefix+'\n\n'.join(merged[k] for k in added)+'\n\n}\n'
        (out/f'vu1_{number:03}.cpp').write_text(additional,newline='\n')
        additional+='\nnamespace {const ps2::vu::Program kAdditional[] = {\n'
        additional+=''.join(f'{{0x{k}ull,gen::vu1_{k}}},\n' for k in added)
        additional+='};}\nvoid register_elf_additional_vu() { ps2::vu::register_programs(kAdditional,'+str(len(added))+'u); }\n'
        (args.output/'additional_registration.cpp').write_text(additional,newline='\n')
    keys=sorted(merged)
    table='#include "ps2/vu_ops.hpp"\n\nnamespace gen {\n'
    table+=''.join(f'void vu1_{k}(ps2::Vu0Regs &r, std::uint8_t *mem);\n' for k in keys)
    table+='\nconst ps2::vu::Program kVuPrograms[] = {\n'
    table+=''.join(f'    {{0x{k}ull, vu1_{k}}},\n' for k in keys)
    table+='};\n}\nvoid register_generated_vu() { ps2::vu::register_programs(gen::kVuPrograms, '+str(len(keys))+'u); }\n'
    (out/'vu1_table.cpp').write_text(table,newline='\n')
    checked,_,_=functions(out)
    require(checked==merged,'merged_body_or_key_changed')
    require(original_pins=={p:sha(Path(p)) for p in original_pins},'input_changed')
    result={'status':'ORIGINAL_ELF_MPG_OFFLINE_GENERATION_PASS','ELF_sha256':ELF_SHA,
            'admitted_payloads':admitted,'rejected_payloads':rejected,'controls':self_test(),
            'input_pins':original_pins,'new_generated_keys':len(new),'common_exact_bodies':len(old.keys()&new.keys()),
            'additional_keys':added,'merged_keys':len(merged),'script_sha256':sha(Path(__file__)),
            'generated_files':[{'name':p.name,'sha256':sha(p)} for p in sorted(out.glob('*.cpp'))],
            'scope':'Exact original file-backed DMA/VIF payload parsing and offline C++ generation. Entry-zero direct reachable words stay inside uploaded bytes. Indirect targets, all external launch roots, payload compositions/runtime patches and execution equivalence remain separate/open. No ownership/transformed/native credit.'}
    record=args.output/'RESULT.json';record.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
    print(json.dumps({'status':result['status'],'result':str(record),'sha256':sha(record),'payloads':len(admitted),
                      'new_keys':len(added),'merged_keys':len(merged)}))

if __name__=='__main__':
    main()
