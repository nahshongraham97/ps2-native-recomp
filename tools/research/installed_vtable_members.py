"""A2/A3 original-VA installed-table evidence. No filesystem access or writes.

The candidate set is the pre-amendment bounded set, never a new census. Confirmed
tables require an original game resource/factory/allocation/constructor chain.
Other candidates stay unresolved unless independently refuted. Class names are
optional: resource-tag labels are explicitly distinguished from C++ names.
"""
import hashlib
import json
from custom_vtable_members import recover_camera_cut

CANDIDATE_SET_SHA256='df6b5875ab0446c3ef8406252b9ef1b074827e8fb5b66a3e943dad2d5d1cbb04'
TABLE_PINS={
 0x64b130:(17,'e44cb87a67ee5a6244bde09d30431b9342cfb35e09bf1eede5996f42101e7363'),
 0x64bf90:(17,'ee7d23cdc4650e7414adca145bfb6d019e71311d166622ca6cf00a69dec8de3f'),
 0x64be30:(17,'3cf104d07a5109497fbcc2b20d648e1c4a735c373f8dffc0abfe1baf69cd9478'),
 0x64b090:(9,'e6a821aaebaab1072efda3ecf018203be0be13e5d798e9b320091078aeced20d')}

def candidate_set_pin(addresses):
    values=sorted(addresses)
    if len(values)!=len(set(values)):raise ValueError('Duplicate candidate table address')
    digest=hashlib.sha256(json.dumps(values,separators=(',',':')).encode()).hexdigest()
    if digest!=CANDIDATE_SET_SHA256:raise ValueError('Installed-table candidate set differs from the preserved audit pin')
    return digest

def _destination(word):
    if word==0:return None
    op=word>>26
    if op in (0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x3f,0x1f,0x39,0x3e):return None
    if op in (8,9,10,11,12,13,14,15,0x18,0x19,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x37,0x1e):return word>>16&31
    if op==0 and word&63 in (0,2,3,4,6,7,0x10,0x12,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x38,0x3a,0x3b,0x3c,0x3e,0x3f):return word>>11&31
    return -1

def _reader(original):
    refs=[]
    def u(pc):
        data=original(pc,4)
        if len(data)!=4:raise ValueError(f'Short original word at {pc:08x}')
        return int.from_bytes(data,'little')
    def require(pc,expected,purpose):
        if u(pc)!=expected:raise ValueError(f'{purpose} at {pc:08x}')
        refs.append({'va':f'{pc:08x}','bytes_le':original(pc,4).hex(),'purpose':purpose})
    def imm(pc,op,rt,rs,value,purpose):require(pc,(op<<26)|(rs<<21)|(rt<<16)|(value&0xffff),purpose)
    def move(pc,dst,src,purpose):require(pc,(src<<21)|(dst<<11)|0x2d,purpose)
    def jal(pc,target,purpose):require(pc,(3<<26)|(target>>2),purpose)
    def hilo(hi,lo,reg,target,purpose):
        imm(hi,15,reg,0,(target+0x8000)>>16,purpose+' high')
        imm(lo,9,reg,reg,target,purpose+' low')
    return u,require,imm,move,jal,hilo,refs

RESOURCE_CASES=[
 {'tag':'MKActorResource','tag_va':0x645c90,'hi':0x4bc098,'lo':0x4bc0a8,'hash_call':0x4bc0a4,
  'compare':0x4bc0ac,'hash_stack':0x7c,'mismatch':0x4bc11c,'alloc':0x4bc0bc,'size':0x41c,
  'null_branch':0x4bc0c4,'null_target':0x4bc10c,'this_move':0x4bc0d0,'ctor_call':0x4bc100,
  'ctor':0x4bb460,'save_this':0x4bb474,'this_reg':20,'vhi':0x4bb4ac,'vlo':0x4bb4b4,
  'vreg':3,'vstore':0x4bb4b8,'table':0x64b130},
 {'tag':'ParticleActorResource','tag_va':0x645cc0,'hi':0x4bc144,'lo':0x4bc150,'hash_call':0x4bc14c,
  'compare':0x4bc154,'hash_stack':0x68,'mismatch':0x4bc1bc,'alloc':0x4bc164,'size':0xd8,
  'null_branch':0x4bc16c,'null_target':0x4bc1ac,'this_move':0x4bc178,'ctor_call':0x4bc1a0,
  'ctor':0x50cd90,'save_this':0x50cda4,'this_reg':17,'vhi':0x50cdc4,'vlo':0x50cdcc,
  'vreg':3,'vstore':0x50cdd4,'table':0x64bf90},
 {'tag':'MKStaticActorResource','tag_va':0x645d00,'hi':0x4bc1e4,'lo':0x4bc1f0,'hash_call':0x4bc1ec,
  'compare':0x4bc1f4,'hash_stack':0x54,'mismatch':0x4bc260,'alloc':0x4bc204,'size':0xdc,
  'null_branch':0x4bc20c,'null_target':0x4bc250,'this_move':0x4bc218,'ctor_call':0x4bc244,
  'ctor':0x4e3050,'save_this':0x4e306c,'this_reg':17,'vhi':0x4e308c,'vlo':0x4e3094,
  'vreg':2,'vstore':0x4e3098,'table':0x64be30}]

