"""Compile sealed offline VU DLLs from every audited original-source entry.

This builds and inspects native modules only. It starts no game, installs no
loader and changes no original/reference input. Runtime integration is separate.
"""
from pathlib import Path
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
from extract_r192_elf_vu import require,sha

ROOT=Path(__file__).resolve().parents[2]
FLAGS=['-std=c++20','-O2','-mavx2','-ffp-contract=off','-DNDEBUG']
FOOTER=r'''
#include <cstdio>
#include <cstdlib>
namespace { ps2::vu::LiveHost offline_host{}; }
namespace ps2::vu {
void interpret(int unit,std::uint32_t pc){if(!offline_host.interpret){std::fputs("VU_AOT_UNBOUND_CHAIN\n",stderr);std::abort();}offline_host.interpret(unit,pc);}
void kick(int unit,std::uint32_t address){if(!offline_host.kick){std::fputs("VU_AOT_UNBOUND_KICK\n",stderr);std::abort();}offline_host.kick(unit,address);}
std::uint32_t top(){if(!offline_host.top)std::abort();return offline_host.top();}
std::uint32_t itop(){if(!offline_host.itop)std::abort();return offline_host.itop();}
}
extern "C" __declspec(dllexport) std::uint64_t mka_vu_aot_stamp(){return @STAMP@ull;}
extern "C" __declspec(dllexport) unsigned mka_vu_aot_regs_size(){return sizeof(ps2::Vu0Regs);}
extern "C" __declspec(dllexport) unsigned mka_vu_aot_host_size(){return sizeof(ps2::vu::LiveHost);}
extern "C" __declspec(dllexport) const ps2::vu::Program* mka_vu_aot_init(const ps2::vu::LiveHost* host,unsigned* count){
 if(!host||!count||!host->interpret||!host->kick||!host->top||!host->itop)return nullptr;
 offline_host=*host;*count=@COUNT@u;return gen::kVuPrograms;
}
'''

PROBE=r'''
#include "ps2/vu_ops.hpp"
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
static void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
static void forbidden(int,unsigned){std::fputs("REFUSED unexpected program execution\n",stderr);std::exit(20);}
static unsigned forbidden_value(){std::fputs("REFUSED unexpected program execution\n",stderr);std::exit(20);}
static bool within(std::uintptr_t base,std::size_t size,std::uintptr_t p,std::size_t n){return p>=base&&n<=size&&p-base<=size-n;}
int wmain(int argc,wchar_t**argv){try{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 if(argc<4||argc>5)return 2;
 const std::wstring plant=argc==5?argv[4]:L"";
 auto expected_stamp=std::stoull(argv[2],nullptr,16);
 if(plant==L"--plant-stamp")expected_stamp^=1;
 HMODULE module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 check(module!=nullptr,"load_native_module");MODULEINFO info{};
 check(GetModuleInformation(GetCurrentProcess(),module,&info,sizeof info),"module_image_info");
 auto symbol=[&](const char*name){const auto p=GetProcAddress(module,name);check(p!=nullptr,"missing_module_export");return p;};
 auto stamp=reinterpret_cast<std::uint64_t(*)()>(symbol("mka_vu_aot_stamp"));
 auto regs=reinterpret_cast<unsigned(*)()>(symbol("mka_vu_aot_regs_size"));
 auto hostsize=reinterpret_cast<unsigned(*)()>(symbol("mka_vu_aot_host_size"));
 auto init=reinterpret_cast<const ps2::vu::Program*(*)(const ps2::vu::LiveHost*,unsigned*)>(symbol("mka_vu_aot_init"));
 check(stamp()==expected_stamp,"source_ABI_stamp");
 check(regs()==sizeof(ps2::Vu0Regs)+(plant==L"--plant-abi"?8:0)&&hostsize()==sizeof(ps2::vu::LiveHost),"native_ABI_sizes");
 ps2::vu::LiveHost host{forbidden,forbidden,forbidden_value,forbidden_value};unsigned n=0xc0dec0de;
 check(init(nullptr,&n)==nullptr&&n==0xc0dec0de,"null_host_not_refused");
 check(init(&host,nullptr)==nullptr,"null_count_not_refused");
 auto bad=host;bad.kick=nullptr;check(init(&bad,&n)==nullptr&&n==0xc0dec0de,"incomplete_host_not_refused");
 const auto table=init(&host,&n);std::vector<std::uint64_t> expected;
 std::ifstream in{std::filesystem::path(argv[3])};std::uint64_t key;unsigned entry,words;
 while(in>>std::hex>>key>>entry>>std::dec>>words)expected.push_back(key);
 check(in.eof()&&!expected.empty(),"entry_manifest_read");std::sort(expected.begin(),expected.end());
 if(plant==L"--plant-count")expected.push_back(0);
 if(plant==L"--plant-key")expected[0]^=1;
 check(n==expected.size(),"native_program_count");
 const auto base=reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
 check(table&&within(base,info.SizeOfImage,reinterpret_cast<std::uintptr_t>(table),std::size_t(n)*sizeof *table),"program_table_image_bounds");
 for(unsigned i=0;i<n;++i){
  check(table[i].key==expected[i],"native_program_key");
  auto address=reinterpret_cast<std::uintptr_t>(table[i].function);
  if(i==0&&plant==L"--plant-function")address=1;
  check(within(base,info.SizeOfImage,address,1),"native_function_image_bounds");
  MEMORY_BASIC_INFORMATION region{};check(VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof region)==sizeof region,"native_function_page_query");
  check(region.State==MEM_COMMIT&&(region.Protect&0xf0)&&!(region.Protect&PAGE_GUARD),"native_function_not_executable");
 }
 std::printf("PASS_NATIVE_MODULE programs=%u regs=%zu host=%zu null_controls=3 no_program_execution=1\n",n,sizeof(ps2::Vu0Regs),sizeof host);
 FreeLibrary(module);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"MODULE_REFUSED:%s\n",e.what());return 1;}}
'''

