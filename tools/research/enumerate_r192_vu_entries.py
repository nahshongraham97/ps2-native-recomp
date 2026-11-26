"""Enumerate every original uploaded VU pair as a conservative entry candidate.

An entry is admitted to this offline compiler input only when its entire
canonical reachable set is uploaded original code. This is not callability,
ownership or whole-function equivalence. No game or original input is changed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
from extract_r192_elf_vu import ELF_SHA, payload, require, sha

ROOT=Path(__file__).resolve().parents[2]

ENUMERATOR=r'''
#include "ps2/vu_ops.hpp"
#include <cstdio>
#include <fstream>
#include <vector>
int main(int argc,char**argv){
 if(argc!=5)return 2;
 std::vector<unsigned char> code(16384),source(2048);
 std::ifstream a(argv[1],std::ios::binary),b(argv[2],std::ios::binary);
 a.read((char*)code.data(),code.size());b.read((char*)source.data(),source.size());
 if(!a||!b||a.peek()!=EOF||b.peek()!=EOF)return 3;
 std::ofstream good(argv[3]),bad(argv[4]);
 unsigned candidates=0,admitted=0,refused=0;
 for(unsigned entry=0;entry<16384;entry+=8){
  if(!source[entry/8])continue;
  ++candidates;
  const auto pcs=ps2::vu::reachable(code.data(),16383,entry);
  unsigned missing=0,first=16384;
  for(auto pc:pcs)if(!source[pc/8]){++missing;if(first==16384)first=pc;}
  char line[160];
  if(missing){std::snprintf(line,sizeof line,"%04X %04X %u\n",entry,first,missing);bad<<line;++refused;}
  else{const auto key=ps2::vu::program_key(code.data(),pcs,entry);
   std::snprintf(line,sizeof line,"%016llX %04X %zu\n",(unsigned long long)key,entry,pcs.size());good<<line;++admitted;}
 }
 if(!good||!bad)return 4;
 std::printf("{\"candidates\":%u,\"admitted\":%u,\"refused\":%u}\n",candidates,admitted,refused);
 return 0;
}
'''

GENERATOR=r'''
#include "vurecomp_gen.hpp"
#include <fstream>
#include <cstdio>
#include <sstream>
#include <vector>
int main(int argc,char**argv){
 if(argc!=4)return 2;
 std::vector<unsigned char> code(16384);std::ifstream a(argv[1],std::ios::binary);
 a.read((char*)code.data(),code.size());if(!a||a.peek()!=EOF)return 3;
 std::ifstream table(argv[2]);std::vector<ps2::vurecomp::Input> inputs;
 unsigned long long key;unsigned entry,words;
 while(table>>std::hex>>key>>entry>>std::dec>>words)inputs.push_back({key,entry,code});
 if(!table.eof())return 4;
 const auto expected=inputs.size();
 const auto r=ps2::vurecomp::generate_files(std::move(inputs),argv[3],0,8,false);
 std::printf("expected=%zu programs=%zu skipped=%zu targets=%zu\n",expected,r.programs,r.skipped,r.jump_targets);
 return r.programs==expected&&!r.skipped&&!r.jump_targets?0:5;
}
'''

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('proof','elf','source-root','compiler','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-proof-sha',required=True)
    p.add_argument('--generate',action='store_true')
    p.add_argument('--reserve-gib',type=int,default=12)
    a=p.parse_args()
    require(a.output.resolve().is_relative_to(ROOT/'build') and not a.output.exists(),'fresh_owned_output_required')
    require(sha(a.proof)==a.expected_proof_sha and sha(a.elf)==ELF_SHA,'original_proof_or_ELF_pin')
    old=json.loads(a.proof.read_text());require(old['status']=='SOURCE_BOUNDED_MPG_TARGET_GENERATION_PASS','source_bounded_proof_required')
    for path in [a.compiler,a.source_root/'recomp/vurecomp_gen.cpp',a.source_root/'recomp/vurecomp_gen.hpp',
                 a.source_root/'runtime/include/ps2/vu_ops.hpp']:
        expected={value for name,value in old['input_pins'].items() if Path(name).name==path.name}
        require(len(expected)==1 and sha(path) in expected,'reference_tool_or_header_changed:'+path.name)
    require(a.reserve_gib>=8,'minimum_disk_reserve_8GiB')
    a.output.mkdir(parents=True)
    def space():
        require(shutil.disk_usage(a.output).free >= a.reserve_gib*(1<<30),'disk_reserve_exhausted_originals_preserved')
    inputs=[a.proof,a.elf,a.compiler,a.source_root/'recomp/vurecomp_gen.cpp',
            a.source_root/'recomp/vurecomp_gen.hpp',a.source_root/'runtime/include/ps2/vu_ops.hpp',
            Path(__file__),Path(__file__).with_name('extract_r192_elf_vu.py')]
    pins={str(path):sha(path) for path in inputs}
    commands=[]
    for name,text in [('enumerator',ENUMERATOR),*([('generator',GENERATOR)] if a.generate else [])]:
        cpp=a.output/(name+'.cpp');cpp.write_text(text,newline='\n')
        command=[str(a.compiler),'-std=c++20','-O2','-static','-I'+str(a.source_root/'recomp'),
                 '-I'+str(a.source_root/'runtime/include'),str(cpp)]
        if name=='generator':command.append(str(a.source_root/'recomp/vurecomp_gen.cpp'))
        command+=['-o',str(a.output/(name+'.exe'))]
        run=subprocess.run(command,capture_output=True,timeout=180)
        (a.output/(name+'-compile.log')).write_bytes(run.stdout+run.stderr)
        require(run.returncode==0,'offline_tool_compile_failed:'+name);commands.append(command)
    raw=a.elf.read_bytes();records=[];all_keys=set();source_keys={p['key'] for row in old['admitted_payloads'] for p in row['source_bounded_programs']}
    total=dict(candidates=0,admitted=0,refused=0)
    for row in old['admitted_payloads']:
        space();image,origin=payload(raw,row['file_offset'])
        require(hashlib.sha256(image).hexdigest()==row['image_sha256'],'original_payload_changed')
        bitmap=bytearray(2048)
        for span in origin['spans']:
            require(not span['VU_offset']%8 and not span['size']%8,'partial_uploaded_pair')
            for pc in range(span['VU_offset'],span['VU_offset']+span['size'],8):bitmap[pc//8]=1
        out=a.output/f"payload-{row['file_offset']:08X}";out.mkdir()
        (out/'image.bin').write_bytes(image);(out/'source.bitmap').write_bytes(bitmap)
        command=[str(a.output/'enumerator.exe'),str(out/'image.bin'),str(out/'source.bitmap'),str(out/'entries.tsv'),str(out/'refused.tsv')]
        run=subprocess.run(command,capture_output=True,text=True,timeout=120)
        require(run.returncode==0,'entry_enumeration_failed');counts=json.loads(run.stdout)
        require(counts['candidates']==sum(bitmap)==counts['admitted']+counts['refused'],'entry_denominator')
        entries=[line.split() for line in (out/'entries.tsv').read_text().splitlines()]
        require(len(entries)==counts['admitted'] and len({line[0] for line in entries})==len(entries),'entry_key_or_count_collision')
        all_keys.update(line[0] for line in entries)
        record={'original_file_offset':row['file_offset'],'original_VMA':row['VMA'],'image_sha256':row['image_sha256'],
                'uploaded_bitmap_sha256':sha(out/'source.bitmap'),'counts':counts,'enumeration_command':command,
                'entries':str(out/'entries.tsv'),'entries_sha256':sha(out/'entries.tsv'),
                'refusals':str(out/'refused.tsv'),'refusals_sha256':sha(out/'refused.tsv')}
        if a.generate:
            command=[str(a.output/'generator.exe'),str(out/'image.bin'),str(out/'entries.tsv'),str(out/'generated')]
            with (out/'generation.log').open('wb') as log:
                run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=1800)
            record['generation']={'argv':command,'exit_code':run.returncode,'log_sha256':sha(out/'generation.log')}
            record['generated_files']=[{'path':str(path),'sha256':sha(path),'bytes':path.stat().st_size} for path in sorted((out/'generated').glob('*.cpp'))]
            # A refusal does not suppress independent original payloads.
            record['generation_pass']=run.returncode==0
        records.append(record)
        for key in total:total[key]+=counts[key]
        print(json.dumps({'payload':f"{row['file_offset']:08X}",**counts,
                          'generated':record.get('generation_pass'),'free_GiB':round(shutil.disk_usage(a.output).free/(1<<30),2)}),flush=True)
    require(source_keys<=all_keys,'earlier_source_admitted_entry_missing')
    require(pins=={path:sha(Path(path)) for path in pins},'input_changed')
    result={'status':'ALL_UPLOADED_ENTRY_ENUMERATION_COMPLETE','totals':total,'unique_keys':len(all_keys),
            'earlier_source_keys_covered':len(source_keys),'records':records,'input_pins':pins,'compile_commands':commands,
            'generation_requested':a.generate,'generation_all_pass':all(r['generation_pass'] for r in records) if a.generate else None,
            'scope':'Conservative original uploaded-entry superset. Source-unwritten reachable sets refuse. No runtime or native credit. Generation/compilation/equivalence/callability/full-game closure are distinct gates.'}
    target=a.output/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
    print(json.dumps({'result':str(target),'sha256':sha(target),'totals':total,'unique_keys':len(all_keys),'generation_all_pass':result['generation_all_pass']}))

if __name__=='__main__':
    main()