def _resource_table(original,case):
    u,require,imm,move,jal,hilo,refs=_reader(original)
    text=case['tag'].encode()+b'\0'
    if original(case['tag_va'],len(text))!=text:raise ValueError('Original resource tag changed')
    hilo(case['hi'],case['lo'],5,case['tag_va'],'original factory game resource tag')
    jal(case['hash_call'],0x4acc20,'hash original resource tag')
    imm(case['compare'],35,3,16,0x14,'load resource type field')
    imm(case['compare']+4,35,2,29,case['hash_stack'],'load exact tag hash')
    displacement=(case['mismatch']-(case['compare']+12))//4
    imm(case['compare']+8,5,2,3,displacement,'nonmatching tag skips this constructor branch')
    jal(case['alloc'],0x3117f0,'allocate branch-specific object')
    imm(case['alloc']+4,9,4,0,case['size'],'allocation extent')
    displacement=(case['null_target']-(case['null_branch']+4))//4
    imm(case['null_branch'],20,0,2,displacement,'null allocation skips constructor')
    move(case['this_move'],4,2,'allocation result is constructor this')
    for pc in range(case['this_move']+4,case['ctor_call'],4):
        if _destination(u(pc)) in (-1,4):raise ValueError(f'Unproved allocation-this preservation at {pc:08x}')
    jal(case['ctor_call'],case['ctor'],'call exact resource-branch constructor')
    move(case['save_this'],case['this_reg'],4,'constructor retains same incoming object')
    for pc in range(case['save_this']+4,case['vstore'],4):
        w=u(pc)
        if w>>26==3:continue # Existing original constructors use preserved s-register this.
        if _destination(w) in (-1,case['this_reg']):raise ValueError(f'Constructor this clobbered or unknown at {pc:08x}')
    hilo(case['vhi'],case['vlo'],case['vreg'],case['table'],'constructor-installed table')
    imm(case['vstore'],43,case['vreg'],case['this_reg'],0,'install table into exact object +0')
    # The factory itself is selected by the original game-content registration,
    # with SceneCustomActor as the original content tag and 004BC060 as callback.
    if original(0x645ba0,len(b'SceneCustomActor\0'))!=b'SceneCustomActor\0':raise ValueError('Game content registration tag changed')
    hilo(0x4bab70,0x4bab7c,5,0x645ba0,'game content registry tag')
    jal(0x4bab78,0x4acc20,'hash game content registry tag')
    hilo(0x4bab80,0x4bab8c,6,0x4bc060,'game resource factory callback')
    jal(0x4bab90,0x4afa10,'register original game resource factory')
    return {'vtable_va':f"{case['table']:08x}",'constructor_va':f"{case['ctor']:08x}",
            'factory_va':'004bc060','factory_resource_tag':case['tag'],'tag_va':f"{case['tag_va']:08x}",
            'name':case['tag'],'name_source':'original-factory-resource-tag; not claimed as an exact C++ class name',
            'allocation_size':case['size'],'game_factory_chain_verified':True,
            'factory_callers':[{'kind':'original callback registration','installer_va':'004ba940',
                                'registration_call_va':'004bab90','registry_tag':'SceneCustomActor',
                                'runtime_caller':'not observed; no indirect-call runtime claim'}],
            'original_refs':refs}

