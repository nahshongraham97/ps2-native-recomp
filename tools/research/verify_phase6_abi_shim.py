"""Compile bounded typed native/AOT transitions against actual generated bundles.

No runtime/game/card/state-loader linkage. Inputs outside this branch are read
only. The old whole-leaf proof and current ownership remain separate from these
constructed ABI component checks and do not award any implementation credit.
"""
from __future__ import annotations
import sys
sys.dont_write_bytecode=True
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from classify_stack_call_boundaries import PINS,reader

X=Path(__file__).resolve().parents[2]
LEAF_PROOF='8987512c582a3eb41c7fbcd91bfa5f6faad0b2b5aed2d66d1459ee989287515b'
COMPILER='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe'
WORDS={0x1a1bb4:0x0c068f10,0x1a1bb8:0x8c44000c,0x1a2068:0x0c068f10,0x1a206c:0x8e24000c,0x1a3c40:0x03e00008,0x1a3c44:0x24020001}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def need(value,reason):
    if not value:raise ValueError(reason)


def reference(text,set32):
    fragments={}
    for pc in (0x1a1bb4,0x1a2068,0x1a3c40):
        marker=f'L_{pc:08X}:';need(text.count(marker)==1,'actual_AOT_label_missing_or_duplicate');at=text.index(marker);end=text.find('\nL_',at+1)
        fragments[f'{pc:08X}']=text[at:end if end>=0 else len(text)]
    leaf=fragments['001A3C40'];need('const std::uint32_t t = c.r[31].w[0]' in leaf and 'set32(c.r[2], 0u + 1u)' in leaf,'actual_leaf_statement_shape_changed')
    pieces=['#pragma once\n#include "ps2/cpu.hpp"\n#include <stdexcept>\n#include <utility>\nnamespace original_abi_reference {\nusing ps2::Cpu;using ps2::Reg128;\n',set32+'\n',
        'inline void stop(Cpu&,std::uint32_t,const char*why){throw std::runtime_error(why);}\ninline void leaf(Cpu&c){\ngoto L_001A3C40;\n'+leaf+'\n}\n',
        'template<class Read32>void call(Cpu&c,std::uint32_t pc,Read32&&read){\nconst unsigned depth=0,kMaxDepth=1;\n[[maybe_unused]] auto ld32=[&](std::uint32_t a){return read(a,std::as_const(c));};\nauto unit_0028=[](Cpu&v,unsigned entry,unsigned){if(entry!=294u)throw std::runtime_error("reference_dispatch_entry");leaf(v);};\n']
    for i,pc in enumerate((0x1a1bb4,0x1a2068)):
        body=fragments[f'{pc:08X}'];needle=f'goto L_{pc+8:08X};';need(body.count(needle)==1,'actual_JAL_return_label_changed')
        pieces.append(('if' if i==0 else 'else if')+f'(pc==0x{pc:08X}){{\ngoto L_{pc:08X};\n'+body.replace(needle,'return;')+'\n}\n')
    pieces.append('else throw std::runtime_error("unproved_reference_call_site");\n}\n')
    cpp=''.join(pieces);call=cpp[cpp.index('template<class Read32>void call'):]
    pattern=r'(c\.r\[31\]\.sd\[0\] = static_cast<std::int32_t>\(0x[0-9A-F]+u\);) (set32\(c\.r\[4\], ld32\(.*?;)'
    need(len(re.findall(pattern,call))==2,'actual_delay_link_mutation_shape_changed')
    reordered=re.sub(pattern,lambda m:m[2]+' '+m[1],call).replace('void call(','void reordered_call(',1)
    dropped=re.sub(pattern,lambda m:m[1],call).replace('void call(','void no_delay_call(',1)
    return cpp+'// Planted original-bundle mutations, test-only.\n'+reordered+dropped+'}\n',fragments


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('parent-root','source-fork','compiler','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    for k,v in vars(a).items():setattr(a,k,v.resolve())
    need(a.output.is_relative_to(X/'build/oracle-telemetry') and not a.output.exists(),'fresh_X_output_required')
    need('r192' not in a.source_fork.name.lower(),'R192_source_access_forbidden')
    need(sha(a.compiler)==COMPILER,'compiler_pin_changed')
    olddir=a.source_fork/'build/oracle-telemetry/stage-A-leaf-05-admitted';oldproof=olddir/'RESULT.json';need(sha(oldproof)==LEAF_PROOF,'admitted_leaf_receipt_changed');old=json.loads(oldproof.read_text())
    reader_source=Path(__file__).with_name('classify_stack_call_boundaries.py')
    need(sha(reader_source)==old['source_pins'][str(a.source_fork/'tools/research/classify_stack_call_boundaries.py')],'original_reader_producer_changed')
    unit=a.source_fork/'build/generated-aot/phase4-all/unit_0028.cpp';need(sha(unit)==old['source_pins'][str(unit)],'measured_original_unit_changed')
    native=olddir/'stage_a_leaf.cpp';need(sha(native)==old['generated_source_sha256'],'admitted_generated_native_source_changed')
    elf=a.parent_root/'research/inputs/SLUS_214.10';read=reader(elf.read_bytes())
    export=a.parent_root/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160'
    need(all(sha(export/name)==expected for name,expected in PINS.items()),'pinned_original_exports_changed')
    joined=[]
    with (export/'instructions.tsv').open(encoding='utf-8',newline='') as f:
        for row in csv.DictReader(f,delimiter='\t'):
            pc=int(row['address'],16)
            if pc in WORDS:
                need(int.from_bytes(read(pc,4),'little')==WORDS[pc] and bytes.fromhex(row['bytes'])==read(pc,4),'original_call_leaf_word_mismatch')
                joined.append({'pc':f'{pc:08X}','word':f'{WORDS[pc]:08X}','listing':row})
    need(len(joined)==len(WORDS),'original_listing_rows_missing')
    ledger=X/'research/ledger/functions.csv'
    with ledger.open(encoding='utf-8',newline='') as f:owned=next(r for r in csv.DictReader(f) if r['module']=='SLUS_214.10' and r['address'].lower()=='001a3c40')
    body=hashlib.sha256(read(0x1a3c40,8)).hexdigest()
    need(owned['ownership_status']=='verified-renderware' and 'E1' in owned['ownership_evidence_kinds'].split('|') and 'sha256='+body in owned['ownership_evidence_ref'],'existing_E1_ownership_not_admitted')
    generated=X/'runtime/include/ps2/generated.hpp';set32=next(line for line in generated.read_text().splitlines() if line.startswith('inline void set32('))
    cpp,fragments=reference(unit.read_text(),set32)
    paths=[Path(__file__),reader_source,reader_source.with_name('verify_stack_exit_paths.py'),X/'runtime/include/mka/stage_a_abi_shim.hpp',X/'tests/test_phase6_abi_shim.cpp',X/'runtime/include/ps2/cpu.hpp',generated,unit,native,olddir/'stage_a_leaf.hpp',oldproof,ledger,elf,a.compiler,*[export/n for n in PINS]]
    pins={str(path):sha(path) for path in paths}
    need(shutil.disk_usage(X).free>(12<<30)+(8<<20),'12GiB_reserve');a.output.mkdir(parents=True)
    ref=a.output/'original_abi_reference.hpp';ref.write_text(cpp,encoding='utf-8',newline='\n')
    exe=a.output/'test_phase6_abi_shim.exe';command=[str(a.compiler),'-std=c++20','-O2','-static','-Wall','-Wextra','-Werror','-I'+str(X/'runtime/include'),'-I'+str(a.output),'-I'+str(olddir),str(X/'tests/test_phase6_abi_shim.cpp'),str(native),'-o',str(exe)]
    compiled=subprocess.run(command,capture_output=True,text=True,timeout=120);(a.output/'build.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8',newline='\n')
    runs=[]
    if compiled.returncode==0:
        for plant in ['', '--planted-width','--planted-stale-ssa','--planted-high-lane','--planted-return','--planted-frame','--planted-delay','--planted-reordered-delay','--planted-invented-input']:
            argv=[str(exe)]+([plant] if plant else []);run=subprocess.run(argv,capture_output=True,text=True,timeout=20);log=a.output/((plant.removeprefix('--') or 'positive')+'.log');log.write_text(run.stdout+run.stderr,encoding='utf-8',newline='\n')
            ok=(run.returncode==1 and 'FAIL_ABI_SHIM' in run.stderr) if plant else (run.returncode==0 and 'constructed=160' in run.stdout and 'structural_refusals=7' in run.stdout)
            runs.append({'argv':argv,'exit_code':run.returncode,'status':'REJECTED' if plant and ok else 'PASS' if ok else 'FAIL','log':str(log),'sha256':sha(log)})
    changed=[path for path,digest in pins.items() if sha(path)!=digest];ok=compiled.returncode==0 and len(runs)==9 and all(r['status'] in ('PASS','REJECTED') for r in runs) and not changed
    report={'status':'BOUNDED_NATIVE_AOT_ABI_COMPONENT_PASS' if ok else 'REFUSED','source_pins':pins,'input_changes':changed,'compiler_command':command,'compile_exit_code':compiled.returncode,
        'invocation':[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]],
        'binary_sha256':sha(exe) if exe.exists() else None,'reference_sha256':sha(ref),'original_evidence':joined,'actual_generated_fragments':{k:{'text':v,'sha256':hashlib.sha256(v.encode()).hexdigest()} for k,v in fragments.items()},
        'reference_edits':'Only dispatch seam uses selected actual leaf; ld32 is supplied as explicit caller-owned typed read callback; terminal goto becomes return. Original RA/LW/delay/return/budget statements are retained verbatim.',
        'typed_signature':'int32_t close_resource_heap(ResourceHeapReference unused), existing admitted generated leaf only','ownership':owned,'runs':runs,'counts':{'constructed_comparisons':160 if ok else None,'exact_call_bundles':128,'entry_boundaries':32,'structural_refusals':7,'planted_comparator_refusals':8,'saved_states_executed':0,'native_credit':0},
        'limits':['No general101 ABI or native callee signature inferred; context restore/tail/unresolved indirect calls are not admitted.','Read callback is a StageA boundary, not a native heap owner or memory implementation.','Whole Cpu validation never repairs a callee mutation; no global source-context swapping/restoration.','This component uses constructed inputs; prior189 whole-leaf comparisons remain dated separate evidence; no game/R192/state/card access.','Call sites explicitly change RA/A0 low64 and budget; preserved upper lanes/SP/otherstate are not confused with restoring old RA.']}
    out=a.output/'RESULT.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'result':str(out),'sha256':sha(out),'status':report['status'],'compile_exit':compiled.returncode}));return 0 if ok else 1


if __name__=='__main__':raise SystemExit(main())
