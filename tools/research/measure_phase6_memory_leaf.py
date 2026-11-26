"""Replay the frozen leaf sample plus a deterministic held-out capability cohort.

Only actual, hash-joined generated statements supply the reference. Native
inputs are mechanical bit-vectors, not recovered receiver types or production
ABI admissions. No emulator, devices, captured-state manufacture or credit.
"""
from __future__ import annotations
import argparse, collections, hashlib, json, os, re, shutil, struct, subprocess, sys, time
from pathlib import Path
import transform_generated_units as tr
import phase6_leaf_lowering as lowering

SEED = 'Phase6:N4:holdout:20261126'
PINS = {'SAMPLE.json':'d5d2e9bc6cf3cd97df0111de20c1a966bdcd05d500e88775da42e361a621aa7e',
        'POPULATION.json':'01239cb9ec1cd96ce5830bfca12e4f0f7beefa0e497a7e9184f4c5abcc7437cb'}
COMPILER_SHA = 'ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe'
CENSUS_SHA = 'a177e25db4e5e49e6b2f2fed059e82079c7d8e5bb1087de00fcc9d0c9cdbd80d'
RESERVE = 12 * 1024**3
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def need(ok, why):
    if not ok: raise RuntimeError(why)
def dump(p, obj): p.write_text(json.dumps(obj, indent=2)+'\n', encoding='utf-8', newline='\n')
def write(p, text): p.write_text(text, encoding='utf-8', newline='\n')
def canonical(obj): return json.dumps(obj, sort_keys=True, separators=(',',':')).encode()
def disk(path): need(shutil.disk_usage(path).free >= RESERVE, '12_GiB_reserve_refused')

class TrackedLeaf(lowering.Leaf):
    """Observe failure locations without changing the owned lowering policy."""
    def sequence(self,name,seq,fallthrough,pc):
        try:return super().sequence(name,seq,fallthrough,pc)
        except tr.Refusal:
            if not hasattr(self,'failure_pc'):self.failure_pc=pc
            raise
    def expr(self,node):
        try:return super().expr(node)
        except tr.Refusal:
            if not hasattr(self,'failure_node'):self.failure_node=node
            raise

def ast_fragment(node,text):
    if not node:return None
    def loc(v):
        while 'expansionLoc' in v:v=v['expansionLoc']
        return v
    a=loc(node.get('range',{}).get('begin',{}));b=loc(node.get('range',{}).get('end',{}))
    if 'offset' not in a or 'offset' not in b:return dict(AST_kind=node.get('kind'))
    return dict(AST_kind=node.get('kind'),start_byte=a['offset'],end_exclusive=b['offset']+b.get('tokLen',1),text=text[a['offset']:b['offset']+b.get('tokLen',1)])

def select(cohort):
    for name,pin in PINS.items(): need(sha(cohort/name)==pin, 'frozen_cohort_pin:'+name)
    old=json.loads((cohort/'SAMPLE.json').read_text()); population=json.loads((cohort/'POPULATION.json').read_text())
    oldkeys={r['key'] for r in old}; need(len(old)==len(oldkeys)==30,'original_30_identity')
    eligible=[r for r in population if r.get('in_scope') and r.get('sample_eligible') and r['key'] not in oldkeys]
    ranked=sorted(eligible,key=lambda r:(hashlib.sha256(SEED.encode()+b'\0'+r['key'].encode()).hexdigest(),r['key']))
    need(len(ranked)>=30,'heldout_population_shortage')
    rows=[dict(r,cohort='original30') for r in old]+[dict(r,cohort='heldout30',N4_seed_rank=hashlib.sha256(SEED.encode()+b'\0'+r['key'].encode()).hexdigest()) for r in ranked[:30]]
    need(len({r['key'] for r in rows})==60 and all(r['module']!='NETR.ovl' for r in rows),'cohort_overlap_or_NETR')
    return rows

