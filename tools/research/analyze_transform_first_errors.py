"""Read-only causal audit of the frozen39 generated-transform first errors.

Replays the unchanged AST lowerer. No guessed bindings, skipped operations,
retail/native execution, code generation or new native credit. Coverage sets
are conditional static targets, not measured removals or whole-unit forecasts.
"""
from pathlib import Path
import argparse,collections,csv,hashlib,json,itertools,gc,sys
import transform_generated_units as tr
import stack_frame_analysis as frames

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

class Trace(tr.Lowering):
 def __init__(self,*args):self.failures=[];super().__init__(*args)
 def capture(self,stage,node,op):
  try:return op()
  except tr.Refusal as error:self.failures.append((error,stage,node,self.pc));raise
 def statement(self,node):return self.capture('statement',node,lambda:super(Trace,self).statement(node))
 def expression(self,node):return self.capture('expression',node,lambda:super(Trace,self).expression(node))
 def helper(self,node):return self.capture('helper',node,lambda:super(Trace,self).helper(node))
 def address(self,node):return self.capture('address',node,lambda:super(Trace,self).address(node))

def slim(n):
 keys=['kind','opcode','castKind','name','value','isPostfix'];d={k:n[k] for k in keys if k in n}
 if 'type'in n:d['type']=n['type']
 if 'referencedDecl'in n:d['referencedDecl']={k:n['referencedDecl'][k] for k in ['kind','name','type'] if k in n['referencedDecl']}
 d['children']=[slim(c) for c in tr.children(n)]
 return d

def loc(n,source):
 def p(value):
  while 'expansionLoc'in value:value=value['expansionLoc']
  return value
 bounds=n.get('range',{});start=p(bounds.get('begin',{}));end=p(bounds.get('end',{}))
 if 'offset'not in start or 'offset'not in end:return None
 a,b=start['offset'],end['offset']+end.get('tokLen',1);raw=source.encode('utf-8')
 return dict(start_byte=a,end_byte_exclusive=b,line=raw.count(b'\n',0,a)+1,text=raw[a:b].decode('utf-8'))

def member_array(node,cpu):
 node=tr.unwrap(node)
 if node.get('kind')!='ArraySubscriptExpr':return None
 base,index=tr.children(node);base=tr.unwrap(base)
 if base.get('kind')!='MemberExpr':return None
 part=base.get('name');idx=tr.literal(index)
 if part in ('d','sd','w','sw'):
  try:r=tr.register_object(tr.children(base)[0],cpu)
  except tr.Refusal:return None
  return dict(bank='GPR',register=r,part=part,lane=idx,bits=64 if part in ('d','sd') else 32)
 if part=='f':return dict(bank='FPR',register=idx,part='raw32',bits=32)
 return None

def register_reads(node,lower):
 out=[];seen=set()
 for n in tr.walk(node):
  try:part=member_array(n,lower.cpu)
  except tr.Refusal:continue
  key=tuple(sorted(part.items())) if part else None
  if part and key not in seen:
   seen.add(key)
   item=dict(part)
   reg=part['register']
   if part['bank']=='GPR':
    if reg in lower.regs:item['binding']='existing_constant_SSA';item['value']=lower.values[lower.regs[reg]]['constant_value']
    elif lower.frame and reg in lower.frame.offsets:item['binding']='reference_only_symbolic_frame_address'
    elif reg==0:item['binding']='architectural_zero'
    else:item['binding']='unbound_entry_or_dynamic_GPR_value'
   else:item['binding']='unbound_FPR_value'
   out.append(item)
 return out

