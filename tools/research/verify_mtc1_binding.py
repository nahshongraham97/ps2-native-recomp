"""Original/AST MTC1 raw-bit SSA and compiled instruction-component checks.

Constructed source bit patterns are not additional retail samples or saved states.
The scalar kernel has no CPU/register/memory interface or production adoption.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,re,subprocess
import transform_generated_units as tr
import stack_frame_analysis as frames


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def check(ok,reason):
    if not ok:raise AssertionError(reason)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-root',type=Path,required=True)
    parser.add_argument('--before-study',type=Path,required=True)
    parser.add_argument('--frames',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compiler',type=Path)
    args=parser.parse_args();F=Path(__file__).resolve().parents[2];P=args.parent_root.resolve();O=args.output.resolve()
    check(not O.exists() and O.is_relative_to((F/'build/oracle-telemetry').resolve()),'new ignored output required');O.mkdir()
    baseline=json.loads(args.before_study.read_bytes())
    check(baseline.get('frame_stage',{}).get('stage')=='LUI_CONSTANT_BINDING_OPT_IN','actual LUI-on baseline required')
    pins={name:sha(F/name) for name in ('tools/research/transform_generated_units.py','tools/research/verify_mtc1_binding.py','tools/research/stack_frame_analysis.py')}
    compiler=args.compiler or Path(next(line.split('=',1)[1] for line in (F/'b/CMakeCache.txt').read_text().splitlines() if line.startswith('CMAKE_CXX_COMPILER:')))
    check(sha(compiler)==baseline['inputs']['compiler_sha256'],'measured compiler identity')
    contractpath=F/'recomp/overlay_initializer_contract.json';check(sha(contractpath)==baseline['inputs']['contract_sha256'],'contract identity')
    contract=json.loads(contractpath.read_bytes())
    for name,expected in contract['header_pins'].items():check(sha(F/name)==expected,'header identity:'+name)
    image=(P/'research/inputs/SLUS_214.10').read_bytes();check(hashlib.sha256(image).hexdigest()==baseline['inputs']['original_elf_sha256'],'original identity');original=frames.elf_reader(image)
    pc=0x1a64ac;check(int.from_bytes(original(pc,4),'little')==0x44820800,'actual MTC1 original word')
    row=next(r for r in baseline['samples'] if r['address']=='001a64a0');unit=F/'build/generated-aot/phase4-all'/row['generated_unit']
    check(sha(unit)==row['source_unit_sha256'],'actual generated unit identity')
    source=unit.read_bytes().decode('utf-8');fragment=source.split('L_001A64AC:',1)[1].split('\nL_',1)[0]
    check(re.fullmatch(r'\s*c\.f\[1\] = c\.r\[2\]\.w\[0\];\s*c\.budget -= 1;\s*',fragment) is not None,'exact generated reference statement/budget')
    definition=tr.function_definition(tr.clang_ast(compiler,unit,[unit.parent,F/'runtime/include'],unit.stem,F),unit.stem)
    def label(at):return next(n for n in tr.walk(definition) if n.get('kind')=='LabelStmt' and n.get('name')=='L_'+f'{at:08X}')
    node=copy.deepcopy(tr.children(label(pc))[0]);desc=dict(source_entry='001a64a0',entry_case=row['entry_case'],_lui_original=original,_mtc1_original=original)
    lower=tr.Lowering(definition,desc,contract);lower.statement(label(0x1a64a4));regs=copy.deepcopy(lower.regs)
    lower.statement(label(pc));check(lower.regs==regs,'MTC1 changed source GPR SSA')
    check(len(lower.mtc1_bindings)==1 and lower.values[-1]['constant_value']==0x3f800000 and lower.fprs=={1:1},'actual integrated MTC1 SSA')
    binding=lower.mtc1_bindings[0];controls=[]
    def refuses(name,action,expected):
        try:action()
        except tr.Refusal as error:
            check(str(error)==expected,name+':wrong rejection '+str(error));controls.append(dict(plant=name,rejection=str(error)));return
        raise AssertionError('false acceptance:'+name)
    def reader(word):return lambda va,n:word.to_bytes(4,'little') if va==pc and n==4 else original(va,n)
    expression=binding['expression']['argument']
    refuses('wrong opcode',lambda:tr.bind_mtc1_assignment(node,lower.cpu,pc,reader(0x3c023f80),expression),'mtc1_source_not_MTC1')
    refuses('nonzero reserved instruction bits',lambda:tr.bind_mtc1_assignment(node,lower.cpu,pc,reader(0x44820801),expression),'mtc1_source_not_MTC1')
    refuses('wrong original FPR destination',lambda:tr.bind_mtc1_assignment(node,lower.cpu,pc,reader(0x44821000),expression),'mtc1_AST_destination_word_mismatch')
    refuses('wrong original GPR source',lambda:tr.bind_mtc1_assignment(node,lower.cpu,pc,reader(0x44830800),expression),'mtc1_AST_source_word_mismatch')
    bad=copy.deepcopy(node);bad['type']={'qualType':'float'}
    refuses('numeric assignment type',lambda:tr.bind_mtc1_assignment(bad,lower.cpu,pc,original,expression),'mtc1_assignment_not_raw32')
    bad=copy.deepcopy(node);tr.children(bad)[1]['castKind']='IntegralToFloating'
    refuses('numeric source conversion AST',lambda:tr.bind_mtc1_assignment(bad,lower.cpu,pc,original,expression),'mtc1_source_numeric_conversion_or_type')
    bad_binding=copy.deepcopy(binding);bad_binding['expression']['op']='numeric_conversion'
    refuses('numeric conversion IR',lambda:tr.emit_mtc1_scalar_probe(bad_binding,'mtc1_probe_0'),'mtc1_probe_requires_raw_bit_move32')
    bad_argument=dict(op='numeric_conversion',type='u32',value=0)
    refuses('numeric conversion forged as u32 source IR',lambda:tr.bind_mtc1_assignment(node,lower.cpu,pc,original,bad_argument),'mtc1_source_SSA_not_admitted_raw32')
    bad_binding=copy.deepcopy(binding);bad_binding['expression']['argument']=bad_argument
    refuses('numeric source IR at scalar emission',lambda:tr.emit_mtc1_scalar_probe(bad_binding,'mtc1_probe_0'),'mtc1_source_SSA_not_admitted_raw32')
    unbound=tr.Lowering(definition,desc,contract)
    refuses('unknown GPR ABI',lambda:unbound.statement(label(pc)),'mtc1_unbound_GPR_value_ABI:register_2')
    refuses('unbound FPR/function native ABI',lambda:tr.emit([(dict(desc,native_name='never_emitted'),dict(mtc1_bindings=[binding]))]),'native_emission_unbound_FPR_liveout_or_function_ABI')
    # Actual frame AST writes invalidate a constructed antecedent scalar value.
    # The new MTC1 source selector below is an in-memory fixture, not original code.
    plans={p['source_entry']:p for p in json.loads(args.frames.read_bytes())['targets']}
    def stale_control(entry,write_pc,reg):
        record=next(r for r in baseline['samples'] if r['address']==entry)
        source_unit=F/'build/generated-aot/phase4-all'/record['generated_unit']
        check(sha(source_unit)==record['source_unit_sha256'],'frame-control source unit identity')
        frame_definition=definition if source_unit==unit else tr.function_definition(tr.clang_ast(compiler,source_unit,[source_unit.parent,F/'runtime/include'],source_unit.stem,F),source_unit.stem)
        def frame_label(at):return next(n for n in tr.walk(frame_definition) if n.get('kind')=='LabelStmt' and n.get('name')=='L_'+f'{at:08X}')
        d=dict(desc,source_entry=entry,_frame_plan=plans[entry],_frame_original=original)
        l=tr.Lowering(frame_definition,d,contract);l.statement(frame_label(int(entry,16)))
        l.values.append(dict(id=0,type='u64',constant_value=0x3f800000,expression=dict(op='constant',type='u64',value=0x3f800000)))
        l.regs[reg]=0;l.statement(frame_label(write_pc));check(reg not in l.regs,'frame write retained stale scalar SSA')
        fixture=copy.deepcopy(label(pc));rhs=tr.unwrap(tr.children(tr.children(fixture)[0])[1])
        obj=tr.unwrap(tr.children(tr.unwrap(tr.children(rhs)[0]))[0]);tr.children(obj)[1]['value']=str(reg)
        l.mtc1_original=reader((0x11<<26)|(4<<21)|(reg<<16)|(1<<11))
        refuses('stale SSA after actual frame write '+f'{write_pc:08x}',lambda:l.statement(fixture),'mtc1_unbound_GPR_value_ABI:register_'+str(reg))
        controls[-1].update(original_write_word=f'{int.from_bytes(original(write_pc,4),"little"):08x}',actual_frame_unit=source_unit.relative_to(F).as_posix(),actual_frame_unit_sha256=sha(source_unit))
    stale_control('001a64a0',0x1a64fc,31)
    stale_control('001a64a0',0x1a6500,16)
    stale_control('001a0160',0x1a01cc,4)
    zero=copy.deepcopy(node);rhs=tr.unwrap(tr.children(zero)[1]);obj=tr.unwrap(tr.children(tr.unwrap(tr.children(rhs)[0]))[0]);tr.children(obj)[1]['value']='0'
    zero_binding=tr.bind_mtc1_assignment(zero,lower.cpu,pc,reader(0x44800800),dict(op='constant',type='u32',value=0))
    check(zero_binding['original_source_GPR']==0,'architectural r0 raw source')
    native=O/'mtc1_kernel.cpp';native.write_text('#include <cstdint>\n'+tr.emit_mtc1_scalar_probe(binding,'mtc1_probe_0'),encoding='utf-8',newline='\n')
    native_ast=tr.clang_ast(compiler,native,[],'mtc1_probe_0',F);fn=tr.function_definition(native_ast,'mtc1_probe_0')
    parameters=[n for n in tr.children(fn) if n.get('kind')=='ParmVarDecl']
    check(len(parameters)==1 and parameters[0]['type'].get('desugaredQualType')=='unsigned int','typed scalar raw-bit argument')
    forbidden={'CallExpr','CXXMemberCallExpr','ArraySubscriptExpr','MemberExpr','CXXReinterpretCastExpr','CXXStaticCastExpr','CStyleCastExpr','UnaryOperator','GCCAsmStmt','FileScopeAsmDecl'}
    check(not any(n.get('kind') in forbidden for n in tr.walk(fn)),'scalar memory/numeric/call structure')
    harness=O/'mtc1_reference_checks.cpp'
    cpp='''#include "ps2/cpu.hpp"
#include <bit>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
std::uint32_t mtc1_probe_0(std::uint32_t) noexcept;
static void reference(ps2::Cpu& c) {'''+fragment+'''}
static std::uint32_t numeric_mutant(std::uint32_t input) {
 return std::bit_cast<std::uint32_t>(static_cast<float>(input));
}
int main(int argc,char**argv){
 static_assert(std::is_trivially_copyable_v<ps2::Cpu>);
 const std::string mode=argc==2?argv[1]:"";
 if(argc>2||(!mode.empty()&&mode!="--planted-numeric-conversion"&&mode!="--planted-source-clobber"&&mode!="--planted-FCR31-clobber"))return 2;
 const std::uint32_t bits[]={0u,0x80000000u,0x7fc01234u,0xffc54321u,0x7f800001u,0xffffffffu,0x3f800000u,1u};
 const std::uint64_t upper[]={0ULL,0x0123456789abcdefULL,0xffffffffffffffffULL,0x8000000000000000ULL};
 unsigned comparisons=0;
 for(unsigned i=0;i<8;++i)for(unsigned k=0;k<4;++k){
  ps2::Cpu initial,expected,actual;
  std::memset(&initial,int(0x31+i*11+k),sizeof initial);
  initial.r[2].d[0]=(std::uint64_t(std::uint32_t(upper[k]))<<32)|bits[i];initial.r[2].d[1]=upper[k];
  initial.fcr31=std::uint32_t(upper[(k+1)%4]);initial.budget=100;
  std::memcpy(&expected,&initial,sizeof initial);std::memcpy(&actual,&initial,sizeof initial);
  reference(expected);
  actual.f[1]=mode=="--planted-numeric-conversion"?numeric_mutant(actual.r[2].w[0]):mtc1_probe_0(actual.r[2].w[0]);actual.budget-=1;
  if(mode=="--planted-source-clobber")actual.r[2].d[1]^=1;
  if(mode=="--planted-FCR31-clobber")actual.fcr31^=1;
  if(std::memcmp(&expected,&actual,sizeof actual)){
   const char*reason=expected.f[1]!=actual.f[1]?"MTC1_RAW32_BITS_DELTA":std::memcmp(&expected.r[2],&actual.r[2],sizeof actual.r[2])?"MTC1_SOURCE_GPR_DELTA":expected.fcr31!=actual.fcr31?"MTC1_FCR31_DELTA":"MTC1_FULL_CPU_DELTA";
   std::fprintf(stderr,"%s bits=%08X canary=%u\\n",reason,bits[i],k);return 1;
  }
  ++comparisons;
 }
 std::printf("PASS_MTC1_COMPONENT patterns=8 GPR_high_canaries=4 full_CPU_comparisons=%u new_native_credit=0\\n",comparisons);return 0;
}
'''
    harness.write_text(cpp,encoding='utf-8',newline='\n')
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''),TEMP=str(F/'b/check-temp'),TMP=str(F/'b/check-temp'));checks=[]
    def run(name,argv,expected=0,required=None):
        result=subprocess.run([str(x) for x in argv],cwd=F,env=env,capture_output=True,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        raw=result.stdout+result.stderr;(O/(name+'.log')).write_bytes(raw)
        check(result.returncode==expected and (required is None or required.encode() in raw),name+':'+raw.decode(errors='replace')[-2500:])
        checks.append(dict(name=name,argv=[str(x) for x in argv],exit_code=result.returncode,status='PASS',log_sha256=hashlib.sha256(raw).hexdigest()));return raw
    obj=O/'mtc1_kernel.obj';binary=O/'mtc1_reference_checks.exe';common=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',F/'runtime/include']
    run('compile-native',common+['-c',native,'-o',obj]);run('link-reference',common+[harness,obj,'-o',binary])
    raw=run('native-object',[compiler.parent/'llvm-nm.exe','--undefined-only',obj]);check(not re.search(rb'\bU\s+\S',raw),'scalar undefined/runtime dependency')
    run('positive',[binary],required='PASS_MTC1_COMPONENT')
    run('planted-numeric-conversion',[binary,'--planted-numeric-conversion'],1,'MTC1_RAW32_BITS_DELTA bits=80000000')
    run('planted-source-clobber',[binary,'--planted-source-clobber'],1,'MTC1_SOURCE_GPR_DELTA')
    run('planted-FCR31-clobber',[binary,'--planted-FCR31-clobber'],1,'MTC1_FCR31_DELTA')
    check(all(sha(F/name)==pin for name,pin in pins.items()),'checker source changed')
    result=dict(schema='compiled_original_MTC1_raw32_component_v1',status='PASS',source_sha256=pins,
        baseline_sha256=sha(args.before_study),original_elf_sha256=hashlib.sha256(image).hexdigest(),
        actual_source_unit=unit.relative_to(F).as_posix(),actual_source_unit_sha256=sha(unit),original_pc='001a64ac',original_word='44820800',
        reference_fragment=fragment,reference_fragment_sha256=hashlib.sha256(fragment.encode()).hexdigest(),compiler_sha256=sha(compiler),
        IR=binding,static_controls=controls,native_scalar_audit=dict(status='PASS',functions=1,raw32_parameters=1,undefined_symbols=0),checks=checks,
        binary_sha256=sha(binary),generated_source_sha256={p.name:sha(p) for p in (native,harness)},full_CPU_comparisons=32,
        source_patterns=['00000000','80000000','7fc01234','ffc54321','7f800001','ffffffff','3f800000','00000001'],GPR_high_canaries=4,saved_state_inputs=0,new_credit=0,
        scope='Independent compiled pinned generated MTC1 statement vs raw-bit scalar kernel; CPU/budget projection exists only in tests. No numeric conversion, whole-function E2, production adoption or native credit.',
        **{'Not verified':['FPR consumers, numeric arithmetic, complete native function/owner ABI and whole-function E2 remain unbound.'],
        'Contradictions with earlier documents':['MTC1 preserves raw bit patterns and FCR31; a numeric conversion fails negative-zero/sign-bit data.'],
        'Falsification attempts':['Numeric conversion, source GPR clobber and FCR31 clobber each fail compiled full-CPU comparison. Wrong original/AST/type, unknown and stale GPR SSA and numeric-IR controls refuse by name.']})
    (O/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(dict(status='PASS',receipt_sha256=sha(O/'RESULT.json'),full_CPU_comparisons=32,static_controls=len(controls))))


if __name__=='__main__':main()