def _inside(pc,length,text_ranges):return any(low<=pc and pc+length<=high for low,high in text_ranges)

def recover_game_tables(original,text_ranges,code_addresses,catalogue_entries):
    """A2 chain proofs, plus method pointers in byte-joined executable code."""
    code=set(code_addresses);catalogue=set(catalogue_entries)
    tables=[_resource_table(original,case) for case in RESOURCE_CASES]
    camera=recover_camera_cut(original,catalogue)
    tables.append({'vtable_va':camera['vtable_va'],'constructor_va':'004b8260','factory_va':'004b82e0',
                   'factory_resource_tag':'CameraCut','tag_va':'00644eb8','name':camera['class_name'],
                   'name_source':'original class-qualified allocation metadata + source assertion; original factory tag CameraCut',
                   'allocation_size':camera['object_size'],'game_factory_chain_verified':True,
                   'factory_callers':[{'kind':'original callback registration','installer_va':'004b49d0',
                                       'registration_call_va':'004b4ae0','registry_tag':'CameraCut',
                                       'runtime_caller':'not observed; no indirect-call runtime claim'}],
                   'original_refs':camera['instruction_witnesses']})
    for table in tables:
        u,_,_,_,_,_,_= _reader(original)
        va=int(table['vtable_va'],16)
        count,expected_hash=TABLE_PINS[va]
        table_bytes=original(va,8+count*4)
        if hashlib.sha256(table_bytes).hexdigest()!=expected_hash:raise ValueError('Original installed-table method body pin differs')
        if u(va)!=0 or u(va+4)!=0:raise ValueError('Confirmed installed-table header differs')
        methods=[]
        for offset in range(8,8+256*4,4):
            target=u(va+offset)
            if target==0:break
            if target%4 or target not in code or not _inside(target,4,text_ranges):
                raise ValueError(f'Method pointer is not original executable code: {va+offset:08x}>{target:08x}')
            methods.append({'slot_va':f'{va+offset:08x}','slot_offset':offset,'method':f'{target:08x}',
                            'pointer_word_le':original(va+offset,4).hex(),
                            'method_first_word_le':original(target,4).hex(),'catalogued':target in catalogue})
        else:raise ValueError('Unbounded confirmed table')
        if not methods:raise ValueError('Confirmed table has no proved original method')
        if len(methods)!=count:raise ValueError('Original installed-table extent differs')
        table['original_table_sha256']=expected_hash
        table['original_table_bytes']=len(table_bytes)
        table['table_extent_source']='pinned original header and uninterrupted code-pointer run, followed by original zero; not a full class hierarchy claim'
        table['methods']=methods;table['classification']='confirmed-vtable'
    return tables