def requirements(family):
 return {
 'LUI_signed32_constant_GPR64':['GPR_low64_SSA_write','explicit_signed32_constant_cast_semantics'],
 'zero_u64_sum_GPR64':['GPR_low64_SSA_write','u64_constant_add_semantics'],
 'GPR64_argument_copy':['GPR_low64_SSA_write','u64_add_zero_move_semantics','source_livein_native_ABI_or_value_SSA'],
 'JAL_return_address_assignment':['native_direct_callee_ABI','reference_only_RA_transport','executed_call_delay_effects','call_return_and_budget_contract'],
 'GPR64_shift':['GPR_low64_SSA_write','u64_shift_semantics','source_livein_native_ABI_or_value_SSA'],
 'FPR_subtract':['FPR_raw32_value_SSA','verified_EE_subtract_and_FCR31_effects','FPR_livein_contract','original_entry_boundary_authentication'],
 'FPR_receiver_load':['FPR_raw32_value_SSA','typed_receiver_field_read','source_livein_native_ABI_or_value_SSA'],
 'conditional_branch_bundle':['scalar_compare_and_branch_CFG','executed_delay_slot_SSA','source_livein_native_ABI_or_value_SSA','continuation_entry_contract'],
 'nested_ld32_receiver':['load_expression_value_SSA','typed_receiver_field_read','source_livein_native_ABI_or_value_SSA'],
 'nested_ld32_GP':['load_expression_value_SSA','source_backed_GP_owner_and_field_binding'],
 'FPR_stack_store':['FPR_raw32_transport','closed_callee_stack_effects','closed_stack_alias_escape_and_slot_proof'],
 'relative_receiver_store':['select_existing_source_verified_relative_receiver_contract'],
 'GP_store':['source_backed_GP_owner_and_field_binding'],
 'missing_generated_entry':['authenticated_original_entry_boundary','matched_generated_entry_and_ABI']
 }[family]


