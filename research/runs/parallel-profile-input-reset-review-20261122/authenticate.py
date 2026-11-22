from pathlib import Path
import json,hashlib,gzip,struct,subprocess,re
own=Path(__file__).resolve().parent;r=own.parents[2].parent/'ps2-native-recomp';d=r/'research/runs/native-retail-profile-input-reset-20261122';v=d/'validation-01';sha=lambda b:hashlib.sha256(b).hexdigest()
def load(p):return json.loads(p.read_text())
def check(p,h):assert sha(p.read_bytes())==h,str(p)
before=load(d/'BEFORE.json');proof=load(v/'VERIFICATION.json');files=load(v/'SOURCE_FILES.json');assert proof['source_commit']==before['base']=='630849fc3b981d65d9f3229a7ae7021612b7c166';check(v/'SOURCE_FILES.json',proof['source_manifest_sha256'])
for p,h in files.items():check(r/p,h)
for p,h in proof['assets_before_and_after'].items():check(Path(p),h)
scoped={}
for p in before['pins']:
 b=(r/p).read_bytes();assert sha(b)==files[p];scoped[p]=sha(b);q=own/'snapshot'/p;q.parent.mkdir(parents=True,exist_ok=True);q.with_name(q.name+'.gz').write_bytes(gzip.compress(b,mtime=0))
receipts={}
for p in v.glob('*.json'):
 x=load(p)
 if isinstance(x,dict) and 'argv' in x and 'log_sha256' in x:
  check(Path(x['argv'][0]),x['executable_sha256']);packed=v/(p.stem+'.log.gz');check(packed,x['compressed_log_sha256']);assert sha(gzip.decompress(packed.read_bytes()))==x['log_sha256'];assert x['source_manifest_sha256']==proof['source_manifest_sha256'];receipts[p.stem]=x
clients=proof['clients'];assert len(clients)==9
for x in clients:
 p=receipts[x['name']];assert p['exit']==0 and p['executable_sha256']==x['sha256']
build=Path(receipts['build']['argv'][2]);check(build/'CMakeCache.txt',proof['cache_sha256']);compiler=Path(proof['configuration']['CMAKE_CXX_COMPILER:STRING']);check(compiler,proof['compiler_sha256'])
sy=gzip.decompress((v/'profile-input-symbols.log.gz').read_bytes()).decode(errors='replace');assert not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_Init|SDL_OpenAudio|NativeAudioDevice::open|open_native_game_sound_output',sy)
negative=receipts['full-entry-negative'];assert negative['exit']!=0;names=re.findall(r'undefined symbol: ([^\r\n]+)',gzip.decompress((v/'full-entry-negative.log.gz').read_bytes()).decode());expected=load(r/'research/runs/native-retail-renderer-history-integrated-20261122/clean-validation-01/VERIFICATION.json')['remaining_undefined_symbols'];assert len(names)==len(set(names))==59 and set(names)==set(expected)==set(proof['remaining_undefined_symbols'])
j=load(d/'SOURCE_JOIN.json');check(d/'SOURCE_JOIN.json',proof['source_join_sha256']);elf=(r.parents[2]/'research/inputs/SLUS_214.10').read_bytes();assert sha(elf)==j['elf_sha256'];ph=struct.unpack_from('<I',elf,28)[0];sz,n=struct.unpack_from('<HH',elf,42);segments=[struct.unpack_from('<8I',elf,ph+i*sz) for i in range(n)]
def extract(a,n):
 s=next(s for s in segments if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return elf[off:off+n]
total=0
for name,x in j['functions'].items():
 p=d/'source-01'/(name+'.asm');b=p.read_bytes();assert sha(b)==x['listing_sha256'];rows=[z.split('\t',2) for z in b.decode().splitlines()];assert len(rows)==x['listed_words'];words={int(a,16):h for a,h,_ in rows}
 for a,h in x['omitted'].items():assert int(a,16) not in words;words[int(a,16)]=h
 assert sorted(words)==list(range(x['begin'],x['end'],4))
 for a,h in words.items():assert extract(a,4)==bytes.fromhex(h)
 total+=len(rows);q=own/'source'/p.name;q.parent.mkdir(exist_ok=True);q.with_name(q.name+'.gz').write_bytes(gzip.compress(b,mtime=0))
assert total==318 and j['implemented_words']==44
for pos,x in j['mapping_word_births'].items():
 a=x['address'];assert a==j['gp']+x['displacement'];assert any(s[0]==1 and s[2]+s[4]<=a and a+4<=s[2]+s[5] for s in segments)
for name in ['SOURCE_JOIN.json','NATIVE_CONTRACT.md','BEFORE.json']:(own/(name+'.gz')).write_bytes(gzip.compress((d/name).read_bytes(),mtime=0))
for name in ['VERIFICATION.json','SOURCE_FILES.json']:(own/(name+'.gz')).write_bytes(gzip.compress((v/name).read_bytes(),mtime=0))
result={'base':before['base'],'canonical_uncommitted':not proof['clean_source'],'scoped_source_sha256':scoped,'all_source_files':len(files),'source_join_sha256':proof['source_join_sha256'],'listed_words_total':total,'implemented_words_included':44,'support_words':total-44,'manual_omitted_zero_words':1,'clients':clients,'receipts_authenticated':list(receipts),'required_names':len(names),'verification_sha256':sha((v/'VERIFICATION.json').read_bytes()),'reviewer_execution':'Read-only source/original-byte/hash/receipt checks. Tests/builds were executed by primary, not reviewer.'};(own/'AUTHENTICATION.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('PASS_REVIEW_AUTHENTICATION',len(files),len(clients),len(names),len(receipts))
