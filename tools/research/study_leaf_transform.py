"""Deterministic full-catalogue leaf selection and existing-binding capability study.

No ownership or native credit is awarded. Original module identities, unresolved
bodies and the standing NETR exclusion remain explicit. Ordinary JR RA is the
necessary function-return exception to the indirect-transfer restriction.
Default inputs are current. Explicit --census/--catalogue-commit replays the
original pinned5S9 catalogue without comparing its admissions to later ones.
"""
from __future__ import annotations
import argparse,collections,copy,csv,hashlib,io,json,re,statistics,struct,subprocess,sys,time
from pathlib import Path
import transform_generated_units as tr
import stack_frame_analysis as frames

VERIFIED={'verified-renderware','verified-game','verified-other-library'}
SEED='Phase5S.9:0x5A092026'
FROZEN_CATALOGUE_COMMIT='7244e8da527e8a3a7400aa53bca64b41b5ce63d4'
FROZEN_LEDGER_SHA='e48f86181d19f14bdee50b4dc95be732c0fad5116da613a58c9ab351723f8853'
FROZEN_CENSUS_SHA='a177e25db4e5e49e6b2f2fed059e82079c7d8e5bb1087de00fcc9d0c9cdbd80d'
ELF_SHA='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
OVERLAY_SHA={'GAMER.ovl':'70714c6f147128fb6462ac57bfe28e3e27a0dcf90730c4054e90c70908feba89',
 'MOVIER.ovl':'3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8',
 'NETR.ovl':'6371c8bd507b62a6cb54431a8735703dcf69919c794c2d2c404957840106d08d'}
