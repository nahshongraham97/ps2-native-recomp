"""Pure original-VA custom class/vtable membership proof; ownership stays separate.

No imports, filesystem access, image mutation, runtime launch or native credit.
"""

CUSTOM_CLASS_SPEC = {'class_name':'CameraCutSequenceInstance','class_name_va':'00645810',
        'source_file':'CameraCutSequenceInstance.cpp','source_file_va':'006457a0',
        'semantic_type_name':'CameraCut','semantic_type_name_va':'00644eb8',
        'factory':'004b82e0','constructor':'004b8260','destructor':'004b81e0',
        'vptr':'0064b090','allocation_size':32,
        'method_slots':[
            {'offset':8,'method':'004b81e0'},{'offset':12,'method':'004b7f60'},
            {'offset':16,'method':'004b3370'},{'offset':20,'method':'004b8060'},
            {'offset':24,'method':'004b8010'},{'offset':28,'method':'004b8000'},
            {'offset':32,'method':'004b7ff0'},{'offset':36,'method':'004b7fe0'},
            {'offset':40,'method':'004b7f90'}]}

def prove_custom_class(original, spec):
    """Validate the specific retail chain; fail closed on any altered link.

    This bounded recognizer proves the CameraCut class, not arbitrary debug
    labels or a generic serialized type tag. No semantic type inference occurs
    from address proximity, class-name suffixes, or a dossier-only field.
    """
    evidence=[]
    def u(pc):
        raw=original(pc,4)
        if len(raw)!=4:raise ValueError(f'Short original word at {pc:08x}')
        return int.from_bytes(raw,'little')
    def require(pc, expected, reason):
        if u(pc)!=expected:raise ValueError(f'{reason} at {pc:08x}')
        evidence.append({'va':f'{pc:08x}','word_le':original(pc,4).hex(),'purpose':reason})
    def move(pc, dst, source, reason):require(pc,(source<<21)|(dst<<11)|0x2d,reason)
    def imm(pc, op, rt, rs, value, reason):require(pc,(op<<26)|(rs<<21)|(rt<<16)|(value&0xffff),reason)
    def jal(pc,target,reason):require(pc,(3<<26)|(target>>2),reason)
    def hilo(hi,lo,reg,target,reason):
        imm(hi,15,reg,0,(target+0x8000)>>16,reason+' high')
        imm(lo,9,reg,reg,target,reason+' low')
    def string(va,value,reason):
        if original(va,len(value)+1)!=value.encode('ascii')+b'\0':raise ValueError(reason)
    name=spec['class_name'];source=spec['source_file']
    if name!='CameraCutSequenceInstance' or source!=name+'.cpp':raise ValueError('Not the proved class/source identity')
    if spec['semantic_type_name']!='CameraCut':raise ValueError('Not the proved custom semantic type')
    addresses={k:int(spec[k],16) for k in ('class_name_va','source_file_va','semantic_type_name_va','factory','constructor','destructor','vptr')}
    if addresses!={'class_name_va':0x645810,'source_file_va':0x6457a0,'semantic_type_name_va':0x644eb8,
                   'factory':0x4b82e0,'constructor':0x4b8260,'destructor':0x4b81e0,'vptr':0x64b090}:
        raise ValueError('Class witness addresses differ from the original measured chain')
    if spec['allocation_size']!=32:raise ValueError('Class allocation extent changed')
    for k,label in [('class_name_va',name),('source_file_va',source),('semantic_type_name_va','CameraCut')]:string(addresses[k],label,'Original class/source/type string changed')

    # The executable custom type registry hashes CameraCut into stack+0x64,
    # copies that hash to the registration key and passes factory as a2.
    hilo(0x4b4a0c,0x4b4a18,5,0x644eb8,'original CameraCut registry type')
    jal(0x4b4a14,0x4acc20,'hash semantic type name')
    imm(0x4b4a10,9,4,29,0x64,'hash destination')
    imm(0x4b4ac8,35,2,29,0x64,'load exact CameraCut key')
    hilo(0x4b4acc,0x4b4ad8,6,0x4b82e0,'registered CameraCut constructor factory')
    imm(0x4b4ad4,9,5,29,0x50,'registration key destination')
    imm(0x4b4ae4,43,2,29,0x50,'store exact hashed registry key')
    jal(0x4b4ae0,0x4afb10,'register exact key and factory')
    # Registration wrapper carries a2 into s1 and then the actual linked-list
    # entry stores its callback at +8 and key at +4.
    move(0x4afb30,17,6,'registration preserves callback')
    move(0x4afb68,6,17,'registration forwards same callback')
    jal(0x4afb78,0x4af840,'actual instancer callback registration')
    move(0x4af864,18,6,'registry node preserves callback argument')
    imm(0x4af914,43,18,2,8,'registry node callback field')
    # Source identity is attached to another actual callback in this same
    # registration bundle, not inferred merely from adjacent string placement.
    string(0x644f00,'StandardHandlers.cpp','Registration bundle source string changed')
    hilo(0x4b4a5c,0x4b4a68,6,0x4b4c20,'same registry bundle source callback')
    jal(0x4b4a70,0x4ae710,'register source-identified callback')
    hilo(0x4b4c40,0x4b4c48,5,0x644f00,'registered callback source assert filename')
    jal(0x4b4c4c,0x2df5d0,'registered callback source assertion')

    # Exact runtime class name, source and line reach the allocator metadata.
    hilo(0x4b8350,0x4b8360,6,0x645810,'allocation class name')
    hilo(0x4b8354,0x4b8364,7,0x6457a0,'allocation class source file')
    imm(0x4b8358,9,4,0,32,'class object extent')
    imm(0x4b835c,9,5,0,3,'allocation alignment enum')
    imm(0x4b836c,9,8,0,0x31,'allocation source line')
    jal(0x4b8368,0x4bae60,'named class allocation helper')
    # This exact helper maps class name a2->allocator a3, file a3->t0,
    # line t0->t1, size a0->a1; these are not serialized resource arguments.
    helper=[0x00a0102d,0x00c0182d,0x0080282d,0x0040302d,0x8f84ab5c,
            0x00e0102d,0x0100482d,0x0060382d,0x0810a748,0x0040402d]
    for i,w in enumerate(helper):require(0x4bae60+i*4,w,'allocator metadata ABI')
    imm(0x429d30,43,7,29,0x40,'allocator retains exact class metadata')
    imm(0x429d38,43,8,29,0x44,'allocator retains source metadata')
    imm(0x429d3c,43,9,29,0x48,'allocator retains source line')

    # No substitution: branch is only a null-allocation skip; then a0 receives
    # precisely that allocation v0, and is not overwritten before constructor.
    require(0x4b8370,0x1040000e,'null allocation skips object construction')
    require(0x4b8374,0,'allocation branch delay is a NOP')
    move(0x4b837c,4,2,'allocation result is constructor this')
    expected_middle=['0400458e','2d204000','0800438e','2d480002','4400a627','4800a727',
                     '4c00a827','4400a3af','0c00428e','4800a2af','1800428e']
    for i,h in enumerate(expected_middle):require(0x4b8378+i*4,int.from_bytes(bytes.fromhex(h),'little'),'unbroken allocation-to-this argument flow')
    jal(0x4b83a4,0x4b8260,'construct exact named object')
    imm(0x4b83a8,43,2,29,0x4c,'constructor call delay preserves this')

    # Constructor preserves its incoming this, calls the base on that same
    # pointer and replaces base vptr at object+0 with this class's vptr.
    move(0x4b8270,17,4,'constructor saves this')
    jal(0x4b8274,0x4b3400,'same-this base constructor')
    move(0x4b8278,16,9,'base call delay preserves this')
    move(0x4b3410,17,4,'base constructor saves same this')
    jal(0x4b3414,0x4ae980,'same-this root constructor')
    imm(0x4ae988,43,2,4,0,'root vptr is same object +0')
    hilo(0x4b827c,0x4b8284,2,0x64b090,'derived class vptr')
    imm(0x4b828c,43,2,17,0,'class vptr stored into exact this +0')
    move(0x4b82c8,2,17,'constructor returns same this')
    # Source assertion belongs to this constructor, independently of the label
    # supplied by its caller.
    hilo(0x4b82a8,0x4b82b0,5,0x6457a0,'constructor class-qualified source')
    jal(0x4b82b4,0x2df5d0,'constructor source assertion')
    imm(0x4b82b8,9,6,0,0x49,'constructor source line')
    # Constructed pointer flows into the scene, not a disposable unrelated
    # logging allocation; registration enables actual object dispatch.
    move(0x4b83bc,5,2,'scene receives exact constructed object')
    jal(0x4b83b8,0x4b3160,'bind constructed object to scene')

    # Table slot and destructor independently close the class identity cycle.
    table=addresses['vptr']
    if u(table)!=0 or u(table+4)!=0:raise ValueError('Observed Metrowerks table header changed')
    required_targets=[0x4b81e0,0x4b7f60,0x4b3370,0x4b8060,0x4b8010,0x4b8000,0x4b7ff0,0x4b7fe0,0x4b7f90]
    expected_slots=[{'offset':8+i*4,'method':f'{p:08x}'} for i,p in enumerate(required_targets)]
    if spec['method_slots']!=expected_slots:raise ValueError('Declared class slot list changed')
    for s in expected_slots:
        if u(table+s['offset'])!=int(s['method'],16):raise ValueError('Original class vtable method slot changed')
    move(0x4b81ec,16,4,'destructor saves same this')
    hilo(0x4b81f8,0x4b81fc,2,table,'destructor reinstalls exact class vptr')
    imm(0x4b8204,43,2,16,0,'destructor same-this vptr store')
    hilo(0x4b8208,0x4b820c,2,0x64af90,'base destructor table')
    imm(0x4b8214,43,2,16,0,'base destructor same-this vptr')
    hilo(0x4b8218,0x4b821c,2,0x64af60,'root destructor table')
    imm(0x4b8220,43,2,16,0,'root destructor same-this vptr')
    return {'class_name':name,'vtable_va':f'{table:08x}','class_membership_verified':True,
            'witness_kind':'original custom type registry plus exact class allocation metadata and same-object constructor/vtable/destructor chain',
            'evidence_kind_recommendation':'E4: named C++ class membership from original runtime type information and original vtable',
            'conventional_rtti_descriptor_required':False,'method_slots':expected_slots,
            'object_size':spec['allocation_size'],'vptr_object_offset':0,'this_adjustment_observed':0,
            'instruction_witnesses':evidence,'native_credit':False,
            'registration_source_provenance':{'registry_function':'004b49d0','source_file':'StandardHandlers.cpp',
                                               'source_file_va':'00644f00','registered_source_callback':'004b4c20',
                                               'limit':'Source filename is proved in a callback registered by this bundle, not directly loaded by the registry function.'},
            'ownership_status':'unresolved',
            'ownership_limit':'Exact named C++ class membership is proved. Midway-game versus an in-house NIS library is not independently resolved by these class-name bytes alone.',
            'limits':['No runtime execution or native differential claim.',
                      'Generic serialized resource tags do not satisfy this verifier.',
                      'This evidence establishes exact CameraCutSequenceInstance class membership; broader ownership must cite original in-house mwNIS context rather than a fabricated MK7 path requirement.']}


