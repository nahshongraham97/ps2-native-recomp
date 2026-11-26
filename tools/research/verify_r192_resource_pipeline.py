"""Whole original-builder programs versus the typed resource-pipeline batch.

Constructed descriptor cases are source-derived test inputs, not observed game
invocations. This compiles original AOT references offline; the native backend
contains only resource algorithms, never an instruction-handler stream.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as original
import audit_r192_effect_schema as schema
import specialize_r192_stub_family as family


def word(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def effect(ident, destination, source=0):
    row = [0]*12
    row[0], row[2], row[4] = ident, destination, source
    if ident in (5, 6, 26):
        row[4:7] = [word(.02), word(-.03), word(.04)]
    if ident == 17:
        row[4:10] = [source, 0, word(.25), word(4.), 255, 0]
    return row


def cases():
    attributes = [[256, 0, 0], [259, 0, 16], [257, 0, 32], [258, 0, 12], [769, 1, 0],
                  [770, 0, 64], [777, 0, 72]]
    effects = [effect(1, 769), effect(2, 256, 256), effect(3, 769, 258),
               effect(25, 769, 258), effect(26, 256), effect(5, 256),
               effect(6, 258), effect(17, 257, 769), effect(22, 0),
               effect(21, 0), effect(24, 0)]
    rows = []
    def add(name, attrs, sequence, filters=0):
        rows.append(dict(name=name, attributes=attrs, effects=sequence, filters=filters))
    for tag, _ in [*(row for _, table, _ in schema.TABLES for row in [(r[0], r[2]) for r in table]), (0x201, 1)]:
        add(f'tag_{tag:X}_identity', [[tag, 0, 0]], [])
    for i, first in enumerate(effects):
        for j, second in enumerate(effects):
            if first[0] == second[0] == 17:
                continue
            for filtered in (0, 1):
                add(f'pair_{i}_{j}_f{filtered}', attributes, [first, second], filtered)
    for ident in (2, 3, 25):
        add(f'id{ident}_vector_to_class2', attributes, [effect(ident, 257, 256)])
        add(f'id{ident}_scalar_reverse', attributes, [effect(ident, 258, 769)])
    for ident in (5, 6, 26):
        add(f'id{ident}_scalar', attributes, [effect(ident, 769)])
        add(f'id{ident}_vector', attributes, [effect(ident, 256)])
    add('all_five_callbacks', attributes, [effects[i] for i in (0, 1, 2, 4, 5, 6, 7, 8)], 1)
    add('early17_final_source', attributes, [effects[i] for i in (7, 0, 2, 6)], 1)
    add('thirteen_scalar_updates', attributes, [effects[0]]*13, 1)
    add('reordered_attributes', list(reversed(attributes)), [effects[i] for i in (7, 0, 2, 6)], 1)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['builder-proof', 'schema-proof', 'boundary', 'candidate-build',
                 'source-fork', 'compiler', 'output']:
        parser.add_argument('--'+name, type=Path, required=True)
    a = parser.parse_args()
    out = a.output.resolve()
    original.need(out.is_relative_to((original.W/'build/oracle-telemetry').resolve()) and not out.exists(), 'fresh_owned_output_required')
    receipt = a.builder_proof/'RESULT.json'
    original.need(original.pin(receipt)['sha256'] == schema.BUILDER_RECEIPT, 'builder_proof_changed')
    record = json.loads(receipt.read_text())
    source = a.builder_proof/'original_builder.cpp'
    original.need(any(original.pin(source) == row for row in record['source_pins']), 'original_builder_fixture_changed')
    schema_receipt = a.schema_proof/'RESULT.json'
    sr = json.loads(schema_receipt.read_text())
    original.need(sr['status'] == 'PASS_BOUNDED_ORIGINAL_SCHEMA_PROOF' and not sr['input_changes'], 'schema_proof_not_passed')
    for pin in sr['source_pins']:
        original.need(original.pin(Path(pin['path'])) == pin, 'schema_input_changed')
    generator = a.source_fork/'build/r192-aot-closure/out_tools/eerecomp.exe'
    original.need(original.pin(generator)['sha256'] == family.GENERATOR_SHA, 'original_v8_generator_changed')
    backend = original.W/'tools/research/r192_aot_modules/semantic_resource_pipeline.hpp'
    cohort = cases()
    out.mkdir(parents=True)
    raw_rows = ',\n'.join('{"'+row['name']+'",{'+','.join('{'+','.join(map(str, item))+'}' for item in row['attributes'])+'},{'+','.join('{'+','.join(str(w)+'u' for w in e)+'}' for e in row['effects'])+'},'+str(row['filters'])+'}' for row in cohort)
    builder_cpp = out/'original_composition_builder.cpp'
    prefix = source.read_text().split('static void read(', 1)[0]
    builder_cpp.write_text(prefix + BUILD.replace('@ROWS@', raw_rows).replace('@BOUNDARY@', a.boundary.resolve().as_posix()).replace('@OUTPUT@', out.as_posix()), encoding='utf8', newline='\n')
    headers = a.candidate_build/'src/runtime/include'
    compiler_args = [str(a.compiler), '-std=c++20', '-mavx2', '-ffp-contract=off', '-O1', '-static-libgcc', '-static-libstdc++', '-I'+str(headers.resolve())]
    commands = []
    def run(command, name):
        process = subprocess.run(command, capture_output=True)
        (out/(name+'.log')).write_bytes(process.stdout+process.stderr)
        commands.append(dict(argv=command, exit=process.returncode, log=original.pin(out/(name+'.log'))))
        return process
    inputs = [Path(__file__), Path(schema.__file__), Path(original.__file__), Path(family.__file__),
              receipt, source, schema_receipt, backend, backend.with_name('semantic_vector_effects.hpp'), a.compiler, generator,
              *(a.boundary/n for n in ['Cpu.bin','RAM.bin','SPR.bin']), *headers.glob('ps2/*.hpp')]
    pins = [original.pin(x) for x in inputs]
    builder_exe = out/'original_composition_builder.exe'
    original.need(run([*compiler_args, str(builder_cpp), '-o', str(builder_exe)], 'compile-builder').returncode == 0, 'original_builder_compile_failed')
    emitted = run([str(builder_exe)], 'original-builder')
    original.need(emitted.returncode == 0, 'original_composition_builder_failed')
    extents = {n:(int(w),int(c)) for n,w,c in re.findall(r'CASE (\w+) words=(\d+) code=(\d+)', emitted.stdout.decode())}
    original.need(set(extents) == {x['name'] for x in cohort}, 'original_composition_denominator_changed')
    functions, program_records, rows = [], [], []
    for case in cohort:
        name = case['name']; total, code = extents[name]
        raw = out/(name+'.bin'); data = raw.read_bytes()
        original.need(len(data) == (total+4)*4 and len(data) <= 4096, 'original_program_extent_changed')
        window = out/(name+'-constructed-window.bin')
        window.write_bytes(data + b'\xff'*max(0, 1024-len(data)))
        generated = out/name
        command = [str(generator.resolve()), '--live-image', str(window), '--base', '01C00000', '--entry', '01C00000', '--stamp', 'A2659FA9BD846661', '--dense-checks', '--out', str(generated)]
        original.need(run(command, name+'-generator').returncode == 0, 'original_whole_program_generation_failed:'+name)
        unit = next(p for p in generated.glob('m_*unit_*.cpp') if 'L_01C00000:' in p.read_text())
        text = unit.read_text(); body = text[text.index('L_01C00000:'):].split('\n}\n',1)[0]
        original.need(not re.search(r'\b(?:call|dispatch|unit_\w+)\s*\(',body), 'unproved_external_program_call:'+name)
        functions.append('static void ref_'+name+'(Cpu&c){'+body+'}\n')
        # Extract full vector constant fetches from the original generated
        # prelude solely for this offline test's descriptor transport.
        fetched = re.findall(r'ld128\(\(c\.r\[11\]\.w\[0\](?: \+ (\d+)u)?\) & ~15u, c\.vu0\.vf\[(\d+)\]\);', body)
        fetches = [row[0] for row in fetched]
        vector_effects = [e for e in case['effects'] if e[0] in (5,6,26) and schema_class(e[2]) == 1]
        original.need(len(fetches) == len(vector_effects), 'vector_constant_full_fetch_transport_unproved:'+name)
        constants = [struct.unpack_from('<4I',data,code*4+(int(offset) if offset else 0)) for offset in fetches]
        init = '[](){Descriptor d{};'
        for i, attr in enumerate(case['attributes']):init += f'd.attributes[{i}]={{'+','.join(map(str,attr))+'};'
        init += f'd.attribute_count={len(case["attributes"])};d.effect_count={len(case["effects"])};d.filter_count={case["filters"]};d.scalar_step=d.vector_step=0x3f000000;'
        vf_loads = {(int(base)-4,int(offset) if offset else 0):int(reg) for base,offset,reg in re.findall(r'ld128\(\(c\.r\[([45])\]\.w\[0\](?: \+ (\d+)u)?\) & ~15u, c\.vu0\.vf\[(\d+)\]\);',body)}
        resource_regs = [vf_loads[(attr[1],attr[2])] if schema_class(attr[0])<=2 else -1 for attr in case['attributes']]
        constant_regs = [-1]*13
        vi=0
        for i,e in enumerate(case['effects']):
            init += f'd.effects[{i}].id={e[0]};d.effects[{i}].destination_tag={e[2]};d.effects[{i}].source_tag={e[4]};'
            if e[0] in (5,6,26):
                value=constants[vi] if schema_class(e[2])==1 else e[4:7]+[0]
                if schema_class(e[2])==1:constant_regs[i]=int(fetched[vi][1])
                vi += schema_class(e[2])==1
                init += f'd.effects[{i}].constant={{'+','.join(str(w)+'u' for w in value)+'};'
            if e[0]==17:init += f'd.effects[{i}].remap={{255,0,{word(.25)}u,{word(4.)}u}};'
        init += 'd.filters[0]={2,769,0x3f800000};return d;}()'
        rows.append('{"'+name+'",'+str(total)+','+str(code)+',&ref_'+name+','+init+',{'+','.join(map(str,resource_regs+[-1]*(20-len(resource_regs))))+'},{'+','.join(map(str,constant_regs))+'}}')
        program_records.append(dict(**case, total_words=total, code_words=code,
            original_output=original.pin(raw), original_generated=original.pin(unit),
            offline_reference_VF_bindings=dict(attributes=resource_regs,constants=constant_regs,scope='Offline comparison transport authenticated by original generated load expressions; no runtime binding admission'),
            constant_full_fetches=[dict(offset=int(x) if x else 0,words=list(v)) for x,v in zip(fetches,constants)]))
    harness = prefix.split('static std::map<unsigned,unsigned> entries_observed;',1)[0]
    # Original bodies call only inline helpers; stores are observed before the
    # actual original memory mutation, including transient/overlapping writes.
    harness += TRACE + '\n#define st128 trace_st128\n#define st32 trace_st32\n#define st64 trace_st64\n'+''.join(functions)+'\n#undef st128\n#undef st32\n#undef st64\n'
    harness += '#include "'+backend.as_posix()+'"\nusing namespace mka::emitted::resources;\n'+CHECK.replace('@ROWS@', ',\n'.join(rows)).replace('@OUTPUT@', out.as_posix())
    cpp = out/'resource_pipeline_differential.cpp';cpp.write_text(harness,encoding='utf8',newline='\n')
    exe=out/'resource_pipeline_differential.exe'
    original.need(run([*compiler_args,str(cpp),'-o',str(exe)],'compile-differential').returncode==0,'resource_pipeline_compile_failed')
    checks=[]
    for name,flag,expected in [('positive','',0),('wrong-value','--planted-value',1),('wrong-order','--planted-order',1),('lost-transient','--planted-transient',1),('early-remap','--planted-early-remap',1),('wrong-budget','--planted-budget',1),('wrong-vector-flags','--planted-vector-flags',1)]:
        command=[str(exe)]+([flag] if flag else []); cp=run(command,name)
        checks.append(dict(name=name,exit=cp.returncode,expected_exit=expected,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[x['path'] for x in pins if original.pin(Path(x['path']))!=x]
    passed=not changes and all(r['exit']==r['expected_exit'] for r in checks)
    result=dict(schema='r192_typed_resource_pipeline_compositions_v1',status='PASS_BOUNDED_RESOURCE_OUTPUT_PIPELINES' if passed else 'FAIL',
        source_pins=pins,input_changes=changes,commands=commands,checks=checks,programs=program_records,
        artifacts=[original.pin(x) for x in [builder_cpp,builder_exe,cpp,exe]],
        scope='Constructed whole original-builder-generated programs versus typed native output pipeline. Ordered writes/all RAM/accepted count/scalar FCR+accumulator/full VU via offline source-assigned resource bindings/no-yield budget. Not full Cpu/scheduler/continuation admission or actual game invocation.',
        limits=['Unique known tags; two streams; aligned nonnegative signed16 offsets; at most13 effects and one valid filter; unsupported IDs/duplicate17/bank mismatches refused.',
                'Vector constants carry all four fetched words, including measured adjacent allocation bytes; no invented zero tail.',
                'Per-configuration actual register mapping/native scratch/VU and true continuation proofs remain OPEN; existing captured-family adapter is unchanged.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],sha256=original.pin(dest)['sha256'],result=str(dest))))
    return 0 if passed else 1


def schema_class(tag):
    return next((r[2] for _,table,_ in schema.TABLES for r in table if r[0]==tag),1 if tag==0x201 else 0)


BUILD=r'''
struct Case{std::string name;std::vector<std::array<unsigned,3>>attrs;std::vector<std::array<unsigned,12>>effects;unsigned filters;};
int main(){try{
 Cpu initial{};auto read=[](const char*n,void*p,unsigned size){std::ifstream f(std::string("@BOUNDARY@/")+n,std::ios::binary);f.read(static_cast<char*>(p),size);if(!f||f.peek()!=EOF)throw std::runtime_error("boundary size");};
 read("Cpu.bin",&initial,sizeof initial);read("RAM.bin",g_ram,kRamSize);read("SPR.bin",g_spr,kScratchpadSize);const std::vector<unsigned char>captured(g_ram,g_ram+kRamSize);
 const Case cases[]={@ROWS@};
 for(const auto&test:cases){std::memcpy(g_ram,captured.data(),kRamSize);Cpu c=initial;const auto object=initial.r[19].w[0];
  constexpr unsigned output=0x1c00000,arena=0x1c10000,blocks=0x1c20000,list=0x1c30000;
  std::memset(g_ram+output,0x95,8000);st32(object+192,test.effects.size());st32(object+80,test.filters);
  for(unsigned i=0;i<test.effects.size();++i)std::memcpy(g_ram+object+208+i*48,test.effects[i].data(),48);
  for(unsigned i=0;i<test.attrs.size();++i)std::memcpy(g_ram+list+i*12,test.attrs[i].data(),12);st32(list+test.attrs.size()*12,0xffffffff);
  if(test.filters){std::memset(g_ram+object+84,0,24);st16(object+84,2);st32(object+88,769);st32(object+104,0x3f800000);}
  c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=output;c.r[7].d[0]=2000;c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=blocks;c.r[11].d[0]=400;
  c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;reference(c,0x2ed4b0);
  unsigned words=c.r[2].w[0],code=ld32(object+0x394);if(!words||words>1996||code>words||c.pc!=0x100000)throw std::runtime_error("constructed descriptor original refusal");
  std::ofstream f(std::string("@OUTPUT@/")+test.name+".bin",std::ios::binary);f.write(reinterpret_cast<const char*>(g_ram+output),(words+4)*4);if(!f)throw std::runtime_error("builder output");
  std::printf("CASE %s words=%u code=%u\n",test.name.c_str(),words,code);
 }return 0;}catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''

TRACE=r'''
#include <span>
struct Event{unsigned address,width;std::array<std::byte,16>value;bool operator==(const Event&)const=default;};
static std::vector<Event>events;
static void observe(void*,const std::byte*to,std::span<const std::byte>value){Event event{unsigned(to-reinterpret_cast<std::byte*>(g_ram)),unsigned(value.size()),{}};std::memcpy(event.value.data(),value.data(),value.size());events.push_back(event);}
static void trace_st128(unsigned a,const Reg128&v){observe(nullptr,reinterpret_cast<std::byte*>(g_ram+a),{reinterpret_cast<const std::byte*>(&v),16});st128(a,v);}
static void trace_st32(unsigned a,unsigned v){observe(nullptr,reinterpret_cast<std::byte*>(g_ram+a),{reinterpret_cast<const std::byte*>(&v),4});st32(a,v);}
static void trace_st64(unsigned a,std::uint64_t v){observe(nullptr,reinterpret_cast<std::byte*>(g_ram+a),{reinterpret_cast<const std::byte*>(&v),8});st64(a,v);}
'''

CHECK=r'''
struct Program{const char*name;unsigned total,code;void(*run)(Cpu&);Descriptor descriptor;std::array<int,20>resource_vf;std::array<int,13>constant_vf;};
static Program programs[]={@ROWS@};
int main(int argc,char**argv){try{const std::string mode=argc>1?argv[1]:"";unsigned comparisons=0;
 for(auto&program:programs)for(unsigned count:{0u,1u,8u})for(unsigned alias:{0u,1u})for(unsigned numeric:{0u,1u,2u}){
  std::memset(g_ram,0x5a,kRamSize);std::ifstream f(std::string("@OUTPUT@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),(program.total+4)*4);if(!f||f.peek()!=EOF)throw std::runtime_error("program size");
  constexpr unsigned input_a=0x100000,input_b=0x200000;const unsigned output_a=alias?input_a+96:0x300000,output_b=alias?input_b:0x400000;
  constexpr unsigned special[]={0u,0x80000000u,0x7fffffffu,0x7fc00000u,0xff800000u,1u,0xbf800000u,0x7f7fffffu};
  for(unsigned i=0;i<10;++i){for(unsigned j=0;j<24;++j)st32(input_a+i*96+j*4,numeric?special[(i+j+numeric)%8]:0x3f000000+i*0x100000+j*0x10000);st32(input_b+i*16,numeric?special[(i+numeric)%8]:0x3f000000+i*0x100000);}
  Cpu c{};c.pc=0x1c00000;c.r[4].d[0]=input_a;c.r[5].d[0]=input_b;c.r[6].d[0]=output_a;c.r[7].d[0]=output_b;c.r[8].d[0]=96;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code*4;c.r[12].d[0]=0x1c00000;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.fcr31=0x00800201;c.f[12]=c.vu0.vf[1].w[0]=0x3f000000;c.vu0.vf[0].w[3]=0x3f800000;c.budget=10000000;
  for(unsigned reg=1;reg<32;++reg)for(unsigned lane=0;lane<4;++lane)c.vu0.vf[reg].w[lane]=0x3f800000+reg*0x100+lane;
  for(unsigned lane=0;lane<4;++lane)c.vu0.acc.w[lane]=special[lane+numeric];
  const unsigned step=numeric==0?0x3f000000:numeric==1?0x80000000:0x7f7fffff;
  c.fcr31=numeric==0?0x00800201:numeric==1?0xffffffff:0;c.f[12]=c.vu0.vf[1].w[0]=step;c.vu0.mac=0x5321;c.vu0.status=numeric?0xffff:0x1234;c.facc=0x5a5a5a5a;
  const auto initial_vu=c.vu0;const auto initial_facc=c.facc,initial_flags=c.fcr31;
  const auto before=std::vector<unsigned char>(g_ram,g_ram+kRamSize);events.clear();program.run(c);const auto expected=std::vector<unsigned char>(g_ram,g_ram+kRamSize);const auto original_events=events;
  if(c.pc!=0x100000||c.r[29].d[0]!=0x1fff000)throw std::runtime_error("original continuation");
  std::memcpy(g_ram,before.data(),kRamSize);events.clear();
  Streams streams{{Stream{{reinterpret_cast<std::byte*>(g_ram+input_a),960},{reinterpret_cast<std::byte*>(g_ram+output_a),960},96},Stream{{reinterpret_cast<std::byte*>(g_ram+input_b),160},{reinterpret_cast<std::byte*>(g_ram+output_b),160},16}},count};
  auto descriptor=program.descriptor;descriptor.scalar_step=descriptor.vector_step=step;
  if(mode=="--planted-early-remap"&&std::string(program.name)=="early17_final_source")descriptor.effects[1].id=21;
  const auto prepared=prepare(descriptor,streams);auto timed=run_no_yield(streams,prepared,10000000,{nullptr,&observe});const auto&result=timed.result;
  if(mode=="--planted-budget")++timed.remaining_budget;
  if(mode=="--planted-value"&&!events.empty()){g_ram[events[0].address]^=1;events[0].value[0]^=std::byte{1};}
  if(mode=="--planted-order"&&events.size()>1)std::swap(events[0],events[1]);
  if(mode=="--planted-transient"&&!events.empty())events.erase(events.begin());
  auto projected_vu=initial_vu;
  for(unsigned i=0;i<descriptor.effect_count;++i)if(program.constant_vf[i]>=0)std::memcpy(&projected_vu.vf[program.constant_vf[i]],prepared.vector_constants[i].data(),16);
  if(result.element_observed)for(unsigned i=0;i<descriptor.attribute_count;++i)if(program.resource_vf[i]>=0)std::memcpy(&projected_vu.vf[program.resource_vf[i]],result.last_resources[i].vector.data(),16);
  if(result.vector_written){projected_vu.mac=result.vector.last_mac;projected_vu.status=result.vector.project_status(initial_vu.status);}
  if(result.vector_accumulator_written)std::memcpy(&projected_vu.acc,result.vector_accumulator.data(),16);
  if(mode=="--planted-vector-flags"&&result.vector_written)projected_vu.mac^=1;
  unsigned flags=initial_flags|result.scalar_flags;if(result.condition_written)flags=(flags&~0x00800000u)|(result.condition?0x00800000u:0);
  if(result.accepted!=c.r[2].w[0]||flags!=c.fcr31||(result.scalar_accumulator_written?result.scalar_accumulator:initial_facc)!=c.facc||events!=original_events||timed.remaining_budget!=c.budget||std::memcmp(&projected_vu,&c.vu0,sizeof projected_vu)||std::memcmp(g_ram,expected.data(),kRamSize)){
   unsigned first=0;while(first<kRamSize&&g_ram[first]==expected[first])++first;
   std::fprintf(stderr,"RESOURCE_PIPELINE_DIFFERENCE %s count=%u alias=%u numeric=%u returns=%u/%u flags=%X/%X writes=%zu/%zu budget=%lld/%lld VUdiff=%d first_ram=%X\n",program.name,count,alias,numeric,result.accepted,c.r[2].w[0],flags,c.fcr31,events.size(),original_events.size(),(long long)timed.remaining_budget,(long long)c.budget,std::memcmp(&projected_vu,&c.vu0,sizeof projected_vu),first);return 1;}
  const auto memory_before_refusal=std::vector<unsigned char>(g_ram,g_ram+kRamSize);const auto event_count=events.size();bool refused=false;
  try{run_no_yield(streams,prepared,static_cast<std::int64_t>(cost(timing(descriptor),count,count))-1,{nullptr,&observe});}catch(const std::runtime_error&e){refused=std::string(e.what()).find("insufficient budget")!=std::string::npos;}
  if(!refused||events.size()!=event_count||std::memcmp(g_ram,memory_before_refusal.data(),kRamSize))throw std::runtime_error("prewrite insufficient budget refusal failed");
  ++comparisons;
 }
 unsigned refusals=0;auto reject=[&](Descriptor d,Streams streams,const char*reason){try{prepare(d,streams);}catch(const std::runtime_error&e){if(std::string(e.what()).find(reason)!=std::string::npos){++refusals;return;}}throw std::runtime_error("named invalid descriptor accepted or wrong reason");};
 Streams empty{};auto valid=programs[20].descriptor,bad=valid;
 bad.effects[0].id=0x7fff;reject(bad,empty,"unsupported original effect ID");
 bad=valid;bad.effects[0].id=2;bad.effects[0].source_tag=257;bad.effects[0].destination_tag=256;reject(bad,empty,"MADD banks incompatible");
 bad=valid;bad.attributes[0].stream=2;reject(bad,empty,"stream, signed16 offset");
 bad=valid;bad.attributes[0].offset=0xffff;reject(bad,empty,"signed16 offset");
 bad=valid;bad.attributes[6].offset=73;reject(bad,empty,"alignment unproved");
 bad=valid;bad.filter_count=2;reject(bad,empty,"descriptor count");
 bad=valid;bad.effects[0].id=bad.effects[1].id=17;bad.effects[0].destination_tag=bad.effects[1].destination_tag=257;bad.effects[0].source_tag=bad.effects[1].source_tag=769;reject(bad,empty,"duplicate17");
 bad=valid;bad.attribute_count=7;unsigned tags[]={769,773,774,258,259,1026,1027};for(unsigned i=0;i<7;++i)bad.attributes[i]={tags[i],0,i*4};bad.effect_count=13;for(auto&e:bad.effects){e.id=6;e.destination_tag=769;}reject(bad,empty,"allocation capacity");
 auto invalid_count=empty;invalid_count.count=0xffffffff;reject(valid,invalid_count,"signed iteration count");
 try{cost({129,1,1},1,1);throw std::logic_error("bad coefficient accepted");}catch(const std::runtime_error&){++refusals;}
 try{cost({1,1,1},1,2);throw std::logic_error("bad accepted count accepted");}catch(const std::runtime_error&){++refusals;}
 std::printf("PASS typed_resource_programs=%zu whole_output_comparisons=%u prewrite_insufficient_budget_refusals=%u named_domain_controls=%u full_RAM_ordered_width_value_return_FCR_facc_fullVU_budget=1 full_Cpu_continuation_admission=0\n",std::size(programs),comparisons,comparisons,refusals);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''


if __name__ == '__main__':
    raise SystemExit(main())