def capability_sets(rows):
    """Conditional site coverage; a set union never counts an address twice."""
    family_caps={
        'LUI_signed32_constant_GPR64':{'GPR_low64_SSA_write','signed32_LUI_constant_semantics'},
        'zero_u64_sum_GPR64':{'GPR_low64_SSA_write','u64_add_zero_semantics'},
        'GPR64_argument_copy':{'GPR_low64_SSA_write','u64_add_zero_semantics','proved_entry_value_SSA_bindings'},
        'GPR64_shift':{'GPR_low64_SSA_write','u64_shift_semantics','proved_entry_value_SSA_bindings'},
        'JAL_return_address_assignment':{'native_direct_callee_ABI_bindings','reference_only_call_RA_transport','proved_call_delay_effects','call_return_budget_contract','closed_native_frame_proof'},
        'FPR_subtract':{'FPR_raw32_value_SSA','verified_EE_sub_FCR31_effects','proved_FPR_liveins','authenticated_entry_boundary'},
        'FPR_receiver_load':{'FPR_raw32_value_SSA','typed_receiver_field_bindings','proved_entry_value_SSA_bindings'},
        'nested_ld32_receiver':{'load_expression_value_SSA','typed_receiver_field_bindings','proved_entry_value_SSA_bindings'},
        'nested_ld32_GP':{'load_expression_value_SSA'},
        'FPR_stack_store':{'FPR_raw32_transport','closed_callee_stack_effects','closed_stack_alias_and_slot_proof'},
        'GP_store':{'typed_GP_field_store'},
        'conditional_branch_bundle':{'scalar_compare_branch_CFG','proved_branch_delay_SSA','proved_entry_value_SSA_bindings','continuation_entry_contract'},
        'existing_relative_receiver_contract_not_selected':{'select_existing_relative_receiver_contract'},
        'missing_generated_entry':{'authenticated_entry_boundary','matched_generated_entry_ABI'},
    }
    sites={
        'ordinary_GPR_value_assignment':{'LUI_signed32_constant_GPR64','zero_u64_sum_GPR64','GPR64_argument_copy','GPR64_shift'},
        'direct_call_bundle_RA_transport':{'JAL_return_address_assignment'},
        'FPR_assignment':{'FPR_subtract','FPR_receiver_load'},
        'nested_ld32_expression':{'nested_ld32_receiver','nested_ld32_GP'},
        'FPR_stack_transport':{'FPR_stack_store'},'GP_field_store':{'GP_store'},
        'conditional_branch_bundle':{'conditional_branch_bundle'},
        'implemented_receiver_contract_selection':{'existing_relative_receiver_contract_not_selected'},
        'missing_generated_entry':{'missing_generated_entry'},
    }
    for row in rows:
        caps=set(family_caps[row['family']])
        if row['family'] in ('GP_store','nested_ld32_GP'):
            caps.add('real_GP_owner_field:'+row['detail']['source_global_va'])
        if row['family']=='GP_store' and row['address']=='001c4a80':caps.add('proved_entry_value_SSA_bindings')
        row['minimum_stopped_instruction_capabilities']=sorted(caps)
        bundle=set(caps)
        if row['family']!='existing_relative_receiver_contract_not_selected' and row.get('original_words') and int(row['original_words'][0]['word'],16)==0x03e00008:
            bundle.add('function_return_liveout_ABI')
        row['minimum_complete_control_bundle_capabilities']=sorted(bundle)
        row['independent_E2_or_native_unit_gates']=['Complete per-function ABI/ownership proof','No residual guest-frame/reference-only effects','Applicable original saved inputs and full-function E2; no prediction from first-operation coverage']
        if row['family']=='existing_relative_receiver_contract_not_selected':
            row['independent_E2_or_native_unit_gates']+=['Existing contract emitted and18 constructed comparisons passed, but0/61 saved receivers admitted; no new credit']
        row['capability_scope']='All requirements are conjunctive. Counts assume each named per-function/field proof is actually supplied; none was invented or implemented in this analysis.'
    singles=[]
    for name,families in sites.items():
        addresses=sorted(r['address'] for r in rows if r['family'] in families)
        singles.append(dict(capability_site_family=name,count=len(addresses),addresses=addresses,
            scope='Exact current error-site family only; a dispatch handler alone does not consume its operands, original instruction or call bundle'))
    singles.sort(key=lambda r:(-r['count'],r['capability_site_family']))
    def coverage(label,caps):
        instruction=sorted(r['address'] for r in rows if set(r['minimum_stopped_instruction_capabilities'])<=caps)
        bundle=sorted(r['address'] for r in rows if set(r['minimum_complete_control_bundle_capabilities'])<=caps)
        return dict(label=label,assumed_capabilities=sorted(caps),stopped_instruction_count=len(instruction),stopped_instruction_addresses=instruction,
                    complete_control_bundle_count=len(bundle),complete_control_bundle_addresses=bundle,
                    measured_repairs=0,measured_whole_native_units=0,predicted_whole_native_units=None,
                    scope='Conditional static coverage of current stopped operations, not a measured barrier removal or whole-function prediction')
    def caps_for(family):return set().union(*(set(r['minimum_stopped_instruction_capabilities']) for r in rows if r['family']==family))
    combinations=[];combined=set()
    for label,family in [('LUI bounded scalar binding','LUI_signed32_constant_GPR64'),
                         ('plus zero-sum definition','zero_u64_sum_GPR64'),
                         ('plus copies AND all seven live-in/value proofs','GPR64_argument_copy'),
                         ('plus u64 shift and its live-in proof','GPR64_shift'),
                         ('plus ld32 expression AND both concrete GP owners','nested_ld32_GP'),
                         ('plus typed receiver load and input proof','nested_ld32_receiver')]:
        combined|=caps_for(family);combinations.append(coverage(label,combined))
    minimal=[coverage(family,caps_for(family)) for family in sorted(family_caps) if any(r['family']==family for r in rows)]
    minimal.sort(key=lambda row:(-row['stopped_instruction_count'],row['label']))
    # Mechanical singleton/union coverage is separate from the conjunctive sets.
    unions=[];seen=set()
    for single in singles:
        seen|=set(single['addresses'])
        unions.append(dict(added_family=single['capability_site_family'],union_count=len(seen),addresses=sorted(seen)))
    assert len(seen)==39 and sum(r['count'] for r in singles)==39
    assert [r['stopped_instruction_count'] for r in combinations[:4]]==[15,16,23,24]
    return dict(level1_exact_error_sites=dict(singletons=singles,cumulative_unions=unions),
                level2_conjunctive_operations=dict(minimal_sets=minimal,ranked_combinations=combinations),
                actual_repairs_applied=0,measured_whole_native_units=0,predicted_whole_native_units=None,
                qualifications=['All coverage denominators are the same39 selected addresses (33 original plus6 additional).',
                    'A generic GPR assignment case targets24 ordinary assignment sites, but alone does not consume the15 explicit casts, seven unbound copies, one u64 shift or zero-sum expression.',
                    'The3 RA assignments are actual JAL call transport; they are excluded from the15 LUI candidates and all ordinary scalar-write coverage.',
                    'Concrete GP owners and per-function live-in/callee contracts are separate proof obligations, not a universal guessed map.',
                    'All28 native frames still carry unresolved callee-stack effects. FPR spill support does not close frame/escape proof.',
                    'No combination estimates completed native functions, adoption, ownership or credit.'])


