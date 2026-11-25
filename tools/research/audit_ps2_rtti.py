"""Read-only PT_LOAD RTTI coverage audit; pointer arrays never earn E4 by themselves.

Complements ps2re's actual CLI output when a stripped ELF has no allocated
non-executable section. Emits bounded candidate evidence, never invented class
names or native credit. All addresses are original ELF virtual addresses.
"""
from __future__ import annotations
import argparse, bisect, collections, csv, hashlib, json, re, struct, sys
from pathlib import Path

ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'

def digest(b): return hashlib.sha256(b).hexdigest()

def segments(raw):
    if raw[:6] != b'\x7fELF\x01\x01': raise ValueError('ELF32 little-endian required')
    phoff, = struct.unpack_from('<I', raw, 28)
    entsize, count = struct.unpack_from('<HH', raw, 42)
    if entsize < 32 or phoff + count * entsize > len(raw): raise ValueError('program headers truncated')
    out=[]
    for i in range(count):
        typ,off,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',raw,phoff+i*entsize)
        if typ==1:
            if filesz>memsz or off+filesz>len(raw): raise ValueError('PT_LOAD bounds invalid')
            out.append({'vaddr':va,'offset':off,'filesz':filesz,'memsz':memsz,'flags':flags})
    return out

def components(s,i=0,count=None):
    out=[]
    while i<len(s) and (count is None or len(out)<count):
        m=re.match(r'[1-9][0-9]*',s[i:])
        if not m: return None
        n=int(m[0]);i+=len(m[0]);part=s[i:i+n]
        if len(part)!=n or not re.fullmatch(r'[A-Za-z_$][A-Za-z0-9_$]*',part):return None
        out.append(part);i+=n
    return (out,i)

def mangled_name(s):
    """Strict length accounting; rejects ps2re's permissive Q0/short Q leads."""
    if s.startswith('N') and s.endswith('E'):
        got=components(s[1:-1]);return '::'.join(got[0]) if got and got[1]==len(s)-2 else None
    if s.startswith('Q') and len(s)>2 and s[1] in '123456789':
        got=components(s,2,int(s[1]));return '::'.join(got[0]) if got and len(got[0])==int(s[1]) and got[1]==len(s) else None
    got=components(s)
    return '::'.join(got[0]) if got and got[1]==len(s) else None

def strings_in_load(raw,loads):
    result={}
    for p in loads:
        blob=raw[p['offset']:p['offset']+p['filesz']]
        for m in re.finditer(rb'(?<![\x20-\x7e])[\x20-\x7e]{3,200}\x00',blob):
            result[p['vaddr']+m.start()]=m[0][:-1].decode('ascii')
    return result

def word_index(raw,loads):
    words={}
    for p in loads:
        for delta in range((-p['vaddr'])%4,p['filesz']-3,4):
            words[p['vaddr']+delta]=struct.unpack_from('<I',raw,p['offset']+delta)[0]
    return words

def method_runs(words,callables):
    """Catalogue pointer runs are heuristic arrays, not authenticated vtables."""
    out=[]
    for stride in [4,8]:
        for at,v in sorted(words.items()):
            if v not in callables or words.get(at-stride) in callables:continue
            targets=[];pos=at
            while words.get(pos) in callables:
                if stride==8 and words.get(pos+4,1)!=0:break
                targets.append(words[pos]);pos+=stride
            if len(targets)>=3:
                out.append({'address':at,'stride':stride,'targets':targets,'end_exclusive':pos,'kind':'heuristic catalogue function-pointer array'})
    return out

def named_header_leads(words,runs,names):
    leads=[]
    for table in runs:
        start=table['address']
        for h in [start-8,start-12,start-16]:
            for slot in [h,h+4]:
                candidate=words.get(slot)
                links=[]
                if candidate in names:links.append((candidate,'direct name pointer'))
                if candidate is not None:
                    for delta in [0,4,8]:
                        name=words.get(candidate+delta)
                        if name in names:links.append((name,f'candidate descriptor +{delta}'))
                for address,kind in links:
                    leads.append({'method_run':start,'header_word':slot,'name_address':address,'name':names[address],'link_kind':kind,'status':'unverified descriptor/table proximity; no E4'})
    return leads