_DECODER_PATH=Path(__file__).resolve().parents[2]/'recomp/r5900.cpp'
_DECODER_SOURCE=_DECODER_PATH.read_bytes()
_ISA={}
for _name,_size in (('kPrimary',64),('kSpecial',64),('kRegimm',32)):
    _match=re.search(r'\b'+_name+r'\s*=\s*\{([^}]+)\}',_DECODER_SOURCE.decode(),re.S)
    _ISA[_name]=re.findall(r'\b[A-Za-z][A-Za-z0-9]*\b',_match[1]) if _match else []
    if len(_ISA[_name])!=_size:raise ValueError('Pinned EE decoder table shape:'+_name)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def require(ok,why):
    if not ok:raise ValueError(why)
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':')).encode()
def dump(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
def validate_frozen_pair(commit,ledger_bytes,census_sha):
    require(commit==FROZEN_CATALOGUE_COMMIT,'frozen_catalogue_commit_pin_mismatch')
    require(census_sha==FROZEN_CENSUS_SHA,'frozen_census_pin_mismatch')
    require(hashlib.sha256(ledger_bytes).hexdigest()==FROZEN_LEDGER_SHA,'frozen_catalogue_ledger_pin_mismatch')
def frozen_input_controls():
    cases=[]
    for name,commit,blob,census_pin,expected in (
        ('wrong catalogue Git ref','0'*40,b'wrong',FROZEN_CENSUS_SHA,'frozen_catalogue_commit_pin_mismatch'),
        ('forged ledger bytes under pinned commit',FROZEN_CATALOGUE_COMMIT,b'wrong',FROZEN_CENSUS_SHA,'frozen_catalogue_ledger_pin_mismatch'),
        ('forged census pin',FROZEN_CATALOGUE_COMMIT,b'wrong','0'*64,'frozen_census_pin_mismatch')):
        try:validate_frozen_pair(commit,blob,census_pin)
        except ValueError as error:
            require(str(error)==expected,'wrong frozen input rejection');cases.append(dict(plant=name,status='PASS',rejection=str(error)))
        else:raise AssertionError('Forged frozen input accepted')
    return dict(status='PASS',cases=cases,scope='Pure input-pin mutations; no Git objects or original files changed.')
def distribution(rows):
    values=sorted(r['body_bytes'] for r in rows if r.get('body_bytes') is not None)
    bins=[(0,32),(33,64),(65,128),(129,256),(257,512),(513,1024),(1025,4096),(4097,1<<32)]
    return dict(count=len(values),total_bytes=sum(values),minimum=min(values) if values else None,
        median=statistics.median(values) if values else None,maximum=max(values) if values else None,
        bins={f'{a}..{b}':sum(a<=v<=b for v in values) for a,b in bins})
def control(word):
    op=word>>26;rt=word>>16&31
    return (op in (2,3,4,5,6,7,20,21,22,23) or op==0 and word&63 in (8,9)
            or op==1 and rt in (0,1,2,3,16,17,18,19)
            or op in (0x10,0x11,0x12) and word>>21&31==8)
def classify(entry,words):
    """Inspect every bounded word, then prove closed reachable control exits."""
    hard=collections.defaultdict(list);unknown=collections.defaultdict(list);returns=[]
    def add(group,key,pc,word,**extra):group[key].append(dict(pc=f'{pc:08x}',word=f'{word:08x}',**extra))
    for pc,word in sorted(words.items()):
        op,fn,rs,rt=word>>26,word&63,word>>21&31,word>>16&31
        if op not in (0x10,0x11,0x12,0x13,0x1c):
            decoded=_ISA['kSpecial'][fn] if op==0 else _ISA['kRegimm'][rt] if op==1 else _ISA['kPrimary'][op]
            if decoded=='Invalid':add(unknown,'invalid_or_unsupported_EE_encoding',pc,word)
        if op==3:add(hard,'direct_calls',pc,word,target=f'{((pc+4)&0xf0000000)|((word&0x3ffffff)<<2):08x}')
        if op==0 and fn==9:add(hard,'indirect_calls',pc,word,target_register=rs,virtual_dispatch='UNRESOLVED; indirect does not prove virtual')
        if op==1 and rt in (16,17,18,19):add(hard,'conditional_link_calls',pc,word)
        if op==0 and fn==8:
            if word==0x03e00008:returns.append(pc)
            else:add(hard,'nonreturn_indirect_jump',pc,word,target_register=rs)
        if op in (0x10,0x11,0x12,0x13,0x31,0x32,0x35,0x36,0x39,0x3a,0x3d,0x3e):add(hard,'FPR_or_COP',pc,word)
        if op in (0x1e,0x1f):add(hard,'wide128_load_store',pc,word)
        if op==0x1c:add(unknown,'MMI_width_or_upper_lane_unproved',pc,word)
        writes_ra=(op==0 and fn in (0,2,3,4,6,7,0x10,0x12,0x14,0x16,0x17,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x38,0x3a,0x3b,0x3c,0x3e,0x3f) and word>>11&31==31
                   or op in (8,9,10,11,12,13,14,15,24,25,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x30,0x34,0x37,0x38,0x3c) and rt==31)
        if writes_ra:add(unknown,'return_RA_identity_requires_separate_proof',pc,word)
        if op==0 and fn in (12,13,0x30,0x31,0x32,0x33,0x34,0x36) or op==1 and rt in (8,9,10,11,12,14):add(hard,'exception_or_trap',pc,word)
        if op in (8,0x18) or op==0 and fn in (0x20,0x22,0x2c,0x2e):add(hard,'potential_integer_overflow_exception',pc,word)
        if control(word):
            if pc+4 not in words:add(unknown,'missing_delay',pc,word)
            elif control(words[pc+4]):add(unknown,'control_in_delay',pc,word)
    graph={};reachable=set();processed=set();work=[entry];return_nodes=set()
    # Calls already refute the leaf predicate; their callee/return CFG is not guessed.
    if not hard:
        while work:
            pc=work.pop()
            if pc in processed:continue
            processed.add(pc)
            if pc not in words:
                unknown['body_boundary_escape'].append(dict(pc=f'{pc:08x}'));continue
            reachable.add(pc);word=words[pc];op=word>>26;rt=word>>16&31
            if word==0x03e00008:
                return_nodes.add(pc);successors=[]
            elif op==2:
                target=((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)
                if target not in words:add(hard,'external_direct_tail',pc,word,target=f'{target:08x}')
                successors=[target]
            elif op in (4,5,6,7,20,21,22,23) or op==1 and rt in (0,1,2,3):
                offset=word&65535;offset-=65536 if offset&32768 else 0
                successors=[pc+8,pc+4+offset*4]
            else:successors=[pc+4]
            graph[pc]=successors;work.extend(successors)
            if control(word) and pc+4 in words:reachable.add(pc+4)
        can_return=set(return_nodes)
        while True:
            added={pc for pc,targets in graph.items() if any(t in can_return for t in targets)}-can_return
            if not added:break
            can_return.update(added)
        if not return_nodes:unknown['no_reachable_ordinary_return'].append(dict(entry=f'{entry:08x}'))
        dead=set(graph)-can_return
        if dead:unknown['reachable_region_without_ordinary_return_path'].append(dict(addresses=[f'{p:08x}' for p in sorted(dead)]))
    return dict(hard_exclusions=dict(hard),unresolved=dict(unknown),ordinary_returns=[f'{p:08x}' for p in returns],
        reachable_words=len(reachable),reachable_pc_sha256=hashlib.sha256(canonical(sorted(reachable))).hexdigest(),
        structural_status='EXCLUDED' if hard else 'UNRESOLVED' if unknown else 'ELIGIBLE')
def instruction_mix(words):
    loads={0x1a,0x1b,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x37}
    stores={0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x3f}
    load_count=sum(w>>26 in loads for w in words.values());store_count=sum(w>>26 in stores for w in words.values())
    all_memory=loads|stores|{0x1e,0x1f,0x31,0x32,0x35,0x36,0x39,0x3a,0x3d,0x3e}
    branches=sum(w>>26 in (4,5,6,7,20,21,22,23) or w>>26==1 and w>>16&31 in (0,1,2,3) or w>>26==2 for w in words.values())
    tags=[]
    if not any(w>>26 in all_memory for w in words.values()):tags.append('memory-free')
    if load_count:tags.append('scalar-load')
    if store_count:tags.append('scalar-store')
    if branches:tags.append('branching')
    return dict(tags=tags,overlap='Tags overlap; denominators must not be summed.',scalar_loads=load_count,scalar_stores=store_count,
        direct_branch_instructions=branches,instruction_count=len(words),NOPs=sum(w==0 for w in words.values()),
        SP_relative_memory=sum(w>>26 in loads|stores and w>>21&31==29 for w in words.values()),
        GP_relative_memory=sum(w>>26 in loads|stores and w>>21&31==28 for w in words.values()))
def selector_controls():
    checks=[]
    plants=[('JR_RA',0x03e00008,None),('JALR',0x0320f809,'indirect_calls'),('FPR',0x46020840,'FPR_or_COP'),
        ('SQ',0x7fbf0010,'wide128_load_store'),('tail',0x08049000,'external_direct_tail'),('syscall',12,'exception_or_trap')]
    for name,word,reason in plants:
        words={0x1000:word,0x1004:0}
        if reason:words.update({0x1008:0x03e00008,0x100c:0})
        result=classify(0x1000,words)
        require(result['structural_status']=='ELIGIBLE' if reason is None else reason in result['hard_exclusions'],'selector plant:'+name)
        checks.append(dict(plant=name,status='PASS',expected=reason or 'ordinary return retained'))
    result=classify(0x1000,{0x1000:0x03e00008})
    require('missing_delay' in result['unresolved'],'missing delay accepted');checks.append(dict(plant='missing_delay',status='PASS'))
    result=classify(0x1000,{0x1000:0x10000008,0x1004:0,0x1008:0x03e00008,0x100c:0})
    require('body_boundary_escape' in result['unresolved'],'uncovered branch target accepted');checks.append(dict(plant='branch_extent_escape',status='PASS'))
    result=classify(0x1000,{0x1000:0x241f0010,0x1004:0x03e00008,0x1008:0})
    require('return_RA_identity_requires_separate_proof' in result['unresolved'],'RA redefinition accepted as ordinary return');checks.append(dict(plant='RA_redefinition',status='PASS'))
    result=classify(0x1000,{0x1000:0x10000000,0x1004:0,0x1008:0x03e00008,0x100c:0})
    require(result['structural_status']=='ELIGIBLE','branch target also used as delay not processed');checks.append(dict(plant='delay_instruction_also_normal_target',status='PASS'))
    result=classify(0x1000,{0x1000:0x74000000,0x1004:0x03e00008,0x1008:0})
    require('invalid_or_unsupported_EE_encoding' in result['unresolved'],'reserved primary opcode accepted');checks.append(dict(plant='reserved_EE_opcode',status='PASS'))
    result=classify(0x1000,{0x1000:0x00851020,0x1004:0x03e00008,0x1008:0})
    require('potential_integer_overflow_exception' in result['hard_exclusions'],'trapping ADD accepted');checks.append(dict(plant='trapping_integer_ADD',status='PASS'))
    return dict(status='PASS',cases=checks,scope='In-memory selector controls only; original bytes unchanged.')
def image_readers(P,F,inventory,pins):
    elf=P/'research/inputs/SLUS_214.10';data=elf.read_bytes();require(hashlib.sha256(data).hexdigest()==ELF_SHA,'main ELF identity');pins[str(elf)]=ELF_SHA
    readers={'SLUS_214.10':frames.elf_reader(data)};identities={'SLUS_214.10':dict(path=str(elf),sha256=ELF_SHA,mapping='ELF PT_LOAD file offset + VA - segment VA')};images={'SLUS_214.10':data}
    for name,expected in OVERLAY_SHA.items():
        path=P/'research/inputs/modules'/name
        if not path.is_file():path=P/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'/name
        data=path.read_bytes();require(hashlib.sha256(data).hexdigest()==expected,'overlay identity:'+name);pins[str(path)]=expected
        require(data[:4]==b'MWo3' and len(data)>=64,'overlay header:'+name)
        _,_,base,a,b,zero,init_start,init_end=struct.unpack_from('<8I',data)
        require(64+a+b==len(data) and base%16==0 and base<=init_start<=init_end<=base+len(data),'overlay byte accounting:'+name)
        def reader(va,size,data=data,base=base):
            require(base<=va and va+size<=base+len(data),'overlay address not file backed')
            return data[va-base:va-base+size]
        readers[name]=reader;images[name]=data;identities[name]=dict(path=str(path),sha256=expected,base=f'{base:08x}',file_bytes=len(data),bss_bytes=zero,
            mapping='Original MWo3 byte offset = VA - header load address; module keys never conflated.',code_data_partition='unresolved outside catalogue bounded CFG')
    return readers,images,identities
def select_population(P,F,census,inventory,pins,seed,rows):
    require(len(rows)==17510,'full catalogue denominator changed')
    readers,images,identities=image_readers(P,F,inventory,pins)
    E=P/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160'
    ranges=collections.defaultdict(list);listing={}
    for name in ('function-bodies.tsv','instructions.tsv','functions.tsv'):pins[str(E/name)]=sha(E/name)
    for r in csv.DictReader((E/'function-bodies.tsv').open(encoding='utf-8',newline=''),delimiter='\t'):
        ranges[('SLUS_214.10',r['entry'])].append((int(r['start'],16),int(r['end'],16)+1))
    for r in csv.DictReader((E/'instructions.tsv').open(encoding='utf-8',newline=''),delimiter='\t'):
        listing[int(r['address'],16)]=(r['bytes'],r['mnemonic'],r['flow_type'],r['function_entry'])
    for module in OVERLAY_SHA:
        path=P/'recomp/overlays'/(module.removesuffix('.ovl')+'-functions.csv');pins[str(path)]=sha(path)
        for r in csv.DictReader(path.open(encoding='utf-8',newline='')):
            a,b=int(r['Start'],16),int(r['End'],16);require(b-a==int(r['Size']) and a%4==b%4==0,'overlay CSV range')
            ranges[(module,f'{a:08x}')].append((a,b))
    population=[];word_cache={}
    for row in sorted(rows,key=lambda r:(r['module'],int(r['address'],16))):
        module,address=row['module'],row['address'];entry=int(address,16);spans=ranges[(module,address)];reader=readers[module]
        words={};problems=[];added=[]
        if row['catalogue_kind']=='descriptor':
            require(census['bounded_ownership']['rows'][module+':'+address]['catalogue_kind']=='descriptor','descriptor admission identity')
            problems.append('verified_descriptor_data_not_callable')
        if not spans:problems.append('missing_original_body_ranges')
        try:
            for a,b in spans:
                require(a%4==b%4==0 and b>a,'unaligned original body range')
                raw=reader(a,b-a)
                for i,(word,) in enumerate(struct.iter_unpack('<I',raw)):words[a+4*i]=word
            for pc,word in list(words.items()):
                if control(word) and pc+4 not in words:
                    words[pc+4]=int.from_bytes(reader(pc+4,4),'little');added.append(pc+4)
        except (ValueError,frames.FrameRefusal) as error:problems.append('original_mapping_or_range:'+str(error))
        joined=0;missing=[];independent_nop_delays=[]
        if module=='SLUS_214.10':
            for pc,word in words.items():
                if pc not in listing:
                    if pc in added and word==0:independent_nop_delays.append(f'{pc:08x}')
                    else:missing.append(f'{pc:08x}')
                else:
                    raw,nm,flow,owner=listing[pc]
                    if bytes.fromhex(raw)!=reader(pc,4):problems.append('listing_original_mismatch:'+f'{pc:08x}')
                    elif nm in ('??','undefined','undefined4'):problems.append('undefined_listing_word:'+f'{pc:08x}')
                    else:joined+=1
            if missing:problems.append('missing_listing_words')
        result=classify(entry,words) if words else dict(structural_status='UNRESOLVED',hard_exclusions={},unresolved={},ordinary_returns=[],reachable_words=0)
        if problems:
            result['unresolved']['body_provenance']=[dict(reasons=problems)]
            if result['structural_status']=='ELIGIBLE':result['structural_status']='UNRESOLVED'
        key=module+':'+address;in_scope=row['status']!='out-of-scope-stubbed';callable_row=row['catalogue_kind']=='callable'
        item=dict(key=key,module=module,address=address,ownership_status=row['ownership_status'],catalogue_kind=row['catalogue_kind'],in_scope=in_scope,
            body_ranges=[dict(start=f'{a:08x}',end_exclusive=f'{b:08x}') for a,b in spans],body_bytes=len(words)*4,
            body_bytes_sha256=hashlib.sha256(b''.join(reader(pc,4) for pc in sorted(words))).hexdigest(),
            addressed_words_sha256=hashlib.sha256(canonical(sorted(words.items()))).hexdigest(),architectural_delay_closure_added=[f'{p:08x}' for p in added],
            listing_joined_words=joined,missing_listing_words=missing,independent_original_NOP_delays=independent_nop_delays,
            body_authority='Ghidra split spans + original delay closure' if module=='SLUS_214.10' else 'Original catalogue CSV bounds independently checked for closed CFG/all return exits; first-JR bound alone is insufficient',
            instruction_mix=instruction_mix(words),**result)
        if row['catalogue_kind']=='descriptor':item['structural_status']='NONFUNCTION_DESCRIPTOR'
        item['sample_eligible']=item['structural_status']=='ELIGIBLE' and in_scope and callable_row
        item['seed_rank']=hashlib.sha256(seed.encode()+b'\0'+key.encode()).hexdigest()
        population.append(item);word_cache[key]=words
    current={r['module']+':'+r['address']:r['ownership_status'] for r in rows if r['catalogue_kind']=='callable' and r['ownership_status'] in VERIFIED}
    expected={k:v['ownership_status'] for k,v in census['bounded_ownership']['rows'].items() if v['catalogue_kind']=='callable' and v['ownership_status'] in VERIFIED}
    require(current==expected,'current admitted census population changed')
    return population,word_cache,readers,images,identities,listing
class FailureTrace(tr.Lowering):
    def __init__(self,*args):self.statement_failures=[];super().__init__(*args)
    def statement(self,node):
        try:return super().statement(node)
        except tr.Refusal as error:self.statement_failures.append((error,node,self.pc));raise
def source_witness(lower,error,unit,reader):
    matches=[(node,pc) for exc,node,pc in lower.statement_failures if exc is error]
    if not matches:return dict(status='NO_STATEMENT_REFUSAL')
    node,pc=matches[0]
    def loc(value):
        while 'expansionLoc' in value:value=value['expansionLoc']
        return value
    a=loc(node.get('range',{}).get('begin',{}));b=loc(node.get('range',{}).get('end',{}));snippet=None
    if 'offset' in a and 'offset' in b:snippet=dict(start_byte=a['offset'],end_exclusive=b['offset']+b.get('tokLen',1),text=unit.read_bytes()[a['offset']:b['offset']+b.get('tokLen',1)].decode())
    source_pc=pc;join='compiler cursor; inline effect identity not inferred'
    if control(int.from_bytes(reader(pc,4),'little')):
        try:
            binding=tr.bind_scalar_statement(node,lower.cpu,pc+4,reader)
            source_pc=pc+4;join='actual AST scalar expression independently joins original control-delay word'
        except tr.Refusal:pass
    return dict(AST_kind=node.get('kind'),actual_statement=snippet,compiler_cursor_pc=f'{pc:08x}',
        original_effect_pc=f'{source_pc:08x}',original_word=f'{int.from_bytes(reader(source_pc,4),"little"):08x}',address_join=join)
def existing_transform_attempts(sample,words,readers,images,F,census,contract,compiler,O):
    by_key={r['key']:r for r in sample};pending=set(by_key);groups=collections.defaultdict(list);sources={}
    for unit in sorted((F/'build/generated-aot/phase4-all').glob('*.cpp')):
        match=re.fullmatch(r'(?:m_(MOVIER|GAMER)_)?unit_[0-9A-Fa-f]+\.cpp',unit.name)
        if not match:continue
        module=(match[1]+'.ovl') if match[1] else 'SLUS_214.10';text=unit.read_bytes().decode()
        cases={address.lower():int(case) for case,address in re.findall(r'case (\d+)u: goto L_([0-9a-fA-F]+);',text)}
        found=[k for k in pending if by_key[k]['module']==module and by_key[k]['address'] in cases]
        if not found:continue
        expected=census['aot']['all_measured_source_sha256'].get(unit.name);require(expected and sha(unit)==expected,'AOT source identity:'+unit.name)
        sources[unit]=expected
        for key in found:groups[unit].append((key,cases[by_key[key]['address']]));pending.remove(key)
        if not pending:break
    attempts=[]
    for key in sorted(pending):attempts.append(dict(key=key,status='REFUSED',reason='missing_existing_generated_entry_no_stub_or_build',first_unlowered_pc=None,new_credit=0))
    for unit,selections in groups.items():
        begin=time.perf_counter()
        try:definition=tr.function_definition(tr.clang_ast(compiler,unit,[unit.parent,F/'runtime/include'],unit.stem,F),unit.stem)
        except tr.Refusal as error:
            attempts.extend(dict(key=k,status='REFUSED',reason=str(error),first_unlowered_pc=None,new_credit=0) for k,_ in selections);continue
        parse_elapsed=time.perf_counter()-begin
        for key,entry_case in selections:
            row=by_key[key];reader=readers[row['module']];local_contract=copy.deepcopy(contract)
            # No main-image GP/native field contract is asserted for an overlay.
            if row['module']!='SLUS_214.10':local_contract['fields']=[]
            descriptor=dict(source_entry=row['address'],entry_case=entry_case,module=row['module'],unit_name=unit.stem,
                _lui_original=reader,_mtc1_original=reader,_scalar_original=reader)
            frame_status='No existing original frame proof requested'
            if words[key] and any(w>>26 in (9,25) and w>>21&31==29 and w>>16&31==29 and w&0x8000 for w in words[key].values()):
                try:
                    spans=[dict(start=s['start'],end=f'{int(s["end_exclusive"],16)-1:08x}') for s in row['body_ranges']]
                    spans += [dict(start=pc,end=f'{int(pc,16)+3:08x}') for pc in row['architectural_delay_closure_added']]
                    plan=frames.recover_stack_frame(reader,int(row['address'],16),spans)
                    descriptor.update(_frame_original=reader,_frame_plan=plan);frame_status='Existing analyzer derived original-only symbolic frame proof; no native input values'
                except frames.FrameRefusal as error:frame_status='Existing frame proof refused:'+str(error)
            if row['module']=='SLUS_214.10':descriptor['_data_symbol_image']=images[row['module']]
            begin=time.perf_counter();lower=None;ir=None;fatal=None
            try:lower=FailureTrace(definition,descriptor,local_contract);ir=lower.lower();status='LOWERED_NOT_ADMITTED_NATIVE_ABI_OR_E2';reason='Whole initializer-shaped IR does not authenticate this distinct function ABI; no native emission or E2.'
            except tr.Refusal as error:status='REFUSED';reason=str(error);fatal=error
            result=dict(key=key,status=status,reason=reason,first_unlowered_pc=f'{lower.pc:08x}' if lower else row['address'],source_unit=unit.relative_to(F).as_posix(),source_unit_sha256=sources[unit],
                AST_parse_seconds_shared=parse_elapsed,AST_parse_shared_by=len(selections),lowering_seconds=time.perf_counter()-begin,
                scalar_bindings=lower.scalar_bindings if lower else [],scalar_candidates=lower.scalar_candidates if lower else [],lui_bindings=lower.lui_bindings if lower else [],mtc1_bindings=lower.mtc1_bindings if lower else [],
                frame_proof=frame_status,frame_IR=lower.frame.report() if lower and lower.frame else None,retail_input_bindings=0,native_emission='NOT_ATTEMPTED_UNADMITTED_FUNCTION_ABI',
                E2='NOT_RUN_NO_ADMITTED_NATIVE_FUNCTION',capability_only=True,new_credit=0,
                limitation='Existing emission/return contract is specialized to initializer selected-field store and register3 liveout, not a proved general leaf ABI.')
            if lower and fatal:result['first_failure_source']=source_witness(lower,fatal,unit,reader)
            if ir is not None:
                result['candidate_IR']=ir
                try:tr.emit([(dict(descriptor,native_name='unadmitted_capability_candidate'),ir)]);result['emitter_gate']='EMISSION_POSSIBLE_BUT_FUNCTION_ABI_AND_E2_UNADMITTED'
                except tr.Refusal as error:result['emitter_gate']=str(error)
            dump(O/(key.replace(':','_')+'.json'),result);attempts.append(result)
    require(all(sha(p)==pin for p,pin in sources.items()),'AOT unit changed during attempts')
    return sorted(attempts,key=lambda r:r['key'])
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--parent-root',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--seed',default=SEED);ap.add_argument('--sample-size',type=int,default=30);ap.add_argument('--select-only',action='store_true')
    ap.add_argument('--census',type=Path,help='Explicit census; with --catalogue-commit, must be original pinned5S9 census. Default uses current published census and working-tree catalogue.')
    ap.add_argument('--catalogue-commit',help='Frozen5S9 replay: exact pinned Git commit for original17 catalogue; requires --census. Does not assert equality to later admissions.')
    ap.add_argument('--report-current-catalogue',action='store_true',help='Report current ownership counts separately; never use them to select a frozen cohort.')
    args=ap.parse_args();F=Path(__file__).resolve().parents[2];P=args.parent_root.resolve();O=args.output.resolve()
    require(args.sample_size>=30,'sample request must be at least30; report population shortage instead of reducing target')
    require(not O.exists() and O.is_relative_to((F/'build/oracle-telemetry').resolve()),'new ignored telemetry output required');O.mkdir()
    record=F/'research/runs/renderer-scope.json';ledger=F/'research/ledger/functions.csv'
    if args.catalogue_commit:
        require(args.census is not None,'frozen_catalogue_requires_explicit_census')
        # Refuse arbitrary refs before asking Git to resolve/read anything.
        require(args.catalogue_commit==FROZEN_CATALOGUE_COMMIT,'frozen_catalogue_commit_pin_mismatch')
        censuspath=args.census.resolve()
        blob=subprocess.check_output(['git','show',args.catalogue_commit+':research/ledger/functions.csv'],cwd=F)
        validate_frozen_pair(args.catalogue_commit,blob,sha(censuspath))
        catalogue_input=dict(mode='FROZEN_ORIGINAL_5S9',git_commit=args.catalogue_commit,git_path='research/ledger/functions.csv',
            ledger_sha256=hashlib.sha256(blob).hexdigest(),census_path=str(censuspath),census_sha256=sha(censuspath),
            scope='Original17 admission metadata and full catalogue frozen from Git; later current admissions do not alter this study population.')
    else:
        published=json.loads(record.read_bytes());censuspath=args.census.resolve() if args.census else F/published['bounded_census']['measured_census']['path']
        require(sha(censuspath)==published['bounded_census']['measured_census']['sha256'],'published measured census identity')
        blob=ledger.read_bytes();catalogue_input=dict(mode='CURRENT_WORKING_TREE',path=str(ledger),ledger_sha256=hashlib.sha256(blob).hexdigest(),census_path=str(censuspath),census_sha256=sha(censuspath),
            scope='Current working-tree catalogue must agree with current published census; use explicit frozen inputs to replay original5S9.')
    catalogue_rows=list(csv.DictReader(io.StringIO(blob.decode('utf-8'),newline='')));census=json.loads(censuspath.read_bytes())
    inventorypath=F/'research/ledger/aot_coverage_inventory.json';inventory=json.loads(inventorypath.read_bytes());contractpath=F/'recomp/overlay_initializer_contract.json';contract=json.loads(contractpath.read_bytes())
    pin_paths=[Path(__file__),censuspath,inventorypath,contractpath,Path(tr.__file__),Path(frames.__file__),_DECODER_PATH]
    if not args.catalogue_commit:pin_paths += [record,ledger]
    pins={str(p):sha(p) for p in pin_paths}
    require(pins[str(_DECODER_PATH)]==hashlib.sha256(_DECODER_SOURCE).hexdigest(),'decoder changed during import')
    for name,pin in contract['header_pins'].items():require(sha(F/name)==pin,'header pin:'+name);pins[str(F/name)]=pin
    compiler=Path(next(line.split('=',1)[1] for line in (F/'b/CMakeCache.txt').read_text().splitlines() if line.startswith('CMAKE_CXX_COMPILER:')));pins[str(compiler)]=sha(compiler)
    controls=selector_controls();input_controls=frozen_input_controls()
    population,words,readers,images,identities,listing=select_population(P,F,census,inventory,pins,args.seed,catalogue_rows)
    eligible=sorted([r for r in population if r['sample_eligible']],key=lambda r:(r['seed_rank'],r['key']));sample=eligible[:args.sample_size]
    for row in sample:row['original_words']=[dict(pc=f'{p:08x}',word=f'{w:08x}') for p,w in sorted(words[row['key']].items())]
    admitted=[r for r in population if r['catalogue_kind']=='callable' and r['ownership_status'] in VERIFIED]
    for row in admitted:
        for group in row['hard_exclusions'].values():
            for w in group:
                if row['module']=='SLUS_214.10' and int(w['pc'],16) in listing:
                    _,nm,flow,owner=listing[int(w['pc'],16)];w.update(listing_mnemonic=nm,listing_flow_type=flow,listing_owner=owner)
        priority=['direct_calls','indirect_calls','conditional_link_calls','nonreturn_indirect_jump','FPR_or_COP','wide128_load_store','exception_or_trap','potential_integer_overflow_exception','external_direct_tail']
        row['first_blocker']=next((k for k in priority if k in row['hard_exclusions']),next(iter(row['unresolved']),None))
        row['call_counts']=dict(direct_JAL=len(row['hard_exclusions'].get('direct_calls',[])),conditional_link=len(row['hard_exclusions'].get('conditional_link_calls',[])),
            indirect_JALR=len(row['hard_exclusions'].get('indirect_calls',[])),virtual_proved=None,virtual_status='UNKNOWN: JALR classification does not establish a concrete vtable/slot')
    dump(O/'POPULATION.json',population);dump(O/'SAMPLE.json',sample);dump(O/'ADMITTED.json',admitted)
    attempts=[] if args.select_only else existing_transform_attempts(sample,words,readers,images,F,census,contract,compiler,O)
    for attempt in attempts:
        attempt['capability_only']=True
        attempt.setdefault('E2','NOT_RUN_NO_ADMITTED_NATIVE_FUNCTION')
        attempt['native_E2_pass']=False
    attempt_map={r['key']:r for r in attempts}
    def rate_table(subset):
        actual=[attempt_map[r['key']] for r in subset if r['key'] in attempt_map]
        return dict(selected=len(subset),attempted=len(actual),native_E2_passes=0,native_pass_rate=0.0 if actual else None,
            lowerer_only_completions=sum(r['status']=='LOWERED_NOT_ADMITTED_NATIVE_ABI_OR_E2' for r in actual),
            first_refusals=dict(collections.Counter(r['reason'] for r in actual)),scope='Capability only; outside StageA proofs are not added to this cohort denominator.')
    bins=[(0,32),(33,64),(65,128),(129,256),(257,512),(513,1024),(1025,4096),(4097,1<<32)]
    prototype=inventory['memory_abi_transform'];measurement=prototype['measurement']
    require(prototype['transformed_count']==2,'existing positive prototype count changed')
    for kind in ('proof','receipt'):
        path=F/measurement[kind+'_path'];require(sha(path)==measurement[kind+'_sha256'],'existing positive '+kind+' identity')
    require(all(sha(p)==pin for p,pin in pins.items()),'measured source/input changed during study')
    summary=dict(schema='full_catalogue_leaf_transform_capability_v1',status='MEASURED_CAPABILITY_ONLY',source_sha256=pins,module_images=identities,catalogue_input=catalogue_input,
        full_catalogue_count=len(population),in_scope_raw_count=sum(r['in_scope'] for r in population),in_scope_callable_count=sum(r['in_scope'] and r['catalogue_kind']=='callable' for r in population),
        selection_rule='Complete original bounded body+delay+closed reachable CFG; no calls/nonreturn indirect/FPR-COP/128-wide/exception/external tail. Uniform seeded SHA256 ranking over module:VA, no size bias. NETR policy-excluded, reported but never sampled/transformed.',
        ordinary_return_exception='Original03E00008 JR RA is permitted as ordinary return, with its delay included; other indirect jumps are excluded.',
        seed=args.seed,sample_target=args.sample_size,sample_count=len(sample),sample_shortage=len(sample)<args.sample_size,
        population_counts=dict(collections.Counter(r['structural_status'] for r in population)),
        module_partitions={m:dict(total=sum(r['module']==m for r in population),counts=dict(collections.Counter(r['structural_status'] for r in population if r['module']==m)),
            eligible_in_scope_callable=sum(r['module']==m and r['sample_eligible'] for r in population)) for m in identities},
        structural_eligible_count=sum(r['structural_status']=='ELIGIBLE' for r in population),in_scope_eligible_count=len(eligible),
        eligibility_bounds=dict(confirmed_structural_eligible=sum(r['structural_status']=='ELIGIBLE' for r in population),
            unresolved_possible_additional=sum(r['structural_status']=='UNRESOLVED' for r in population),
            in_scope_confirmed_eligible=len(eligible),in_scope_callable_unresolved_possible_additional=sum(r['in_scope'] and r['catalogue_kind']=='callable' and r['structural_status']=='UNRESOLVED' for r in population),
            scope='Confirmed eligible count only. Unresolved provenance/ISA/width/return identity may hide additional leaves; no exact all-leaves claim.'),
        structural_eligible_size_distribution=distribution([r for r in population if r['structural_status']=='ELIGIBLE']),in_scope_eligible_size_distribution=distribution(eligible),sample_size_distribution=distribution(sample),
        admitted_verified_count=len(admitted),admitted_leaf_count=sum(r['structural_status']=='ELIGIBLE' for r in admitted),admitted_size_distribution=distribution(admitted),
        selector_controls=controls,frozen_input_controls=input_controls,attempts=attempts,attempt_status_counts=dict(collections.Counter(r['status'] for r in attempts)),first_refusal_counts=dict(collections.Counter(r['reason'] for r in attempts)),
        native_E2_passes=0,new_native_credit=0,existing_two_positive_prototypes=dict(count=2,measurement=measurement,scope='Existing separate initializer templates; not selected-cohort pass/credit or a new run.'),
        pass_rates_by_body_bytes={f'{a}..{b}':rate_table([r for r in sample if a<=r['body_bytes']<=b]) for a,b in bins},
        pass_rates_by_instruction_mix={tag:rate_table([r for r in sample if tag in r['instruction_mix']['tags']]) for tag in ('memory-free','scalar-load','scalar-store','branching')},
        instruction_mix_overlap='Memory-free is disjoint from load/store; load,store,branching overlap. Do not sum tag denominators.',
        in_scope_eligible_instruction_mix={tag:sum(tag in r['instruction_mix']['tags'] for r in eligible) for tag in ('memory-free','scalar-load','scalar-store','branching')},
        population_manifest_sha256=sha(O/'POPULATION.json'),sample_manifest_sha256=sha(O/'SAMPLE.json'),admitted_manifest_sha256=sha(O/'ADMITTED.json'),
        **{'Not verified':['No ownership promotion or new function ABI; no new native emission, saved-state/72sidecar equivalence or game run. A lowerer completion alone is not PASS. Missing/ambiguous listing, ranges or CFG remains unresolved, never silently omitted.'],
        'Contradictions with earlier documents':[f'The {catalogue_input["mode"]} admitted-only population has{len(admitted)} functions and{sum(r["structural_status"]=="ELIGIBLE" for r in admitted)} qualifying leaves. This full-catalogue sample is capability-only and changes no2/238 credit.'],
        'Falsification attempts':['In-memory selector controls reject call/FPR/wide/tail/exception/missing-delay/escaped-boundary and retain ordinaryJRRA. Original image, listing, AOT and prior positive receipt identities are checked.']})
    if args.report_current_catalogue:
        current=list(csv.DictReader(ledger.open(encoding='utf-8',newline='')))
        summary['current_catalogue_report_only']=dict(sha256=sha(ledger),ownership_counts=dict(collections.Counter(r['ownership_status'] for r in current if r['catalogue_kind']=='callable')),
            admitted_verified_count=sum(r['catalogue_kind']=='callable' and r['ownership_status'] in VERIFIED for r in current),scope='Report only; never changes frozen population or sample.')
    dump(O/'RESULT.json',summary);print(json.dumps(dict(status=summary['status'],full=len(population),eligible=len(eligible),sample=len(sample),attempts=len(attempts),sha256=sha(O/'RESULT.json'))))
if __name__=='__main__':main()
