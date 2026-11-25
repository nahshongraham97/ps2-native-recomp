"""Build TEST-ONLY original generated-C++ frame-boundary references.

No EE word interpreter, native frame implementation or whole-function claim.
Original AOT operations are copied unchanged; labels call a test boundary guard
and memory helpers call observation wrappers around the same original helpers.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

TARGETS = {0x100320,0x1225D0,0x12E4E0,0x130AC0,0x1588C0,0x171B10,0x18D970,
           0x197BC0,0x19F0B0,0x19F1E0,0x1A2BC0,0x1A0160,0x1A1B30,0x1A2270,
           0x1A3DF0,0x1A3190,0x1A64A0,0x1A6F40,0x1A7DE0,0x1A61D0,0x1A63D0,
           0x1A89C0,0x1A9E80,0x1C4A80,0x1C49B0,0x1CF300,0x2E06E0,0x334700}
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
def digest(data): return hashlib.sha256(data).hexdigest()
def pc(value): return int(value,16) if isinstance(value,str) else value
def quoted(value): return json.dumps(str(value))
def reg(value):
    if value is None:return 32  # Unbound local; never invent a GPR binding.
    if isinstance(value,int): return value
    names=['zero','at','v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','s0','s1','s2','s3','s4','s5','s6','s7','t8','t9','k0','k1','gp','sp','fp','ra']
    value=str(value).lower().lstrip('$')
    if value in names:return names.index(value)
    return int(value.lstrip('r'))

def generate(manifest,generated,original,out,replace=False):
    fork=Path(__file__).resolve().parents[2];out=out.resolve()
    if not out.is_relative_to(fork/'b') or not out.name.startswith('generated-stack-reference'):
        raise ValueError('reference output must be owned generated-stack-reference child under b')
    raw=manifest.read_bytes();analysis=json.loads(raw)
    if analysis['schema']!='generated_stack_frame_slices_v1':raise ValueError('unsupported stack slice schema')
    targets=analysis['targets']
    if {pc(t['source_entry'])for t in targets}!=TARGETS or len(targets)!=28:raise ValueError('must preserve exact original28 stack-first targets')
    image=original.read_bytes()
    if digest(image)!=ELF_SHA or analysis['original_elf_sha256']!=ELF_SHA:raise ValueError('original ELF pin changed')
    ph=struct.unpack_from('<I',image,28)[0];step,n=struct.unpack_from('<HH',image,42)
    loads=[struct.unpack_from('<8I',image,ph+i*step)for i in range(n)]
    def word(address):
        matches=[off+address-va for typ,off,va,_,size,*_ in loads if typ==1 and va<=address and address+4<=va+size]
        if len(matches)!=1:raise ValueError('slice instruction not uniquely file-backed')
        return struct.unpack_from('<I',image,matches[0])[0]
    header=['// TEST REFERENCE ONLY: no product target may link these Cpu/RAM copies.',
            '#pragma once','#include "ps2/generated.hpp"','#include <cstddef>','#include <cstdint>',
            'namespace stack_reference {',
            'using RawUnit=void(*)(ps2::Cpu&,std::uint32_t,std::uint32_t);',
            'struct Delay { std::uint32_t owner,delay; };',
            'struct StorePin { std::uint32_t pc; unsigned width; };',
            'struct Slot { int offset; unsigned width,reg,bank; const std::uint32_t* stores; std::size_t store_count; const std::uint32_t* loads; std::size_t load_count; const char* classification; };',
            'struct Slice { std::uint32_t target,start,entry,frame,jr,delay; bool epilogue; RawUnit raw; const std::uint32_t* allowed; std::size_t allowed_count; const std::uint32_t* stops; std::size_t stop_count; const Delay* delays; std::size_t delay_count; const Slot* slots; std::size_t slot_count; const StorePin* stores; std::size_t store_count; const char* native_status; };',
            'extern const Slice slices[]; extern const std::size_t slice_count;',
            'void boundary(ps2::Cpu&,std::uint32_t);',
            'std::uint8_t observed_ld8(std::uint32_t); std::uint16_t observed_ld16(std::uint32_t);',
            'std::uint32_t observed_ld32(std::uint32_t); std::uint64_t observed_ld64(std::uint32_t); void observed_ld128(std::uint32_t,ps2::Reg128&);',
            'void observed_st8(std::uint32_t,std::uint8_t); void observed_st16(std::uint32_t,std::uint16_t);',
            'void observed_st32(std::uint32_t,std::uint32_t); void observed_st64(std::uint32_t,std::uint64_t); void observed_st128(std::uint32_t,const ps2::Reg128&);']
    source=['#include "stack_reference_slices.hpp"','namespace stack_reference {']
    units={};cases=[];proof=[]
    def array(name,values,kind='std::uint32_t'):
        values=list(values);source.append(f'static const {kind} {name}[]={{'+(','.join(values)if values else '{}')+'};');return name,len(values)
    for ti,t in enumerate(targets):
        unit_path=Path(t['source_unit']);unit_name=unit_path.name;unit=(generated/unit_name).resolve()
        if not unit.is_relative_to(generated.resolve()) or unit_path.as_posix()!='build/generated-aot/phase4-all/'+unit_name:raise ValueError('generated unit path escaped')
        data=unit.read_bytes()
        if digest(data)!=t['source_unit_sha256']:raise ValueError('generated unit identity changed')
        text=data.decode();function=unit.stem
        if not re.search(rf'case {t["entry_case"]}u: goto L_{pc(t["source_entry"]):08X};',text):raise ValueError('generated entry case mismatch')
        units[function]=(data,text)
        header.append(f'void {function}(ps2::Cpu&,std::uint32_t,std::uint32_t);')
        slots=[]
        for si,s in enumerate(t.get('slots',[])):
            bank=s.get('register_bank')
            if s['source_register'] is not None and bank not in ['GPR','FPR']:
                raise ValueError('known stack register requires explicit GPR/FPR bank')
            bank_id={'GPR':0,'FPR':1}.get(bank,2)
            sn,sc=array(f'store_{ti}_{si}',(f'0x{pc(x):08x}u'for x in s.get('store_pcs',[])))
            ln,lc=array(f'load_{ti}_{si}',(f'0x{pc(x):08x}u'for x in s.get('load_pcs',[])))
            slots.append('{'+f'{s["offset_from_allocated_sp"]},{s["width_bits"]},{reg(s["source_register"])},{bank_id},{sn},{sc},{ln},{lc},'+quoted(s.get('classification','unresolved'))+'}')
        source.append(f'static const Slot slots_{ti}[]={{'+(','.join(slots)if slots else '{}')+'};')
        frame=t.get('frame_bytes') or 0
        specs=[dict(t['entry_slice'],entry_case=t['entry_case'],epilogue=False)]+[dict(s,epilogue=True)for s in t.get('epilogue_slices',[])]
        for si,s in enumerate(specs):
            ident=f'{ti}_{si}';allowed=[pc(x)for x in s['original_instruction_pcs']]
            start=pc(s['start_pc']);ep=s['epilogue'];jr=pc(s.get('JR_pc',0));delay=pc(s.get('delay_pc',0))
            if not allowed or start not in allowed:raise ValueError('empty or unpinned original slice')
            stops=[pc(x)for x in s.get('stop_before_pcs',[])]
            delay_pairs=[(pc(x['owner_pc']),pc(x['delay_pc']))for x in s.get('inline_delay_pcs',[])]
            if ep and (jr,delay) not in delay_pairs:delay_pairs.append((jr,delay))
            if ep and (jr not in allowed or delay not in allowed or word(jr)!=0x03e00008):raise ValueError('epilogue lacks authenticated JR RA/delay pair')
            if not ep and not stops:raise ValueError('entry prefix lacks safe stop set')
            for address in allowed:
                # Static admission only, never executes/decodes EE words.
                w=word(address);op=w>>26
                if op==3 or op==0 and w&63==9:raise ValueError('callee call cannot execute in a frame reference slice')
                if op==0 and w&63==8 and not(ep and address==jr):raise ValueError('unexpected indirect transfer in slice')
                marker=f'L_{address:08X}:'.encode()
                if marker not in data:raise ValueError('original instruction has no generated source label')
            offsets=s.get('source_label_offsets',{})
            for address,offset in offsets.items():
                if not data[int(offset):].startswith(f'L_{pc(address):08X}:'.encode()):raise ValueError('generated source label byte pin changed')
            an,ac=array('allowed_'+ident,(f'0x{x:08x}u'for x in allowed));bn,bc=array('stops_'+ident,(f'0x{x:08x}u'for x in stops))
            dn,dc=array('delays_'+ident,('{'+f'0x{a:08x}u,0x{b:08x}u'+'}'for a,b in delay_pairs),'Delay')
            # Static original-word classification only. Runtime executes copied
            # C++, not these words; pins authenticate observed store PC/width.
            store_widths={0x28:1,0x29:2,0x2b:4,0x3f:8,0x1f:16,0x39:4,0x3d:8,0x3e:16}
            stores=[(a,store_widths[word(a)>>26])for a in allowed if word(a)>>26 in store_widths]
            sn,sc=array('original_stores_'+ident,('{'+f'0x{a:08x}u,{w}'+'}'for a,w in stores),'StorePin')
            if not re.search(rf'case {s["entry_case"]}u: goto L_{start:08X};',text):raise ValueError('slice generated start case mismatch')
            native=t.get('native_admission',{}).get('status','UNRESOLVED')
            cases.append('{'+f'0x{pc(t["source_entry"]):08x}u,0x{start:08x}u,{s["entry_case"]},{frame},0x{jr:08x}u,0x{delay:08x}u,{str(ep).lower()},&{function},{an},{ac},{bn},{bc},{dn},{dc},slots_{ti},{len(slots)},{sn},{sc},'+quoted(native)+'}')
            proof.append({'target':t['source_entry'],'start':f'{start:08x}','epilogue':ep,'unit':unit_name,'unit_sha256':digest(data),
                'original_words':[{'pc':f'{a:08x}','word':f'{word(a):08x}'}for a in allowed],
                'copied_operations':'Entire hash-pinned generated unit; only namespace/test label/helper observation instrumentation differs',
                'stop_before_pcs':[f'{a:08x}'for a in stops]})
    header.append('} // namespace stack_reference')
    source+=['const Slice slices[]={'+','.join(cases)+'};','const std::size_t slice_count=sizeof(slices)/sizeof(slices[0]);','}']
    out.mkdir(parents=True,exist_ok=True)
    outputs={'stack_reference_slices.hpp':'\n'.join(header)+'\n','stack_reference_slices.cpp':'\n'.join(source)+'\n'}
    for function,(data,text) in units.items():
        if text.count('namespace gen {')!=1:raise ValueError('unexpected generated namespace')
        copied=text.replace('namespace gen {','namespace stack_reference {\nusing namespace gen;',1)
        copied=re.sub(r'(L_([0-9A-F]{8}):)',lambda m:m[1]+f'\n    stack_reference::boundary(c,0x{m[2]}u);',copied)
        macros='\n'.join(f'#define {kind}{width} stack_reference::observed_{kind}{width}'for kind in ['ld','st']for width in [8,16,32,64,128])
        undefs='\n'.join(f'#undef {kind}{width}'for kind in ['ld','st']for width in [8,16,32,64,128])
        outputs['reference_'+function+'.cpp']='#include "stack_reference_slices.hpp"\n#include "units.hpp"\n'+macros+'\n'+copied+'\n'+undefs+'\n'
    for name,contents in outputs.items():
        path=out/name;encoded=contents.encode()
        if path.exists() and path.read_bytes()!=encoded and not replace:raise ValueError('reference output differs; use fresh output directory')
        path.write_bytes(encoded)
    report={'schema':'compiled_original_stack_reference_proof_v1','manifest_sha256':digest(raw),'original_elf_sha256':digest(image),
        'targets':28,'slices':proof,'outputs':{n:digest(s.encode())for n,s in outputs.items()},
        'scope':'Reference-only compiled original AOT entry/exit frame geometry and transport; no native emission, whole-function E2 or historical invocation claim'}
    (out/'stack_reference_proof.json').write_text(json.dumps(report,indent=2)+'\n',newline='\n')
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['manifest','generated-dir','original-elf','out-dir']:parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--replace-reference-output',action='store_true')
    a=parser.parse_args();report=generate(a.manifest,a.generated_dir,a.original_elf,a.out_dir,a.replace_reference_output)
    print(json.dumps({'targets':report['targets'],'slices':len(report['slices']),'outputs':len(report['outputs'])}))
if __name__=='__main__':main()
