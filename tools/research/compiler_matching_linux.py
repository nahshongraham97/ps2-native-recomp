"""Private fixed-roster compiler search using the pinned decomp-kit APIs.

No game repository, acceptance/commit operation, grinder, or oracle execution.
The toolkit score is not called byte identity: original-address relocations
and complete untrimmed function bytes are checked independently.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time

SAMPLE_SHA='1bf2aea3cc78c066868e27aae7e580a52a56e350ba2116f9b2c2e5eb0c5dae30'
PACK_SHA='04d476ee36d69f1db0bd51a5ae609e2d959ff63c6404e0f20a6347935a19577f'

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def save(path,data):
    path.write_text(json.dumps(data,indent=2)+'\n')

def symbol_value(name):
    match=re.fullmatch(r'(?:func|D)_([0-9A-Fa-f]{8})',name)
    return int(match[1],16) if match else None

def object_bytes(path,name):
    from elftools.elf.elffile import ELFFile
    from elftools.elf.relocation import RelocationSection
    with path.open('rb') as stream:
        elf=ELFFile(stream)
        if elf.elfclass!=32 or not elf.little_endian or elf['e_machine']!='EM_MIPS' or elf['e_type']!='ET_REL':
            raise ValueError('required ELF32 little-endian MIPS relocatable object')
        tab=elf.get_section_by_name('.symtab')
        syms=tab.get_symbol_by_name(name) if tab else None
        if not syms or len(syms)!=1 or syms[0]['st_info']['type']!='STT_FUNC':
            raise ValueError('unique function symbol required')
        symbol=syms[0];section_index=symbol['st_shndx']
        if not isinstance(section_index,int) or not 0<section_index<elf.num_sections():
            raise ValueError('function section index')
        section=elf.get_section(section_index)
        start=symbol['st_value'];size=symbol['st_size']
        section_data=section.data()
        if (section['sh_type']!='SHT_PROGBITS' or not section['sh_flags']&4 or
                not size or start%4 or size%4 or start+size>len(section_data)):
            raise ValueError('complete aligned executable function extent required')
        data=bytearray(section_data[start:start+size])
        if len(data)!=size:
            raise ValueError('truncated function data')
        relocs=[]
        seen=set()
        for relsection in elf.iter_sections():
            if isinstance(relsection,RelocationSection) and relsection['sh_info']==section_index:
                for reloc in relsection.iter_relocations():
                    off=reloc['r_offset']-start
                    if reloc['r_offset']%4 or reloc['r_offset']+4>len(section_data) or off<0<off+4:
                        raise ValueError('relocation outside section or crossing function boundary')
                    if 0<=off<len(data):
                        if relsection.is_RELA():
                            raise ValueError('explicit-addend relocation unsupported')
                        if off%4 or off+4>len(data) or off in seen:
                            raise ValueError('unaligned, duplicate or crossing relocation')
                        seen.add(off)
                        sym=elf.get_section(relsection['sh_link']).get_symbol(reloc['r_info_sym'])
                        if sym['st_shndx']!='SHN_UNDEF':
                            raise ValueError('defined relocation symbol placement unproved')
                        relocs.append(dict(offset=off,kind=reloc['r_info_type'],symbol=sym.name))
        return data,relocs

def relocate(data,relocs,gp,vram=0):
    data=bytearray(data);pending=[]
    seen=set()
    for reloc in relocs:
        off=reloc['offset'];kind=reloc['kind'];name=reloc['symbol']
        if off%4 or off<0 or off+4>len(data) or off in seen:
            raise ValueError('relocation extent or duplicate')
        seen.add(off)
        value=symbol_value(name)
        if value is None:
            raise ValueError('unresolved relocation '+name)
        word=struct.unpack_from('<I',data,off)[0]
        if kind==4:
            addend=(word&0x3ffffff)<<2
            if addend&(1<<27):addend-=1<<28
            target=value+addend
            if word>>26 not in (2,3) or target%4 or target>0xffffffff or (target>>28)!=((vram+off+4)>>28):
                raise ValueError('R26 opcode, alignment, extent or jump-region mismatch')
            word=(word&0xfc000000)|((target>>2)&0x3ffffff)
        elif kind==5:
            if word>>26!=15:
                raise ValueError('HI16 does not patch LUI')
            pending.append((off,name,word&0xffff));continue
        elif kind==6:
            low=word&0xffff
            signed=low-65536 if low&0x8000 else low
            for hi_off,hi_name,hi in list(pending):
                if hi_name==name:
                    hi_word=struct.unpack_from('<I',data,hi_off)[0]
                    hi_word=(hi_word&0xffff0000)|(((value+(hi<<16)+signed+0x8000)>>16)&0xffff)
                    struct.pack_into('<I',data,hi_off,hi_word)
                    pending.remove((hi_off,hi_name,hi))
            word=(word&0xffff0000)|((value+signed)&0xffff)
        elif kind==7:
            if gp is None:
                raise ValueError('overlay GP is unproved')
            addend=word&0xffff
            if addend&0x8000:addend-=65536
            delta=value+addend-gp
            if not -32768<=delta<=32767:
                raise ValueError('GP relocation out of range')
            word=(word&0xffff0000)|(delta&0xffff)
        else:
            raise ValueError('unsupported relocation kind '+str(kind))
        struct.pack_into('<I',data,off,word)
    if pending:
        raise ValueError('unpaired HI16 relocation')
    return bytes(data)

def assembly(row,pack,rabbitizer):
    start=int(row['address'],16);raw=bytes.fromhex(row['raw_hex'])
    words=list(struct.unpack('<'+'I'*(len(raw)//4),raw))
    ins=[rabbitizer.Instruction(word,start+4*i,category=rabbitizer.InstrCategory.R5900) for i,word in enumerate(words)]
    texts=[x.disassemble() for x in ins]
    labels=set();name='func_'+row['address'].upper()
    for i,x in enumerate(ins):
        if x.isBranch():
            target=x.getBranchVramGeneric()
            label=f'.L{target:08X}';labels.add(target)
            if ',' in texts[i]:
                texts[i]=texts[i].rsplit(',',1)[0]+', '+label
            else:
                texts[i]=x.getOpcodeName()+' '+label
        elif x.isJumpWithAddress():
            target=x.getInstrIndexAsVram()
            if start<=target<start+len(raw):
                labels.add(target);label=f'.L{target:08X}'
            else:
                label=f'func_{target:08X}'
            texts[i]=x.getOpcodeName()+' '+label
    # Reify original static symbol addresses only for a mechanically proved
    # adjacent LUI+low sequence. This is a PS2 matching experiment, not native
    # field/type evidence. Unknown sequences remain literal instructions.
    for i,word in enumerate(words[:-1]):
        if word>>26!=15:
            continue
        reg=word>>16&31;nextword=words[i+1];op=nextword>>26
        if nextword>>21&31!=reg or op not in (9,25,32,33,35,36,37,40,41,43,49,57):
            continue
        low=nextword&0xffff
        if low&0x8000:low-=65536
        address=((word&0xffff)<<16)+low
        if not any(a<=address<b for a,b in pack['mapped_ranges']):
            continue
        symbol=f'D_{address:08X}'
        texts[i]=re.sub(r'0x[0-9A-Fa-f]+|-?\d+$',f'%hi({symbol})',texts[i])
        if '(' in texts[i+1]:
            texts[i+1]=re.sub(r'-?(?:0x[0-9A-Fa-f]+|\d+)(?=\()',f'%lo({symbol})',texts[i+1])
        else:
            texts[i+1]=texts[i+1].rsplit(',',1)[0]+f', %lo({symbol})'
    if row['module']=='SLUS_214.10':
        for i,word in enumerate(words):
            if word>>21&31==28 and word>>26 in (32,33,35,36,37,40,41,43,49,57):
                low=word&0xffff
                if low&0x8000:low-=65536
                address=pack['main_gp']+low
                texts[i]=re.sub(r'-?(?:0x[0-9A-Fa-f]+|\d+)(?=\()',f'%gp_rel(D_{address:08X})',texts[i])
    out=[f'glabel {name}']
    for i,text in enumerate(texts):
        pc=start+4*i
        if pc in labels:out.append(f'.L{pc:08X}:')
        out.append(f'/* {i*4:06X} {pc:08X} {raw[i*4:i*4+4].hex().upper()} */ {text}')
    out.append(f'endlabel {name}')
    return name,'\n'.join(out)+'\n'

def prepare(args,dk,rabbitizer):
    pack=json.loads(args.pack.read_bytes());root=args.work
    if root.exists():raise ValueError('fresh private project required')
    root.mkdir(parents=True)
    shutil.copytree(args.kit/'templates/ps2',root,dirs_exist_ok=True)
    for name in ('src','config','asm/nonmatchings/sample','asm/data','work','build'):
        (root/name).mkdir(parents=True,exist_ok=True)
    for name in ('game.h','structs.h','functions.h'):
        (root/'include'/name).write_text('/* no invented semantic context */\n')
    shutil.copy(Path(os.environ['DK_TOOLCHAIN'])/'src/m2c/m2c_macros.h',root/'include/m2c_macros.h')
    (root/'include/macro.inc').write_text('.macro glabel name\n.globl \\name\n.type \\name,@function\n.ent \\name\n\\name:\n.endm\n.macro endlabel name\n.size \\name, .-\\name\n.end \\name\n.endm\n')
    for key in dir(rabbitizer.config):
        if key.startswith('pseudos_') and isinstance(getattr(rabbitizer.config,key),bool):
            setattr(rabbitizer.config,key,False)
    rows=[]
    for row in pack['sample']:
        if not row['contiguous']:
            rows.append(dict(row,prepare_failure='split original body unbound'));continue
        name,text=assembly(row,pack,rabbitizer)
        (root/'asm/nonmatchings/sample'/f'{name}.s').write_text(text)
        rows.append(dict(row,function=name))
    (root/'src/main.c').write_text('\n'.join(f'INCLUDE_ASM("asm/nonmatchings/sample", {row["function"]});' for row in rows if 'function'in row)+'\n')
    cfg=dict(name='mkar-fixed-sample',platform='ps2',gcc='2.96',g=0,cflags='-O3',maspsx='',lib_by_name=False,exe='PRIVATE-SAMPLE',sha1='',chunks=[])
    save(root/'dk.json',cfg)
    project=dk.Project(root,cfg);project.save()
    drafts=[]
    for row in rows:
        if 'function'not in row:
            drafts.append(row);continue
        name=row['function'];log=io.StringIO()
        try:
            with contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
                draft=dk.m2c(project,name,strict=False)
            if not draft:
                raise ValueError('m2c draft refused')
            # dk inserts per-function O0 pragmas; remove those for the primary
            # uniform-flag experiment and preserve the untouched draft too.
            (root/'work'/f'{name}.kit.c').write_text(draft)
            plain='\n'.join(line for line in draft.splitlines() if not line.startswith('#pragma optimization_level'))+'\n'
            (root/'work'/f'{name}.c').write_text(plain)
            row['draft_sha256']=sha(plain.encode())
        except (Exception,SystemExit) as error:
            row['draft_failure']=str(error)
        row['draft_log']=log.getvalue()
        drafts.append(row)
    save(root/'PREPARE.json',dict(status='PREPARED',sample_sha256=pack['sample_sha256'],pack_sha256=sha(args.pack.read_bytes()),
        kit_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=args.kit,text=True).strip(),
        rows=drafts,uniform_flags_no_function_O0_pragmas=True,game_repository_created=False))
    print(json.dumps(dict(prepared=len(rows),drafts=sum('draft_sha256'in row for row in rows))))

def matrix(args,dk):
    root=args.work;data=json.loads((root/'PREPARE.json').read_bytes())
    pack=json.loads(args.pack.read_bytes());project=dk.Project(root)
    project._asm={path.stem:path for path in (root/'asm').rglob('*.s')}
    os.environ['DK_WARM']='0'
    base_run=dk.run
    def bounded_run(command,*positional,**keywords):
        if Path(str(command[0])).name=='cc.sh':
            command=['timeout','--signal=TERM','--kill-after=5','60',*command]
        return base_run(command,*positional,**keywords)
    dk.run=bounded_run
    tc=Path(os.environ['DK_TOOLCHAIN'])
    versions=[d.name for d in sorted((tc/'ee-gcc').iterdir()) if d.is_dir()]
    versions += ['mwcps2-'+d.name for d in sorted((tc/'mwcps2').iterdir()) if d.is_dir()]
    if args.compiler:versions=args.compiler.split(',')
    results=[]
    for version in versions:
        for optimization in ('-O3','-O4'):
            if not version.startswith('mwcps2-') and optimization=='-O4':
                # Try the requested flag even if a historical build rejects it.
                pass
            for g in (0,8):
                if shutil.disk_usage('/mnt/c').free<12*(1<<30):
                    raise ValueError('host12GiBreserve')
                flags=optimization+(f' -sdatathreshold {g}' if version.startswith('mwcps2-') else '')
                artifact_dir=root/'objects'/version/(sha((flags+' G'+str(g)).encode())[:12])
                artifact_dir.mkdir(parents=True,exist_ok=True)
                def run(row):
                    item=dict(module=row['module'],address=row['address'],instructions=row['instructions'],
                              observed_in_matched_replay=row['observed_in_matched_replay'],observation=row['observation'])
                    if version=='3.2-030210-beta2':
                        return dict(item,status='COMPILER_UNAVAILABLE',
                            reason='Actual version probe fails cygwin_internal import; sample attempt hangs under wibo. Preserved diagnostic; not retried per function.')
                    if 'draft_sha256'not in row:
                        return dict(item,status='DRAFT_REFUSED',reason=row.get('draft_failure',row.get('prepare_failure')))
                    name=row['function'];start=time.monotonic()
                    try:
                        source=root/'work'/f'{name}.c'
                        source_bytes=source.read_bytes()
                        if sha(source_bytes)!=row['draft_sha256'] or re.search(rb'INCLUDE_ASM|GLOBAL_ASM|\b(?:__asm__|asm)\s*\(',source_bytes):
                            raise ValueError('primary draft changed or contains assembly fallback')
                        score,error=dk.try_func(project,name,root/'work'/f'{name}.c',gcc=version,g=g,cflags=flags)
                        if score is None:
                            return dict(item,status='COMPILER_ERROR',reason=error,seconds=time.monotonic()-start)
                        item.update(kit_matched=score[0],kit_target=score[1],kit_candidate=score[2],
                                    kit_soft=score[3],instruction_diff=score[4])
                        obj=root/'build/try'/name/f'{name}.o'
                        preserved=artifact_dir/(name+'.o')
                        shutil.copyfile(obj,preserved)
                        item.update(candidate_object=str(preserved),draft_sha256=sha(source_bytes))
                        raw,relocs=object_bytes(obj,name)
                        linked=relocate(raw,relocs,pack['main_gp'] if row['module']=='SLUS_214.10' else None,int(row['address'],16))
                        original=bytes.fromhex(row['raw_hex'])
                        item.update(status='BYTE_EXACT' if linked==original else 'MISMATCH',
                            original_sha256=sha(original),candidate_unrelocated_sha256=sha(raw),candidate_resolved_sha256=sha(linked),
                            original_bytes=len(original),candidate_bytes=len(linked),relocations=relocs,
                            raw_instruction_diff=[dict(index=i//4,original=original[i:i+4].hex(),candidate=linked[i:i+4].hex())
                                for i in range(0,max(len(original),len(linked)),4) if original[i:i+4]!=linked[i:i+4]],
                            candidate_object_sha256=sha(obj.read_bytes()))
                    except (Exception,SystemExit) as error:
                        item.update(status='CHECK_REFUSED',reason=repr(error))
                    item['seconds']=time.monotonic()-start
                    return item
                with ThreadPoolExecutor(max_workers=args.jobs) as workers:
                    rows=list(workers.map(run,data['rows']))
                cell=dict(compiler=version,flags=flags,g=g,rows=rows,exact=sum(r['status']=='BYTE_EXACT'for r in rows),
                    nontrivial_exact=sum(r['status']=='BYTE_EXACT'and 25<=r['instructions']<=120 for r in rows),
                    total_denominator=51,threshold_denominator=40)
                results.append(cell)
                save(root/'MATRIX.json',dict(status='IN_PROGRESS',results=results))
                print(json.dumps({k:v for k,v in cell.items() if k!='rows'}),flush=True)
    save(root/'MATRIX.json',dict(status='COMPLETE',sample_sha256=pack['sample_sha256'],
        driver_sha256=sha(Path(__file__).read_bytes()),kit_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=args.kit,text=True).strip(),
        results=results,
        threshold_denominator=40,credit_delta=0,ownership_delta=0,native_delta=0,
        limits=['PS2 relocations only; no host/native ABI or pointer equivalence.','All failed drafts remain in the fixed denominator.']))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage',choices=['prepare','matrix'])
    parser.add_argument('--kit',type=Path,required=True)
    parser.add_argument('--pack',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--compiler')
    args=parser.parse_args()
    pack=json.loads(args.pack.read_bytes())
    if sha(args.pack.read_bytes())!=PACK_SHA or pack['sample_sha256']!=SAMPLE_SHA:
        raise ValueError('frozen original-byte pack or sample identity changed')
    if len(pack['sample'])!=51 or sum(25<=row['instructions']<=120 for row in pack['sample'])!=40:
        raise ValueError('fixed denominators')
    for row in pack['sample']:
        if sha(bytes.fromhex(row['raw_hex']))!=row['original_body_sha256']:
            raise ValueError('original body bytes changed')
    if not 1<=args.jobs<=8:raise ValueError('half-CPU concurrency limit')
    if shutil.disk_usage(args.work.parent).free<12*(1<<30):raise ValueError('12GiB reserve')
    sys.path.insert(0,str(args.kit))
    import dk,rabbitizer
    if subprocess.check_output(['git','rev-parse','HEAD'],cwd=args.kit,text=True).strip()!='3ae9a231d887f187617e25756c7fa866703fa5a1':
        raise ValueError('toolkit revision changed')
    if args.stage=='prepare':prepare(args,dk,rabbitizer)
    else:matrix(args,dk)

if __name__=='__main__':
    main()