def reader(image, module):
    if module!='SLUS_214.10':
        def read(va,size):
            at=va-0x7c4880; need(0<=at<=len(image)-size,'overlay_read_out_of_bounds'); return image[at:at+size]
        return read
    need(image[:4]==b'\x7fELF' and image[4:6]==b'\x01\x01','ELF32_LE_required')
    phoff=struct.unpack_from('<I',image,28)[0]; ents,count=struct.unpack_from('<HH',image,42)
    loads=[]
    for i in range(count):
        typ,off,va,_,filesz,_,_,_=struct.unpack_from('<8I',image,phoff+i*ents)
        if typ==1: loads.append((va,off,filesz))
    def read(va,size):
        found=[(off+va-base) for base,off,n in loads if base<=va and va+size<=base+n]
        need(len(found)==1,'original_PT_LOAD_identity'); return image[found[0]:found[0]+size]
    return read

def words_for(row, read):
    pcs=set()
    for span in row['body_ranges']: pcs.update(range(int(span['start'],16),int(span['end_exclusive'],16),4))
    pcs.update(int(p,16) for p in row['architectural_delay_closure_added'])
    words={pc:int.from_bytes(read(pc,4),'little') for pc in sorted(pcs)}
    need(hashlib.sha256(b''.join(read(pc,4) for pc in sorted(words))).hexdigest()==row['body_bytes_sha256'],'original_body_pin:'+row['key'])
    need(hashlib.sha256(canonical(sorted(words.items()))).hexdigest()==row['addressed_words_sha256'],'addressed_original_pin:'+row['key'])
    if 'original_words' in row: need(words=={int(x['pc'],16):int(x['word'],16) for x in row['original_words']},'sample_words_changed')
    return words

def fragments(unit_text, definition, words):
    body=next(n for n in tr.children(definition) if n.get('kind')=='CompoundStmt')
    end=body['range']['end']['offset']
    labels=list(re.finditer(r'^L_([0-9A-F]{8}):',unit_text,re.M)); selected=[]
    for i,m in enumerate(labels):
        if int(m[1],16) in words:
            stop=min(labels[i+1].start() if i+1<len(labels) else end,end)
            selected.append(unit_text[m.start():stop])
    need(selected,'reference_has_no_selected_labels')
    return ''.join(selected)

COMMON = r'''
#include "ps2/generated.hpp"
#include "mka/stage_a_guest_memory.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <string>
#include <stdexcept>
#include <algorithm>
using namespace ps2;
static_assert(sizeof(Cpu)==1456 && alignof(Cpu)==16 && offsetof(Cpu,pc)==548);
struct Event {std::uint32_t address;unsigned width;std::uint64_t value;bool operator==(const Event&)const=default;};
std::vector<Event> original_writes,native_writes;
namespace ps2 {
alignas(16) std::uint8_t g_ram[kRamSize],g_spr[kScratchpadSize];
bool g_watch_on=false;std::uint32_t g_watch_base{},g_watch_end{};
void watch_hit(std::uint32_t,std::uint32_t,const char*){throw std::runtime_error("reference_watch_unadmitted");}
void watch_span(std::uint32_t,std::uint32_t,const char*){throw std::runtime_error("reference_watch_unadmitted");}
#define SLOW_LOAD(N,T) T slow_ld##N(std::uint32_t){throw std::runtime_error("reference_unmapped_load" );}
SLOW_LOAD(8,std::uint8_t) SLOW_LOAD(16,std::uint16_t) SLOW_LOAD(32,std::uint32_t) SLOW_LOAD(64,std::uint64_t)
#define SLOW_STORE(N,T) void slow_st##N(std::uint32_t,T){throw std::runtime_error("reference_unmapped_store");}
SLOW_STORE(8,std::uint8_t) SLOW_STORE(16,std::uint16_t) SLOW_STORE(32,std::uint32_t) SLOW_STORE(64,std::uint64_t)
void slow_ld128(std::uint32_t,Reg128&){throw std::runtime_error("reference_wide_unadmitted");}
void slow_st128(std::uint32_t,const Reg128&){throw std::runtime_error("reference_wide_unadmitted");}
void dispatch(Cpu&,std::uint32_t,std::uint32_t){throw std::runtime_error("reference_external_dispatch");}
void call(Cpu&,std::uint32_t,std::uint32_t){throw std::runtime_error("reference_external_call");}
void bad_entry(Cpu&,std::uint32_t){throw std::runtime_error("reference_bad_entry");}
bool preempt(Cpu&){throw std::runtime_error("reference_yield_unadmitted");}
void stop(Cpu&,std::uint32_t,const char*){throw std::runtime_error("reference_stop");}
}
namespace reference {
using namespace ps2;
#define LOG_STORE(N,T) inline void st##N(std::uint32_t a,T v){ps2::st##N(a,v);original_writes.push_back({a,N/8,std::uint64_t(v)});}
LOG_STORE(8,std::uint8_t) LOG_STORE(16,std::uint16_t) LOG_STORE(32,std::uint32_t) LOG_STORE(64,std::uint64_t)
}
void observe(void*,const mka::stage_a::Access&a){native_writes.push_back({a.address,a.width,a.value});}
std::uint64_t random_word(std::uint64_t&s){s^=s<<13;s^=s>>7;s^=s<<17;return s;}
'''