def recover_camera_cut(original, catalogue_entries):
    """Return original-verified class membership, with unresolved ownership.

    original(va: int, length: int) -> bytes reads original PS2 virtual addresses.
    catalogue_entries is an iterable of integer PS2 entry VAs or hexadecimal
    address strings. This function performs no filesystem access or writes.
    """
    entries={int(value,16) if isinstance(value,str) else int(value) for value in catalogue_entries}
    result=prove_custom_class(original,CUSTOM_CLASS_SPEC)
    result['catalogued_methods']=[]
    result['outside_raw_catalogue']=[]
    table=int(result['vtable_va'],16)
    for slot in result['method_slots']:
        member={'class_name':result['class_name'],'vtable_va':result['vtable_va'],
                'method':slot['method'],'method_slot':f"{table+slot['offset']:08x}",
                'slot_offset':slot['offset'],'class_evidence_kind':'E4',
                'class_membership_verified':True,'ownership_status':'unresolved',
                'native_verification_level':'none','native_credit':False}
        target=result['catalogued_methods'] if int(slot['method'],16) in entries else result['outside_raw_catalogue']
        target.append(member)
    result['verified_game_count']=0
    return result


def self_test(original):
    """Original positive plus semantic mutations; no binary/file modifications."""
    full={int(slot['method'],16) for slot in CUSTOM_CLASS_SPEC['method_slots']}
    positive=recover_camera_cut(original,full)
    assert len(positive['method_slots'])==9 and len(positive['catalogued_methods'])==9
    assert positive['ownership_status']=='unresolved' and positive['verified_game_count']==0
    mutations={'class name bytes':(0x645810,b'X'),
               'semantic registry name':(0x644eb8,b'X'),
               'registered factory':(0x4b4ad8,(0x24c682f0).to_bytes(4,'little')),
               'allocation result substitution':(0x4b837c,(0x0060202d).to_bytes(4,'little')),
               'constructor source name':(0x6457a0,b'X'),
               'derived vptr':(0x4b8284,(0x2442b0c0).to_bytes(4,'little')),
               'method slot':(0x64b098,(0x4b7f60).to_bytes(4,'little')),
               'destructor class vptr':(0x4b81fc,(0x2442af90).to_bytes(4,'little')),
               'allocator label ABI':(0x4bae7c,(0x0040382d).to_bytes(4,'little')),
               'source callback name':(0x644f00,b'X'),
               'registry callback forwarding':(0x4afb68,(0x0200302d).to_bytes(4,'little'))}
    rejected=[]
    for label,(site,replacement) in mutations.items():
        def mutated(va,length):
            data=bytearray(original(va,length))
            begin=max(va,site);end=min(va+length,site+len(replacement))
            if begin<end:data[begin-va:end-va]=replacement[begin-site:end-site]
            return bytes(data)
        try:recover_camera_cut(mutated,full)
        except ValueError as error:rejected.append({'mutation':label,'rejection':str(error)})
        else:raise AssertionError('Planted failure accepted: '+label)
    def short_header(va,length):return b'' if va==0x64b090 else original(va,length)
    try:recover_camera_cut(short_header,full)
    except ValueError as error:rejected.append({'mutation':'short zero-header read','rejection':str(error)})
    else:raise AssertionError('Short original read accepted')
    bad_spec=dict(CUSTOM_CLASS_SPEC,class_name='MKActorResource')
    try:prove_custom_class(original,bad_spec)
    except ValueError as error:rejected.append({'mutation':'serialized resource tag substituted for class specification','rejection':str(error)})
    else:raise AssertionError('Serialized resource class specification accepted')
    subset=full-{0x4b8000,0x4b7ff0,0x4b7fe0}
    split=recover_camera_cut(original,subset)
    assert len(split['catalogued_methods'])==6 and len(split['outside_raw_catalogue'])==3
    return {'status':'PASS','positive_original':True,'positive_catalogue_split':[6,3],
            'ownership_not_promoted':True,'planted_rejections':rejected}

