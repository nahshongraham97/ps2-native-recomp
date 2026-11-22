from pathlib import Path
import json,hashlib,gzip,struct,re,subprocess
out=Path(__file__).parent;r=out.parents[2].parent/'ps2-native-recomp-sound-history';d=r/'research/runs/parallel-normal-control-history-20261122';v=d/'validation-03';sha=lambda b:hashlib.sha256(b).hexdigest()
def raw(p):return p.read_bytes() if p.exists() else gzip.decompress(Path(str(p)+'.gz').read_bytes())
load=lambda p:json.loads(p.read_text())
commit='c9f3db8c71bc1b3d18594329c8478ed59798e327';assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=r).decode().strip()==commit;assert not subprocess.check_output(['git','diff','HEAD','--','runtime','tests','cmake'],cwd=r)
j=load(v/'VERIFICATION.json');assert j['status']=='PASS_RETAINED_NORMAL_CONTROL_HISTORY_COMPONENTS' and len(j['clients'])==26
pins={}
for name in ['SOURCE_FILES.json','ADDITIONAL_PINS.json']:
 values=load(v/name)
 for name,h in values.items():
  p=Path(name);p=p if p.is_absolute() else r/p;assert sha(raw(p))==h,name
 pins[len(values)]=sha((v/('SOURCE_FILES.json' if len(values)==853 else 'ADDITIONAL_PINS.json')).read_bytes())
clients={};reused=0
for c in j['clients']:
 p=Path(c['reuse']) if 'reuse'in c else v/c['receipt'];a=load(p);assert a['exit']==0 and sha(raw(Path(a['argv'][0])))==a['executable_sha256']==c['binary_sha256'];log=raw(p.with_suffix('.log'));assert sha(log)==a['log_sha256']
 if 'reuse'in c:assert sha(p.read_bytes())==c['receipt_sha256'];reused+=1
 clients[c['name']]={'binary':c['binary_sha256'],'receipt_sha256':sha(p.read_bytes()),'log_sha256':sha(log),'reused':'reuse'in c}
assert reused==5
archives=load(d/'ARCHIVES.json')
for a in archives.values():
 z=(d/a['archive']).read_bytes();assert sha(z)==a['archive_sha256'] and sha(gzip.decompress(z))==a['raw_sha256']
source=load(d/'SOURCE_JOIN.json');images={};loads={}
for platform,image in source['images'].items():
 b=Path(image['path']).read_bytes();assert sha(b)==image['sha256'];images[platform]=b;po=struct.unpack_from('<I',b,28)[0];ps,pn=struct.unpack_from('<HH',b,42);loads[platform]=[struct.unpack_from('<8I',b,po+i*ps) for i in range(pn)]
def mapped(platform,a,n):
 b=images[platform];s=next(s for s in loads[platform] if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);return b[s[1]+a-s[2]:s[1]+a-s[2]+n]
words=0
for name,f in source['listings'].items():
 data=raw(d/name);assert sha(data)==f['sha256'];lines=data.decode().splitlines();assert len(lines)==f['words'] and not f['undecoded'];start=int(f['begin'],16);platform='ee' if name.startswith('ee/') else 'msl'
 for i,line in enumerate(lines):
  a,h,_=line.split('\t',2);assert int(a,16)==start+4*i and mapped(platform,int(a,16),4)==bytes.fromhex(h)
 words+=len(lines)
assert words==2527 and len(source['listings'])==28
fixture=(r/'tests/fixtures/retail_sound_curve_data.hpp').read_text();arrays={}
for name,body in re.findall(r'(initial_gain|initial_pan|linear_gain|linear_pan)\s*\{([^}]+)\}',fixture):arrays[name]=list(map(int,re.findall(r'\d+',body)))
assert len(arrays)==4 and sum(map(len,arrays.values()))==600
assert arrays['initial_gain']==list(struct.unpack('<200H',mapped('msl',0x102ac,400))) and arrays['initial_pan']==list(mapped('msl',0x1043c,100))
assert arrays['linear_gain']==[(i*16383)//199 for i in range(199,-1,-1)] and arrays['linear_pan']==[2*i for i in range(100)]
curve=load(d/'CURVE_AUTHENTICATION.json')
for address,w in curve['linear_writer_words'].items():assert int.from_bytes(mapped('msl',int(address,16),4),'little')==int(w,16)
corpus=raw(d/'reference-03/history-corpus.txt');assert sha(corpus)==j['reference_corpus_sha256'];lines=corpus.decode().splitlines();assert int(lines[0])==26576 and len(lines)==26577 and all(len(line.split())==26 for line in lines[1:])
gen=load(d/'reference-03/generate.json');assert gen['exit']==0 and sha(Path(gen['argv'][0]).read_bytes())==gen['executable_sha256'] and sha(corpus)==gen['corpus_sha256'];reference=raw(d/'reference_history.cpp');assert sha(reference)==j['reference_generator_sha256'];assert b'mka/audio_controls' not in reference and b'reference_history(' in reference
actual=[x.strip() for x in re.findall(r'undefined symbol: (.*)',raw(v/'full-entry-negative.log').decode())];assert actual==j['remaining_undefined_symbols'] and len(actual)==34 and sorted(actual)==sorted(load(v/'EXPECTED_BASE34.json')['names'])
# Narrow initial caller join from the prior preserved source record,not a new master export.
prior=r.parent/'ps2-native-recomp-section-timing/research/runs/parallel-normal-envelope-policy-20261122/source-final/LISTING.json';prior_rows=load(prior)['instructions'];rows=[x for x in prior_rows if 0x6330<=int(x['original_vma'],16)<0x6468];assert len(rows)==78
for x in rows:assert int.from_bytes(mapped('msl',int(x['original_vma'],16),4),'little')==int(x['word'],16)
# Static direct JAL census; doesnot exclude indirect/overlay calls.
calls={hex(t):[] for t in [0x9f1c,0x959c,0x3fcc]}
for s in loads['msl']:
 if s[0]!=1 or not s[6]&1:continue
 for off in range(s[1],s[1]+s[4]-3,4):
  w=struct.unpack_from('<I',images['msl'],off)[0];target=hex((w&0x3ffffff)*4)
  if w>>26==3 and target in calls:calls[target].append(hex(s[2]+off-s[1]))
result={'status':'PASS_BOUNDED_HISTORY_REVIEW_NO_CONCRETE_FINDING','candidate':commit,'base':j['base'],'source_files':853,'additional_pins':106,'source_views':28,'listed_words':words,'archive_pairs':len(archives),'clients':clients,'fresh':21,'exact_prior_reused':5,'curve_coefficients':600,'corpus_rows':26576,'corpus_sha256':sha(corpus),'reference_generator_sha256':sha(reference),'initial_6330_reuse_sha256':sha(prior.read_bytes()),'initial_6330_words':78,'direct_msl_callers':calls,'historical_required_names':34,'limits':'Independent source/math/ownership review plus saved-artifact authentication. No candidate/reference rerun/build,source retail execution,heard audio or full pause/envelope/device/game acceptance.'}
(out/'REVIEW.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print('PASS853_SOURCE_106_ADDITIONAL_28VIEWS2527WORDS_26ARTIFACTS_600COEFFICIENTS_26576ROWS_BASE34')