def classify_candidates(original,candidates,text_ranges,code_addresses,catalogue_entries):
    """Classify every pinned candidate; preserve unresolved evidence and sharing."""
    candidate_set_pin([c['vtable_va'] for c in candidates])
    confirmed={t['vtable_va']:t for t in recover_game_tables(original,text_ranges,code_addresses,catalogue_entries)}
    if not set(confirmed)<= {c['vtable_va'] for c in candidates}:raise ValueError('Confirmed table absent from original candidate set')
    code=set(code_addresses);catalogue=set(catalogue_entries);inventory=[]
    ordered=sorted(candidates,key=lambda r:r['vtable_va'])
    next_candidate={int(a['vtable_va'],16):int(b['vtable_va'],16) for a,b in zip(ordered,ordered[1:])}
    for candidate in ordered:
        table=int(candidate['vtable_va'],16);u,require,_,_,_,_,_= _reader(original)
        stores=candidate['stores']
        if not stores:raise ValueError('Installed candidate lacks its original store witness')
        for store in stores:
            site=int(store['store_va'],16);w=u(site)
            if w>>26!=43 or (w&0xffff)!=store['object_offset']:raise ValueError('Original candidate store changed')
            for key in ('producer','consumer'):
                proof=store['constant_address_proof'][key]
                if original(int(proof['va'],16),proof['bytes']).hex()!=proof['bytes_le']:raise ValueError('Original candidate constant-load witness changed')
            high=u(int(store['constant_address_proof']['producer']['va'],16))
            low=u(int(store['constant_address_proof']['consumer']['va'],16))
            lower=low&0xffff
            if low>>26 in (9,25) and lower&0x8000:lower-=65536
            if (high>>26!=15 or low>>26 not in (9,13,25) or high>>16&31!=low>>21&31
                    or ((high&0xffff)*65536+lower)&0xffffffff!=table or w>>16&31!=low>>16&31):
                raise ValueError('Candidate store does not install its original proved table constant')
        prefix=[];stop=None
        for offset in range(8,8+256*4,4):
            pointer=u(table+offset)
            if not pointer:stop={'offset':offset,'reason':'zero word'};break
            if pointer%4 or pointer not in code or not _inside(pointer,4,text_ranges):
                stop={'offset':offset,'reason':'word lacks executable instruction-address proof','word':f'{pointer:08x}'};break
            prefix.append({'slot_offset':offset,'slot_va':f'{table+offset:08x}','method':f'{pointer:08x}',
                           'pointer_word_le':original(table+offset,4).hex(),'method_first_word_le':original(pointer,4).hex(),
                           'catalogued':pointer in catalogue})
        # Retain possible pointers after null/unknown words too. Unresolved
        # tables may have holes; an early zero cannot erase a shared target.
        scan_end=min(next_candidate.get(table,table+8+256*4),table+8+256*4)
        possible=[]
        for slot in range(table+8,scan_end,4):
            pointer=u(slot)
            if pointer and pointer%4==0 and pointer in code and _inside(pointer,4,text_ranges):
                possible.append({'slot_va':f'{slot:08x}','slot_offset':slot-table,'method':f'{pointer:08x}',
                                 'pointer_word_le':original(slot,4).hex(),'method_first_word_le':original(pointer,4).hex(),
                                 'catalogued':pointer in catalogue})
        item={'vtable_va':candidate['vtable_va'],'classification':'unresolved',
              'classification_reason':'Original constant store is verified; a constructor/factory/allocation chain has not been established. Shape alone never confirms a vtable or a non-vtable.',
              'header_bytes_le':original(table,8).hex(),'stores':stores,'method_prefix_observations':prefix,
              'possible_method_pointer_words':possible,
              'sharing_scan_end_exclusive':f'{scan_end:08x}',
              'sharing_scan_policy':'all code-pointer words after header, including holes, up to next pinned candidate or 256 slots; possible sharing, not confirmed table extent',
              'prefix_stop':stop,'methods_complete':False,
              'unknowns':['installing function constructor semantics','factory game-resource tag call chain','allocation size','full table extent/ownership']}
        if item['vtable_va'] in confirmed:
            item.update(confirmed[item['vtable_va']]);item['classification_reason']='Original game factory tag, allocation, unchanged object/constructor/vptr store, and executable-code method pointers proved under A2.'
            item['methods_complete']=True;item['unknowns']=['runtime execution and native implementation verification']
        inventory.append(item)
    references={}
    for table in inventory:
        for member in table['possible_method_pointer_words']:
            references.setdefault(member['method'],set()).add(table['vtable_va'])
    members=[]
    for table in inventory:
        if table['classification']!='confirmed-vtable':continue
        for slot in table['methods']:
            others=sorted(references.get(slot['method'],set())-{table['vtable_va']})
            outside=sorted(set(others)-set(confirmed))
            members.append({'method':slot['method'],'table':table['vtable_va'],'constructor':table['constructor_va'],
                            'factory':table['factory_va'],'tag':table['factory_resource_tag'],'name':table['name'],
                            'name_source':table['name_source'],'factory_callers':table['factory_callers'],
                            'allocation_size':table['allocation_size'],'slot_va':slot['slot_va'],'slot_offset':slot['slot_offset'],
                            'catalogued':slot['catalogued'],'game_factory_chain_verified':True,
                            'other_candidate_tables':others,'shared_outside_confirmed_game_tables':outside,
                            'promotion_eligible':slot['catalogued'] and not outside,
                            'promotion_limit':'Shared targets in unresolved/non-game candidates retain ambiguity; no outside-catalogue row is created.',
                            'original_refs':table['original_refs']+[{'va':slot['slot_va'],'bytes_le':slot['pointer_word_le'],'purpose':'original method pointer'},
                                                                   {'va':slot['method'],'bytes_le':slot['method_first_word_le'],'purpose':'original executable instruction address'}],
                            'native_verification_level':'none','native_credit':False})
    counts={status:sum(t['classification']==status for t in inventory) for status in ('confirmed-vtable','not-vtable','unresolved')}
    if sum(counts.values())!=len(candidates):raise ValueError('Candidate inventory classification is incomplete')
    return {'candidate_set_sha256':CANDIDATE_SET_SHA256,'candidate_count':len(candidates),'classification_counts':counts,
            'candidates':inventory,'game_members':members,
            'promotion_policy':'Only original game factory/constructor reachable slots are game membership evidence. A shared target in an unconfirmed candidate remains withheld. Other pointer arrays/callback-like shapes never promote ownership.',
            'not_vtable_policy':'No negative classification without actual refutation; unproved constructor semantics remain unresolved.'}

