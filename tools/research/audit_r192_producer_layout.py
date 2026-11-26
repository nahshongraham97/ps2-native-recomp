"""Prove bounded original producer block lengths and physical slot allocation.

Observes actual original AOT through side-effect-free private label hooks.
Intact output admission is separate from source-supported partial emission:
overflowing descriptors are retained as OPEN domains, never called impossible.
This offline model emits no instruction stream and changes no runtime route.
"""
from pathlib import Path
import argparse
import csv
import json
import struct
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import verify_r192_resource_pipeline as cohort_source
import analyze_r192_builder_domain as auth

CAPACITIES=[16,8,20,64,12,24,16]
OFFSETS=[32,36,40,28,44,48,52]
FIXED=[(10,2),(2,2),(11,2),(0,2),(0,0),(4,2),(6,2),(8,2),(5,2),(7,2),(9,2),(12,1),(1,0)]


def allocator_model(case,context=0x1c10000):
    nodes=[];occupied=[set(),set(range(20,32)),set()]
    def add(tag,reg,bank,base=0,offset=0):
        occupied[min(bank,2)].add(reg);nodes.append([tag&0xffffffff,reg,bank,base,offset&0xffffffff])
    for reg,bank in FIXED:add(-1,reg,bank)
    def allocate(tag,bank,base=0,offset=0):
        pool=occupied[min(bank,2)]
        free=next((i for i in range(32) if i not in pool),None)
        auth.need(free is not None and len(nodes)<128,'original_allocator_exhausted_not_modeled')
        add(tag,free,bank,base,offset)
    for tag,stream,offset in case['attributes']:
        cls=cohort_source.schema_class(tag);bank=0 if cls<=2 else cls-2
        allocate(tag,bank,context+3740+stream*16,offset)
    retained=next((a for a in case['attributes'] if a[0]==259),None)
    if retained:allocate(358,2,context+3740+retained[1]*16,retained[2])
    used=0;intended=0;V=S=R=ops=0;zero=False;writes={};vectors=[]
    def append(role):
        nonlocal used,intended
        intended+=1
        if used>=20:return -1
        at=used;used+=1;writes[at*4]=role;return at
    def scalar_constant(role):
        at=append(role);allocate(0,1,context+3772,at*4 if at>=0 else -1)
    def scalar_zero():
        nonlocal zero
        if not zero:allocate(-1,1);zero=True
    for ei,e in enumerate(case['effects']):
        if e[0] in (5,6,26):
            if cohort_source.schema_class(e[2])==1:
                V+=1;aligned=(used+3)//4*4
                if aligned<=20:used=aligned
                intended=(intended+3)//4*4
                first=append(f'effect{ei}.vector_literal0');append(f'effect{ei}.vector_literal1');append(f'effect{ei}.vector_literal2')
                allocate(0,0,context+3772,first*4 if first>=0 else -1);vectors.append(dict(effect_index=ei,data_byte_offset=first*4 if first>=0 else 0xffffffff))
            else:S+=1;scalar_constant(f'effect{ei}.scalar_literal')
            ops+=1
        elif e[0] in (2,3,25):
            ops+=2
            if cohort_source.schema_class(e[4])==3:scalar_zero()
        elif e[0]==1:ops+=1
        elif e[0]==17:
            R+=1;scalar_zero()
            for role in ['upper_converted','scale','start','extent']:scalar_constant(f'effect{ei}.remap_{role}')
            allocate(-1,1)
    for fi in range(case['filters']):scalar_constant(f'filter{fi}.limit')
    F=case['filters'];L=len(case['attributes']);Q=int(retained is not None)
    lengths=[V+S+4*R+F+int(zero),V+S,intended,6+L+Q+ops,3*F,1+L+Q+6*R,8+R]
    return dict(nodes=nodes,intended_lengths=lengths,original_data_used=used,data_write_roles=writes,vector_fetches=vectors,
                overflow_components=[i for i,(n,c) in enumerate(zip(lengths,CAPACITIES)) if n>c])