def audit(raw,catalogue):
    loads=segments(raw);strings=strings_in_load(raw,loads);words=word_index(raw,loads)
    names={a:(s,mangled_name(s)) for a,s in strings.items() if mangled_name(s)}
    plain={a:s for a,s in strings.items() if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_:]{2,119}',s) and re.search('[A-Z]',s)}
    all_names={a:v[1] for a,v in names.items()}|plain
    runs=method_runs(words,set(catalogue));leads=named_header_leads(words,runs,all_names)
    prefixes=['_ZTV','_ZTI','_ZTS','__RTTI__','__vt__','__tf','__ti','type_info','__class_type_info']
    markers=[{'address':f'{a:08x}','text':s} for a,s in strings.items() if any(s.startswith(p) for p in prefixes)]
    name_refs=collections.defaultdict(list)
    for a,v in words.items():
        if v in names:name_refs[v].append(a)
    sections=[]
    shoff,=struct.unpack_from('<I',raw,32);entsize,count,shstrndx=struct.unpack_from('<HHH',raw,46)
    for i in range(count):
        sh=struct.unpack_from('<10I',raw,shoff+i*entsize)
        sections.append({'index':i,'type':sh[1],'flags':sh[2],'vaddr':sh[3],'offset':sh[4],'size':sh[5]})
    comments=[raw[s['offset']:s['offset']+s['size']].decode('ascii',errors='replace') for s in sections if s['type']==1 and not s['flags'] and s['size']<256]
    return {'file_sha256':digest(raw),'file_backed_load_bytes':sum(p['filesz'] for p in loads),'load_segments':loads,'sections':sections,'compiler_metadata_candidates':comments,'section_data_scan_bytes':sum(s['size'] for s in sections if s['type']==1 and s['flags']&2 and not s['flags']&4),'coverage_limit':'ps2re data_sections requires SHF_ALLOC without SHF_EXECINSTR; raw header-only output is not a complete absence proof for this RWX-section image. This independent scan covers all file-backed PT_LOAD bytes.','strict_mangled_candidates':[{'address':f'{a:08x}','mangled':s,'decoded':n,'aligned_pointer_refs':[f'{x:08x}' for x in name_refs[a]]} for a,(s,n) in names.items()],'abi_symbol_string_markers':markers,'plain_name_candidates':len(plain),'unnamed_pointer_runs':[{'address':f"{r['address']:08x}",'stride':r['stride'],'end_exclusive':f"{r['end_exclusive']:08x}",'methods':[f'{x:08x}' for x in r['targets']],'classification':r['kind']} for r in runs],'named_header_leads':leads,'named_vtable_methods':[],'E4_count':0,'status':'NO_AUTHENTICATED_NAMED_RTTI_VTABLE; pointer arrays require independent ABI/class proof'}

def self_test():
    assert mangled_name('9MidwayFoo')=='MidwayFoo'
    assert mangled_name('Q26Midway4Task')=='Midway::Task'
    assert mangled_name('N6Midway4TaskE')=='Midway::Task'
    for bad in ['Q23','Q04','Q0H','99MidwayFoo','N6Midway5TaskE']:assert mangled_name(bad) is None
    names={0x90020:'MidwayFoo'}
    words={0x90040:0,0x90044:0x90020,0x90078:0,0x9007c:0x90040,0x90080:0x101000,0x90084:0x101100,0x90088:0x101200}
    runs=method_runs(words,{0x101000,0x101100,0x101200})
    assert len(runs)==1 and named_header_leads(words,runs,names)
    broken=dict(words);broken[0x90044]=0x90024
    assert not named_header_leads(broken,method_runs(broken,{0x101000,0x101100,0x101200}),names)
    broken=dict(words);broken[0x90084]=0xdeadbeef
    assert not method_runs(broken,{0x101000,0x101100,0x101200})
    return {'status':'PASS','checks':['strict GCC2/GCC3 name length accounting','truncated/invalid name refusal','synthetic descriptor and method-run recognition','planted broken name link rejected','planted non-callable method rejected'],'synthetic_only':True}

