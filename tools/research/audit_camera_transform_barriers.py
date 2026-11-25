"""Full camera target audit: original/AST scope and named whole-function gates.
No transformation, product change, caller-input invention or E2 credit.
"""
from pathlib import Path
import argparse,csv,hashlib,json
from classify_stack_call_boundaries import reader

ELF='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
BODY='c27ee861601bb38263e7ee430287b8ca18c9d65e6807fc158eb6feb73cadb398'
FRAME='624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906'
LISTING='4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'
CALLS=[0x126174,0x17f0c0,0x1815d8,0x21ca50,0x21d314]
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--parent-root',type=Path,required=True)
 ap.add_argument('--current-study',type=Path)
 ap.add_argument('--output',type=Path,required=True)
 a=ap.parse_args();F=Path(__file__).resolve().parents[2];P=a.parent_root.resolve();O=a.output.resolve()
 assert not O.exists() and O.is_relative_to(F/'build/oracle-telemetry')
 image=(P/'research/inputs/SLUS_214.10').read_bytes();assert sha(image)==ELF
 original=reader(image);body=original(0x1a64a0,0x6c);frame=original(0x1a7510,0x68)
 assert sha(body)==BODY and sha(frame)==FRAME
 listing=P/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv'
 raw=listing.read_bytes();assert sha(raw)==LISTING
 wanted={p for p in range(0x1a64a0,0x1a650c,4)}|{p for p in range(0x1a7510,0x1a7578,4)}
 for c in CALLS:wanted.update(range(c-16,c+12,4))
 rows={}
 with listing.open(encoding='utf-8-sig',newline='')as handle:
  for row in csv.DictReader(handle,delimiter='	'):
   va=int(row['address'],16)
   if va in wanted:
    assert bytes.fromhex(row['bytes'])==original(va,4)
    rows[va]=row
 assert all(p in rows for p in range(0x1a64a0,0x1a650c,4))
 assert all(p in rows for p in range(0x1a7510,0x1a7578,4))
 unit=F/'build/generated-aot/phase4-all/unit_0029.cpp';generated=unit.read_text()
 words=[]
 for pc in range(0x1a64a0,0x1a650c,4):
  label=f'L_{pc:08X}:';assert label in generated
  fragment=generated.split(label,1)[1].split('\nL_',1)[0]
  words.append({'pc':f'{pc:08x}','word':f'{int.from_bytes(original(pc,4),"little"):08x}',
                'listing':rows[pc],'generated_fragment':fragment})
 callers=[]
 for pc in CALLS:
  assert int.from_bytes(original(pc,4),'little')==0x0c000000|(0x1a64a0>>2)
  callers.append({'call_pc':f'{pc:08x}','context':[rows[p]for p in range(pc-16,pc+12,4)if p in rows],
                  'input_admission':'Source SDK pointer roles corroborated; native identity/lifetime/memory mapping NOT admitted.'})
 sdk=P.parent/'forks/rwsdk-v3.1-ps2/rwsdk/src/bacamera.c';sdkraw=sdk.read_bytes()
 marker=b'RwCameraSetViewWindow(RwCamera *camera, const RwV2d *viewWindow)';assert marker in sdkraw
 lo=sdkraw.index(marker);hi=sdkraw.index(b'RWRETURN(camera)',lo)
 sdkfragment=sdkraw[lo:hi+len(b'RWRETURN(camera)')]
 native=F/'runtime/src/native/retail_renderer_cameras.cpp';header=F/'runtime/include/mka/retail_renderer_cameras.hpp'
 nt=native.read_text();assert 'Base records have no attached frame' in nt and 'volatile float r=a/b' in nt
 barriers=[
 {'id':'CAM1','pcs':['001a64b4','001a64bc','001a64c8','001a64d4'],
  'requirement':'Typed FPR32 field loads from proved native Window/Camera owners, preserving raw payloads, source alignment and alias/observation order.',
  'state':'UNIMPLEMENTED_IN_TRANSFORMER'},
 {'id':'CAM2','pcs':['001a64c0','001a64c4','001a64d0','001a64e0'],
  'requirement':'Typed paired window/inverse field stores at source104/108/112/116 with host-owned field mapping; source offsets are evidence, never a native object layout.',
  'state':'NATIVE_FIELD_OWNER_MAPPING_UNBOUND'},
 {'id':'CAM3','pcs':['001a64cc','001a64d8'],
  'requirement':'Both EE binary32 divisions, ordering and FCR31 effects/consumers; constructed zero,negative,subnormal,infinity/NaN/payload and rounding cases. SDK debug assertions cannot remove release-binary cases.',
  'state':'SCALAR_INTEGER_BINDING_DOES_NOT_IMPLEMENT_FP_DIVISION'},
 {'id':'CAM4','pcs':['001a64e4','001a64e8','001a64f0'],
  'requirement':'Camera parent reference +4, nullable branch and real native RwFrameUpdateObjects producer for nonnull parent; no forced-null fixture.',
  'state':'FULL_PARENT_FRAME_PATH_UNBOUND'},
 {'id':'CAM5','pcs':['001a7510..001a7577'],
  'requirement':'Source frame/root dirty flags and dirty-list registration through typed native frame ownership, lifetime and side effects; original body hash identifies the helper but does not implement it.',
  'state':'CALLEE_NATIVE_EFFECTS_AND_CALL_CONTRACT_UNPROVED'},
 {'id':'CAM6','pcs':['001a64e8','001a64ec','001a64f0','001a64f4','001a64f8','001a6504','001a6508'],
  'requirement':'Complete conditional/call/return bundles: execute V0=retained-camera delay effect before either branch, preserve call-return/liveout and restoreSP in JR delay after capturing target.',
  'state':'FULL_NATIVE_CONTROL_RETURN_ABI_UNBOUND'},
 {'id':'CAM7','pcs':['001a64a0','001a64a8','001a64b0','001a64fc','001a6500'],
  'requirement':'Eliminate/reference-project source RA64/S0128 frame transport only after escape,callee-clobber and liveout proofs; preserve source widths without a guest frame/register object.',
  'state':'REFERENCE_ONLY_FRAME_IR_NOT_NATIVE_EMITTABLE'},
 {'id':'CAM8','pcs':['001a64b8','001a64ec','001a64f8'],
  'requirement':'Typed native receiver/return identity for A0->S0->V0 copy, with full object lifetime/admission; scalar syntax support is not a camera pointer binding.',
  'state':'SOURCE_INPUT_ROLES_KNOWN_NATIVE_INPUT_ADMISSION_UNBOUND'},
 {'id':'CAM9','pcs':['whole001a64a0'],
  'requirement':'Authenticated whole generated native function, independent observable-state projection and all61 state differential admissions, plus branch/numeric/alias/error cases. A forced entry with unrelated snapshot registers is not an authenticated historical invocation.',
  'state':'EQUIVALENCE_STAGE_NOT_REACHED'}]
 current=None
 if a.current_study:
  raw=a.current_study.read_bytes();study=json.loads(raw)
  sample=next(r for r in study['samples']if r['address']=='001a64a0')
  assert study['inputs']['original_elf_sha256']==ELF
  current={'receipt_sha256':sha(raw),'reason':sample['reason'],'first_pc':sample['first_unlowered_pc'],
           'cause':sample.get('first_failure_cause'),'full_function_equivalence':sample['differential']}
 result={'schema':'camera_whole_function_gate_inventory_v1','status':'WHOLE_FUNCTION_TARGET_OPEN_COMPLETE_BARRIER_LIST',
  'source_sha256':sha(Path(__file__).read_bytes()),'original_elf_sha256':ELF,'camera_body_sha256':BODY,
  'camera_range':'001a64a0..001a650b','instruction_count':27,'listing_sha256':LISTING,
  'actual_AOT_unit':unit.relative_to(F).as_posix(),'actual_AOT_unit_sha256':sha(unit.read_bytes()),
  'all27_original_listing_AST_fragments':words,'all5_original_callers':callers,
  'source_input_roles':{'A0':'Original32-bit camera reference; native receiver identity/lifetime is unbound.',
                       'A1':'Original32-bit const2component binary32 vector reference; host Window value type is corroborating reference, not guest-pointer widening.',
                       'native_handwritten_API':'NativeRetailRendererCameraHandle + RetailRendererCameraWindow by value, base/unattached-frame scope only.'},
  'SDK_corroboration':{'path':str(sdk),'sha256':sha(sdkraw),'fragment':sdkfragment.decode(errors='replace'),
                      'scope':'Reference type/role corroboration; actual retail instructions govern behavior, not debug assertions.'},
  'nonnull_parent_callee':{'entry':'001a7510','body_sha256':FRAME,'all26_listing_words':[rows[p]for p in range(0x1a7510,0x1a7578,4)],
                          'role':'RwFrameUpdateObjects','typed_native_production_effects':'UNBOUND'},
  'handwritten_source_pins':{str(p.relative_to(F)):sha(p.read_bytes())for p in [native,header]},
  'remaining_whole_function_barriers':barriers,'current_probe':current,
  '61_state_E2_status':'NOT_RUN_EQUIVALENCE_STAGE_NOT_REACHED; all61 remain required when a whole native candidate and admitted inputs exist.',
  'new_verified_units':0,'native_credit_added':0,'protected_or_runtime_changes':False}
 O.mkdir();(O/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
 print(json.dumps({'status':result['status'],'receipt_sha256':sha((O/'RESULT.json').read_bytes()),'barriers':len(barriers),'camera_instructions':27,'callee_instructions':26}))
if __name__=='__main__':main()