def harness(row, case, unit_name, original, native, ir, words):
    # Original helpers remain original. Only the store observation seam changes
    # identifier spelling; each wrapper invokes the exact ps2::stN implementation.
    original=re.sub(r'(?<![\w:])st(8|16|32|64)\(',r'reference::st\1(',original)
    external=sorted(set(re.findall(r'\b((?:m_[A-Za-z]+_)?unit_[0-9A-Fa-f]+)\(',original))-{unit_name})
    stubs=''.join('void '+name+'(Cpu&,std::uint32_t,std::uint32_t){throw std::runtime_error("reference_external_unit:'+name+'");}\n' for name in external)
    fn=stubs+'void '+unit_name+'(Cpu&c,std::uint32_t entry,std::uint32_t depth){switch(entry){case '+str(case)+'u:goto L_'+row['address'].upper()+';default:throw std::runtime_error("reference_entry_refused");}\n'+original+'}\n'
    bases=sorted({w>>21&31 for w in words.values() if w>>26 in (0x20,0x21,0x23,0x24,0x25,0x27,0x28,0x29,0x2b,0x37,0x3f)})
    delay_regs=set()
    for pc,w in words.items():
        if w==0x03e00008 and pc+4 in words:
            d=words[pc+4];op=d>>26
            if d and op==0: delay_regs.add(d>>11&31)
            elif d and op in (9,0xd,0xf,0x19,0x23,0x27,0x37):delay_regs.add(d>>16&31)
    inputs=''.join('input.r'+str(r)+'=before.r['+str(r)+'].d[0];' for r in ir['inputs'])
    outputs=''.join('candidate.r['+str(r)+'].d[0]=out.r'+str(r)+';' for r in ir['outputs'])
    mutated_input=('input.r'+str(ir['inputs'][0])+'^=4;' if ir['inputs'] else '')
    stale=('candidate.r['+str(ir['outputs'][0])+'].d[0]=before.r['+str(ir['outputs'][0])+'].d[0];' if ir['outputs'] else '')
    width=''.join('candidate.r['+str(r)+'].d[0]=static_cast<std::uint32_t>(candidate.r['+str(r)+'].d[0]);' for r in ir['outputs'])
    delay=''.join('candidate.r['+str(r)+'].d[0]=before.r['+str(r)+'].d[0];' for r in sorted(delay_regs-{0}))
    # An independent stale-edge plant changes actual block-parameter transport,
    # not the comparator's expectation. It never executes in an ordinary run.
    mutant=native[native.index('kernel_out kernel('):].replace('kernel_out kernel(','kernel_out kernel_phi(',1)
    mutant,phi_sites=re.subn(r'(L_[A-Z0-9_]+_r(\d+))=value_\d+;',lambda m:m[1]+'=input.r'+m[2]+';',mutant)
    tail=r'''
int main(int argc,char**argv){
 const std::string plant=argc==2?argv[1]:"";unsigned admitted=0,declined=0;std::string first_decline;
 std::vector<std::byte> ram(kRamSize),spr(kScratchpadSize);
 const std::array<std::uint64_t,8> edges{0,1,0x7fffffffull,0x80000000ull,0xffffffffull,0x8000000000000000ull,0xffffffffffffffffull,0x1234567887654321ull};
 for(unsigned fixture=0;fixture<24;++fixture){
  std::uint64_t seed=0xfedcba9876543210ull+fixture;Cpu before;auto*bytes=reinterpret_cast<unsigned char*>(&before);
  for(std::size_t i=0;i<sizeof before;++i)bytes[i]=static_cast<unsigned char>(random_word(seed));
  for(unsigned r=1;r<32;++r)before.r[r].d[0]=0x00100000u+r*0x1000u;
  const std::array<unsigned,@BASECOUNT@> bases{@BASES@};
  if(fixture>=8)for(unsigned r=1;r<31;++r)if(std::find(bases.begin(),bases.end(),r)==bases.end())before.r[r].d[0]=edges[fixture%8];
  // Zero is a mapped RAM address in this component. These cases also exercise
  // source null tests without asserting any production receiver admission.
  if(fixture==4||fixture==12||fixture==20)for(auto r:bases)if(r&&r!=31)before.r[r].d[0]=0;
  if(fixture==5||fixture==13||fixture==21)for(auto r:bases)if(r&&r!=31)before.r[r].d[0]=0x70001000u;
  before.r[31].d[0]=0x0000000010010000ull;std::memset(&before.r[0],0,sizeof before.r[0]);before.pc=@ENTRY@;before.budget=100000;
  // Pattern groups include pointer-valid chains and numeric load sign edges.
  const std::uint32_t pattern=fixture<16?0x00100000u:static_cast<std::uint32_t>(edges[fixture%8]);
  for(std::size_t i=0;i<kRamSize;i+=4)std::memcpy(g_ram+i,&pattern,4);
  for(std::size_t i=0;i<kScratchpadSize;i+=4)std::memcpy(g_spr+i,&pattern,4);
  std::memcpy(ram.data(),g_ram,kRamSize);std::memcpy(spr.data(),g_spr,kScratchpadSize);
  Cpu expected,candidate;std::memcpy(&expected,&before,sizeof before);std::memcpy(&candidate,&before,sizeof before);original_writes.clear();native_writes.clear();
  try{reference::@UNIT@(expected,@CASE@u,0u);}catch(const std::exception&e){++declined;if(first_decline.empty())first_decline=e.what();std::printf("DECLINED fixture=%u side=reference reason=%s\n",fixture,e.what());continue;}
  try{
   mka::stage_a::GuestMemory memory(ram,spr,{nullptr,observe});kernel_in input{};@INPUTS@
   if(plant=="--planted-invented-input"){@INVENTED@}
   const auto out=plant=="--planted-phi"?kernel_phi(memory,input,before.budget):kernel(memory,input,before.budget);@OUTPUTS@ candidate.budget=static_cast<std::int32_t>(out.budget);candidate.pc=before.r[31].w[0];
   if(plant=="--planted-width"){@WIDTH@}
   if(plant=="--planted-stale-ssa"){@STALE@}
   if(plant=="--planted-delay"){@DELAY@}
  }catch(const std::exception&e){++declined;if(first_decline.empty())first_decline=e.what();std::printf("DECLINED fixture=%u side=native reason=%s\n",fixture,e.what());continue;}
  const bool same_cpu=std::memcmp(&expected,&candidate,sizeof before)==0;
  const bool same_memory=std::memcmp(g_ram,ram.data(),kRamSize)==0&&std::memcmp(g_spr,spr.data(),kScratchpadSize)==0;
  const bool same_writes=original_writes==native_writes;
  if(!same_cpu||!same_memory||!same_writes){
   std::printf("DIFFERENCE fixture=%u cpu=%u memory=%u writes=%u admitted=%u\n",fixture,same_cpu,same_memory,same_writes,admitted);
   std::printf("CPU expected_pc=%08x actual_pc=%08x expected_budget=%d actual_budget=%d\n",expected.pc,candidate.pc,expected.budget,candidate.budget);
   if(!same_cpu)for(unsigned r=0;r<32;++r)if(std::memcmp(&expected.r[r],&candidate.r[r],16))std::printf("REG %u expected=%016llx actual=%016llx upper_expected=%016llx upper_actual=%016llx\n",r,(unsigned long long)expected.r[r].d[0],(unsigned long long)candidate.r[r].d[0],(unsigned long long)expected.r[r].d[1],(unsigned long long)candidate.r[r].d[1]);
   return 1;
  }++admitted;
 }
 std::printf("%s constructed=%u declined=%u full_Cpu_bytes=%zu RAM_bytes=%u SPR_bytes=%u first_decline=%s native_credit=0\n",plant.empty()?"PASS_CAPABILITY":"PLANT_NOT_OBSERVED",admitted,declined,sizeof(Cpu),kRamSize,kScratchpadSize,first_decline.c_str());
 return plant.empty()?(admitted?0:2):3;
}
'''
    replacements={'BASECOUNT':str(len(bases)),'BASES':','.join(map(str,bases)),'ENTRY':'0x'+row['address']+'u','UNIT':unit_name,'CASE':str(case),'INPUTS':inputs,'OUTPUTS':outputs,'INVENTED':mutated_input,'WIDTH':width,'STALE':stale,'DELAY':delay}
    for key,value in replacements.items(): tail=tail.replace('@'+key+'@',value)
    return COMMON+'\nnamespace reference {\n'+fn+'}\n'+native+mutant+tail,dict(return_delay_registers=sorted(delay_regs),memory_base_registers=bases,unreachable_external_stubs=external,phi_mutation_sites=phi_sites,phi_mutation='Test-only wrong transport: source-current SSA edge values replaced by incoming function values.')

