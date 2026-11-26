"""Pin original Track B words/callers and run native component checks only.
No oracle, replay, GUI, audio, dispatch, or guest instruction execution is used.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,shutil,struct,subprocess
from pathlib import Path

F=Path(__file__).resolve().parents[2]
P=F.parents[2]
SOURCE=P/'research/inputs/SLUS_214.10'
EXPORT=P/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160'
ELF_SHA='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
LISTING_SHA='4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'
def sha(b):return hashlib.sha256(b).hexdigest()
def identity(p):return {'path':str(p),'sha256':sha(p.read_bytes()),'size':p.stat().st_size}
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',required=True,type=Path)
    ap.add_argument('--compiler',required=True,type=Path)
    ap.add_argument('--completed-input-root',type=Path)
    args=ap.parse_args();out=args.output.resolve()
    if out.exists():raise ValueError('preserve previous attempt; choose another output filename')
    if shutil.disk_usage(F).free<12*(1<<30):raise ValueError('12 GiB reserve tripped; generation paused')
    if not out.parent.is_dir():raise ValueError('use an existing ignored output directory')
    elf=SOURCE.read_bytes();assert sha(elf)==ELF_SHA
    listing_path=EXPORT/'instructions.tsv';listing=listing_path.read_bytes();assert sha(listing)==LISTING_SHA
    with listing_path.open() as f:rows=list(csv.DictReader(f,delimiter='\t'))
    by_address={int(r['address'],16):r for r in rows}
    phoff=struct.unpack_from('<I',elf,28)[0];phnum=struct.unpack_from('<H',elf,44)[0]
    segments=[struct.unpack_from('<8I',elf,phoff+32*i) for i in range(phnum)]
    def original(vma,size):
        options=[s for s in segments if s[0]==1 and s[2]<=vma and vma+size<=s[2]+s[4]]
        assert len(options)==1;s=options[0];off=s[1]+vma-s[2]
        return elf[off:off+size],{'p_offset':hex(s[1]),'p_vaddr':hex(s[2]),'p_filesz':hex(s[4]),'file_offset':hex(off)}
    proof={}
    for entry,end in [(0x162e70,0x1631ec),(0x2e60e0,0x2e6dec),(0x2e6ec0,0x2e7030),(0x2e95a0,0x2e95f0)]:
        raw,mapping=original(entry,end-entry);mapped=[];omitted=[];words=[]
        for address in range(entry,end,4):
            data=raw[address-entry:address-entry+4];row=by_address.get(address)
            if row:
                assert bytes.fromhex(row['bytes'])==data
                assert int(row['function_entry'],16)==entry
                mapped.append(row)
            else:
                # All 13 omitted words are zero SLL zero,zero,0. They follow
                # terminal/unconditional transfers or align split case blocks;
                # they are not folded into the catalogue body's byte count.
                assert data==bytes(4);omitted.append({'address':hex(address),'word':'00000000','decode':'SLL zero,zero,0 (NOP)','previous':by_address.get(address-4)})
            words.append({'address':hex(address),'le_bytes':data.hex(),'word':f'{int.from_bytes(data,"little"):08x}','listing':row})
        ranges=[]
        with (EXPORT/'function-bodies.tsv').open() as f:
            ranges=[r for r in csv.DictReader(f,delimiter='\t') if int(r['entry'],16)==entry]
        proof[f'{entry:08x}']={'range':[hex(entry),hex(end)],'segment':mapping,'body_sha256':sha(raw),'listed_words':len(mapped),'span_words':len(words),'exported_ranges':ranges,'omitted_words':omitted,'words':words}
    callers={}
    for target in [0x162e70,0x2e60e0]:
        matches=[]
        for address,row in by_address.items():
            word=int.from_bytes(bytes.fromhex(row['bytes']),'little')
            if word>>26==3 and ((address+4)&0xf0000000)|((word&0x3ffffff)<<2)==target:
                matches.append([by_address.get(address+i) for i in range(-8,20,4)])
        callers[f'{target:08x}']=matches
    continuation={hex(a):by_address[a] for a in range(0x2e612c,0x2e6188,4)}
    for a,w in [(0x2e6178,0x0180f809),(0x2e617c,0x01825821),(0x2e6180,0x10000307),(0x2e6184,0xae620000)]:
        assert original(a,4)[0]==struct.pack('<I',w)
    with (EXPORT/'functions.tsv').open() as f:catalogue=list(csv.DictReader(f,delimiter='\t'))
    assert not any(int(r['entry'],16)==0x2e6180 for r in catalogue)
    table,table_mapping=original(0x630c90,23*4)
    capture_rows=[]
    from verify_phase7_capture import parse_capture
    capture_files=[]
    capture_files+=sorted((F/'b/phase7-capture-run/syn_pad_out').glob('cap_00162e70_*.bin'))
    capture_files+=sorted((F/'b/phase7-capture-run/captures_stub_2e6180').rglob('cap_002e60e0_*.bin'))
    if args.completed_input_root:capture_files+=sorted(args.completed_input_root.resolve().rglob('cap_00162e70_*.bin'))
    for p in capture_files:
        raw=p.read_bytes();info=parse_capture(raw)
        info['path']=str(p);info['sha256']=sha(raw);info['ordinary_return']=info['exit_pc']==info['entry_ra']
        capture_rows.append(info)
    owned=['runtime/include/mka/phase7_input_native.hpp','runtime/include/mka/phase7_stub_native.hpp','tests/test_phase7_input_differential.cpp','tests/test_phase7_stub_differential.cpp','tests/phase7_track_b_capture.hpp','tools/research/verify_phase7_track_b.py']
    report={'scope':'source-derived and tested native component only; no new ownership, credit or production admission','elf':identity(SOURCE),'listing':identity(listing_path),'program':identity(EXPORT.parent/'get_program_info.json'),'compiler':identity(args.compiler),'extension_version':'not present in export; no new Ghidra session claimed','original':proof,'caller_contexts':callers,'caller_002e9d18_remaining_body':[r for a,r in by_address.items() if 0x2e9d18<=a<=0x2e9da8],'continuation_002e6180':continuation,'catalogue_entry_002e6180':False,'continuation_conclusion':'JALR t4 at 002E6178 writes RA=002E6180, t4 loaded from a0+76; delay slot computes t3=target+4*[object+916]; continuation branches2E6DA0 and stores callback v0 to*object. Catalogue absence alone is not a global no-interior-entry proof.','jump_table':{'mapping':table_mapping,'raw':table.hex(),'targets':[hex(v) for v in struct.unpack('<23I',table)]},'source':[identity(F/p) for p in owned],'dependencies':[identity(F/p) for p in ['runtime/include/ps2/fpu.hpp','runtime/include/ps2/cpu.hpp','tests/native_test_sha256.hpp','runtime/include/mka/retail_action_history.hpp','runtime/include/mka/stage_a_guest_memory.hpp']],'generated_aot':[identity(F/'build/generated-aot/phase4-all'/n) for n in ['unit_0018.cpp','unit_003B.cpp','unit_0079.cpp','unit_007A.cpp']],'captures':capture_rows,'commands':[],'historical_claims_preserved':[{'claim':'phase7_stub_native.hpp at fb814c76: a0==0 -> no guest stores,v0==3; abort active emitters','disposition':'withdrawn: first object+76 load is unconditional; no null a0 guard. 3 was incidental helper residue in some active calls.'},{'claim':'stub_family_002e6180, 0 hits in six states; suggested standalone emitter','disposition':'bounded superseding proof: observed original continuation within2E60E0. No standalone catalogue entry; no universal absence-of-interior-entry inference.'},{'claim':'five active synthetic00162E70 captures and fourteen active002E60E0 captures','disposition':'captures are real but five input and five dynamic-stub files end in preemption/unwind rather than ordinary return; excluded from whole-function pass counts.'}],'limits':['Input001EF2C0 context resolver and00162C20 dispatch bodies unadmitted. Original5syntheticcaptures prove six-store prefixes only. Any additional ordinary-return capture is compared in full only if neither unbound service is needed.','5dynamiccallback slices are refused before native effects; independent typed callback service tested with a synthetic service, not substituted into capture comparisons.','Direct002E60E0 supports captured field kinds1,2,4,5,6,17,18,19; remaining kinds and secondary texture service fail by name.','Effects-only comparison at captured local caller002E9D18, which does not read v0 in its remaining body before next calls or return. An outward return-value ABI is not proved; caller-saved helper residue v0 is not compared or fabricated.','No oracle or connected game run; ownership unresolved, credit2/238 unchanged.']}
    def run(command,expected,name):
        result=subprocess.run([str(x) for x in command],cwd=F,capture_output=True,text=True)
        log=out.with_name(out.stem+'-'+name+'.log');log.write_text(result.stdout+result.stderr,encoding='utf-8',newline='\n')
        item={'argv':[str(x) for x in command],'returncode':result.returncode,'expected':expected,'log':identity(log)};report['commands'].append(item)
        if result.returncode!=expected:raise RuntimeError(f'{name}: rc{result.returncode} expected{expected}; {result.stdout}{result.stderr}')
    try:
        for target,captures,require_option in [('input','syn_pad_out','--require-complete'),('stub','captures_stub_2e6180','--require-all')]:
            binary=out.with_name(out.stem+'-'+target+'.exe')
            run([args.compiler,'-std=c++20','-O2','-Iruntime/include','-Itests',f'tests/test_phase7_{target}_differential.cpp','-lbcrypt','-static-libgcc','-static-libstdc++','-o',binary],0,target+'-compile')
            report.setdefault('binaries',[]).append(identity(binary))
            root=F/'b/phase7-capture-run'/captures
            run([binary,root],0,target+'-normal')
            for option in ['--planted-store','--planted-order',require_option]:run([binary,root,option],1,target+option)
        if args.completed_input_root:
            binary=out.with_name(out.stem+'-input.exe')
            run([binary,args.completed_input_root.resolve()],0,'input-completed-normal')
            run([binary,args.completed_input_root.resolve(),'--require-complete'],0,'input-completed-required')
            for option in ['--planted-store','--planted-order']:run([binary,args.completed_input_root.resolve(),option],1,'input-completed'+option)
        for pin in report['source']+report['dependencies']:
            if identity(Path(pin['path']))!=pin:raise RuntimeError('source changed during component checks')
        report['status']='PASS_BOUNDED_COMPONENT_AND_PLANTED_REJECTIONS'
    except Exception as error:
        report['status']='FAIL_PRESERVED';report['error']=repr(error);raise
    finally:
        out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'status':report['status'],'report':str(out),'sha256':sha(out.read_bytes())}))
if __name__=='__main__':main()
