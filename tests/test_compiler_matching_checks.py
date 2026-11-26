"""No compilers: strict matching falsification using an actual pilot ELF object.

All variants derive from the pinned 608-byte Metrowerks pilot object. Mutations
are explicit ELF/code/relocation changes, never relabelled as compiler outputs.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,struct,sys

ROOT=Path(__file__).resolve().parents[1]
DRIVER_PATH=ROOT/'tools/research/compiler_matching_linux.py'
DRIVER_SOURCE=DRIVER_PATH.read_bytes()
SPEC=importlib.util.spec_from_file_location('compiler_matching_checks_target',DRIVER_PATH)
DRIVER=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(DRIVER)
NAME='func_0016AE10';VRAM=0x0016AE10;GP=0x006540F0

def sha(raw):return hashlib.sha256(raw).hexdigest()
class Mutant:
    def __init__(self,raw):
        self.header=bytearray(raw[:52]);off=struct.unpack_from('<I',raw,32)[0];n=struct.unpack_from('<H',raw,48)[0]
        self.sections=[list(struct.unpack_from('<10I',raw,off+i*40)) for i in range(n)]
        self.data=[bytearray(raw[s[4]:s[4]+s[5]]) for s in self.sections]
    def symbol(self,index,**fields):
        keys={'name':0,'value':1,'size':2,'info':3,'other':4,'section':5}
        values=list(struct.unpack_from('<IIIBBH',self.data[1],index*16))
        for key,value in fields.items():values[keys[key]]=value
        struct.pack_into('<IIIBBH',self.data[1],index*16,*values)
    def add_symbol(self,name,info=0x10,section=0,value=0,size=0):
        name_at=len(self.data[2]);self.data[2]+=name.encode()+b'\0';index=len(self.data[1])//16
        self.data[1]+=struct.pack('<IIIBBH',name_at,value,size,info,0,section);return index
    def words(self,*words):
        for i,word in enumerate(words):struct.pack_into('<I',self.data[5],4*i,word)
    def relocations(self,rows,rela=False):
        self.sections[7][1]=4 if rela else 9;self.sections[7][6]=1;self.sections[7][7]=5;self.sections[7][9]=12 if rela else 8
        self.data[7]=bytearray(b''.join(struct.pack('<IIi',off,(sym<<8)|kind,0) if rela else struct.pack('<II',off,(sym<<8)|kind) for off,kind,sym in rows))
    def bytes(self):
        out=bytearray(self.header)
        for i,section in enumerate(self.sections):
            alignment=max(1,section[8]);out+=bytes((-len(out))%alignment);section[4]=len(out);section[5]=len(self.data[i]);out+=self.data[i]
        out+=bytes((-len(out))%4);offset=len(out);struct.pack_into('<I',out,32,offset)
        for section in self.sections:out+=struct.pack('<10I',*section)
        return bytes(out)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    if args.output.exists():raise ValueError('preserve prior controls: fresh output directory required')
    args.output.mkdir(parents=True);original=args.fixture.read_bytes();(args.output/'pilot-original.o').write_bytes(original)
    if sha(original)!='26a7a1670e116d65306f87c4462abba259bed908e077fbc3be2bf6af54a44c2a':raise ValueError('actual pilot fixture changed')
    (args.output/'driver-source.py').write_bytes(DRIVER_SOURCE)
    original_text,rels=DRIVER.object_bytes(args.fixture,NAME);assert len(original_text)==20 and rels==[]
    results=[]
    def check(name,mutant,expect,expected_bytes=None,gp=GP):
        raw=mutant if isinstance(mutant,bytes) else mutant.bytes();path=args.output/(name+'.o');path.write_bytes(raw)
        item={'name':name,'fixture_sha256':sha(raw),'expected':expect,'mutation_object':str(path)}
        try:
            data,relocations=DRIVER.object_bytes(path,NAME);linked=DRIVER.relocate(data,relocations,gp,VRAM)
            item.update(actual='ACCEPTED',body_bytes=len(linked),body_sha256=sha(linked),relocations=relocations)
            item['full_body_equivalence']='BYTE_EXACT' if linked==original_text else 'MISMATCH'
            okay=(expect=='ACCEPT' and (expected_bytes is None or linked==expected_bytes)) or (expect=='MISMATCH' and linked!=original_text)
        except Exception as error:
            item.update(actual='REFUSED',reason=repr(error));okay=expect=='REFUSE'
        item['passed']=okay;results.append(item);print(name,'PASS' if okay else 'FAIL',item['actual'],item.get('reason',''))
    def fresh():return Mutant(original)
    check('valid_actual_object',original,'ACCEPT',bytes(original_text))
    m=fresh();m.symbol(2,size=24);check('oversized_function_extent',m,'REFUSE')
    m=fresh();m.symbol(2,value=2,size=16);check('unaligned_function_start',m,'REFUSE')
    m=fresh();m.symbol(2,size=18);check('unaligned_function_size',m,'REFUSE')
    m=fresh();m.header[5]=2;check('wrong_endianness',m,'REFUSE')
    m=fresh();struct.pack_into('<H',m.header,18,40);check('wrong_machine',m,'REFUSE')
    m=fresh();struct.pack_into('<H',m.header,16,2);check('wrong_elf_type',m,'REFUSE')
    m=fresh();m.header[4]=2;check('wrong_elf_class',m,'REFUSE')
    m=fresh();m.symbol(2,info=0x11);check('non_function_symbol',m,'REFUSE')
    m=fresh();m.symbol(1,name=1,value=0,size=20,info=0x12,section=5);check('duplicate_function_symbol',m,'REFUSE')
    m=fresh();m.sections[5][2]=2;check('non_executable_function_section',m,'REFUSE')
    m=fresh();m.data[5][-1]^=1;check('changed_last_body_byte',m,'MISMATCH')
    m=fresh();m.data[5]+=bytes(4);m.symbol(2,size=24);check('extra_untrimmed_body_word',m,'MISMATCH')
    m=fresh();m.symbol(2,size=16);check('shortened_body_not_trimmed_match',m,'MISMATCH')
    m=fresh();sym=m.add_symbol('func_0016AE20');m.words(0x0c000000);m.relocations([(0,4,sym)]);expected=bytearray(original_text);struct.pack_into('<I',expected,0,0x0c05ab88);check('valid_R26_undefined_external_call',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('func_0016AE20');m.words(0x0fffffff);m.relocations([(0,4,sym)]);expected=bytearray(original_text);struct.pack_into('<I',expected,0,0x0c05ab87);check('valid_R26_negative_external_addend',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('func_0016AE24');m.words(0x0c000000);m.relocations([(0,4,sym)]);check('changed_call_target_is_full_body_mismatch',m,'MISMATCH')
    m=fresh();m.words(0x0c000000);m.relocations([(0,4,2)]);check('defined_self_call_placement_unproved',m,'REFUSE')
    m=fresh();sym=m.add_symbol('func_0016AE11');m.words(0x0c000000);m.relocations([(0,4,sym)]);check('unaligned_R26_target',m,'REFUSE')
    m=fresh();sym=m.add_symbol('func_1016AE10');m.words(0x0c000000);m.relocations([(0,4,sym)]);check('wrong_R26_jump_region',m,'REFUSE')
    m=fresh();sym=m.add_symbol('func_0016AE20');m.words(0);m.relocations([(0,4,sym)]);check('R26_on_non_call_word',m,'REFUSE')
    m=fresh();sym=m.add_symbol('func_0016AE20');m.words(0x0c000000);m.relocations([(0,4,sym),(0,4,sym)]);check('duplicate_relocation_offset',m,'REFUSE')
    m=fresh();m.relocations([(20,4,2)]);check('relocation_outside_target_section',m,'REFUSE')
    m=fresh();m.symbol(2,value=4,size=16);m.relocations([(2,4,2)]);check('relocation_crosses_into_function_start',m,'REFUSE')
    m=fresh();m.words(0x0c000000);m.relocations([(0,4,2)],rela=True);check('explicit_RELA_refused',m,'REFUSE')
    m=fresh();sym=m.add_symbol('unknown_external');m.words(0x0c000000);m.relocations([(0,4,sym)]);check('unknown_symbol_refused',m,'REFUSE')
    m=fresh();sym=m.add_symbol('D_00128010');m.relocations([(0,255,sym)]);check('unknown_relocation_kind_refused',m,'REFUSE')
    m=fresh();sym=m.add_symbol('D_00128010');m.words(0x3c080000,0x25080000);m.relocations([(0,5,sym),(4,6,sym)]);expected=bytearray(original_text);struct.pack_into('<II',expected,0,0x3c080013,0x25088010);check('valid_HI_LO_signed_carry',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('D_00128010');m.words(0x3c080000,0x2508fff0);m.relocations([(0,5,sym),(4,6,sym)]);expected=bytearray(original_text);struct.pack_into('<II',expected,0,0x3c080013,0x25088000);check('valid_HI_LO_negative_addend',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('D_00128010');m.words(0x3c080000,0x3c090001,0x25080000);m.relocations([(0,5,sym),(4,5,sym),(8,6,sym)]);expected=bytearray(original_text);struct.pack_into('<III',expected,0,0x3c080013,0x3c090014,0x25088010);check('multiple_HI_one_LO',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('D_00128010');m.words(0x3c080000);m.relocations([(0,5,sym)]);check('unpaired_HI_refused',m,'REFUSE')
    m=fresh();sym=m.add_symbol('D_00128010');m.words(0);m.relocations([(0,5,sym)]);check('HI_on_non_LUI_refused',m,'REFUSE')
    m=fresh();a=m.add_symbol('D_00128010');b=m.add_symbol('D_00138010');m.words(0x3c080000,0x25080000);m.relocations([(0,5,a),(4,6,b)]);check('mismatched_HI_LO_symbols',m,'REFUSE')
    m=fresh();sym=m.add_symbol('D_0064DEB4');m.words(0x8f820000);m.relocations([(0,7,sym)]);expected=bytearray(original_text);struct.pack_into('<I',expected,0,0x8f829dc4);check('valid_main_GP_reference',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('D_0064DEB4');m.words(0x8f82fff0);m.relocations([(0,7,sym)]);expected=bytearray(original_text);struct.pack_into('<I',expected,0,0x8f829db4);check('GP_signed_negative_addend',m,'ACCEPT',bytes(expected))
    m=fresh();sym=m.add_symbol('D_0064DEB4');m.words(0x8f820000);m.relocations([(0,7,sym)]);check('unproved_overlay_GP_refused',m,'REFUSE',gp=None)
    m=fresh();sym=m.add_symbol('D_0065C0F0');m.words(0x8f820000);m.relocations([(0,7,sym)]);check('GP_out_of_signed16_range',m,'REFUSE')
    m=fresh();sym=m.add_symbol('D_0064DEB4',info=0x01,section=5);m.words(0x8f820000);m.relocations([(0,7,sym)]);check('defined_local_GP_placement_unproved',m,'REFUSE')
    report={'status':'PASS' if all(r['passed'] for r in results) else 'FAIL_PRESERVED','actual_pilot_fixture':{'path':str(args.fixture),'sha256':sha(original),'bytes':len(original)},'driver_sha256':sha(DRIVER_SOURCE),'driver_unchanged_during_controls':DRIVER_SOURCE==DRIVER_PATH.read_bytes(),'test_sha256':sha(Path(__file__).read_bytes()),'command':sys.argv,'tests':results,'count':len(results),'passed':sum(r['passed'] for r in results),'scope':'Actual pilot object copied and mutated by explicit byte changes. No assembler/compiler or game process runs. Strict body comparisons include final delay/tail bytes; kit normalized scores do not determine these results.'}
    if not report['driver_unchanged_during_controls']:report['status']='SOURCE_CHANGED_REFUSED'
    (args.output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'passed':report['passed'],'count':report['count']}));return 0 if report['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
