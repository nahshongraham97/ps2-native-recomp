"""Offline original-generator/translator/native parameter-space differential.

The 15 full-recording signatures are test seeds, never runtime whitelists.
Enumerates every two-stream assignment for each seed; larger integer/numeric
domains use explicit boundaries and seeded joint samples, not an exhaustiveness
claim. No game, Live worker, runtime compiler or protected output is invoked.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import random
import re
import shutil
import struct
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth
import audit_r192_effect_schema as schema
import specialize_r192_stub_family as family
import verify_r192_resource_pipeline as pipeline

CENSUS_SHA='827d55e09d3707c4eb333f4a2b25a115793daaef1ccf20611a1652af53934e97'
SPECIAL=[0,0x80000000,1,0x007fffff,0x00800000,0x3f800000,0xbf800000,0x7f7fffff,
         0xff7fffff,0x7f800000,0xff800000,0x7fc00000,0x7fffffff,0xffffffff,0x3f000000,0x00800001]
CORE=auth.W/'tools/research/r192_aot_modules'


def triples(raw):
    values=[int(x) for x in raw.strip(':').split(':')] if raw else []
    auth.need(len(values)%3==0,'malformed_signature_triples')
    return [values[i:i+3] for i in range(0,len(values),3)]


def seeds(record):
    full=record['cases'][0]
    auth.need(full['name']=='full_recording' and full['summary']['matched_signatures']==15,'exact_full_recording_15_required')
    rows=[]
    for r in full['rows']:
        if not r['equal']:continue
        key=r['candidate_signature'];m=re.fullmatch(r'a(.*)e(.*)f(.*)\|code_identity_only',key)
        auth.need(m is not None and r['intact'],'unproved_signature_seed')
        attrs,effects=triples(m[1]),triples(m[2]);f=[int(x) for x in m[3].strip(':').split(':')]
        auth.need(f==[2,769],'unexpected_seed_filter')
        rows.append(dict(signature=key,attributes=attrs,effects=effects,filters=1))
    auth.need(len(rows)==15,'seed_denominator_changed')
    return rows


def cohort(patterns,seed,random_cases):
    rng=random.Random(seed);rows=[];coverage=[]
    for p,pattern in enumerate(patterns):
        made=[]
        def add(label,attrs=None,numeric=5,filters=1,numeric_random=False):
            attrs=copy.deepcopy(attrs if attrs is not None else pattern['attributes'])
            e=[]
            for k,(ident,dest,source) in enumerate(pattern['effects']):
                raw=pipeline.effect(ident,dest,source)
                if ident in (5,6,26):raw[4:7]=[SPECIAL[(numeric+k+j)%len(SPECIAL)] for j in range(3)]
                if ident==17:
                    # Original source producer computes scale from signed upper
                    # and lower, float start and extent; no invented clamp.
                    integers=[0,1,-1,255,-255,32767,-32768,0x7fffffff,-0x80000000]
                    raw[6]=SPECIAL[numeric%16];raw[7]=SPECIAL[(numeric+5)%16]
                    raw[8]=integers[numeric%len(integers)]&0xffffffff
                    raw[9]=integers[(numeric+3)%len(integers)]&0xffffffff
                if numeric_random:
                    if ident in (5,6,26):raw[4:7]=[rng.getrandbits(32) for _ in range(3)]
                    if ident==17:raw[6:10]=[rng.getrandbits(32) for _ in range(4)]
                e.append(raw)
            row=dict(name=f'p{p:02}_{label}',pattern=p,attributes=attrs,effects=e,filters=filters,
                     filter_limit=rng.getrandbits(32) if numeric_random else SPECIAL[(numeric+2)%16],sample=label,numeric_index=numeric,numeric_random=numeric_random)
            made.append(row);rows.append(row)
        for mask in range(1<<len(pattern['attributes'])):
            attrs=copy.deepcopy(pattern['attributes'])
            for i,a in enumerate(attrs):a[1]=(mask>>i)&1
            add(f's{mask:03}',attrs)
        for i,a in enumerate(pattern['attributes']):
            align=16 if pipeline.schema_class(a[0])<=2 else 8 if pipeline.schema_class(a[0])==5 else 4
            for value in sorted({0,align,32768-align,rng.randrange(32768//align)*align}):
                attrs=copy.deepcopy(pattern['attributes']);attrs[i][2]=value
                add(f'o{i}_{value}',attrs)
        for n in range(16):add(f'n{n:02}',numeric=n)
        add('unfiltered',filters=0)
        add('reverse',list(reversed(pattern['attributes'])))
        for n in range(random_cases):
            attrs=copy.deepcopy(pattern['attributes']);rng.shuffle(attrs)
            for a in attrs:
                align=16 if pipeline.schema_class(a[0])<=2 else 8 if pipeline.schema_class(a[0])==5 else 4
                a[1]=rng.randrange(2);a[2]=rng.randrange(32768//align)*align
            add(f'r{n:02}',attrs,numeric=rng.randrange(16),filters=rng.randrange(2),numeric_random=True)
        coverage.append(dict(pattern=p,signature=pattern['signature'],descriptor_cases=len(made),
            exhaustive_stream_assignments=1<<len(pattern['attributes']),offset_fields=len(pattern['attributes']),
            numeric_boundary_sets=16,seeded_joint_sets=random_cases,filter_enable_values=[0,1]))
    return rows,coverage


BUILD=pipeline.BUILD.replace('unsigned filters;','unsigned filters,limit;').replace('st32(object+104,0x3f800000);','st32(object+104,test.limit);')

CHECK=r'''
#include <iostream>
struct Program{const char*name;unsigned total,code;void(*run)(Cpu&);Descriptor descriptor;};
static Program programs[]={@ROWS@};
static bool guard(void*,std::int64_t&){throw std::runtime_error("unexpected constructed no-yield guard");}
int main(int argc,char**argv){try{const std::string mode=argc>1?argv[1]:"";unsigned compared=0,differences=0;
 constexpr unsigned stride=32768,input_a=0x100000,input_b=0x500000;
 constexpr unsigned special[]={0u,0x80000000u,1u,0x007fffffu,0x00800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0x7fffffffu,0xffffffffu,0x3f000000u,0x00800001u};
 for(unsigned pi=0;pi<std::size(programs);++pi){const auto&program=programs[pi];
  for(unsigned count:{0u,1u,7u})for(unsigned alias:{0u,1u})for(unsigned numeric:{0u,1u}){
   std::memset(g_ram,0x5a,kRamSize);std::memset(g_spr,0x95,kScratchpadSize);
   std::ifstream f(std::string("@OUTPUT@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),(program.total+4)*4);if(!f||f.peek()!=EOF)throw std::runtime_error("original program byte extent");
   const unsigned output_a=alias?input_a+stride:0x900000,output_b=alias?input_b:0xd00000;
   // Initialize full bounded spans, including repeated/overlapping resources.
   for(unsigned stream:{input_a,input_b})for(unsigned i=0;i<8*stride;i+=4)st32(stream+i,numeric?special[(i/4+pi)%16]:0x3e800000u+((i/4+pi)%16)*0x10000u);
   Cpu c{};for(unsigned r=0;r<32;++r){c.r[r].d[0]=0x1122334455660000ull+r;c.r[r].d[1]=0xa5a55a5afedc0000ull+r;c.f[r]=special[(r+pi)%16];for(unsigned j=0;j<4;++j)c.vu0.vf[r].w[j]=special[(r+j+pi)%16];}
   c.r[0]={};c.vu0.vf[0]={};c.vu0.vf[0].w[3]=0x3f800000;c.pc=0x1c00000;c.r[4].d[0]=input_a;c.r[5].d[0]=input_b;c.r[6].d[0]=output_a;c.r[7].d[0]=output_b;c.r[8].d[0]=stride;c.r[9].d[0]=stride;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code*4;c.r[12].d[0]=0x1c00000;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.f[12]=c.vu0.vf[1].w[0]=special[(pi+numeric*7)%16];c.budget=10000000;c.fcr31=numeric?0xffffffff:0x00800201;c.vu0.status=numeric?0xffff:0x1234;c.vu0.mac=0x5321;c.facc=special[pi%16];
   const auto initial=c;const auto before=std::vector<unsigned char>(g_ram,g_ram+kRamSize);events.clear();program.run(c);const auto expected=std::vector<unsigned char>(g_ram,g_ram+kRamSize);const auto original_events=events;
   std::memcpy(g_ram,before.data(),kRamSize);events.clear();Streams streams{{Stream{{reinterpret_cast<std::byte*>(g_ram+input_a),8*stride},{reinterpret_cast<std::byte*>(g_ram+output_a),8*stride},stride},Stream{{reinterpret_cast<std::byte*>(g_ram+input_b),8*stride},{reinterpret_cast<std::byte*>(g_ram+output_b),8*stride},stride}},count};
   auto d=program.descriptor;d.scalar_step=d.vector_step=initial.f[12];if(mode=="--planted-parameter")d.attributes[0].offset^=16;const auto prepared=prepare(d,streams);Continuation native(streams,d);auto boundary=native.advance(initial.budget,{nullptr,&guard},{nullptr,&observe});auto projected=mka::emitted::reference_resources::project(initial,0x1c00000,prepared,boundary);
   if(mode=="--planted-value"&&!events.empty()){events[0].value[0]^=std::byte{1};g_ram[events[0].address]^=1;}
   if(mode=="--planted-order"&&events.size()>1)std::swap(events[0],events[1]);
   if(mode=="--planted-preserved-lane")projected.r[31].d[1]=0;
   if(mode=="--planted-budget")++projected.budget;
   if(std::memcmp(&projected,&c,sizeof c)||events!=original_events||std::memcmp(g_ram,expected.data(),kRamSize)){
    unsigned cb=0;while(cb<sizeof c&&reinterpret_cast<unsigned char*>(&projected)[cb]==reinterpret_cast<unsigned char*>(&c)[cb])++cb;
    unsigned rb=0;while(rb<kRamSize&&g_ram[rb]==expected[rb])++rb;
    std::cerr<<"PARAMETER_DIFFERENCE "<<program.name<<" count="<<count<<" alias="<<alias<<" numeric="<<numeric<<" cpu_byte="<<cb<<" ram_byte="<<rb<<" stores="<<events.size()<<"/"<<original_events.size()<<"\n";++differences;if(!mode.empty())return 1;continue;
   }++compared;
  }
  if(pi%100==0)std::cerr<<"progress programs="<<pi+1<<" comparisons="<<compared<<"\n";
 }
 std::cout<<(differences?"FAIL":"PASS")<<" parameter_programs="<<std::size(programs)<<" full_Cpu_RAM_ordered_store_comparisons="<<compared<<" differences="<<differences<<" original_builder_and_offline_translator=1 runtime_admission=0\n";return differences?1:0;
}catch(const std::exception&e){std::cerr<<"REFUSED "<<e.what()<<"\n";return 2;}}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('census','builder-proof','boundary','source-fork','candidate-build','compiler','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--reuse-generated-proof',type=Path)
    p.add_argument('--seed',type=int,default=0x1925A15);p.add_argument('--random-cases',type=int,default=16)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    auth.need(a.random_cases>=16,'at_least16_seeded_joint_cases_per_pattern')
    def reserve():auth.need(shutil.disk_usage(out.parent).free>(12<<30)+(32<<20),'12GiB_reserve_required')
    reserve();auth.need(auth.pin(a.census)['sha256']==CENSUS_SHA,'15_signature_census_changed')
    patterns=seeds(json.loads(a.census.read_text()));cases,coverage=cohort(patterns,a.seed,a.random_cases)
    receipt=a.builder_proof/'RESULT.json';auth.need(auth.pin(receipt)['sha256']==schema.BUILDER_RECEIPT,'original_builder_proof_changed')
    br=json.loads(receipt.read_text());source=a.builder_proof/'original_builder.cpp';auth.need(any(auth.pin(source)==r for r in br['source_pins']),'original_builder_source_changed')
    generator=a.source_fork/'build/r192-aot-closure/out_tools/eerecomp.exe';auth.need(auth.pin(generator)['sha256']==family.GENERATOR_SHA,'original_offline_translator_changed')
    inputs=[Path(__file__),Path(auth.__file__),Path(pipeline.__file__),Path(schema.__file__),Path(family.__file__),a.census,receipt,source,a.compiler,generator,
            *(a.boundary/n for n in ('Cpu.bin','RAM.bin','SPR.bin')),*(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'),
            *(CORE/n for n in ('semantic_resource_pipeline.hpp','semantic_vector_effects.hpp','reference_resource_layout.hpp','resource_pipeline_continuation.hpp','reference_resource_projection.hpp'))]
    for row in br['source_pins']:
        auth.need(auth.pin(Path(row['path']))==row,'original_builder_dependency_changed');inputs.append(Path(row['path']))
    reused=None;reuse_rows={};permitted_subject_changes=[]
    if a.reuse_generated_proof:
        reused=json.loads(a.reuse_generated_proof.read_text());auth.need(reused['seed']==a.seed,'reuse_seed_changed')
        auth.need(all(x['exit']==0 for x in reused['commands'] if x['log']['path'].endswith(('compile-builder.log','original-builder.log','-translate.log'))),'reused_generator_failed')
        for command in reused['commands']:
            if command['log']['path'].endswith(('compile-builder.log','original-builder.log','-translate.log')):
                q=Path(command['log']['path']);auth.need(auth.pin(q)==command['log'],'reused_generator_log_changed');inputs.append(q)
        for pin in reused['source_pins']:
            if Path(pin['path']).resolve()==Path(__file__).resolve():
                old_subject=a.reuse_generated_proof.parent/'failed-source.py';auth.need(auth.pin(old_subject)['sha256']==pin['sha256'],'reused_old_subject_snapshot_missing')
                inputs.append(old_subject);permitted_subject_changes.append(dict(before=auth.pin(old_subject),after=auth.pin(Path(__file__))))
            else:auth.need(auth.pin(Path(pin['path']))==pin,'reused_original_or_subject_dependency_changed');inputs.append(Path(pin['path']))
        reuse_rows={r['name']:r for r in reused['descriptor_cases']};auth.need(set(reuse_rows)=={r['name'] for r in cases},'reuse_case_population_changed')
        for r in cases:
            prior=reuse_rows[r['name']];auth.need(all(prior[k]==v for k,v in r.items()),'reuse_descriptor_parameters_changed')
            for key in ('original_output','translation'):auth.need(auth.pin(Path(prior[key]['path']))==prior[key],'reused_original_program_or_translation_changed');inputs.append(Path(prior[key]['path']))
        inputs.append(a.reuse_generated_proof)
    pins=[auth.pin(q) for q in dict.fromkeys(inputs)];out.mkdir();commands=[]
    def run(argv,name,timeout=180):
        reserve();cp=subprocess.run(argv,capture_output=True,timeout=timeout);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=auth.pin(log)));return cp
    args=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())]
    rawrows=',\n'.join('{"'+r['name']+'",{'+','.join('{'+','.join(map(str,x))+'}' for x in r['attributes'])+'},{'+','.join('{'+','.join(str(w)+'u' for w in e)+'}' for e in r['effects'])+'},'+str(r['filters'])+','+str(r['filter_limit'])+'u}' for r in cases)
    prefix=source.read_text().split('static void read(',1)[0];builder=out/'parameter_original_builder.cpp';builder.write_text(prefix+BUILD.replace('@ROWS@',rawrows).replace('@BOUNDARY@',a.boundary.resolve().as_posix()).replace('@OUTPUT@',out.as_posix()),newline='\n')
    exe=out/'parameter_original_builder.exe'
    if reused:extents={n:(r['return_words'],r['pool_words']) for n,r in reuse_rows.items()}
    else:
        auth.need(run([*args,str(builder),'-o',str(exe)],'compile-builder').returncode==0,'parameter_builder_compile_failed')
        built=run([str(exe)],'original-builder');auth.need(built.returncode==0,'parameter_original_builder_failed')
        extents={n:(int(w),int(c)) for n,w,c in re.findall(r'CASE (\w+) words=(\d+) code=(\d+)',built.stdout.decode())};auth.need(set(extents)=={r['name'] for r in cases},'parameter_builder_denominator_changed')
    functions=[];rows=[];records=[];translated={}
    for index,r in enumerate(cases):
        reserve();name=r['name'];total,pool=extents[name];raw=Path(reuse_rows[name]['original_output']['path']) if reused else out/(name+'.bin');data=raw.read_bytes();auth.need(len(data)==4*(total+4) and pool<=total,'original_parameter_extent_changed')
        # This is offline test deduplication only. Literal values remain live
        # inputs. Every reused translation has byte-identical executable words
        # and exact pool/total placement; no observed image whitelist is used.
        key=hashlib.sha256(struct.pack('<II',total,pool)+data[:pool*4]).hexdigest()
        if key not in translated:
            if reused:unit=Path(reuse_rows[name]['translation']['path'])
            else:
                window=out/(name+'-window.bin');window.write_bytes(data+b'\xff'*max(0,1024-len(data)));folder=out/name
                argv=[str(generator.resolve()),'--live-image',str(window),'--base','01C00000','--entry','01C00000','--stamp','A2659FA9BD846661','--dense-checks','--out',str(folder)]
                auth.need(run(argv,name+'-translate').returncode==0,'offline_parameter_translation_failed:'+name)
                unit=next(q for q in folder.glob('m_*unit_*.cpp') if 'L_01C00000:' in q.read_text())
            text=unit.read_text();body=text[text.index('L_01C00000:'):].split('\n}\n',1)[0]
            # The offline frontend may discover OTHER entries in arbitrary
            # literal bits. The original producer's exact data-start bounds
            # this selected program; such unrelated entries are not its CFG.
            beyond=[m.start() for m in re.finditer(r'^L_([0-9A-F]{8}):',body,re.M) if int(m[1],16)>=0x1c00000+pool*4]
            if beyond:body=body[:min(beyond)]
            # Source appends JR RA, its zero delay, two unexecuted zeros and
            # optional alignment. Random literals may cause the frontend to
            # discover an entry in that unreachable padding too. Pin the exact
            # original JR/delay and retain the complete translated return.
            raw_words=struct.unpack('<'+str(pool)+'I',data[:pool*4]);returns=[i for i,w in enumerate(raw_words) if w==0x03e00008]
            auth.need(len(returns)==1 and returns[0]+3<pool and raw_words[returns[0]+1:returns[0]+4]==(0,0,0),'original_single_JR_delay_and_padding_unproved')
            terminal='{ const std::uint32_t t = c.r[31].w[0];  c.pc = t; return; }'
            auth.need(body.count(terminal)==1,'original_translated_return_seam_missing')
            body=body[:body.index(terminal)+len(terminal)]
            labels=set(re.findall(r'^L_([0-9A-F]{8}):',body,re.M));targets=set(re.findall(r'goto L_([0-9A-F]{8})',body))
            auth.need(targets<=labels,'original_program_edge_outside_data_start_bound')
            auth.need(not re.search(r'\b(?:call|dispatch|unit_\w+)\s*\(',body),'unproved_parameter_external_call')
            function='ref_'+name;functions.append('static void '+function+'(Cpu&c){'+body+'}\n');translated[key]=(function,unit,body)
        function,unit,body=translated[key]
        fetched=re.findall(r'ld128\(\(c\.r\[11\]\.w\[0\](?: \+ (\d+)u)?\) & ~15u, c\.vu0\.vf\[(\d+)\]\);',body)
        vector_effects=[e for e in r['effects'] if e[0] in (5,6,26) and pipeline.schema_class(e[2])==1];auth.need(len(fetched)==len(vector_effects),'parameter_vector_full_fetch_unproved')
        constants=[struct.unpack_from('<4I',data,pool*4+(int(offset) if offset else 0)) for offset,_ in fetched]
        init='[](){Descriptor d{};'
        for i,attr in enumerate(r['attributes']):init+=f'd.attributes[{i}]={{'+','.join(str(w)+'u' for w in attr)+'};'
        init+=f'd.attribute_count={len(r["attributes"])};d.effect_count={len(r["effects"])};d.filter_count={r["filters"]};'
        vi=0
        for i,e in enumerate(r['effects']):
            init+=f'd.effects[{i}].id={e[0]};d.effects[{i}].destination_tag={e[2]};d.effects[{i}].source_tag={e[4] if e[0] in (2,3,25,17) else 0}u;'
            if e[0] in (5,6,26):
                value=constants[vi] if pipeline.schema_class(e[2])==1 else e[4:7]+[0];vi+=pipeline.schema_class(e[2])==1
                init+=f'd.effects[{i}].constant={{'+','.join(str(w)+'u' for w in value)+'};'
            if e[0]==17:init+=f'd.effects[{i}].remap={{std::int32_t({e[8]}u),std::int32_t({e[9]}u),{e[6]}u,{e[7]}u}};'
        init+=f'd.filters[0]={{2,769,{r["filter_limit"]}u}};return d;}}()'
        rows.append('{"'+name+'",'+str(total)+','+str(pool)+',&'+function+','+init+'}')
        records.append(dict(**r,return_words=total,pool_words=pool,original_output=auth.pin(raw),translation=auth.pin(unit),executable_and_placement_sha256=key))
        if index%100==0:print(json.dumps(dict(stage='translated',done=index+1,total=len(cases))),flush=True)
    harness=prefix.split('static std::map<unsigned,unsigned> entries_observed;',1)[0]+pipeline.TRACE+'\n#define st128 trace_st128\n#define st32 trace_st32\n#define st64 trace_st64\n'+''.join(functions)+'\n#undef st128\n#undef st32\n#undef st64\n'
    programs_root=Path(reuse_rows[cases[0]['name']]['original_output']['path']).parent if reused else out
    harness+='#include "'+(CORE/'reference_resource_projection.hpp').as_posix()+'"\nusing namespace mka::emitted::resources;\n'+CHECK.replace('@ROWS@',',\n'.join(rows)).replace('@OUTPUT@',programs_root.as_posix())
    cpp=out/'parameter_differential.cpp';cpp.write_text(harness,newline='\n');binary=out/'parameter_differential.exe';compiled=run([*args,str(cpp),'-o',str(binary)],'compile-differential',timeout=600);checks=[]
    if compiled.returncode==0:
        for name,flag,expected in [('positive','',0),('value','--planted-value',1),('order','--planted-order',1),('parameter','--planted-parameter',1),('preserved-lane','--planted-preserved-lane',1),('budget','--planted-budget',1)]:
            cp=run([str(binary)]+([flag] if flag else []),name,timeout=900);checks.append(dict(name=name,exit=cp.returncode,expected_exit=expected,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[r['path'] for r in pins if auth.pin(Path(r['path']))!=r];passed=compiled.returncode==0 and len(checks)==6 and all(r['exit']==r['expected_exit'] for r in checks) and not changes
    result=dict(status='PASS_BOUNDED_15_PATTERN_PARAMETER_SPACE' if passed else 'PARAMETER_SPACE_HAS_DIFFERENCES',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,reused_generated_proof=auth.pin(a.reuse_generated_proof) if reused else None,permitted_subject_changes=permitted_subject_changes,seed=a.seed,pattern_coverage=coverage,descriptor_cases=records,distinct_offline_translations=len(translated),commands=commands,checks=checks,
        artifacts=[auth.pin(q) for q in (builder,exe,cpp,binary) if q.exists()],scope='Original source builder plus offline live-image translator complete-body C++ versus typed core and reference-only full Cpu projection. Full RAM and ordered address/width/value stores; constructed no-yield boundaries only. No game/Live worker or ownership/lifetime/runtime adoption claim.',
        limits=['All two-stream assignments are exhaustive for each fixed 15 seed topology. Offsets/32-bit numeric/state domains use boundary and seeded joint samples, not full Cartesian or mathematical universal proof.','Unique known tags; aligned nonnegative signed16 offsets; source-proved intact capacities and one effective filter; tag259 retains aligned128-bit effects.','Original positive partial/clamp descriptors, unknown IDs, duplicate17, zero attributes, negative/unaligned offsets and unproved allocator overflow stay outside this intact batch and are NOT declared invalid game inputs.','Count cases0/1/7, both disjoint and forward/same-stream alias inputs, sticky/NaN/overflow boundary bits; actual scheduler/low-budget/caller lifetime are separate existing proofs.','Translation dedup is exact executable bytes plus pool/total placement only, for this offline test; original generator runs for EVERY descriptor case, and each case loads its own original literal bytes.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],patterns=len(patterns),descriptor_cases=len(cases),translations=len(translated),receipt=auth.pin(target))))
    return 0 if passed else 2


if __name__=='__main__':raise SystemExit(main())