def alternate_views_and_game_seeds(raw,catalogue,toolsrc,source_map):
    """Exercise ps2re on explicit analysis views; original bytes never change."""
    sys.path.insert(0,str(toolsrc))
    from ps2re.elf import Elf,Section
    from ps2re.eerefs import Functions,refs_by_target
    from ps2re import rtti
    e=Elf.parse(raw);loads=segments(raw);views=[]
    for n,p in enumerate(loads):
        if not p['filesz']:continue
        for flags,kind in [(2,'data'),(6,'exec')]:
            views.append(Section(1000+n*2+(flags==6),f'ANALYSIS_ONLY_LOAD_{kind}_{n}',1,flags,p['vaddr'],p['offset'],p['filesz'],0,0,4,0))
    e.sections=[s for s in e.sections if not s.is_alloc]+views
    funcs=Functions(catalogue);found=rtti.scan(e,funcs)
    broad={'scope':'Overlapping in-memory PT_LOAD data/exec views for tool coverage only; these are not original ELF sections and no original bytes were patched. The toolkit supports GCC ABIs; original compiler metadata is Metrowerks.','rows':[{'name':r.name,'mangled':r.mangled,'name_va':f'{r.name_addr:08x}','vtables':[f'{x:08x}' for x in r.vtables],'typeinfos':[f'{x:08x}' for x in r.typeinfos],'referencing_functions':[f'{x:08x}' for x in r.tf_funcs]} for r in found],'strict_rows':sum(bool(r.vtables) for r in found)}
    actual=Elf.parse(raw);reginfo=next((s for s in actual.sections if s.name=='.reginfo'),None)
    gp=struct.unpack_from('<I',raw,reginfo.offset+20)[0] if reginfo and reginfo.size>=24 else None
    refs=refs_by_target(actual,funcs,gp)
    strings=strings_in_load(raw,loads);words=word_index(raw,loads)
    selected={a:s for a,s in strings.items() if s in ['motor kombat engines','motor kombat FX','motor kombat 2 FX','konquest_mode_bank_list','Motor Kombat','konquest_scene_test_art.sec','konquest_common.sec']}
    links=collections.defaultdict(list)
    for a,s in selected.items():links[a].append((a,None))
    for at,value in words.items():
        if value in selected:links[at].append((value,at))
    spans=sorted((int(r['Start'],16),int(r['End'],16),r['Name']) for r in csv.DictReader(source_map.read_text('utf-8-sig').splitlines()) if int(r['Start'],16) in catalogue)
    starts=[r[0] for r in spans];seeds=[]
    for target,pairs in links.items():
        for ref in refs.get(target,[]):
            i=bisect.bisect_right(starts,ref.pc)-1
            if i<0 or not spans[i][0]<=ref.pc<spans[i][1]:continue
            begin,end,name=spans[i]
            for string,slot in pairs:
                offset=ref.pc-0x100000+0x100
                seeds.append({'method':f'{begin:08x}','catalogue_name':name,'classification':'inferred-game','evidence_kinds':['E6'],'string_va':f'{string:08x}','string':selected[string],'string_sha256':digest(selected[string].encode()+b'\0'),'reference_pc':f'{ref.pc:08x}','reference_kind':ref.kind,'reference_word_le':raw[offset:offset+4].hex(),'preceding_word_le':raw[offset-4:offset].hex(),'pointer_word_va':f'{slot:08x}' if slot is not None else None,'pointer_word_le':struct.pack('<I',words[slot]).hex() if slot is not None else None,'source_span':[f'{begin:08x}',f'{end:08x}'],'source_span_sha256':digest(raw[begin-0x100000+0x100:end-0x100000+0x100]),'scope':'Source-joined game resource label is a heuristic seed, not RTTI/class membership or verified-game ownership.'})
    return {'ps2re_analysis_only_segment_views':broad,'reginfo_gp':f'{gp:08x}' if gp is not None else None,'game_specific_E6_seeds':seeds,'source_map_sha256':digest(source_map.read_bytes())}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--elf',type=Path);ap.add_argument('--catalogue',type=Path);ap.add_argument('--output',type=Path);ap.add_argument('--self-test',action='store_true');ap.add_argument('--ps2re-src',type=Path);ap.add_argument('--source-map',type=Path);args=ap.parse_args()
    if args.self_test:out=self_test()
    else:
        if not(args.elf and args.catalogue):ap.error('--elf and --catalogue required')
        raw=args.elf.read_bytes()
        if digest(raw)!=ELF_SHA:raise ValueError('retail ELF pin mismatch')
        catalogue={int(r['address'],16):r['symbol'] for r in csv.DictReader(args.catalogue.read_text('utf-8-sig').splitlines()) if r['module']=='SLUS_214.10' and int(r['address'],16)!=0x005bf218}
        out=audit(raw,catalogue);out['self_test']=self_test();out['catalogue_sha256']=digest(args.catalogue.read_bytes())
        if args.ps2re_src:
            if not args.source_map:ap.error('--source-map required with --ps2re-src')
            out.update(alternate_views_and_game_seeds(raw,catalogue,args.ps2re_src,args.source_map))
    text=json.dumps(out,indent=2)+'\n'
    if args.output:args.output.write_text(text,encoding='utf-8',newline='\n')
    else:print(text,end='')

if __name__=='__main__':main()