def inspect_historical(states,rows,source):
    """Only the admitted reader's EE prefix, not a machine restore."""
    text=source.read_text()
    need('cpu = 20, ram = cpu + 1456' in text and 'offsetof(ps2::Cpu, pc) == 548' in text and 'only source-proved native version8 admitted' in text,'historical_reader_source_shape_changed')
    entries=collections.defaultdict(list)
    for r in rows:entries[int(r['address'],16)].append(r['key'])
    found=[];files=sorted(states.glob('*.state'));need(len(files)==61,'historical_state_count')
    for p in files:
        with p.open('rb') as f:
            prefix=f.read(20+1456);f.seek(-8,2);end=f.read(8)
        need(len(prefix)==1476 and prefix[:8]==b'PS2STATE' and int.from_bytes(prefix[8:12],'little')==8 and prefix[12:20]==b'ee\0\0\0\0\0\0' and end==b'end\0\0\0\0\0','historical_v8_prefix_refused:'+p.name)
        need(p.stat().st_size>=20+1456+33554432+16384+8*4+1+4+8,'historical_truncation')
        pc=int.from_bytes(prefix[20+548:20+552],'little')
        found.append(dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p),EE_pc=f'{pc:08x}',selected_entry_matches=entries.get(pc,[]),validation='Source-pinned version8 EE prefix + extents/end tag only; no full restore claimed.'))
    return dict(reader_source=str(source),reader_sha256=sha(source),count=len(found),matched_entries=sum(bool(r['selected_entry_matches']) for r in found),states=found)

