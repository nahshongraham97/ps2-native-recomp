"""Bounded original/AST scalar-family and constructed-input component checks.

No fixture input is a retail ABI binding. No complete native function is admitted.
"""
from pathlib import Path
import argparse,copy,csv,hashlib,json,os,re,subprocess
import transform_generated_units as tr
import stack_frame_analysis as frames


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def check(ok,reason):
    if not ok:raise AssertionError(reason)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('parent-root','before-study','frames','output'):ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();F=Path(__file__).resolve().parents[2];P=args.parent_root.resolve();O=args.output.resolve()
    check(not O.exists() and O.is_relative_to((F/'build/oracle-telemetry').resolve()),'new ignored output required');O.mkdir()
    baseline=json.loads(args.before_study.read_bytes());pins={name:sha(F/name) for name in ('tools/research/transform_generated_units.py','tools/research/verify_scalar_copy_bindings.py','tools/research/stack_frame_analysis.py','recomp/codegen.cpp','runtime/include/ps2/generated.hpp')}
    compiler=Path(next(line.split('=',1)[1] for line in (F/'b/CMakeCache.txt').read_text().splitlines() if line.startswith('CMAKE_CXX_COMPILER:')))
    check(sha(compiler)==baseline['inputs']['compiler_sha256'],'compiler identity')
    contractpath=F/'recomp/overlay_initializer_contract.json';check(sha(contractpath)==baseline['inputs']['contract_sha256'],'contract identity');contract=json.loads(contractpath.read_bytes())
    for name,pin in contract['header_pins'].items():check(sha(F/name)==pin,'header pin:'+name)
    image=(P/'research/inputs/SLUS_214.10').read_bytes();check(hashlib.sha256(image).hexdigest()==baseline['inputs']['original_elf_sha256'],'original identity');original=frames.elf_reader(image)
    plans={x['source_entry']:x for x in json.loads(args.frames.read_bytes())['targets']}
    cache={};cases={};refusals=[];zero_cases={}
    def definition(unit):
        if unit not in cache:
            roots=tr.clang_ast(compiler,unit,[unit.parent,F/'runtime/include'],unit.stem,F)
            fn=tr.function_definition(roots,unit.stem);cache[unit]=(fn,{int(n['name'][2:],16):n for n in tr.walk(fn) if n.get('kind')=='LabelStmt' and re.fullmatch(r'L_[0-9A-F]{8}',n.get('name',''))})
        return cache[unit]
    def family(word):return f'{word>>26:02x}:'+f'{word&63:02x}' if word>>26==0 else f'{word>>26:02x}'
    for row in baseline['samples']:
        if not row.get('generated_unit'):continue
        unit=F/'build/generated-aot/phase4-all'/row['generated_unit'];check(sha(unit)==row['source_unit_sha256'],'actual unit identity')
        spans=row.get('original_function_context',{}).get('body_spans',[])
        pcs={pc for s in spans for pc in range(int(s['start'],16),int(s['end'],16)+1,4)}
        if row['address'] in plans:pcs.update(int(w['pc'],16) for w in plans[row['address']]['original_words'])
        for pc in sorted(pcs):
            word=int.from_bytes(original(pc,4),'little')
            try:shape=tr.scalar_word_shape(word)
            except tr.Refusal:continue
            if shape is None or shape['destination'] in (28,29,31):continue
            key=family(word)
            if key in cases and cases[key]['binding']['original_destination']!=0 and zero_cases.keys()>={'zero_destination','zero_source'}:continue
            fn,labels=definition(unit)
            if pc not in labels:continue
            node=tr.children(labels[pc])[0];cpu=next(n['id'] for n in tr.children(fn) if n.get('kind')=='ParmVarDecl')
            try:b=tr.bind_scalar_statement(node,cpu,pc,original)
            except tr.Refusal as error:refusals.append(dict(pc=f'{pc:08x}',family=key,reason=str(error)));continue
            start=node['range']['begin'];end=node['range']['end']
            if 'offset' not in start or 'offset' not in end:continue
            text=unit.read_bytes()[start['offset']:end['offset']+end.get('tokLen',1)].decode()
            case=dict(binding=b,unit=unit,node=node,definition=fn,cpu=cpu,statement=text,owner=row['address'])
            if b['original_destination']==0:zero_cases.setdefault('zero_destination',case)
            elif not any(n['op']=='source_GPR' for n in tr.walk_scalar_expression(b['expression'])):zero_cases.setdefault('zero_source',case)
            if key not in cases or cases[key]['binding']['original_destination']==0:cases[key]=case
    required={f'00:{fn:02x}' for fn in (0,2,3,0x21,0x23,0x24,0x25,0x26,0x27,0x2d,0x2f,0x38,0x3a,0x3b,0x3c,0x3e,0x3f)}|{'09','19','0c','0d','0e'}
    extra={0x100304:0x00401026,0x100714:0x00031bf8,0x100768:0x64a5027c,0x100c20:0x0082102f,0x101260:0x0002137b,0x105e9c:0x00101027}
    listing=P/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv'
    check(sha(listing)==baseline['inputs']['instructions_sha256'],'pinned original instruction listing')
    joined=set()
    with listing.open(encoding='utf-8',newline='') as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            pc=int(row['address'],16)
            if pc in extra:
                check(bytes.fromhex(row['bytes'])==original(pc,4) and int.from_bytes(original(pc,4),'little')==extra[pc],'extra source-only original/listing word join');joined.add(pc)
            if len(joined)==len(extra):break
    check(joined==extra.keys(),'extra original listing coverage')
    pending=dict(extra)
    for unit in sorted((F/'build/generated-aot/phase4-all').glob('unit_*.cpp')):
        source=unit.read_bytes()
        for pc in list(pending):
            if f'L_{pc:08X}:'.encode() not in source:continue
            fn,labels=definition(unit);node=tr.children(labels[pc])[0];cpu=next(n['id'] for n in tr.children(fn) if n.get('kind')=='ParmVarDecl')
            b=tr.bind_scalar_statement(node,cpu,pc,original);start=node['range']['begin'];end=node['range']['end']
            text=source[start['offset']:end['offset']+end.get('tokLen',1)].decode()
            cases[family(extra[pc])]=dict(binding=b,unit=unit,node=node,definition=fn,cpu=cpu,statement=text,owner='SOURCE_ONLY_NOT_EXTRA_STUDY_PROBE')
            del pending[pc]
        if not pending:break
    check(not pending,'extra actual generated AST coverage')
    found=set(cases);missing=sorted(required-found)
    (O/'source-selection.json').write_text(json.dumps(dict(found=sorted(found),missing=missing,refusals=refusals),indent=2)+'\n',newline='\n')
    check(not missing,'Original39 lack source examples for families:'+','.join(missing))
    check(zero_cases.keys()>={'zero_destination','zero_source'},'original zero source/destination coverage')
    cases=[cases[key] for key in sorted(cases)]+list(zero_cases.values())
    controls=[]
    def refuses(name,action,expected):
        try:action()
        except tr.Refusal as error:
            check(str(error)==expected,name+':wrong refusal '+str(error));controls.append(dict(plant=name,rejection=str(error)));return
        raise AssertionError('false acceptance:'+name)
    sample=next(c for c in cases if c['binding']['width_bits']==64 and c['binding']['original_destination']!=0)
    bad=copy.deepcopy(sample['node']);bad['type']={'qualType':'unsigned int'}
    refuses('width mutation',lambda:tr.bind_scalar_statement(bad,sample['cpu'],int(sample['binding']['source_pc'],16),original),'scalar_assignment_width_mismatch')
    desc=dict(source_entry=sample['owner'],entry_case=0,_scalar_original=original,_scalar_inputs={'invented':dict(type='u64',value=1)})
    refuses('invented retail input',lambda:tr.Lowering(sample['definition'],desc,contract),'scalar_invented_input_contract')
    bad=copy.deepcopy(sample['binding']);bad['write_projection']['bits127_64']='CLEARED'
    refuses('upper-lane metadata mutation',lambda:tr.emit_scalar_probe(bad,'scalar_probe_0'),'scalar_probe_width_projection_unproved')
    refuses('native emission without function ABI',lambda:tr.emit([(dict(native_name='never_emitted'),dict(scalar_bindings=[sample['binding']]))]),'native_emission_scalar_function_ABI_unbound')
    # Prove a real load kills an antecedent fixture SSA; no concrete SP input.
    row=next(r for r in baseline['samples'] if r['address']=='001a64a0');unit=F/'build/generated-aot/phase4-all'/row['generated_unit'];fn,labels=definition(unit)
    lower=tr.Lowering(fn,dict(source_entry=row['address'],entry_case=row['entry_case'],_scalar_original=original,_frame_plan=plans[row['address']],_frame_original=original),contract)
    lower.statement(labels[0x1a64a0]);lower.values.append(dict(id=0,type='u64',constant_value=9));lower.regs[16]=0
    lower.statement(labels[0x1a6500]);check(16 not in lower.regs,'actual LQ retained stale scalar SSA')
    candidate=next((n for pc,n in labels.items() if (int.from_bytes(original(pc,4),'little')&0x3f)==0x2d and int.from_bytes(original(pc,4),'little')>>26==0 and int.from_bytes(original(pc,4),'little')>>21&31==16 and int.from_bytes(original(pc,4),'little')>>11&31 not in (0,28,29,31)),None)
    check(candidate is not None,'actual post-LQ scalar copy witness')
    refuses('stale SSA after actual LQ',lambda:lower.statement(candidate),'scalar_unbound_GPR_value_ABI:register_16')
    # A real JAL delay scalar operation is joined, but does not admit its call.
    delay=None
    for unit,(fn,labels) in cache.items():
        for pc,label in labels.items():
            if pc+4 not in labels or int.from_bytes(original(pc,4),'little')>>26!=3:continue
            cpu=next(n['id'] for n in tr.children(fn) if n.get('kind')=='ParmVarDecl')
            try:b=tr.bind_scalar_statement(tr.children(labels[pc+4])[0],cpu,pc+4,original)
            except tr.Refusal:continue
            if b['original_destination']==0:continue
            delay=tr.scalar_delay_witness(pc,[b],original)
            refuses('dropped actual scalar delay slot',lambda:tr.scalar_delay_witness(pc,[],original),'scalar_missing_original_delay_slot');break
        if delay:break
    check(delay is not None,'original scalar delay witness')
    native='#include <cstdint>\n';references='';declarations='';calls=[];evidence=[]
    for i,c in enumerate(cases):
        binding=c['binding'];text,inputs=tr.emit_scalar_probe(binding,'scalar_probe_'+str(i));native+=text
        declarations+=text.split('{',1)[0]+';\n'
        statement=c['statement'];references+=f'static void reference_{i}(ps2::Cpu& c) {{ {statement};'+('' if binding['original_destination']==0 else 'c.budget-=1;')+' }\n'
        arguments=','.join(f'actual.r[{reg}].d[0]' for reg in inputs)
        invoke=f'actual.r[{binding["original_destination"]}].d[0]=scalar_probe_{i}({arguments});' if binding['original_destination'] else f'(void)scalar_probe_{i}();'
        calls.append(f'case {i}:reference_{i}(expected);{invoke}actual.budget-=1;break;')
        evidence.append(dict(IR=binding,actual_statement=statement,unit=c['unit'].relative_to(F).as_posix(),unit_sha256=sha(c['unit']),constructed_input_scope='Raw scalar fixture inputs only, not proved retail native parameters.'))
    bundle_unit=F/'build/generated-aot/phase4-all/unit_0000.cpp';bundle_source=bundle_unit.read_bytes().decode();bundle_fn,bundle_labels=definition(bundle_unit)
    bundle_cpu=next(n['id'] for n in tr.children(bundle_fn) if n.get('kind')=='ParmVarDecl')
    check(int.from_bytes(original(0x100358,4),'little')==0x0c0494dc and int.from_bytes(original(0x10035c,4),'little')==0x0040282d,'actual JAL/scalar delay words')
    delay_binding=tr.bind_scalar_statement(tr.children(bundle_labels[0x10035c])[0],bundle_cpu,0x10035c,original)
    delay_name='scalar_probe_'+str(len(cases));delay_kernel,delay_inputs=tr.emit_scalar_probe(delay_binding,delay_name)
    check(delay_inputs==[2] and delay_binding['original_destination']==5,'actual scalar delay argument projection')
    native+=delay_kernel;declarations+=delay_kernel.split('{',1)[0]+';\n'
    bundle_fragment=bundle_source.split('L_00100358:',1)[1].split('\nL_',1)[0]
    check('unit_0009(c, 49u, depth + 1u)' in bundle_fragment and 'goto L_00100360;' in bundle_fragment,'actual complete generated JAL bundle')
    bundle_reference='''struct Observation { std::uint64_t argument{},upper{},return_low{},return_upper{};unsigned entry{},depth{};bool seen{}; };
static Observation observation;
static bool same_observation(const Observation&a,const Observation&b){return a.argument==b.argument&&a.upper==b.upper&&a.return_low==b.return_low&&a.return_upper==b.return_upper&&a.entry==b.entry&&a.depth==b.depth&&a.seen==b.seen;}
// Constructed observer, not the original125370 callee or a retail native ABI.
static void unit_0009(Cpu&c,std::uint32_t entry,std::uint32_t depth){
 observation={c.r[5].d[0],c.r[5].d[1],c.r[31].d[0],c.r[31].d[1],entry,depth,true};c.pc=0x00100360u;
}
static void reference_bundle(Cpu&c,std::uint32_t depth){
'''+bundle_fragment+'\nL_00100360:return;\n}\n'
    nativepath=O/'scalar_kernels.cpp';nativepath.write_text(native,encoding='utf-8',newline='\n')
    roots=tr.clang_ast(compiler,nativepath,[],'scalar_probe_',F)
    functions=[n for root in roots for n in tr.walk(root) if n.get('kind')=='FunctionDecl' and n.get('name','').startswith('scalar_probe_') and any(x.get('kind')=='CompoundStmt' for x in tr.children(n))]
    forbidden={'CallExpr','CXXMemberCallExpr','ArraySubscriptExpr','MemberExpr','CXXReinterpretCastExpr','CStyleCastExpr','GCCAsmStmt','FileScopeAsmDecl'}
    check(len(functions)==len(cases)+1 and not any(n.get('kind') in forbidden for f in functions for n in tr.walk(f)),'scalar kernel memory/state/call structure')
    cpp='''#include "ps2/generated.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
using namespace ps2;
'''+declarations+references+bundle_reference+'''int main(int argc,char**argv){
 const std::string mode=argc==2?argv[1]:"";
 if(argc>2||(!mode.empty()&&mode!="--planted-word-width"&&mode!="--planted-upper-lane"&&mode!="--planted-dropped-delay"&&mode!="--planted-reordered-delay"))return 2;
 const std::uint64_t values[]={0ULL,1ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x8000000000000000ULL,0xffffffffffffffffULL,0x1234567887654321ULL};
 unsigned comparisons=0;
 for(unsigned which=0;which<CASECOUNT;++which)for(unsigned v=0;v<8;++v)for(unsigned k=0;k<4;++k){
  Cpu initial,expected,actual;std::memset(&initial,int(0x31+v+k),sizeof initial);
  for(unsigned reg=0;reg<32;++reg){initial.r[reg].d[0]=values[(v+reg)%8];initial.r[reg].d[1]=values[(k+reg)%8];}
  initial.budget=100;std::memcpy(&expected,&initial,sizeof initial);std::memcpy(&actual,&initial,sizeof initial);
  switch(which){SWITCH default:return 2;}
  const unsigned destination=DESTINATIONS[which];
  if(mode=="--planted-word-width"&&destination)actual.r[destination].d[0]&=0xffffffffULL;
  if(mode=="--planted-upper-lane"&&destination)actual.r[destination].d[1]^=1;
  if(std::memcmp(&expected,&actual,sizeof actual)){std::fprintf(stderr,"SCALAR_FULL_CPU_DELTA case=%u value=%u upper=%u\\n",which,v,k);return 1;}
  ++comparisons;
 }
 unsigned bundle_comparisons=0;
 for(unsigned v=0;v<8;++v)for(unsigned k=0;k<4;++k){
  Cpu initial,expected,actual;std::memset(&initial,int(0x21+v+k),sizeof initial);
  initial.r[2].d[0]=values[v];initial.r[2].d[1]=values[k];initial.r[5].d[0]=values[v]^0x13579bdf2468ace0ULL;initial.budget=100;
  std::memcpy(&expected,&initial,sizeof initial);std::memcpy(&actual,&initial,sizeof initial);
  observation={};reference_bundle(expected,0u);const Observation reference_seen=observation;observation={};
  actual.budget-=2;actual.r[31].sd[0]=static_cast<std::int32_t>(0x00100360u);
  const std::uint64_t scalar_argument=DELAY_KERNEL(actual.r[2].d[0]);
  const bool plant=mode=="--planted-dropped-delay"||mode=="--planted-reordered-delay";
  if(!plant)actual.r[5].d[0]=scalar_argument;
  unit_0009(actual,49u,1u);
  // Deliberately repair the final register after both plants. Only the captured
  // callee input can expose the missing/reordered effect at the call boundary.
  if(plant)actual.r[5].d[0]=scalar_argument;
  if(std::memcmp(&expected,&actual,sizeof actual)){std::fprintf(stderr,"SCALAR_BUNDLE_CPU_DELTA\\n");return 1;}
  if(!same_observation(reference_seen,observation)){std::fprintf(stderr,"SCALAR_DELAY_OBSERVER_DELTA final_CPU_equal=1 value=%u upper=%u\\n",v,k);return 1;}
  ++bundle_comparisons;
 }
 std::printf("PASS_SCALAR_COMPONENT families=22 full_CPU_comparisons=%u delay_observer_comparisons=%u retail_input_bindings=0 native_credit=0\\n",comparisons,bundle_comparisons);return 0;
}
'''
    cpp=cpp.replace('CASECOUNT',str(len(cases))).replace('SWITCH',''.join(calls)).replace('DELAY_KERNEL',delay_name).replace('DESTINATIONS','std::array<unsigned,'+str(len(cases))+'> {'+','.join(str(c['binding']['original_destination']) for c in cases)+'}')
    cpp=cpp.replace('#include <string>','#include <string>\n#include <array>');harness=O/'scalar_reference_checks.cpp';harness.write_text(cpp,encoding='utf-8',newline='\n')
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''),TEMP=str(F/'b/check-temp'),TMP=str(F/'b/check-temp'));checks=[]
    def run(name,argv,expected=0,required=None):
        r=subprocess.run([str(x) for x in argv],cwd=F,env=env,capture_output=True,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        raw=r.stdout+r.stderr;(O/(name+'.log')).write_bytes(raw);check(r.returncode==expected and (required is None or required.encode() in raw),name+':'+raw.decode(errors='replace')[-2500:])
        checks.append(dict(name=name,argv=[str(x) for x in argv],exit_code=r.returncode,status='PASS',log_sha256=hashlib.sha256(raw).hexdigest()));return raw
    obj=O/'scalar_kernels.obj';binary=O/'scalar_reference_checks.exe';common=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',F/'runtime/include']
    run('compile-native',common+['-c',nativepath,'-o',obj]);run('link-reference',common+[harness,obj,'-o',binary]);raw=run('native-object',[compiler.parent/'llvm-nm.exe','--undefined-only',obj]);check(not re.search(rb'\bU\s+\S',raw),'scalar object runtime dependencies')
    run('positive',[binary],required='PASS_SCALAR_COMPONENT');run('planted-word-width',[binary,'--planted-word-width'],1,'SCALAR_FULL_CPU_DELTA');run('planted-upper-lane',[binary,'--planted-upper-lane'],1,'SCALAR_FULL_CPU_DELTA')
    run('planted-dropped-delay',[binary,'--planted-dropped-delay'],1,'SCALAR_DELAY_OBSERVER_DELTA final_CPU_equal=1')
    run('planted-reordered-delay',[binary,'--planted-reordered-delay'],1,'SCALAR_DELAY_OBSERVER_DELTA final_CPU_equal=1')
    check(all(sha(F/name)==pin for name,pin in pins.items()),'source changed during check')
    result=dict(schema='compiled_original_AST_scalar_family_component_v1',status='PASS',source_sha256=pins,original_elf_sha256=hashlib.sha256(image).hexdigest(),compiler_sha256=sha(compiler),
        source_families=sorted(found),family_count=len(found),constructed_case_count=len(cases),missing_families=missing,original_examples=evidence,static_controls=controls,delay_presence_witness=delay,checks=checks,
        full_CPU_comparisons=len(cases)*32,constructed_inputs_only=True,retail_input_bindings=0,saved_state_inputs=0,new_credit=0,
        compiled_delay_order=dict(status='PASS',constructed_observer=True,comparisons=32,control_pc='00100358',control_word='0c0494dc',delay_pc='0010035c',delay_word='0040282d',
            actual_unit=bundle_unit.relative_to(F).as_posix(),actual_unit_sha256=sha(bundle_unit),actual_generated_bundle=bundle_fragment,fragment_sha256=hashlib.sha256(bundle_fragment.encode()).hexdigest(),
            scope='Actual generated JAL/RA/scalar-delay sequence compiled against constructed observer only; both plants preserve final full CPU equality but fail observed callee input. No retail callee/native ABI admission.'),
        native_scalar_audit=dict(functions=len(functions),undefined_symbols=0),binary_sha256=sha(binary),generated_source_sha256={p.name:sha(p) for p in (nativepath,harness)},
        scope='Compiled actual original-joined generated scalar statement vs emitted scalar-only kernel on constructed raw inputs. No original pointer/native input contract or complete function E2.',
        **{'Not verified':['Unbound retail live-ins, native pointer/owner ABI, control/callee/delay execution and full functions remain refused.'],
        'Contradictions with earlier documents':['64bit writes compute bits63:32 and preserve128bit GPR bits127:64;32bit results signextend bit31 into bits63:32.'],
        'Falsification attempts':['Wrong width and upper-lane projection, stale SSA, dropped actual scalar delay and invented retail input controls reject.']})
    (O/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps(dict(status='PASS',sha256=sha(O/'RESULT.json'),families=len(found),test_cases=len(cases),comparisons=len(cases)*32)))


if __name__=='__main__':main()