def classifier_controls(rows):
    ranked=capability_sets(rows)
    union=ranked['level1_exact_error_sites']['cumulative_unions'][-1]
    assert union['union_count']==len(set(union['addresses']))==39
    # Duplicate union members cannot increase coverage.
    a=set(ranked['level1_exact_error_sites']['singletons'][0]['addresses'])
    assert len(a|a)==len(a)
    lui=[r for r in rows if r['family']=='LUI_signed32_constant_GPR64']
    assert len(lui)==15 and all(int(r['original_words'][0]['word'],16)>>26==15 for r in lui)
    assert all(r['detail']['destination']['register'] not in (28,29,31) for r in lui)
    assert not {r['address'] for r in lui}&{r['address'] for r in rows if r['family']=='JAL_return_address_assignment'}
    assert all(c['measured_whole_native_units']==0 and c['predicted_whole_native_units'] is None for c in ranked['level2_conjunctive_operations']['ranked_combinations'])
    assert len([r for r in rows if not r['AST_replayed']])==1
    assert next(r for r in rows if not r['AST_replayed'])['address']=='00103fa0'
    return dict(status='PASS',controls=['Union deduplicates addresses','LUI15 requires original opcode0x0F and excludes RA/SP/GP',
        'JAL RA casts cannot inflate LUI count','No conditional capability set earns whole-native-unit credit',
        'Exact39 cohort includes the single missing generated entry'])


