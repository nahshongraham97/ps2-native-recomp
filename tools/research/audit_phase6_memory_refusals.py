"""Join each of the frozen13 memory refusals to its actual access width.

Instruction width/addressing is evidence; a field name/host struct/receiver
ownership is not inferred from one offset. No ownership or credit is changed.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,sys
from classify_stack_call_boundaries import reader
import retail_data_symbol_map as symbols
ROOT=Path(__file__).resolve().parents[2]
def pin(q):return dict(path=str(q.resolve()),sha256=hashlib.sha256(q.read_bytes()).hexdigest())
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('refusals','cohort','memory-proof','output'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();out=a.output.resolve();assert out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists();assert shutil.disk_usage(ROOT).free>(12<<30)
 report=json.loads(a.refusals.read_text());cohort=json.loads(a.cohort.read_text());memory=json.loads(a.memory_proof.read_text());assert memory['status']=='PASS_TYPED_GUEST_MEMORY'
 rows=[r for r in report['rows'] if r['primary_semantic_cause']=='typed_memory'];assert len(rows)==13
 images=cohort['module_images'];pins=[pin(q) for q in (Path(__file__),a.refusals,a.cohort,a.memory_proof,ROOT/'runtime/include/mka/stage_a_guest_memory.hpp')];data={}
 for name,row in images.items():
  q=Path(row['path']);assert pin(q)['sha256']==row['sha256'];pins.append(pin(q));data[name]=q.read_bytes()
 gp_witness=symbols.object_ref64E348(data['SLUS_214.10']);gp=int(gp_witness['gp_base'],16)
 pins.append(pin(Path(symbols.__file__)))
 elf=reader(data['SLUS_214.10']);table=[]
 for r in rows:
  module=r['key'].split(':')[0];pc=int(r['original_effect_PC'],16);offset=r['original_file_offset']
  original=elf(pc,4) if module=='SLUS_214.10' else data[module][offset:offset+4]
  word=int.from_bytes(original,'little');assert word==int(r['original_word'],16)
  op=word>>26;assert op in (0x28,0x2b,0x23,0x37);width={0x28:8,0x2b:32,0x23:32,0x37:64}[op];kind='store' if op in (0x28,0x2b) else 'load';base=(word>>21)&31;imm=word&65535;imm-=65536 if imm&32768 else 0
  missing=['typed_access_lowering_and_effect_order']
  if base==28:missing+=['GP_field_meaning_and_native_owner_unproved']
  else:missing+=['semantic_receiver_or_pointee_type_unproved']
  if r['compiler_cursor_PC']!=r['original_effect_PC']:missing+=['JR_delay_effect_order_must_be_retained']
  table.append(dict(key=r['key'],old_first_refusal=r['raw_first_refusal'],source_pc=r['original_effect_PC'],word=r['original_word'],width_bits=width,operation=kind,load_projection='SIGN_EXTEND32_TO_LOW64' if op==0x23 else 'LOW64_BITS' if op==0x37 else 'TRUNCATE_TO_STORE_WIDTH',base_register=base,signed_displacement=imm,nominal_ELF_REGINFO_GP_source_address=f'{(gp+imm)&0xffffffff:08x}' if base==28 else None,missing_pieces=missing,layer=f'GuestMemory.{kind}<std::uint{width}_t> plus explicit generated SSA projection',alignment=width//8,alias_policy='Shared supplied region storage; preserve source loads/stores and phi-edge parallel assignment order.',typed_struct_view='NONE_ADMITTED; explicit guest access retained',native_signature='Mechanical capability inputs only; receiver/field semantics require independent ABI evidence.'))
 changes=[r['path'] for r in pins if pin(Path(r['path']))!=r];assert not changes;out.mkdir()
 result=dict(status='PASS_13_SOURCE_JOINED_MEMORY_CLASSIFICATIONS',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,denominator=13,width_unknown=0,GP_based=sum(r['base_register']==28 for r in table),typed_struct_views_added=0,REGINFO_GP_witness=gp_witness,rows=table,limits=['Memory widths and nominal ELF GP addresses are established; none admits these three GP symbols as native fields/receivers or changes ownership/credit.','Actual StageA GP remains an explicit mechanical input; the ELF default is not invented as every possible caller value.','The layer is intermediate StageA storage, never final native memory/ABI acceptance.','Arbitrary TLB mappings, devices/MMIO, uninstalled RAM and unproved alignment modes remain refused.'])
 q=out/'RESULT.json';q.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],count=13,width_unknown=0,GP_based=result['GP_based'],sha256=pin(q)['sha256'])))
if __name__=='__main__':main()
