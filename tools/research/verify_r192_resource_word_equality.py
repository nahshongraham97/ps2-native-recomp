"""Compare descriptor equations to 281 original-builder programs, without execution.

This establishes bounded code identity, never descriptor ownership, a Plan lease,
numerical equivalence, game integration or universal generated-code coverage.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

import analyze_r192_builder_domain as auth


def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(4<<20),b''):h.update(block)
    return h.hexdigest()


HOST=r'''
#include "@HEADER@"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <iterator>
using namespace mka::emitted::reference_resources;
struct Case {const char*name;std::vector<Attribute>a;std::vector<Effect>e;std::vector<Filter>f;unsigned pool,total,data;const char*file;};
static const std::vector<Case> cases={@CASES@};
int main(){
    try{unsigned programs=0,compared=0,plants=0,partial=0,alignment=0;
        for(const auto&c:cases){
            std::ifstream file(c.file,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
            if(!file.eof()&&file.fail())throw std::runtime_error("original fixture read failed");
            if(bytes.size()%4)throw std::runtime_error("unaligned original fixture");
            std::vector<Bits> words(bytes.size()/4);std::memcpy(words.data(),bytes.data(),bytes.size());const auto before=words;
            auto check=[&](std::span<const Bits> view,unsigned pool,unsigned total){return resource_word_equality(c.a,c.e,c.f,view,pool,total);};
            const auto r=check(words,c.pool,c.total);
            if(!r.equal||r.data_words!=c.data){std::cerr<<"MISMATCH "<<c.name<<" at="<<r.mismatch_index<<" expected="<<std::hex<<r.expected<<" actual="<<r.actual<<std::dec<<" compared="<<r.compared<<" code="<<r.code_words<<" pool="<<r.data_start_words<<" total="<<r.total_words<<"\n";return 2;}
            for(unsigned i=0;i<r.code_words;++i)for(unsigned bit=0;bit<32;++bit){
                words[i]^=Bits(1)<<bit;const auto bad=check(words,c.pool,c.total);words[i]^=Bits(1)<<bit;
                if(bad.equal||bad.mismatch_index!=i)throw std::runtime_error("changed executable bit accepted");++plants;
            }
            if(check(words,c.pool+1,c.total).equal||check(words,c.pool,c.total+1).equal||check(std::span<const Bits>{words}.first(r.code_words-1),c.pool,c.total).equal)
                throw std::runtime_error("pool extent or truncated storage accepted");plants+=3;
            // Unwritten alignment/data bytes have no executable identity claim.
            for(unsigned i=r.code_words;i<words.size();++i){words[i]^=0xffffffffu;if(!check(words,c.pool,c.total).equal)throw std::runtime_error("noncode bytes treated as executable identity");words[i]^=0xffffffffu;++alignment;}
            if(words!=before)throw std::runtime_error("comparator or plant modified source fixture");
            const auto layout=derive_layout(c.a,c.e,c.f);partial+=!layout.intact();++programs;compared+=r.compared;
            std::cout<<"CASE "<<c.name<<" PASS code_words="<<r.code_words<<" pool="<<c.pool<<" data_words="<<r.data_words<<" partial="<<!layout.intact()<<"\n";
        }
        std::cout<<"PASS descriptor_word_equality programs="<<programs<<" compared_words="<<compared<<" partial_programs="<<partial<<" executable_bit_and_metadata_plants="<<plants<<" noncode_controls="<<alignment<<" runtime_admission=0\n";
    }catch(const std::exception&e){std::cerr<<"REFUSED "<<e.what()<<"\n";return 1;}
}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['layout-proof','producer-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    layout=a.layout_proof/'RESULT.json';producer=a.producer_proof/'RESULT.json'
    auth.need(digest(layout)=='a4fa1d05c3f8136d17c799cc4b2debc5593ec5691264bd691b1c7696ed6b13c3','original_layout_proof_changed')
    auth.need(digest(producer)=='e3f46531c6026be4331a6b7f558a3cafad66ef3d35f0691d4a51c661f40fddf5','original_producer_proof_changed')
    lr=json.loads(layout.read_text());pr=json.loads(producer.read_text())
    auth.need(lr['status']=='PASS_BOUNDED_INTACT_LAYOUT_AND_PARTIAL_SOURCE_EVIDENCE' and not lr['input_changes'] and len(lr['rows'])==281,'original_layout_population_changed')
    core=auth.W/'tools/research/r192_aot_modules';header=core/'reference_resource_word_equality.hpp'
    deps=[Path(__file__),Path(auth.__file__),layout,producer,header,core/'reference_resource_layout.hpp',core/'semantic_resource_pipeline.hpp',core/'semantic_vector_effects.hpp',a.compiler,*(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')]
    # Existing source audit joins original ELF/listing words. Recheck its exact
    # immutable inputs, not a newly inferred source or decompiler signature.
    for row in pr['source_pins']:
        auth.need(digest(row['path'])==row['sha256'],'original_producer_input_changed:'+row['path']);deps.append(Path(row['path']))
    deps.append(Path(pr['record']['path']));auth.need(digest(deps[-1])==pr['record']['sha256'],'original_producer_records_changed')
    lines=[];cases=[]
    for c in lr['rows']:
        pins=[c['raw_original_output'],c['raw_observation']]
        for row in pins:auth.need(digest(row['path'])==row['sha256'],'original_builder_output_changed:'+c['name']);deps.append(Path(row['path']))
        observation=json.loads(Path(pins[1]['path']).read_text());pool=observation['source_data_start_words'];total=c['original_positive_return_words'];data=observation['blocks'][2][2]
        attrs='{'+','.join('{'+','.join(str(v)+'u' for v in row)+'}' for row in c['attributes'])+'}'
        effects='{'+','.join('{'+','.join(str(v)+'u' for v in (e[0],e[2],e[4] if e[0] in (2,3,25,17) else 0))+'}' for e in c['effects'])+'}'
        filters='{'+','.join('{2u,769u,0u}' for _ in range(c['filters']))+'}'
        lines.append('{'+','.join([json.dumps(c['name']),attrs,effects,filters,str(pool),str(total),str(data),json.dumps(str(Path(pins[0]['path'])).replace('\\','/'))])+'}')
        cases.append(dict(name=c['name'],original_output=pins[0],observation=pins[1],intact=c['intact_block_layout']))
    before=[dict(path=str(q.resolve()),sha256=digest(q)) for q in dict.fromkeys(deps)]
    out.mkdir(parents=True);cpp=out/'resource_word_equality.cpp';cpp.write_text(HOST.replace('@HEADER@',header.as_posix()).replace('@CASES@',',\n'.join(lines)),newline='\n')
    exe=out/'resource_word_equality.exe';argv=[str(a.compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
    cp=subprocess.run(argv,capture_output=True,timeout=120);(out/'compile.log').write_bytes(cp.stdout+cp.stderr)
    run=None
    if cp.returncode==0:
        run=subprocess.run([str(exe)],capture_output=True,timeout=120);(out/'run.log').write_bytes(run.stdout+run.stderr)
    changes=[row['path'] for row in before if digest(row['path'])!=row['sha256']]
    text=run.stdout.decode(errors='replace') if run else '';status='PASS_BOUNDED_DESCRIPTOR_WORD_EQUALITY' if cp.returncode==0 and run.returncode==0 and text.count('\nCASE ')+int(text.startswith('CASE '))==281 and not changes else 'WORD_EQUALITY_HAS_REFUSALS'
    result=dict(status=status,invocation=__import__('sys').argv,source_pins=before,cases=cases,compile_argv=argv,compile_exit=cp.returncode,run_exit=run.returncode if run else None,stdout=text,stderr=run.stderr.decode(errors='replace') if run else None,input_changes=changes,artifacts=[dict(path=str(q),sha256=digest(q)) for q in out.iterdir() if q.is_file()],scope='Code equality only: all bits of bounded source executable words, not data/numerics, ownership, ABI, Plan lifetime or runtime admission.',limits=['Original281 constructed programs, not a universal descriptor census.','Existing derive_layout limits unique ordered tags, nonempty attributes, two streams, aligned nonnegative offsets, at most20 attributes/13 effects/4 filters and compatible banks.','Positive partial source append policies are compared, not promoted to actual-caller/runtime admission.','Alignment gaps, literal pool and ambient data are intentionally outside executable identity and need separate input bindings.','No runtime instruction decoding/emission/execution, no protected input writes, zero native credit.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=status,compile_exit=cp.returncode,run_exit=result['run_exit'],summary=text.splitlines()[-1:] if text else [],stderr=result['stderr'],result_sha256=digest(target))))
    return 0 if status.startswith('PASS_') else 2


if __name__=='__main__':raise SystemExit(main())