def aggregate(rows):
    result={}
    for cohort in ('original30','heldout30'):
        selected=[r for r in rows if r['cohort']==cohort]
        def rate(group):
            passed=sum(r['status']=='PASS_CONSTRUCTED_CAPABILITY' for r in group)
            return dict(denominator=len(group),passed=passed,rate=passed/len(group) if group else None)
        bins={}
        for a,b in ((0,32),(33,64),(65,128),(129,256),(257,512),(513,1024),(1025,4096),(4097,1<<32)):
            bins[f'{a}..{b}']=rate([r for r in selected if a<=r.get('body_bytes',-1)<=b])
        tags=sorted({tag for r in selected for tag in r.get('instruction_mix',{}).get('tags',[])})
        result[cohort]=dict(denominator=len(selected),statuses=dict(collections.Counter(r['status'] for r in selected)),constructed_comparisons=sum(r.get('constructed_comparisons',0) for r in selected),constructed_declined=sum(r.get('constructed_declined',0) for r in selected),semantic_controls={p:dict(collections.Counter(r.get('semantic_controls',{}).get(p,{}).get('status','NOT_RUN') for r in selected)) for p in ('width','stale-ssa','delay','invented-input','phi')},pass_by_body_bytes=bins,pass_by_instruction_mix={tag:rate([r for r in selected if tag in r.get('instruction_mix',{}).get('tags',[])]) for tag in tags},instruction_mix_overlap=True,native_credit=0)
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--source-fork',type=Path,required=True);ap.add_argument('--cohort-dir',type=Path,required=True);ap.add_argument('--compiler',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--mode',choices=('memory','control'),default='memory');ap.add_argument('--prior-memory',type=Path);ap.add_argument('--states-dir',type=Path)
    args=ap.parse_args();X=Path(__file__).resolve().parents[2];F=args.source_fork.resolve();O=args.output.resolve();C=args.cohort_dir.resolve();compiler=args.compiler.resolve()
    need(O.is_relative_to(X/'build/oracle-telemetry'),'output_not_owned_telemetry');need(not O.exists(),'fresh_output_required');disk(X);O.mkdir(parents=True);(X/'b/check-temp').mkdir(parents=True,exist_ok=True)
    pins={};attempts=[];result=dict(schema='phase6_memory_leaf_capability_v1',status='STARTED',mode=args.mode,invocation=subprocess.list2cmdline([sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]]),native_credit=0)
    def pin(p,expected=None):
        p=p.resolve();actual=sha(p);need(expected is None or actual==expected,'input_pin:'+str(p));pins[str(p)]=actual;return actual
    def unchanged():need(all(sha(p)==s for p,s in pins.items()),'INPUT_CHANGED_no_admitted_effects')
    try:
        for p in (Path(__file__),X/'tools/research/phase6_leaf_lowering.py',X/'tools/research/transform_generated_units.py',X/'runtime/include/mka/stage_a_guest_memory.hpp',*sorted((X/'runtime/include/ps2').glob('*.hpp'))):pin(p)
        pin(compiler,COMPILER_SHA);pin(C/'SAMPLE.json',PINS['SAMPLE.json']);pin(C/'POPULATION.json',PINS['POPULATION.json']);pin(C/'RESULT.json');pin(F/'build/generated-aot/phase4-all/units.hpp')
        old=json.loads((C/'RESULT.json').read_text());census=F/'build/oracle-telemetry/amended-ownership-arena-correction-01.json';pin(census,CENSUS_SHA);catalogue=json.loads(census.read_text())
        rows=select(C);dump(O/'SAMPLE.json',rows)
        if args.states_dir:
            state_source=X/'tests/test_phase4_aot_equivalence.cpp';pin(state_source)
            result['historical_entry_inspection']=inspect_historical(args.states_dir.resolve(),rows,state_source)
            need(result['historical_entry_inspection']['matched_entries']==0,'historical_entry_requires_actual_state_case_not_constructed_substitution')
        if args.mode=='control':
            need(args.prior_memory is not None,'control_mode_requires_actual_memory_receipt');prior=json.loads(args.prior_memory.read_text());pin(args.prior_memory)
            need(prior.get('mode')=='memory' and prior.get('status')=='MEASURED_CAPABILITY_ONLY' and prior['cohorts']['heldout30']['statuses'].get('PASS_CONSTRUCTED_CAPABILITY',0)>=1,'heldout_memory_pass_gate')
            for dependency in (Path(__file__).resolve(),X/'tools/research/phase6_leaf_lowering.py',X/'runtime/include/mka/stage_a_guest_memory.hpp',X/'tools/research/transform_generated_units.py'):
                need(prior['source_pins'].get(str(dependency))==pins[str(dependency)],'prior_memory_source_changed:'+str(dependency))
            need({a['key'] for a in prior['attempts']}=={r['key'] for r in rows},'prior_memory_cohort_changed')
        readers={}
        for module in sorted({r['module'] for r in rows}):
            meta=old['module_images'][module];p=Path(meta['path']);pin(p,meta['sha256']);readers[module]=reader(p.read_bytes(),module)
        words={r['key']:words_for(r,readers[r['module']]) for r in rows};bykey={r['key']:r for r in rows};pending=set(bykey);groups=collections.defaultdict(list)
        for unit in sorted((F/'build/generated-aot/phase4-all').glob('*.cpp')):
            m=re.fullmatch(r'(?:m_(MOVIER|GAMER)_)?unit_[0-9a-fA-F]+\.cpp',unit.name)
            if not m:continue
            module=m[1]+'.ovl' if m[1] else 'SLUS_214.10';text=unit.read_text();cases={a.lower():int(c) for c,a in re.findall(r'case (\d+)u: goto L_([0-9a-fA-F]+);',text)}
            found=sorted(k for k in pending if bykey[k]['module']==module and bykey[k]['address'] in cases)
            if found:
                pin(unit,catalogue['aot']['all_measured_source_sha256'].get(unit.name));need(unit.name in catalogue['aot']['all_measured_source_sha256'],'unmeasured_source_unit')
                for key in found:groups[unit].append((key,cases[bykey[key]['address']]));pending.remove(key)
            if not pending:break
        for key in sorted(pending):attempts.append(dict(key=key,cohort=bykey[key]['cohort'],status='REFUSED',reason='missing_original_generated_entry',native_credit=0))
        for unit,selections in groups.items():
            unchanged();disk(X);print('AST '+unit.name+' entries='+str(len(selections)),flush=True);start=time.perf_counter()
            definition=tr.function_definition(tr.clang_ast(compiler,unit,[unit.parent,X/'runtime/include'],unit.stem,X),unit.stem);parse_seconds=time.perf_counter()-start;text=unit.read_text()
            for key,case in selections:
                row=bykey[key];a=dict(key=key,cohort=row['cohort'],body_bytes=row['body_bytes'],instruction_mix=row['instruction_mix'],source_unit=str(unit),source_unit_sha256=pins[str(unit.resolve())],AST_parse_seconds_shared=parse_seconds,AST_shared_by=len(selections),classification='capability only',native_credit=0,E2_historical='NOT_RUN_no_authenticated_historical_entry',production_ABI='UNADMITTED_mechanical_bitvectors_only')
                directory=O/key.replace(':','_');directory.mkdir();attempts.append(a)
                try:
                    leaf=TrackedLeaf.__new__(TrackedLeaf);leaf.__init__(definition,row['address'],case,words[key])
                    if args.mode=='memory':need(not any(b['term'][0]=='branch' for b in leaf.blocks.values()),'N3_control_mode_disabled_branch_terminator')
                    native,ir=lowering.emit(leaf,'kernel');write(directory/'native.hpp',native);dump(directory/'IR.json',ir)
                    original=fragments(text,definition,words[key]);source,meta=harness(row,case,unit.stem,original,native,ir,words[key]);write(directory/'comparison.cpp',source);a.update(meta)
                    unchanged();command=[str(compiler),'-std=c++20','-O2','-Wno-unused-label','-Wno-unused-variable','-I'+str(X/'runtime/include'),str(directory/'comparison.cpp'),'-o',str(directory/'comparison.exe')]
                    start=time.perf_counter();compiled=subprocess.run(command,capture_output=True,text=True,timeout=120);a['compile_seconds']=time.perf_counter()-start;write(directory/'compile.log',compiled.stdout+compiled.stderr);a['compile_command']=subprocess.list2cmdline(command)
                    need(compiled.returncode==0,'ROOT_MODULE_OR_HARNESS_COMPILE_ERROR')
                    env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','');start=time.perf_counter();run=subprocess.run([str(directory/'comparison.exe')],capture_output=True,text=True,timeout=120,env=env);a['differential_seconds']=time.perf_counter()-start;write(directory/'run.log',run.stdout+run.stderr)
                    a.update(executable_sha256=sha(directory/'comparison.exe'),reference_fragment_sha256=hashlib.sha256(original.encode()).hexdigest(),native_source_sha256=sha(directory/'native.hpp'),run_exit=run.returncode)
                    need(run.returncode!=1,'ROOT_MODULE_OR_HARNESS_SEMANTIC_DIFFERENCE')
                    found=re.search(r'constructed=(\d+) declined=(\d+)',run.stdout);need(found is not None,'HARNESS_PROTOCOL_ERROR')
                    a.update(constructed_comparisons=int(found[1]),constructed_declined=int(found[2]),status='PASS_CONSTRUCTED_CAPABILITY' if run.returncode==0 else 'REFUSED_CONSTRUCTED_INPUTS',reason='full_Cpu_RAM_SPR_ordered_writes_equal' if run.returncode==0 else run.stdout.strip())
                    a['constructed_refusals']=[dict(fixture=int(n),side=side,reason=why) for n,side,why in re.findall(r'DECLINED fixture=(\d+) side=(\w+) reason=([^\r\n]+)',run.stdout)]
                    a['semantic_controls']={}
                    if run.returncode==0:
                        for plant in ('width','stale-ssa','delay','invented-input',*(['phi'] if args.mode=='control' else [])):
                            check=subprocess.run([str(directory/'comparison.exe'),'--planted-'+plant],capture_output=True,text=True,timeout=120,env=env);write(directory/('plant-'+plant+'.log'),check.stdout+check.stderr)
                            a['semantic_controls'][plant]=dict(status='REJECTED' if check.returncode==1 else 'NOT_EXERCISED_NO_OBSERVABLE_EFFECT',exit=check.returncode)
                    if args.mode=='memory':a['semantic_controls']['phi']=dict(status='NOT_EXERCISED_MEMORY_MODE')
                except (tr.Refusal,RuntimeError) as e:
                    a.update(status='REFUSED',reason=str(e))
                    if hasattr(leaf,'failure_pc'):a['first_unlowered_pc']=f'{leaf.failure_pc:08x}'
                    a['first_failure_source']=ast_fragment(getattr(leaf,'failure_node',None),text)
                    if not isinstance(e,tr.Refusal) and str(e)!='N3_control_mode_disabled_branch_terminator':
                        a['status']='STOPPED_ERROR';dump(directory/'RESULT.json',a);raise
                    if str(e).startswith(('N1_unproved_AST_type:','N1_value_AST_unproved:ArraySubscriptExpr','N3_cursor_assignment_unproved')):
                        a['status']='STOPPED_ROOT_MODULE_ERROR';dump(directory/'RESULT.json',a);raise RuntimeError(str(e))
                dump(directory/'RESULT.json',a);print(key+' '+a['status']+' '+a.get('reason',''),flush=True)
            del definition
        unchanged()
        required=('width','stale-ssa','delay','invented-input',*(['phi'] if args.mode=='control' else []))
        result['semantic_control_coverage']={p:[a['key'] for a in attempts if a.get('semantic_controls',{}).get(p,{}).get('status')=='REJECTED'] for p in required}
        need(all(result['semantic_control_coverage'].values()),'semantic_control_not_observed_in_any_admitted_case')
        result['status']='MEASURED_CAPABILITY_ONLY'
    except Exception as e:
        result.update(status='STOPPED_ERROR',reason=str(e));print('STOPPED '+str(e),flush=True)
    finally:
        changed=[p for p,s in pins.items() if not Path(p).exists() or sha(p)!=s]
        if changed:
            result.update(status='INPUT_CHANGED',changed_inputs=changed)
            for a in attempts:a.update(status='INPUT_CHANGED_NOT_ADMITTED',constructed_comparisons=0)
        result.update(source_pins=pins,source_pins_after={p:sha(p) for p in pins if Path(p).exists()},attempts=attempts,cohorts=aggregate(attempts),selection=dict(seed=SEED,rank='SHA256(UTF8(seed) + NUL + UTF8(key)), then key',original_denominator=30,heldout_denominator=30),historical_states='No historical entries exercised; constructed capability only. No forced 61-state claim.',limitations=['Mechanical input/output bitvectors are not recovered production ABI.','No ownership admission, StageB implementation or native credit.','Memory mode refuses branch terminators; full control/phi is a distinct gated run.'])
        dump(O/'RESULT.json',result);print('RESULT '+str(O/'RESULT.json')+' '+sha(O/'RESULT.json'),flush=True)
    return 0 if result['status']=='MEASURED_CAPABILITY_ONLY' else 1

if __name__=='__main__':sys.exit(main())
