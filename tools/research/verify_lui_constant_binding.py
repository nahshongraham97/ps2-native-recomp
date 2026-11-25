"""Bounded LUI AST/IR and compiled generated-reference instruction checks.

Constructed immediate variants are test fixtures, not additional retail samples.
The scalar DUT has no Cpu/register/RAM/guest-address interface or production use.
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
    baseline=json.loads(args.before_study.read_bytes());pins={name:sha(F/name) for name in ['tools/research/transform_generated_units.py','tools/research/verify_lui_constant_binding.py','tools/research/stack_frame_analysis.py']}
    compiler=args.compiler
    if compiler is None:
        compiler=Path(next(line.split('=',1)[1] for line in (F/'b/CMakeCache.txt').read_text().splitlines() if line.startswith('CMAKE_CXX_COMPILER:')))
    check(sha(compiler)==baseline['inputs']['compiler_sha256'],'measured compiler identity')
    contractpath=F/'recomp/overlay_initializer_contract.json';check(sha(contractpath)==baseline['inputs']['contract_sha256'],'contract identity')
    contract=json.loads(contractpath.read_bytes())
    for name,expected in contract['header_pins'].items():check(sha(F/name)==expected,'header identity:'+name)
    image=(P/'research/inputs/SLUS_214.10').read_bytes();check(hashlib.sha256(image).hexdigest()==baseline['inputs']['original_elf_sha256'],'original identity');original=frames.elf_reader(image)
    pc=0x1a64a4;check(int.from_bytes(original(pc,4),'little')==0x3c023f80,'actual LUI original word')
    row=next(r for r in baseline['samples'] if r['address']=='001a64a0');unit=F/'build/generated-aot/phase4-all'/row['generated_unit']
    check(sha(unit)==row['source_unit_sha256'],'actual generated unit identity')
    source=unit.read_bytes().decode('utf-8');label='L_001A64A4:';fragment=source.split(label,1)[1].split('\nL_',1)[0]
    check(fragment.count('0x3F800000u')==1 and re.search(r'c\.budget\s*-=\s*1;',fragment),'exact generated reference statement/budget')
    roots=tr.clang_ast(compiler,unit,[unit.parent,F/'runtime/include'],unit.stem,F);definition=tr.function_definition(roots,unit.stem)
    labelnode=next(n for n in tr.walk(definition) if n.get('kind')=='LabelStmt' and n.get('name')=='L_001A64A4')
    node=copy.deepcopy(tr.children(labelnode)[0]);desc=dict(source_entry='001a64a0',entry_case=row['entry_case'],unit_name=unit.stem,module='SLUS_214.10',_lui_original=original)
    lower=tr.Lowering(definition,desc,contract);lower.statement(labelnode)
    check(len(lower.lui_bindings)==1 and lower.values[-1]['expression']['op']=='sign_extend32','actual integrated LUI binding')
    binding=lower.lui_bindings[0];check(binding['write_projection']=={'portion':'low64','upper64':'PRESERVED'},'write projection')
    controls=[]
    def refuses(name,action,expected=None):
        try:action()
        except tr.Refusal as error:
            if expected:check(str(error)==expected,name+':wrong rejection '+str(error))
            controls.append(dict(plant=name,rejection=str(error)));return
        raise AssertionError('false acceptance:'+name)
    def changed_literal(n,value):
        values=[x for x in tr.walk(n) if x.get('kind')=='IntegerLiteral' and int(x.get('value','-1'))==0x3f800000]
        check(len(values)==1,'single source literal');values[0]['value']=str(value)
    def reader(word):
        return lambda va,n:word.to_bytes(4,'little') if va==pc and n==4 else original(va,n)
    for reg in (0,28,29,31):
        refuses('forbidden destination '+str(reg),lambda reg=reg:tr.bind_lui_assignment(node,lower.cpu,pc,reader((15<<26)|(reg<<16)|0x3f80)),'lui_forbidden_abi_destination')
    refuses('JAL return address is not LUI',lambda:tr.bind_lui_assignment(node,lower.cpu,pc,reader(0x0c000800)),'lui_source_not_LUI')
    mutated=copy.deepcopy(node);changed_literal(mutated,0x3f810000)
    refuses('literal mismatch',lambda:tr.bind_lui_assignment(mutated,lower.cpu,pc,original),'lui_AST_literal_word_mismatch')
    mutated=copy.deepcopy(node);cast=next(n for n in tr.walk(mutated) if n.get('kind')=='CXXStaticCastExpr');cast['type']={'qualType':'std::uint32_t','desugaredQualType':'unsigned int'}
    refuses('unsigned cast',lambda:tr.bind_lui_assignment(mutated,lower.cpu,pc,original),'lui_missing_explicit_signed32_cast')
    mutated=copy.deepcopy(node);lhs=tr.unwrap(tr.children(mutated)[0]);tr.children(lhs)[1]['value']='1'
    refuses('upper lane destination',lambda:tr.bind_lui_assignment(mutated,lower.cpu,pc,original),'lui_destination_not_signed_low64')
    mutated=copy.deepcopy(node);bank=tr.unwrap(tr.children(tr.unwrap(tr.children(mutated)[0]))[0]);obj=tr.unwrap(tr.children(bank)[0]);tr.children(obj)[1]['value']='4'
    refuses('wrong original destination',lambda:tr.bind_lui_assignment(mutated,lower.cpu,pc,original),'lui_AST_destination_word_mismatch')
    mutated=copy.deepcopy(node);tr.children(mutated)[1].update(kind='CallExpr')
    refuses('call expression replacing cast',lambda:tr.bind_lui_assignment(mutated,lower.cpu,pc,original),'lui_missing_signed64_extension')
    bad=copy.deepcopy(binding);bad['expression']['op']='zero_extend32'
    refuses('zero-extension IR',lambda:tr.emit_lui_scalar_probe(bad,'lui_probe_0'),'lui_probe_requires_explicit_sign_extend32')
    bad=copy.deepcopy(binding);bad['write_projection']['upper64']='CLEAR'
    refuses('upper64 clear metadata',lambda:tr.emit_lui_scalar_probe(bad,'lui_probe_0'),'lui_probe_projection_unproved')
    plan=next(p for p in json.loads(args.frames.read_bytes())['targets'] if p['source_entry']=='001a64a0')
    aliasdesc=dict(desc,_frame_plan=plan,_frame_original=original);alias=tr.Lowering(definition,aliasdesc,contract)
    alias.frame.offsets[2]=-16 # Analysis-only antecedent provenance, not a native SP/address value.
    alias.statement(labelnode)
    check(2 not in alias.frame.offsets and alias.lui_bindings[0]['overwritten_symbolic_frame_alias'],'LUI kills prior frame alias')
    controls.append(dict(plant='stale frame alias overwritten by actual LUI',rejection='old symbolic frame provenance removed'))
    immediates=[0,0x1234,0x3f80,0x8000,0xffff];fixtures=[];kernels='#include <cstdint>\n';references=''
    declarations=''
    for index,imm in enumerate(immediates):
        fixture=copy.deepcopy(node);changed_literal(fixture,imm<<16);word=(15<<26)|(2<<16)|imm
        b=tr.bind_lui_assignment(fixture,lower.cpu,pc,reader(word));value=lower.evaluate(b['expression'])
        name='lui_probe_'+str(index);kernels+=tr.emit_lui_scalar_probe(b,name)
        kernels+=f'std::uint64_t lui_probe_fold_{index}() noexcept {{ return {value}ULL; }}\n'
        references+=f'static void reference_{index}(ps2::Cpu& c) {{'+fragment.replace('0x3F800000u',f'0x{imm<<16:08X}u')+'}\n'
        declarations+=f'std::uint64_t {name}() noexcept; std::uint64_t lui_probe_fold_{index}() noexcept;\n'
        fixtures.append(dict(immediate=f'{imm:04x}',word=f'{word:08x}',scope='actual pinned instruction' if imm==0x3f80 else 'constructed immediate derivative; not an extra retail sample',IR=b,folded_value=value))
    native=O/'lui_kernels.cpp';native.write_text(kernels,encoding='utf-8',newline='\n')
    # Audit the generated scalar function bodies. There are no Cpu parameters,
    # memory expressions, helper calls, globals or source-address interfaces.
    native_roots=tr.clang_ast(compiler,native,[],'lui_probe_',F)
    functions=[n for root in native_roots for n in tr.walk(root) if n.get('kind')=='FunctionDecl' and any(c.get('kind')=='CompoundStmt' for c in tr.children(n))]
    check(len(functions)==10,'scalar native function census')
    forbidden={'CallExpr','CXXMemberCallExpr','ArraySubscriptExpr','MemberExpr','CXXReinterpretCastExpr','CStyleCastExpr','UnaryOperator','GCCAsmStmt','FileScopeAsmDecl'}
    for function in functions:
        check(not any(n.get('kind')=='ParmVarDecl' for n in tr.children(function)),'scalar has no state interface')
        check(not any(n.get('kind') in forbidden for n in tr.walk(function)),'scalar memory/call/address structure')
    harness=O/'lui_reference_checks.cpp'
    cpp='''#include "ps2/cpu.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
'''+declarations+references
    cpp+='using Ref=void(*)(ps2::Cpu&);using Native=std::uint64_t(*)() noexcept;\n'
    cpp+='static Ref refs[]={'+','.join('reference_'+str(i) for i in range(5))+'};\n'
    cpp+='static Native ops[]={'+','.join('lui_probe_'+str(i) for i in range(5))+'};\n'
    cpp+='static Native folds[]={'+','.join('lui_probe_fold_'+str(i) for i in range(5))+'};\n'
    cpp+='static const std::uint32_t bits[]={'+','.join(f'0x{imm<<16:08x}u' for imm in immediates)+'};\n'
    cpp+='static const unsigned imms[]={'+','.join(str(imm) for imm in immediates)+'};\n'
    cpp+='''int main(int argc,char**argv){
 static_assert(std::is_trivially_copyable_v<ps2::Cpu>);
 const std::string mode=argc==2?argv[1]:"";
 if(argc>2||(!mode.empty()&&mode!="--planted-zero-extension"&&mode!="--planted-upper64"))return 2;
 const std::uint64_t canaries[]={0ULL,0x0123456789abcdefULL,0xffffffffffffffffULL,0x8000000000000000ULL};
 unsigned comparisons=0;
 for(unsigned i=0;i<5;++i)for(unsigned k=0;k<4;++k)for(unsigned route=0;route<2;++route){
  ps2::Cpu initial,reference,adapted;
  std::memset(&initial,int(0x31+i*11+k),sizeof initial);
  initial.r[2].d[0]=0xdeadbeef01234567ULL;initial.r[2].d[1]=canaries[k];initial.budget=100;
  std::memcpy(&reference,&initial,sizeof initial);std::memcpy(&adapted,&initial,sizeof initial);
  refs[i](reference);
  adapted.r[2].d[0]=route?folds[i]():ops[i]();adapted.budget-=1;
  if(mode=="--planted-zero-extension")adapted.r[2].d[0]=static_cast<std::uint64_t>(bits[i]);
  if(mode=="--planted-upper64")adapted.r[2].d[1]=0;
  if(std::memcmp(&reference,&adapted,sizeof reference)){
   const char*reason=reference.r[2].d[0]!=adapted.r[2].d[0]?"LUI_LOW64_VALUE_DELTA":reference.r[2].d[1]!=adapted.r[2].d[1]?"LUI_UPPER64_PROJECTION_DELTA":"LUI_FULL_CPU_DELTA";
   std::fprintf(stderr,"%s imm=%04X canary=%u route=%u\\n",reason,imms[i],k,route);return 1;
  }
  ++comparisons;
 }
 std::printf("PASS_LUI_COMPONENT immediate_cases=5 upper_canaries=4 full_CPU_comparisons=%u actual_instruction_comparisons=8 constructed_comparisons=32 new_native_credit=0\\n",comparisons);
 return 0;
}
'''
    harness.write_text(cpp,encoding='utf-8',newline='\n')
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''),TEMP=str(F/'b/check-temp'),TMP=str(F/'b/check-temp'))
    checks=[]
    def run(name,command,expected=0,required=None):
        result=subprocess.run([str(x) for x in command],cwd=F,env=env,capture_output=True,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        raw=result.stdout+result.stderr;(O/(name+'.log')).write_bytes(raw)
        check(result.returncode==expected and (required is None or required.encode() in raw),name+':'+raw.decode(errors='replace')[-2000:])
        checks.append(dict(name=name,argv=[str(x) for x in command],exit_code=result.returncode,status='PASS',log_sha256=hashlib.sha256(raw).hexdigest()))
        return raw
    obj=O/'lui_kernels.obj';binary=O/'lui_reference_checks.exe'
    common=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',F/'runtime/include']
    run('compile-native',common+['-c',native,'-o',obj]);run('link-reference',common+[harness,obj,'-o',binary])
    raw=run('native-object',[compiler.parent/'llvm-nm.exe','--undefined-only',obj]);check(not re.search(rb'\bU\s+\S',raw),'scalar undefined/runtime dependencies')
    run('positive',[binary],required='PASS_LUI_COMPONENT')
    run('planted-zero-extension',[binary,'--planted-zero-extension'],1,'LUI_LOW64_VALUE_DELTA imm=8000')
    run('planted-upper64',[binary,'--planted-upper64'],1,'LUI_UPPER64_PROJECTION_DELTA')
    check(all(sha(F/name)==pin for name,pin in pins.items()),'checker source changed')
    result=dict(schema='compiled_original_LUI_scalar_component_v1',status='PASS',source_sha256=pins,
        original_elf_sha256=hashlib.sha256(image).hexdigest(),actual_source_unit=unit.relative_to(F).as_posix(),actual_source_unit_sha256=sha(unit),
        original_pc='001a64a4',original_word='3c023f80',reference_fragment=fragment,reference_fragment_sha256=hashlib.sha256(fragment.encode()).hexdigest(),
        compiler_sha256=sha(compiler),fixtures=fixtures,static_controls=controls,native_scalar_audit=dict(status='PASS',functions=10,undefined_symbols=0),
        checks=checks,binary_sha256=sha(binary),generated_source_sha256={p.name:sha(p) for p in (native,harness)},
        full_CPU_comparisons=40,actual_instruction_comparisons=8,constructed_comparisons=32,saved_state_inputs=0,
        scope='Independent compiled pinned generated-statement reference vs explicit sign-extension IR scalar and Python-folded IR scalar; test adapters only. No whole-function E2, game launch, production adoption or credit.',
        new_credit=0,**{'Not verified':['Whole function, native ownership/adoption and saved-state E2 remain separate.'],
         'Contradictions with earlier documents':['LUI low64 is sign-extended from32; zero-extension fails negative constructed immediates.'],
         'Falsification attempts':['Changed zero-extension and upper64 projection each fail compiled reference comparison; forbidden destinations/calls/type/literal/provenance controls refuse.']})
    (O/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'status':'PASS','receipt_sha256':sha(O/'RESULT.json'),'full_CPU_comparisons':40,'static_controls':len(controls)}))


if __name__=='__main__':main()
