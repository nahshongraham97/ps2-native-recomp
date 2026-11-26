"""Compare effective construction inputs to actual compiled original routines.

No game launch or runtime replacement. Original2ED4B0 constructs both the raw
and effective-prefix programs; original2E8110/2E7A30 prove copy/relocation effects.
The constructed allocation witnesses do not authenticate a production lifetime.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth

HOST = r'''
#include "@HEADER@"
#include <iostream>
#include <iterator>
using namespace mka::emitted::reference_resources;
using namespace mka::emitted::reference_resources::construction;
struct Test {const char*name;std::int32_t count;std::vector<RawFilter>raw;unsigned expected;FilterBreak stop;};
static void load_elf(){std::ifstream f("@ELF@",std::ios::binary);std::vector<unsigned char>raw((std::istreambuf_iterator<char>(f)),{});
 auto u32=[&](unsigned at){unsigned v;if(at+4>raw.size())throw std::runtime_error("ELF bounds");std::memcpy(&v,raw.data()+at,4);return v;};
 auto u16=[&](unsigned at){unsigned short v;if(at+2>raw.size())throw std::runtime_error("ELF bounds");std::memcpy(&v,raw.data()+at,2);return v;};
 std::memset(g_ram,0,kRamSize);for(unsigned i=0;i<u16(44);++i){auto at=u32(28)+i*u16(42);if(u32(at)!=1)continue;auto from=u32(at+4),to=u32(at+8),size=u32(at+16);if(from>raw.size()||size>raw.size()-from||to>kRamSize||size>kRamSize-to)throw std::runtime_error("ELF segment bounds");std::memcpy(g_ram+to,raw.data()+from,size);}}
struct Built {unsigned words,pool;std::vector<unsigned char>output;std::vector<unsigned char>descriptor;};
static Built build(const Test&t,const std::vector<RawFilter>&filters,std::int32_t count){
 load_elf();constexpr unsigned object=0x1b00000,out=0x1c00000,arena=0x1c10000,blocks=0x1c20000,list=0x1c30000;
 std::memset(g_ram+out,0x95,8000);std::memset(g_ram+object,0,928);
 st32(object+192,2);for(unsigned i=0;i<2;++i){st16(object+208+48*i,1);st32(object+216+48*i,769);}
 st32(object+80,unsigned(count));
 for(unsigned i=0;i<filters.size();++i){auto at=object+84+i*24;st16(at,unsigned(short(filters[i].id)));st32(at+4,filters[i].source_tag);st32(at+16,filters[i].linked_descriptor);st32(at+20,filters[i].limit);}
 const unsigned attrs[][3]={{769,0,0},{256,0,16}};std::memcpy(g_ram+list,attrs,sizeof attrs);st32(list+sizeof attrs,0xffffffff);
 Cpu c{};c.r[28].d[0]=0x6540f0;c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=out;c.r[7].d[0]=2000;c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=blocks;c.r[11].d[0]=400;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;
 reference(c,0x2ed4b0);if(c.pc!=0x100000||!c.r[2].w[0]||c.r[2].w[0]>2000)throw std::runtime_error(std::string("original constructor refusal:")+t.name);
 return {c.r[2].w[0],ld32(object+916),{g_ram+out,g_ram+out+8000},{g_ram+object,g_ram+object+928}};
}
int main(int argc,char**argv){try{
 const std::string mode=argc>1?argv[1]:"";
 const RawFilter valid{2,769,0,0x3f800000},linked{2,769,0x1b01000,0x3f800000},unknown{99,0xdeadbeef,0,0},bank{2,256,0,0};
 const Test cases[]={
 {"empty",0,{},0,FilterBreak::none},{"valid",1,{valid},1,FilterBreak::none},
 {"linked",1,{linked},0,FilterBreak::linked_descriptor},{"unknown",1,{unknown},0,FilterBreak::unknown_id},
 {"non_scalar",1,{bank},0,FilterBreak::non_scalar_bank},
 {"prefix_linked_suffix",3,{valid,linked,valid},1,FilterBreak::linked_descriptor},
 {"prefix_unknown_suffix",3,{valid,unknown,valid},1,FilterBreak::unknown_id},
 {"first_unknown_suffix",3,{unknown,valid,valid},0,FilterBreak::unknown_id},
 {"four_valid",4,{valid,valid,valid,valid},4,FilterBreak::none},
 {"negative_count",-1,{},0,FilterBreak::none},
 {"large_count_first_break",1000,{linked},0,FilterBreak::linked_descriptor}};
 const Attribute attrs[]={{769,0,0},{256,0,16}};const Effect effects[]={{1,769,0},{1,769,0}};
 unsigned programs=0,flags=0,relocations=0;
 for(const auto&t:cases){auto input=capture_plan_input(attrs,effects,t.raw,t.count);auto effective=input.outcome;
  if(mode=="--planted-ignore-link"&&t.raw.size()&&t.raw[0].linked_descriptor){effective.effective.push_back({2,769,valid.limit});}
  if(mode=="--planted-continue-after-break"&&t.name==std::string("prefix_linked_suffix"))effective.effective.push_back({2,769,valid.limit});
  if(effective.effective.size()!=t.expected||effective.reason!=t.stop)throw std::runtime_error("effective prefix outcome mismatch");
  auto original=build(t,t.raw,t.count);std::vector<RawFilter>native;for(const auto&f:effective.effective)native.push_back({2,f.source_tag,0,f.limit});auto prefix=build(t,native,int(native.size()));
  if(original.words!=prefix.words||original.pool!=prefix.pool||original.output!=prefix.output)throw std::runtime_error("whole original program differs from effective prefix");
  for(unsigned i=0;i<t.raw.size()&&i<4;++i){const auto flag=original.descriptor[84+24*i+3];if(flag!=(i<t.expected?1:0))throw std::runtime_error("original successful filter flag mismatch");++flags;}
  if(input.runtime_lifetime_admitted)throw std::runtime_error("constructed events admitted runtime lifetime");
  std::cout<<"CASE "<<t.name<<" PASS effective="<<native.size()<<" attempted="<<effective.attempted<<" words="<<original.words<<"\n";++programs;
 }
 // Actual original descriptor copy followed by original pointer relocation.
 load_elf();constexpr unsigned owner=0x1a00000,table=owner+0x1000,final0=owner+0x2000,final1=final0+928,temp=0x1b00000,temp1=temp+928,oldtable=0x1b10000;
 std::memset(g_ram+temp,0,1856);st32(temp+80,1);st32(temp+100,temp1);st32(temp1+80,1);st32(temp1+100,temp);
 st32(owner+524,2);st32(owner+528,table);st32(table,final0);st32(table+4,final1);st32(oldtable,temp);st32(oldtable+4,temp1);
 Cpu c{};c.r[4].d[0]=owner;c.r[5].d[0]=2;c.r[6].d[0]=temp;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=100000;copy_reference(c,0x2e8110);
 if(c.pc!=0x100000||std::memcmp(g_ram+temp,g_ram+final0,928)||std::memcmp(g_ram+temp1,g_ram+final1,928))throw std::runtime_error("original 928 copy mismatch");
 c={};c.r[4].d[0]=table;c.r[5].d[0]=oldtable;c.r[6].d[0]=2;c.r[31].d[0]=0x100000;c.budget=100000;copy_reference(c,0x2e7a30);
 const Bits old[]={temp,temp1},fresh[]={final0,final1};
 for(unsigned i=0;i<2;++i){const auto previous=i?temp:temp1,where=i?final1:final0;auto predicted=relocated_filter_reference(previous,old,fresh);if(mode=="--planted-keep-temporary-reference")predicted=previous;
  if(ld32(where+100)!=predicted)throw std::runtime_error("original descriptor reference relocation mismatch");++relocations;}
 bool refused=false;try{source_filter_outcome(attrs,{&valid,1},2);}catch(const std::exception&){refused=true;}if(!refused)throw std::runtime_error("missing original record accepted");
 std::cout<<"PASS effective_construction original_program_pairs="<<programs<<" filter_flags="<<flags<<" actual_original_928_copies=2 actual_original_relocations="<<relocations<<" missing_record_refusals=1 runtime_lifetime_admission=0\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"REFUSED "<<e.what()<<"\n";return 2;}}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('elf','listing','original-builder','generated-directory','candidate-build','compiler','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    auth.need(shutil.disk_usage(out.parent).free>(12<<30)+(16<<20),'12GiB_reserve_required')
    auth.need(auth.pin(a.elf)['sha256']==auth.ELF_SHA and auth.pin(a.listing)['sha256']==auth.LISTING_SHA,'original_inputs_changed')
    auth.need(auth.pin(a.original_builder)['sha256']=='ad4dd9a564da533f8d7e9532452fc638cbed346049293b6aba765eeab05489c1','original_builder_fixture_changed')
    elf_raw=a.elf.read_bytes();section_offset=struct.unpack_from('<I',elf_raw,32)[0];section_size,section_count=struct.unpack_from('<HH',elf_raw,46)
    reginfo=[]
    for i in range(section_count):
        section=struct.unpack_from('<10I',elf_raw,section_offset+i*section_size)
        if section[1]==0x70000006:reginfo.append(dict(file_offset=section[4],gp=struct.unpack_from('<I',elf_raw,section[4]+20)[0]))
    auth.need(len(reginfo)==1 and reginfo[0]['gp']==0x6540f0,'original_ELF_GP_changed')
    original=a.original_builder.read_text();auth.need(original.count('int main(')==1,'original_main_seam_changed');original=original.split('int main(',1)[0]
    pieces=[];inputs=[Path(__file__),Path(auth.__file__),a.elf,a.listing,a.original_builder,a.compiler]
    for unit,lo,hi in [('0079',0x2e7a30,0x2e7ac0),('0079',0x2e7f90,0x2e7fd0),('007A',0x2e8110,0x2e8190)]:
        path=a.generated_directory/f'unit_{unit}.cpp';inputs.append(path);text=path.read_text();start=text.index(f'L_{lo:08X}:');end=text.index(f'L_{hi:08X}:');pieces.append(text[start:end])
    body='\n'.join(pieces);labels=re.findall(r'^L_([0-9A-F]{8}):',body,re.M)
    pattern=r'if \(depth < kMaxDepth\) unit_\w+\(c, \d+u, depth \+ 1u\); else \{ c.pc = (0x[0-9A-F]+u); return; \}'
    def call(m):
        pc=int(m.group(1)[:-1],16);auth.need(pc in (0x2e7f90,0x1db8a8),'unexpected_original_copy_call')
        return ('reference' if pc==0x1db8a8 else 'copy_reference')+'(c,'+m.group(1)+',depth+1u);'
    body=re.sub(pattern,call,body);auth.need(not re.search(r'\bunit_\w+\(',body),'copy_call_mapping_incomplete')
    copy_source='static void copy_reference(Cpu&c,unsigned entry,unsigned depth=0){switch(entry){'+''.join(f'case0x{x}u:goto L_{x};' for x in labels).replace('case0x','case 0x')+'default:throw std::runtime_error("copy reference entry");}\n'+body+'}\n'
    header=auth.W/'tools/research/r192_aot_modules/effective_resource_construction.hpp'
    inputs += [header,header.parent/'reference_resource_layout.hpp',header.parent/'semantic_resource_pipeline.hpp',header.parent/'semantic_vector_effects.hpp',*(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')]
    pins=[auth.pin(q) for q in dict.fromkeys(inputs)]
    elf=auth.Original(a.elf.read_bytes());joins=[]
    with a.listing.open(encoding='utf8',newline='') as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            pc=int(row['address'],16)
            if any(lo<=pc<hi for lo,hi in [(0x2ed300,0x2ed3d8),(0x2ed8c8,0x2ed960),(0x2e7a30,0x2e7ac0),(0x2e7f90,0x2e7fd0),(0x2e8110,0x2e8190)]):
                word=int.from_bytes(bytes.fromhex(row['bytes']),'little');auth.need(elf.word(pc)==word,'original_word_join_failed');joins.append(row)
    out.mkdir(parents=True);cpp=out/'effective_construction.cpp';cpp.write_text(original+copy_source+HOST.replace('@HEADER@',header.as_posix()).replace('@ELF@',a.elf.resolve().as_posix()),encoding='utf8',newline='\n')
    exe=out/'effective_construction.exe';command=[str(a.compiler),'-std=c++20','-O1','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
    cp=subprocess.run(command,capture_output=True,timeout=120);(out/'compile.log').write_bytes(cp.stdout+cp.stderr);auth.need(cp.returncode==0,'original_comparison_compile_failed')
    runs=[]
    for name,args,expected in [('positive',[],0),('ignore-link',['--planted-ignore-link'],2),('continue-after-break',['--planted-continue-after-break'],2),('keep-temporary',['--planted-keep-temporary-reference'],2)]:
        argv=[str(exe),*args];run=subprocess.run(argv,capture_output=True,timeout=120);(out/(name+'.log')).write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,argv=argv,exit=run.returncode,expected=expected,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace')))
    changes=[p['path'] for p in pins if auth.pin(Path(p['path']))!=p]
    passed=not changes and all(r['exit']==r['expected'] for r in runs)
    result=dict(status='PASS_ORIGINAL_EFFECTIVE_CONSTRUCTION' if passed else 'FAIL',source_pins=pins,input_changes=changes,original_ELF_reginfo=reginfo,
                original_word_joins=joins,compile_argv=command,runs=runs,artifacts=[auth.pin(q) for q in (cpp,exe)],
                scope='Constructed whole original builder comparisons and actual original928 copy/descriptor-reference relocation. No observed producer call, runtime hook, Plan lifetime, numerical execution or native admission.',
                command=[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
    (out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],receipt=auth.pin(out/'RESULT.json'))));return 0 if passed else 2


if __name__=='__main__':raise SystemExit(main())
