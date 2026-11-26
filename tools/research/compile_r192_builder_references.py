"""Compile complete original-builder programs as offline semantic references.

These are test-only whole-program C++ references, not production dispatch
templates. They establish neither a generic native backend nor all descriptors.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['builder-proof','source-fork','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.builder_proof/'RESULT.json'
    family.require(family.pin(receipt)['sha256']=='ffa154fa526bd0af42c3404f9454ee45a28a196b9936f82281fc61fa062a603c','builder_proof_pin_changed')
    result=json.loads(receipt.read_text());positive=next(r for r in result['runs'] if r['name']=='positive')
    family.require(result['status']=='PASS_ORIGINAL_DESCRIPTOR_REGENERATION' and positive['exit']==0,'original_builder_proof_not_passed')
    cases=[dict(name='regenerated',words=61,code_words=52)]
    cases += [dict(name=n,words=int(w),code_words=int(c)) for n,w,c in re.findall(r'constructed_variant=(\w+) words=(\d+) code_words=(\d+)',positive['stdout'])]
    family.require(len(cases)==15 and len({c['name'] for c in cases})==15,'builder_reference_denominator_changed')
    generator=a.source_fork/'build/r192-aot-closure/out_tools/eerecomp.exe'
    family.require(family.pin(generator)['sha256']==family.GENERATOR_SHA,'original_v8_generator_changed')
    out.mkdir(parents=True);commands=[];functions=[];records=[];inputs=[receipt,generator,Path(__file__),Path(family.__file__),a.compiler]
    pins={Path(r['path']).resolve():r for r in result['outputs']}
    for case in cases:
        name=case['name'];raw=a.builder_proof/(name+'.bin');observed=family.pin(raw)
        family.require(raw.resolve() in pins and observed['sha256']==pins[raw.resolve()]['sha256'] and observed['size']==case['words']*4,'original_builder_program_changed')
        inputs.append(raw);generated=out/name
        # The old offline frontend requires a minimum1024-byte input. These
        # are CONSTRUCTED proof windows: exact complete producer output plus
        # explicit FF guard bytes, never a reconstructed original image or
        # unknown body tails. The selected program returns before the guard.
        window=out/(name+'-constructed-window.bin');payload=raw.read_bytes()
        family.require(len(payload)<=1024,'constructed_reference_window_bound')
        window.write_bytes(payload+b'\xff'*(1024-len(payload)))
        command=[str(generator.resolve()),'--live-image',str(window.resolve()),'--base','01C00000','--entry','01C00000',
                 '--stamp','A2659FA9BD846661','--dense-checks','--out',str(generated)]
        cp=subprocess.run(command,capture_output=True);(out/(name+'-generator.log')).write_bytes(cp.stdout+cp.stderr)
        family.require(cp.returncode==0,'original_program_AOT_generation_failed:'+name);commands.append(command)
        unit=next(p for p in generated.glob('m_*unit_*.cpp') if 'L_01C00000:' in p.read_text())
        source=unit.read_bytes().decode().replace('\r\n','\n');body=source[source.index('L_01C00000:'):].split('\n}\n',1)[0]
        family.require(not re.search(r'\b(?:call|dispatch|unit_\w+)\s*\(',body),'unproved_external_program_call:'+name)
        functions.append('static void reference_'+name+'(Cpu&c){'+body+'}\n')
        records.append(dict(**case,program_pin=observed,original_AOT_pin=family.pin(unit),constructed_window_pin=family.pin(window),
                            guard_contract='Exact complete original-builder output followed by explicit FF proof guard to satisfy frontend1024-byte minimum; not an original image identity or sparse reconstruction.'))
    prefix=family.HARNESS.split('#define st128',1)[0]
    harness=prefix+'\nusing namespace ps2;\n#define st128 checked_st128\n#define st32 checked_st32\n'+''.join(functions)+'\n#undef st128\n#undef st32\n'
    rows=','.join('{"'+r['name']+'",'+str(r['words'])+','+str(r['code_words'])+',&reference_'+r['name']+'}' for r in records)
    harness+=TEST.replace('@ROWS@',rows).replace('@PROGRAMS@',a.builder_proof.resolve().as_posix())
    cpp=out/'reference_programs.cpp';cpp.write_text(harness,encoding='utf8',newline='\n');binary=out/'reference_programs.exe'
    headers=a.candidate_build/'src/runtime/include'
    inputs += list((headers/'ps2').glob('*.hpp'))
    input_pins=[family.pin(p) for p in inputs]
    command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str(headers.resolve()),str(cpp),'-o',str(binary)]
    cp=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(cp.stdout+cp.stderr)
    family.require(cp.returncode==0,'whole_program_reference_compile_failed')
    runs=[]
    for name,args,expected in [('positive',[],0),('planted-write-outside',['--planted-write-outside'],1),
                               ('planted-return',['--planted-return'],1),('planted-store-width',['--planted-store-width'],1)]:
        run=subprocess.run([str(binary),*args],capture_output=True);(out/(name+'.log')).write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,argv=[str(binary),*args],exit=run.returncode,expected_exit=expected,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace')))
    changed=[r['path'] for r in input_pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=all(r['exit']==r['expected_exit'] for r in runs) and not changed
    record=dict(status='PASS_COMPILED_ORIGINAL_REFERENCES' if passed else 'FAIL',programs=records,generator_commands=commands,compile_command=command,
                source_pins=input_pins,fixture_pins=[family.pin(cpp),family.pin(binary)],input_changes=changed,runs=runs,
                scope='Fifteen complete original-builder programs compiled offline; constructed call-boundary input/output footprint and continuation controls only. No candidate/game launch, ISA interpreter, generic native-backend equivalence, implementation credit or universal descriptor claim.')
    target=out/'RESULT.json';target.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=record['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


TEST=r'''
#include <fstream>
struct Program{const char*name;unsigned words,code_words;void(*run)(Cpu&);};
static Program programs[]={@ROWS@};
static void put(unsigned a,unsigned v){std::memcpy(g_ram+a,&v,4);}
int main(int argc,char**argv){
 const std::string mode=argc>1?argv[1]:"";unsigned cases=0;
 for(const auto&program:programs)for(unsigned count:{0u,1u,8u}){
  std::memset(g_ram,0,kRamSize);std::memset(g_spr,0x95,kScratchpadSize);
  std::ifstream f(std::string("@PROGRAMS@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),program.words*4);if(!f||f.peek()!=EOF)return 2;
  for(unsigned i=0;i<8;++i){for(unsigned j=0;j<12;++j)put(0x100000+i*64+j*4,0x3f800000u+j*0x10000u);put(0x200000+i*16,0x3f800000);}
  Cpu c{};c.pc=0x1c00000;c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=0x300000;c.r[7].d[0]=0x400000;
  c.r[8].d[0]=64;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code_words*4;c.r[12].d[0]=0x1c00000;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;
  c.f[12]=c.vu0.vf[1].w[0]=c.vu0.vf[0].w[3]=0x3f800000;c.budget=10000000;
  std::vector<unsigned char> before(g_ram,g_ram+kRamSize);writes.clear();preempt_calls=0;yield_requested=false;
  program.run(c);
  if(mode=="--planted-write-outside")checked_st32(0x1234,7);
  if(mode=="--planted-return")c.pc^=4;
  if(mode=="--planted-store-width"&&!writes.empty())writes[0].width=2;
  if(c.pc!=0x100000||c.r[2].w[0]!=count||c.r[29].d[0]!=0x1fff000||preempt_calls){std::fprintf(stderr,"REFERENCE_CONTINUATION_OR_COUNT_DIFFERENCE %s count=%u\n",program.name,count);return 1;}
  std::vector<bool> changed(kRamSize,false);
  for(const auto&write:writes){
   bool allowed=false;
   for(unsigned i=0;i<count;++i)if((write.address>=0x300000+i*64&&write.address+write.width<=0x300000+i*64+48)||
                                  (write.address>=0x400000+i*16&&write.address+write.width<=0x400000+i*16+4))allowed=true;
   if(!allowed||(write.width!=4&&write.width!=16)){std::fprintf(stderr,"REFERENCE_WRITE_FOOTPRINT_DIFFERENCE %s address=%X width=%u\n",program.name,write.address,write.width);return 1;}
   for(unsigned i=0;i<write.width;++i)changed[write.address+i]=true;
  }
  for(unsigned i=0;i<kRamSize;++i)if(!changed[i]&&g_ram[i]!=before[i]){std::fprintf(stderr,"REFERENCE_UNOBSERVED_WRITE %s address=%X\n",program.name,i);return 1;}
  ++cases;
 }
 std::printf("PASS compiled_original_programs=%zu constructed_whole_boundary_cases=%u full_native_backend_comparisons=0 source_code_not_runtime_interpreter=1\n",std::size(programs),cases);
}
'''

if __name__=='__main__':
    raise SystemExit(main())