def extra_cases():
    attributes=cohort_source.cases()[20]['attributes']
    tags=[r[0] for _,table,_ in cohort_source.schema.TABLES for r in table]+[0x201]
    all_attributes=[[tag,0,i*16] for i,tag in enumerate(tags)]
    scalar=cohort_source.effect(6,769);vector=cohort_source.effect(5,256)
    remap=cohort_source.effect(17,257,769);madd=cohort_source.effect(2,256,256)
    return [dict(name='source_partial_multiply9',attributes=attributes,effects=[scalar]*9,filters=0),
            dict(name='source_partial_data23',attributes=attributes,effects=[vector]*6,filters=0),
            dict(name='source_partial_load17',attributes=[[257,0,0],[769,1,0]],effects=[scalar]*8+[remap],filters=4),
            dict(name='intact_main53_unique_tags',attributes=all_attributes,effects=[madd]*13,filters=4),
            dict(name='source_partial_accepted28',attributes=all_attributes,effects=[remap],filters=0)]


def literal(case):
    return '{"'+case['name']+'",{'+','.join('{'+','.join(map(str,a))+'}' for a in case['attributes'])+'},{'+','.join('{'+','.join(str(w)+'u' for w in e)+'}' for e in case['effects'])+'},'+str(case['filters'])+'}'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['pipeline-proof','elf','listing','compiler','output']:
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.pipeline_proof/'RESULT.json';auth.need(auth.pin(receipt)['sha256']=='6a22a60c45bd46e78b00a737f519015809de187055fb2aac00bd8025099769ab','whole_resource_proof_changed')
    auth.need(auth.pin(a.elf)['sha256']==auth.ELF_SHA and auth.pin(a.listing)['sha256']==auth.LISTING_SHA,'original_ELF_listing_changed')
    prior=json.loads(receipt.read_text());source=a.pipeline_proof/'original_composition_builder.cpp'
    auth.need(any(auth.pin(source)==r for r in prior['artifacts']),'original_builder_fixture_changed')
    for pin in prior['source_pins']:auth.need(auth.pin(Path(pin['path']))==pin,'whole_resource_source_changed')
    text=source.read_text();mark='static std::map<unsigned,unsigned> entries_observed;'
    auth.need(text.count(mark)==1 and text.count('L_002EDA04:\n')==1,'original_observation_label_missing')
    text=text.replace(mark,HOOK+mark).replace('L_002EDA04:\n','L_002EDA04:\n    capture_layout(c);\n')
    text=text.replace('const Case cases[]={','std::vector<Case> cases={')
    needle=' for(const auto&test:cases){';auth.need(text.count(needle)==1,'original_case_loop_missing')
    extra=extra_cases();text=text.replace(needle,' const Case extra[]={'+','.join(map(literal,extra))+'};cases.insert(cases.end(),std::begin(extra),std::end(extra));\n'+needle+'captured_layout=false;')
    old='if(test.filters){std::memset(g_ram+object+84,0,24);st16(object+84,2);st32(object+88,769);st32(object+104,0x3f800000);}'
    auth.need(text.count(old)==1,'filter_fixture_seam_missing')
    text=text.replace(old,'for(unsigned i=0;i<test.filters;++i){const auto at=object+84+i*24;std::memset(g_ram+at,0,24);st16(at,2);st32(at+4,769);st32(at+20,0x3f800000);}')
    text=text.replace(a.pipeline_proof.resolve().as_posix(),out.as_posix())
    needle='unsigned words=c.r[2].w[0],code=ld32(object+0x394);';auth.need(text.count(needle)==1,'original_final_return_seam_missing')
    text=text.replace(needle,DUMP.replace('@OUTPUT@',out.as_posix())+needle)
    original=auth.Original(a.elf.read_bytes());joins=[]
    with a.listing.open(encoding='utf8',newline='') as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            pc=int(row['address'],16)
            if not (0x2eda70<=pc<0x2ee2e0 or 0x2ee3b0<=pc<0x2ee5f0 or 0x2eff20<=pc<0x2f0600):continue
            word=int.from_bytes(bytes.fromhex(row['bytes']),'little');auth.need(original.word(pc)==word,'allocator_or_block_word_join_changed')
            joins.append(dict(pc=f'{pc:08X}',word=f'{word:08X}',mnemonic=row['mnemonic']))
    inputs=[Path(__file__),Path(cohort_source.__file__),Path(auth.__file__),receipt,source,a.elf,a.listing,a.compiler,
            *(Path(p['path']) for p in prior['source_pins'])]
    pins=[auth.pin(p) for p in dict.fromkeys(inputs)]
    out.mkdir(parents=True);cpp=out/'observed_original_layout.cpp';cpp.write_text(text,encoding='utf8',newline='\n')
    command=list(prior['commands'][0]['argv']);auth.need(Path(command[0]).resolve()==a.compiler.resolve(),'compiler_identity_changed')
    command[-3:]=[str(cpp),'-o',str(out/'observed_original_layout.exe')]
    cp=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(cp.stdout+cp.stderr);auth.need(cp.returncode==0,'original_producer_observer_compile_failed')
    run=subprocess.run([str(out/'observed_original_layout.exe')],capture_output=True);(out/'run.log').write_bytes(run.stdout+run.stderr);auth.need(run.returncode==0,'original_producer_observation_failed')
    rows=[]
    for case in cohort_source.cases()+extra:
        measured=json.loads((out/(case['name']+'.layout.json')).read_text());model=allocator_model(case)
        actual=[b[2] for b in measured['blocks']];caps=[b[0] for b in measured['blocks']]
        auth.need(caps==CAPACITIES,'original_sub_block_capacities_changed')
        auth.need(measured['nodes']==model['nodes'],'source_derived_register_node_mapping_mismatch:'+case['name'])
        auth.need(actual==[min(n,c) for n,c in zip(model['intended_lengths'],CAPACITIES)],'source_block_length_or_clamp_mismatch:'+case['name'])
        auth.need(measured['return_words']>0,'source_partial_positive_return_lost')
        fetched=[];raw=(out/(case['name']+'.bin')).read_bytes()
        for item in model['vector_fetches']:
            at=item['data_byte_offset']
            if at==0xffffffff:
                fetched.append(dict(**item,fourth_word_origin='FAILED_ORIGINAL_APPEND_OFFSET_PARTIAL_DOMAIN',value=None));continue
            relative=at+12;absolute=measured['source_data_start_words']*4+relative
            origin=model['data_write_roles'].get(relative)
            if origin is None:origin='UNWRITTEN_POOL_ALIGNMENT_WORD' if relative<model['original_data_used']*4 else 'OUTSIDE_COPIED_DATA_OUTPUT_ALLOCATION_WORD'
            auth.need(absolute+4<=len(raw),'explicit_full_vector_fetch_bytes_missing')
            fetched.append(dict(**item,fourth_word_origin=origin,value=f'{struct.unpack_from("<I",raw,absolute)[0]:08X}',
                                status='MEASURED_SOURCE_APPEND' if relative in model['data_write_roles'] else 'EXPLICIT_AMBIENT_INPUT_NOT_ZERO_ASSUMED'))
        rows.append(dict(**case,node_count=len(model['nodes']),node_mapping=model['nodes'],intended_lengths=model['intended_lengths'],original_lengths=actual,
                         capacities=caps,original_positive_return_words=measured['return_words'],
                         intact_block_layout=not model['overflow_components'],partial_emission_components=model['overflow_components'],
                         vector_constant_full_fetches=fetched,raw_original_output=auth.pin(out/(case['name']+'.bin')),raw_observation=auth.pin(out/(case['name']+'.layout.json'))))
    controls=[]
    for label,mutate in [('wrong_physical_slot',lambda m:m['nodes'][13].__setitem__(1,(m['nodes'][13][1]+1)%32)),
                         ('wrong_retained_width_bank',lambda m:m['nodes'][20].__setitem__(2,4)),
                         ('wrong_constant_alignment',lambda m:m['nodes'][22].__setitem__(4,m['nodes'][22][4]+4))]:
        case=next(c for c in cohort_source.cases() if c['name']=='all_five_callbacks');model=allocator_model(case);mutate(model)
        actual=json.loads((out/'all_five_callbacks.layout.json').read_text())['nodes']
        auth.need(model['nodes']!=actual,'planted_allocator_mapping_accepted:'+label);controls.append(label)
    for row in rows:
        if row['partial_emission_components']:controls.append(row['name']+':intact_block_layout_refused_source_partial_preserved')
    # Components with no overflow in the restricted unique-tag domain still
    # require their ORIGINAL capacities, rather than an invented test shape.
    worst=next(r for r in rows if r['name']=='intact_main53_unique_tags')
    auth.need(worst['original_lengths'][3]==53,'main_unique_tag_bound_changed')
    controls.append('main_capacity52_mutation_refused_against_actual53_not_fake_main64_overflow')
    changes=[p['path'] for p in pins if auth.pin(Path(p['path']))!=p]
    result=dict(schema='r192_original_producer_layout_v1',status='PASS_BOUNDED_INTACT_LAYOUT_AND_PARTIAL_SOURCE_EVIDENCE' if not changes else 'FAIL',
        invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,compile_argv=command,compile_exit=cp.returncode,run_exit=run.returncode,
        observer=auth.pin(cpp),binary=auth.pin(out/'observed_original_layout.exe'),original_word_joins=joins,rows=rows,controls=controls,
        block_context_offsets=OFFSETS,block_capacities=CAPACITIES,
        block_roles=['constant_loads_and_shared_zero','constant_step_multiplications','aligned_literal_data','main_loads_PREF_effects_input_advances','filter_comparisons_branches_delays','retained_quad_remap_arithmetic_increment_attribute_stores','remap_scalar_store_output_advances_loop_guard_return_padding'],
        growth_scope='For these exact original callbacks and selected transfers, existing blocks retain their constructor capacities. Raw2EFFC0 refuses when full; inline emitting helpers test their current block and skip when full. Original code/reference model does not invent growth; complete instruction-level reachable helper audit remains distinct from observed281cases.',
        limits=['277 measured intact constructed descriptors,4 positive-return partial-emission descriptors; none are actual game builder observations.',
                'Partial emission is SOURCE-SUPPORTED, not impossible/invalid. Its original omitted-operations/ambient-register behavior remains required for universal closure and is NOT implemented by the semantic core.',
                'Physical mapping model is reference-only allocator provenance, not a runtime opcode decoder or accepted native register bank.',
                'Every vector fourth word is measured from final source output and classified as a following append, unwritten pool alignment or outside-copied-data allocation word. Ambient words remain explicit native inputs/unadmitted producer bindings, never assumed zero/descriptor+28.',
                'One-filter native semantic subset, stream/offset/alignment/alias/caller/lifetime restrictions remain; source4-filter cases here are capacity evidence only.',
                'No runtime adoption, code-buffer mutation, ownership/credit change or complete producer-universe closure.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],sha256=auth.pin(dest)['sha256'],result=str(dest))))
    return 0 if not changes else 2


HOOK=r'''
static bool captured_layout=false;
static std::array<std::array<unsigned,8>,7> captured_blocks;
static std::vector<std::array<unsigned,5>> captured_nodes;
static void capture_layout(Cpu&c){
 constexpr unsigned offsets[]={32,36,40,28,44,48,52};
 for(unsigned i=0;i<7;++i){const unsigned block=ld32(c.r[29].w[0]+160+offsets[i]);for(unsigned j=0;j<7;++j)captured_blocks[i][j]=ld32(block+j*4);captured_blocks[i][7]=block;}
 const unsigned context=ld32(c.r[28].w[0]-21920),count=ld32(context+4);if(count>128)throw std::runtime_error("node count");
 captured_nodes.resize(count);for(unsigned i=0;i<count;++i)for(unsigned j=0;j<5;++j)captured_nodes[i][j]=ld32(context+12+i*20+j*4);captured_layout=true;
}
'''
DUMP=r'''
if(!captured_layout)throw std::runtime_error("layout capture missing");
std::ofstream j(std::string("@OUTPUT@/")+test.name+".layout.json");j<<"{\"return_words\":"<<c.r[2].w[0]<<",\"source_data_start_words\":"<<ld32(object+0x394)<<",\"blocks\":[";
for(unsigned i=0;i<7;++i){if(i)j<<",";j<<"[";for(unsigned k=0;k<8;++k){if(k)j<<",";j<<captured_blocks[i][k];}j<<"]";}
j<<"],\"nodes\":[";for(unsigned i=0;i<captured_nodes.size();++i){if(i)j<<",";j<<"[";for(unsigned k=0;k<5;++k){if(k)j<<",";j<<captured_nodes[i][k];}j<<"]";}j<<"]}\n";j.close();
'''


if __name__=='__main__':raise SystemExit(main())