def finalize(out,F,P,O,original,sourcepins,reportpath,planpath):
    import re
    rows=out['rows']
    for row in rows:
        row.setdefault('detail',{})
        if row['family']=='relative_receiver_store':
            row['family']='existing_relative_receiver_contract_not_selected'
            row['availability']='Implemented separately; not selected by this general-contract study'
            row['existing_contract']={'path':'recomp/index_selection_contract.json','sha256':sha(F/'recomp/index_selection_contract.json'),
                'constructed_comparisons':18,'original_saved_receivers_admitted':0,'original_saved_receivers_inapplicable':61,'new_credit':0}
        if row['family']=='nested_ld32_GP':
            match=re.search(r'c\.r\[28\]\.w\[0\]\s*([+-])\s*(\d+)u',row['source_witness']['text']);assert match
            delta=int(match[2])*(1 if match[1]=='+' else -1)
            row['detail']['source_global_va']=f'{(0x6540f0+delta)&0xffffffff:08x}'
        if row['family']=='GP_store':
            row['detail']['type_limit']='SW width32 is proved. Native owner/value role is not created from the storage bits.'
        row['actual_repairs_applied']=0;row['whole_native_units_verified']=0;row['predicted_full_unit_unlock']=None
        row.pop('requirements',None)
    # Join the actual instruction listing, including executed delay words.
    listing=P/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv'
    selected={x['pc'] for r in rows for x in r.get('original_words',[])}|{f'{x:08x}' for x in range(0x1a64a4,0x1a64b0,4)}
    witnesses={}
    with listing.open(encoding='utf-8',newline='') as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            if row['address'].lower() in selected:
                pc=row['address'].lower();assert bytes.fromhex(row['bytes'])==original(int(pc,16),4)
                witnesses[pc]={'pc':pc,'word':f'{int.from_bytes(bytes.fromhex(row["bytes"]),"little"):08x}',
                               'instruction':row['mnemonic'],'function_owner':row['function_entry'] or None}
    for row in rows:
        for word in row.get('original_words',[]):
            if word['pc'] in witnesses:word.update(witnesses[word['pc']])
        if row['family']=='nested_ld32_GP' and row['first_unlowered_pc']=='00185cb0':
            row['detail']['effective_load_pc']='00185cb4';row['detail']['bundle_limit']='The stopped load executes in JR delay; completing the bundle also requires this getter\'s return/liveout ABI. Fixing ld32 alone does not establish full return lowering.'
        if row['family']=='existing_relative_receiver_contract_not_selected':row['detail']['effective_store_pc']='0014de88'
    rankings=capability_sets(rows);controls=classifier_controls(rows)
    counts=dict(collections.Counter(r['family'] for r in rows))
    assert counts['LUI_signed32_constant_GPR64']==15 and counts['GPR64_argument_copy']==7 and counts['JAL_return_address_assignment']==3
    assert sum(counts[k] for k in ['LUI_signed32_constant_GPR64','zero_u64_sum_GPR64','GPR64_argument_copy','JAL_return_address_assignment','GPR64_shift','FPR_subtract','FPR_receiver_load'])==29
    probe=next(r for r in rows if r['address']=='001a64a0')
    recommendation=dict(single_next_binding='Original LUI constant to explicit signed32→low64 SSA definition; preserve upper64',
        probe_entry='001a64a0',first_pc='001a64a4',original_word='3c023f80',actual_AST=probe['source_witness'],
        original_low64_bits='000000003f800000',interpretation='A bit pattern, not an inferred host float or native address.',
        conditional_same_shape_count=15,covered_addresses=sorted(r['address'] for r in rows if r['family']=='LUI_signed32_constant_GPR64'),
        required_capabilities=['GPR_low64_SSA_write','signed32_LUI_constant_semantics'],
        exclusions='No RA/SP/GP destination, source-pointer conversion, JAL/native-call substitution or ownership promotion.',
        next_source_operations=[witnesses[f'{p:08x}'] for p in (0x1a64a8,0x1a64ac)],
        next_refusal_prediction='Likely FPR bit-transfer at001A64AC after the intervening SD RA at001A64A8, but no new lowering/binding was run; actual next refusal remains unmeasured.',
        acceptance_plan=['Original-word and actual-AST destination/cast-width joins; reject changed signedness/type/destination.',
            'Compare actual compiled original instruction/slice and typed SSA effect over admitted saved inputs and constructed upper64/bit canaries; no game launch.',
            'Plant wrong constant, wrong destination and upper64 clobber. Separate prefix/instruction proof from whole-function E2.',
            'Rerun the unchanged39 and record the real new frontier. Retain every independent frame/callee/owner blocker and zero credit until complete E2.'],
        implemented=False,new_credit=0,predicted_whole_native_units=None)
    def canonical_types(value):
        if isinstance(value,dict):return {k:canonical_types(v) for k,v in value.items() if k!='typeAliasDeclId'}
        if isinstance(value,list):return [canonical_types(v) for v in value]
        return value
    out=canonical_types(out);out['rows']=canonical_types(rows)
    out.update(family_counts=counts,raw_cursor_error_breakdown={'LUI_signed32_constant':15,'zero_u64_sum':1,'GPR_argument_copy':7,
        'JAL_return_address_call_transport':3,'u64_shift':1,'EE_FP_subtract':1,'FPR_load':1,'actual_cursor_PC_assignments':0},
        denominators={'original_primary':33,'additional':6,'selected_union':39,'AST_replayed_exact_reason_and_PC':38,'missing_generated_entry':1},
        rankings=rankings,controls=controls,single_recommendation=recommendation,
        instruction_listing_sha256=sha(listing),driver_sha256=sha(Path(__file__)),
        source_pins_before_after_equal=all(sha(F/name)==pin for name,pin in sourcepins.items()),
        **{'Not verified':['No capability was implemented or counterfactually skipped; measured removals0 and all whole-native-unit forecastsUNKNOWN.',
                          'Native frame/callee/ownership/ABI blockers remain independent. Observed source features are not unlock counts.'],
           'Contradictions with earlier documents':['The29 cursor_assignment errors describe27 GPR writes and2 FPR writes, including3 JAL RA bookkeeping sites; none is an actual c.pc assignment or proof of virtual dispatch.',
                          '14DE80 already has a separate verified-source relative contract; general-contract selection differs, while all61 saved receivers remain inapplicable.'],
           'Falsification attempts':['Unchanged lowerer replay matched all38 available reason/PC pairs. Cohort, missing-entry, opcode/destination and no-double-count/credit controls passed.']})
    assert out['source_pins_before_after_equal']
    (O/'RESULT.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'status':'PASS','report_sha256':sha(O/'RESULT.json'),'family_counts':counts,'conditional_combinations':[r['stopped_instruction_count'] for r in rankings['level2_conjunctive_operations']['ranked_combinations']]}))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-root',type=Path,required=True)
    parser.add_argument('--study',type=Path,required=True)
    parser.add_argument('--frames',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--elf',type=Path)
    parser.add_argument('--compiler',type=Path)
    args=parser.parse_args()
    F=Path(__file__).resolve().parents[2]
    P=args.parent_root.resolve();T=F/'build/oracle-telemetry';O=args.output.resolve()
    if O.exists() or not O.is_relative_to(T.resolve()):raise ValueError('Causal report requires a new ignored telemetry directory')
    O.mkdir()
    sourcepins={name:sha(F/name) for name in ['tools/research/transform_generated_units.py','tools/research/stack_frame_analysis.py','tools/research/retail_data_symbol_map.py','tools/research/study_generated_transform.py','tools/research/analyze_transform_first_errors.py']}
    reportpath=args.study.resolve();report=json.loads(reportpath.read_bytes())
    import study_generated_transform as study
    expected={f'{x:08x}' for x in set(study.ORIGINAL_PRIMARY)|set(study.SUPPLEMENTAL)|study.CORRECTED_PROJECTIONS}
    if len(report['samples'])!=39 or {r['address'] for r in report['samples']}!=expected:raise ValueError('Exact original39 cohort required')
    if report.get('frame_stage',{}).get('stage')!='S6_EXPERIMENTAL_BORROWED_REFERENCE_CLEAR':raise ValueError('Actual frozen S6 report required')
    if report['inputs']['transformer_sha256']!=sourcepins['tools/research/transform_generated_units.py']:raise ValueError('Measured transformer changed')
    planpath=args.frames.resolve()
    if sha(planpath)!=report['frame_stage']['frame_plan_sha256']:raise ValueError('Measured frame plans changed')
    plans={r['source_entry']:r for r in json.loads(planpath.read_bytes())['targets']}
    if len(plans)!=28:raise ValueError('Exact28 frame targets required')
    image=(args.elf or P/'research/inputs/SLUS_214.10').read_bytes()
    if hashlib.sha256(image).hexdigest()!=report['inputs']['original_elf_sha256']:raise ValueError('OriginalELF pin changed')
    original=frames.elf_reader(image)
    contract_path=F/'recomp/overlay_initializer_contract.json'
    if sha(contract_path)!=report['inputs']['contract_sha256']:raise ValueError('Measured overlay contract changed')
    contract=json.loads(contract_path.read_bytes())
    for name,expected_pin in contract['header_pins'].items():
     if sha(F/name)!=expected_pin:raise ValueError('Source helper pin changed:'+name)
    generated=F/'build/generated-aot/phase4-all'
    compiler=args.compiler
    if compiler is None:
     cache=(F/'b/CMakeCache.txt').read_text()
     compiler=Path(next(x.split('=',1)[1] for x in cache.splitlines() if x.startswith('CMAKE_CXX_COMPILER:')))
    if sha(compiler)!=report['inputs']['compiler_sha256']:raise ValueError('Measured Clang compiler changed')
    if sha(F/'tools/research/stack_frame_analysis.py')!=report['frame_stage']['frame_analyzer_sha256']:raise ValueError('Measured frame analyzer changed')
    if sha(F/'tools/research/retail_data_symbol_map.py')!=report['frame_stage']['retail_symbol_map_sha256']:raise ValueError('Measured data symbol validator changed')
    results=[];groups=collections.defaultdict(list)
    for row in report['samples']:
     if row.get('generated_unit'):groups[row['generated_unit']].append(row)
     else:results.append(dict(address=row['address'],cohort=row['cohort'],reason=row['reason'],first_unlowered_pc=None,family='missing_generated_entry',requirements=requirements('missing_generated_entry'),AST_replayed=False,source_witness=None,original_words=[]))
    for unitname,rows in groups.items():
     unit=generated/unitname;assert all(sha(unit)==row['source_unit_sha256'] for row in rows);source=unit.read_bytes().decode('utf-8');definitions=tr.clang_ast(compiler,unit,[generated,F/'runtime/include'],unit.stem,F);definition=tr.function_definition(definitions,unit.stem)
     for row in rows:
      desc=dict(source_entry=row['address'],entry_case=row['entry_case'],unit_name=unit.stem,module='SLUS_214.10',_data_symbol_image=image)
      if row['address']in plans:desc.update(_frame_plan=plans[row['address']],_frame_original=original)
      current=Trace(definition,desc,contract)
      try:current.lower()
      except tr.Refusal as error:failure=error
      else:raise AssertionError('Unexpected successful lower')
      assert str(failure)==row['reason'] and current.pc==int(row['first_unlowered_pc'],16)
      chain=[(stage,node,pc) for error,stage,node,pc in current.failures if error is failure];assert chain
      stage,node,pc=chain[0];statements=[n for st,n,p in chain if st=='statement'];statement=next((n for n in statements if n.get('kind') not in ('CompoundStmt','LabelStmt')),node)
      family=None;detail={};lhs=None;rhs=None
      w=int.from_bytes(original(pc,4),'little');transfer=frames.control(pc,w);originalpcs=[pc]+([pc+4] if transfer else [])
      if row['reason']=='unbound_indirect_target:cursor_assignment':
       assert node.get('kind')=='BinaryOperator' and node['opcode']=='=';lhs,rhs=tr.children(node);destination=member_array(lhs,current.cpu);detail['destination']=destination;detail['RHS_register_reads']=register_reads(rhs,current)
       if destination and destination['bank']=='GPR':
        reg=destination['register']
        if reg==31 and transfer and transfer['kind']=='call':family='JAL_return_address_assignment';detail['callee_va']=f"{transfer['target']:08x}"
        elif w>>26==15:
         family='LUI_signed32_constant_GPR64';assert w>>16&31==reg
         v=(w&65535)<<16;v=v if v<0x80000000 else v-0x100000000;detail['original_signed_low64_bits']=f'{v&0xffffffffffffffff:016x}';detail['literal_role']='Bit pattern only; native address/float interpretation not inferred from LUI'
        elif tr.unwrap(rhs).get('kind')=='BinaryOperator' and tr.unwrap(rhs).get('opcode')=='<<':family='GPR64_shift'
        elif not detail['RHS_register_reads']:family='zero_u64_sum_GPR64';assert all(int(n['value'])==0 for n in tr.walk(rhs) if n.get('kind')=='IntegerLiteral')
        else:family='GPR64_argument_copy'
       elif destination and destination['bank']=='FPR':
        calls=[tr.declaration(tr.children(c)[0]).get('name') for c in tr.walk(rhs) if c.get('kind')=='CallExpr'];detail['RHS_calls']=calls;family='FPR_receiver_load' if 'ld32'in calls else 'FPR_subtract'
       else:raise AssertionError(('Unknown cursor assignment destination',row['address'],slim(lhs)))
      elif row['reason']=='unbound_scalar_register_part':family='conditional_branch_bundle'
      elif row['reason']=='unsupported_value_ast:CallExpr':
       assert node.get('kind')=='CallExpr';helper=tr.declaration(tr.children(node)[0]).get('name');detail['nested_helper']=helper;assert helper=='ld32'
       text=loc(node,source)['text'];family='nested_ld32_GP' if 'c.r[28]'in text else 'nested_ld32_receiver'
      elif row['reason']=='unbound_stack_FPR_or_mixed_source_type':family='FPR_stack_store'
      elif row['reason']=='unbound_memory:non_GP_base':family='relative_receiver_store'
      elif row['reason']=='unbound_memory:GP_field_or_width':family='GP_store'
      else:raise AssertionError(('Unclassified',row['address'],row['reason']))
      requirements_here=requirements(family)
      if family=='GP_store':
       address_node=node;op=tr.unwrap(address_node);base,offset=tr.children(op);delta=tr.literal(offset)*(1 if op['opcode']=='+' else -1);detail['source_global_va']=f'{(int(contract["source_gp"],16)+delta)&0xffffffff:08x}'
      result=dict(address=row['address'],cohort=row['cohort'],reason=row['reason'],first_unlowered_pc=row['first_unlowered_pc'],AST_replayed=True,family=family,requirements=requirements_here,detail=detail,
       source_unit=unitname,source_unit_sha256=sha(unit),source_witness=loc(statement,source),innermost_failure_stage=stage,innermost_AST=slim(node),failure_trace=[dict(stage=st,kind=n.get('kind'),source=loc(n,source)) for st,n,p in chain],
       original_words=[dict(pc=f'{p:08x}',word=f'{int.from_bytes(original(p,4),"little"):08x}') for p in originalpcs],
       independent_native_blockers=(current.frame.report()['native_emission_blockers'] if current.frame else [])+(current.data_symbols.report()['native_emission_blockers'] if current.data_symbols else []),
       actual_repairs_applied=0,actually_removed_first_barriers=0,whole_native_units_verified=0,predicted_full_unit_unlock=None)
      results.append(result);print(row['address'],family,flush=True)
     del definition,definitions;gc.collect()
    for name,digest in sourcepins.items():assert sha(F/name)==digest
    for unitname,items in groups.items():assert all(sha(generated/unitname)==row['source_unit_sha256'] for row in items)
    for name,pin in contract['header_pins'].items():assert sha(F/name)==pin
    assert sha(compiler)==report['inputs']['compiler_sha256'] and sha(contract_path)==report['inputs']['contract_sha256']
    results.sort(key=lambda row:row['address']);assert len(results)==39
    out=dict(schema='frozen39_actual_AST_first_refusal_causes_v1',status='READ_ONLY_CAUSAL_ANALYSIS',input_receipt_sha256=sha(reportpath),frame_analysis_sha256=sha(planpath),source_sha256=sourcepins,original_elf_sha256=hashlib.sha256(image).hexdigest(),rows=results,family_counts=dict(collections.Counter(r['family'] for r in results)),scope='Exact unchanged lowerer replay, no skip/counterfactual binding, native emission, production change or credit. Capability coverage is conditional static analysis; actual fixes/unlocks remain unmeasured.')
    (O/'PER39.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n');finalize(out,F,P,O,original,sourcepins,reportpath,planpath)

if __name__=='__main__':main()