def response_arg(value):
    return '"'+str(value).replace('\\','/').replace('"','\\"')+'"'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('generation','audit','source-root','compiler','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-generation-sha',required=True);p.add_argument('--expected-audit-sha',required=True)
    p.add_argument('--only-payload',type=lambda s:int(s,16))
    p.add_argument('--jobs',type=int,default=2);p.add_argument('--reserve-gib',type=int,default=12)
    p.add_argument('--resume',action='store_true')
    a=p.parse_args();a.output=a.output.resolve()
    require(a.output.is_relative_to(ROOT/'build') and 1<=a.jobs<=4 and a.reserve_gib>=8,'owned_output_and_resource_bounds')
    require(sha(a.generation)==a.expected_generation_sha and sha(a.audit)==a.expected_audit_sha,'input_receipt_pin')
    generation=json.loads(a.generation.read_text());audit=json.loads(a.audit.read_text())
    require(generation['generation_all_pass'] and audit['status']=='BODY_IDENTITY_AND_SUPPORTED_LOWER_AUDIT_PASS','complete_source_generation_and_body_audit_required')
    headers={str(path):sha(path) for path in sorted((a.source_root/'runtime/include/ps2').glob('*.hpp'))}
    require(headers,'missing_frozen_headers')
    pins={'generation_sha256':sha(a.generation),'audit_sha256':sha(a.audit),'compiler_sha256':sha(a.compiler),
          'script_sha256':sha(Path(__file__)),'headers':headers,'flags':FLAGS,'only_payload':a.only_payload}
    stamp='0x'+hashlib.sha256(json.dumps(pins,sort_keys=True).encode()).hexdigest()[:16]
    if a.output.exists():
        require(a.resume and json.loads((a.output/'START.json').read_text())['input_pins']==pins,'resume_input_identity')
        if (a.output/'RESULT.json').exists():
            completed=json.loads((a.output/'RESULT.json').read_text())
            require(completed['input_pins']==pins and completed['status']=='NATIVE_VU_MODULE_BUILD_COMPLETE','completed_resume_identity')
            for item in completed['module_receipts']:
                record=Path(item['path']);require(sha(record)==item['sha256'],'completed_module_receipt_changed')
                module=json.loads(record.read_text());require(sha(Path(module['DLL']))==module['DLL_sha256'],'completed_module_binary_changed')
            print(json.dumps({'status':'VERIFIED_COMPLETED_BUILD_UNCHANGED','result':str(a.output/'RESULT.json'),
                              'sha256':sha(a.output/'RESULT.json'),'modules':completed['modules']}))
            return
    else:
        require(not a.resume,'resume_output_missing');a.output.mkdir(parents=True)
        (a.output/'START.json').write_text(json.dumps({'input_pins':pins,'source_ABI_stamp':stamp,'scope':'Native-object/DLL build and metadata validation only; no game execution or registration.'},indent=2)+'\n',newline='\n')
    def space(extra=0):require(shutil.disk_usage(a.output).free>=(a.reserve_gib<<30)+extra+(256<<20),'disk_reserve_exhausted_originals_preserved')
    env=dict(os.environ);env['PATH']=str(a.compiler.parent)+os.pathsep+env.get('PATH','')
    probe_attempt=1
    while (a.output/f'probe-attempt-{probe_attempt:03}').exists():probe_attempt+=1
    probe_folder=a.output/f'probe-attempt-{probe_attempt:03}';probe_folder.mkdir()
    probe_cpp=probe_folder/'probe.cpp';probe=probe_folder/'probe.exe'
    probe_cpp.write_text(PROBE,newline='\n')
    command=[str(a.compiler),*FLAGS,'-static-libgcc','-static-libstdc++','-municode','-I'+str(a.source_root/'runtime/include'),str(probe_cpp),'-lpsapi','-o',str(probe)]
    run=subprocess.run(command,capture_output=True,env=env,timeout=180)
    (probe_folder/'build.log').write_bytes(run.stdout+run.stderr);require(run.returncode==0,'native_probe_compile_failed')
    rows=[r for r in generation['records'] if a.only_payload is None or r['original_file_offset']==a.only_payload]
    require(rows,'payload_selection_empty');results=[];start=time.monotonic()
    for ordinal,row in enumerate(rows):
        space();module=a.output/f"payload-{row['original_file_offset']:08X}"
        receipt=module/'RESULT.json'
        if receipt.exists():
            old=json.loads(receipt.read_text());require(old['input_pins']==pins and sha(Path(old['DLL']))==old['DLL_sha256'],'resume_module_changed')
            require(old['status']=='COMPILED_NATIVE_MODULE_METADATA_PASS','refused_module_requires_new_output')
            results.append(old);continue
        if module.exists():
            require(a.resume,'partial_module_requires_explicit_resume')
            preserved=module.with_name(module.name+'-interrupted-'+str(time.time_ns()))
            require(module.resolve().is_relative_to(a.output) and preserved.resolve().is_relative_to(a.output) and not preserved.exists(),
                    'interrupted_module_move_outside_owned_output')
            module.rename(preserved) # Keep partial objects/logs; never delete them.
        module.mkdir(exist_ok=False);objects=module/'objects';objects.mkdir()
        source=Path(row['entries']).parent/'generated';sources=[]
        for file in row['generated_files']:
            path=source/Path(file['path']).name;require(sha(path)==file['sha256'],'actual_generated_source_changed')
            sources.append(path)
        table=(source/'vu1_table.cpp').read_text()
        table,n=re.subn(r'void register_generated_vu\(\) \{ ps2::vu::register_programs\(gen::kVuPrograms, \d+u\); \}\s*$',
                        lambda _:FOOTER.replace('@STAMP@',stamp).replace('@COUNT@',str(row['counts']['admitted'])),table)
        require(n==1,'static_table_footer_changed')
        adapted=module/'vu1_table.cpp';adapted.write_text(table,newline='\n');sources=[p for p in sources if p.name!='vu1_table.cpp']+[adapted]
        def compile_one(path):
            space();target=objects/(path.stem+'.obj');command=[str(a.compiler),*FLAGS,'-I'+str(a.source_root/'runtime/include'),'-c',str(path),'-o',str(target)]
            began=time.monotonic();run=subprocess.run(command,capture_output=True,env=env,timeout=900)
            log=objects/(path.stem+'.log');log.write_bytes(run.stdout+run.stderr)
            return {'source':str(path),'source_sha256':sha(path),'object':str(target),'object_sha256':sha(target) if target.exists() else None,
                    'argv':command,'exit_code':run.returncode,'seconds':time.monotonic()-began,'log_sha256':sha(log)}
        with ThreadPoolExecutor(max_workers=a.jobs) as pool:compiled=list(pool.map(compile_one,sources))
        passed=all(c['exit_code']==0 for c in compiled);dll=module/'programs.dll';link_receipt=None;checks=[]
        if passed:
            space(2*sum(Path(c['object']).stat().st_size for c in compiled))
            argv=['-shared','-static-libgcc','-static-libstdc++',*(c['object'] for c in compiled),'-o',str(dll)]
            response=module/'link.rsp';response.write_text('\n'.join(response_arg(arg) for arg in argv)+'\n',newline='\n')
            command=[str(a.compiler),'@'+str(response)];began=time.monotonic()
            run=subprocess.run(command,capture_output=True,env=env,timeout=600);(module/'link.log').write_bytes(run.stdout+run.stderr)
            link_receipt={'argv':command,'response_sha256':sha(response),'exit_code':run.returncode,'seconds':time.monotonic()-began};passed=run.returncode==0
        if passed:
            for option in ['',*(['--plant-stamp','--plant-abi','--plant-count','--plant-key','--plant-function'] if ordinal==0 else [])]:
                argv=[str(probe),str(dll),stamp,str(row['entries']),*([option] if option else [])]
                run=subprocess.run(argv,capture_output=True,env=env,timeout=60)
                log=module/('probe-'+(option.removeprefix('--') or 'positive')+'.log');log.write_bytes(run.stdout+run.stderr)
                expected=1 if option else 0
                checks.append({'argv':argv,'exit_code':run.returncode,'expected_exit':expected,'log_sha256':sha(log)})
                passed&=run.returncode==expected and (b'MODULE_REFUSED:' in run.stderr if option else b'PASS_NATIVE_MODULE' in run.stdout)
        result={'status':'COMPILED_NATIVE_MODULE_METADATA_PASS' if passed else 'REFUSED_NATIVE_MODULE_BUILD_OR_METADATA','input_pins':pins,
                'original_file_offset':row['original_file_offset'],'programs':row['counts']['admitted'],'DLL':str(dll),
                'DLL_sha256':sha(dll) if dll.exists() else None,'compiled_sources':compiled,'link':link_receipt,'metadata_checks':checks,
                'source_ABI_stamp':stamp,'probe_sha256':sha(probe),'adapted_table_sha256':sha(adapted),
                'scope':'Ahead-of-time native code and exact table/ABI/image validation. No program execution, game registration, whole-function equivalence or native credit.'}
        receipt.write_text(json.dumps(result,indent=2)+'\n',newline='\n');results.append(result)
        print(json.dumps({'payload':f"{row['original_file_offset']:08X}",'status':result['status'],'programs':result['programs'],'DLL_bytes':dll.stat().st_size if dll.exists() else None,'free_GiB':round(shutil.disk_usage(a.output).free/(1<<30),2)}),flush=True)
    require(pins['headers']=={path:sha(Path(path)) for path in pins['headers']},'frozen_headers_changed')
    final={'status':'NATIVE_VU_MODULE_BUILD_COMPLETE' if all(r['status']=='COMPILED_NATIVE_MODULE_METADATA_PASS' for r in results) else 'NATIVE_VU_MODULE_BUILD_HAS_REFUSALS',
           'input_pins':pins,'modules':len(results),'program_instances':sum(r['programs'] for r in results),
           'complete_original_payload_set':len(rows)==len(generation['records']),'elapsed_seconds':time.monotonic()-start,
           'module_receipts':[{'path':str(Path(r['DLL']).parent/'RESULT.json'),'sha256':sha(Path(r['DLL']).parent/'RESULT.json')} for r in results],
           'scope':'Native compilation/metadata only. Runtime loader, execution equivalence and full-game acceptance remain separate.'}
    output=a.output/'RESULT.json';require(not output.exists(),'final_receipt_immutable')
    output.write_text(json.dumps(final,indent=2)+'\n',newline='\n');print(json.dumps({'result':str(output),'sha256':sha(output),'status':final['status'],'modules':len(results)}))

if __name__=='__main__':
    main()