def self_test(original,candidates,text_ranges,code_addresses,catalogue_entries):
    """Actual positive replay plus semantic refusals and shared-slot control."""
    result=classify_candidates(original,candidates,text_ranges,code_addresses,catalogue_entries)
    failures=[]
    def refuse(label,callback):
        try:callback()
        except ValueError as error:failures.append({'mutation':label,'rejection':str(error)})
        else:raise AssertionError('Planted failure accepted: '+label)
    refuse('candidate removed',lambda:classify_candidates(original,candidates[:-1],text_ranges,code_addresses,catalogue_entries))
    refuse('extra candidate substituted',lambda:classify_candidates(original,candidates+[{'vtable_va':'00000000','stores':[]}],text_ranges,code_addresses,catalogue_entries))
    refuse('method text range removed',lambda:classify_candidates(original,candidates,[],code_addresses,catalogue_entries))
    refuse('joined code proof removed for actual method',lambda:classify_candidates(original,candidates,text_ranges,set(code_addresses)-{0x4bb3e0},catalogue_entries))
    first=next(c for c in candidates if c['vtable_va']=='0064b130')['stores'][0]
    site=int(first['store_va'],16);word=int.from_bytes(original(site,4),'little')
    mutations={'candidate store register':(site,(word&~(31<<16)).to_bytes(4,'little')),
               'game resource tag':(0x645c90,b'X'),
               'resource allocated pointer substituted':(0x4bc0d0,(0x0060202d).to_bytes(4,'little')),
               'resource constructor pointer offset':(0x4bb4b8,(0xae830004).to_bytes(4,'little')),
               'method points into executable-load data':(0x64b138,(0x645c90).to_bytes(4,'little'))}
    mutations['method changed to another genuine code address']=(0x64b138,(0x19f0b0).to_bytes(4,'little'))
    def overlay(site,replacement):
        def changed(va,length):
            data=bytearray(original(va,length));begin=max(va,site);end=min(va+length,site+len(replacement))
            if begin<end:data[begin-va:end-va]=replacement[begin-site:end-site]
            return bytes(data)
        return changed
    for label,(site,replacement) in mutations.items():
        refuse(label,lambda site=site,replacement=replacement:classify_candidates(overlay(site,replacement),candidates,text_ranges,code_addresses,catalogue_entries))
    # A plausible shared slot in an unconfirmed table must withhold promotion.
    shared=classify_candidates(overlay(0x59a988,(0x4bb3e0).to_bytes(4,'little')),candidates,text_ranges,code_addresses,catalogue_entries)
    member=next(m for m in shared['game_members'] if m['method']=='004bb3e0')
    if member['promotion_eligible'] or '0059a980' not in member['shared_outside_confirmed_game_tables']:
        raise AssertionError('Shared unconfirmed table did not withhold ownership promotion')
    if set(r['vtable_va'] for r in result['candidates'])!=set(c['vtable_va'] for c in candidates):raise AssertionError('Lost inventory candidate')
    if sum(result['classification_counts'].values())!=len(candidates):raise AssertionError('Classification count mismatch')
    return {'status':'PASS','positive_original':True,'candidate_inventory_complete':True,
            'classification_counts':result['classification_counts'],'planted_rejections':failures,
            'shared_unconfirmed_slot_withholds_promotion':True}
