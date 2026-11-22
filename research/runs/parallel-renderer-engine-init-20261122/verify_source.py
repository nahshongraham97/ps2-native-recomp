from pathlib import Path
import json,hashlib,struct
r=Path(__file__).resolve().parents[3];d=Path(__file__).parent;parent=r.parents[2]
sha=lambda b:hashlib.sha256(b).hexdigest()
j=json.loads((d/'SOURCE_JOIN.json').read_text());b=(parent/'research/inputs/SLUS_214.10').read_bytes();assert sha(b)==j['image_sha256']
ph=struct.unpack_from('<I',b,28)[0];ps,pn=struct.unpack_from('<HH',b,42);loads=[struct.unpack_from('<8I',b,ph+i*ps) for i in range(pn)]
def extract(a,n):
 s=next(s for s in loads if s[0]==1 and s[2]<=a and a+n<=s[2]+s[4]);off=s[1]+a-s[2];return b[off:off+n]
words=0
for name,x in j['full_listings'].items():
 p=r/name if name.startswith('research/') else d/name;raw=p.read_bytes();assert sha(raw) in [x['sha256'],x.get('committed_blob_sha256')]
 rows=[l.split('\t',2) for l in raw.decode().splitlines()];start,end=int(x['begin'],16),int(x['end_exclusive'],16);assert len(rows)==x['listed_words'];assert len(rows)*4==end-start and not x['omitted_zero_words']
 for i,(address,h,ins) in enumerate(rows):assert int(address,16)==start+4*i and extract(int(address,16),4)==bytes.fromhex(h)
 assert extract(start,end-start)==bytes.fromhex(x['mapping']['bytes']);words+=len(rows)
for name,x in j['data'].items():
 a=int(x['vma'],16)
 if x.get('loader_zero'):
  s=next(s for s in loads if s[0]==1 and s[2]<=a and a+x['size']<=s[2]+s[5]);assert a>=s[2]+s[4]
 else:assert extract(a,len(bytes.fromhex(x['bytes'])))==bytes.fromhex(x['bytes'])
x=json.loads((d/'XBOX_WORD_JOIN.json').read_text());xb=(parent/'private-inputs/xbox-mka-20261121/default.xbe').read_bytes();assert sha(xb)==x['xbox_sha256']
for name,f in x['functions'].items():
 p=d/name;assert sha(p.read_bytes())==f['sha256'];rows=[l.split('\t',2) for l in p.read_text().splitlines()];assert len(rows)==f['instructions']
 for (a,h,ins),(va,n,off) in zip(rows,f['mapped_spans']):assert int(a,16)==va and len(bytes.fromhex(h))==n and xb[off:off+n]==bytes.fromhex(h)
for values in json.loads((d/'XBOX_ALIGNMENT.json').read_text())['gaps'].values():
 for g in values:assert xb[g['file_offset']:g['file_offset']+len(bytes.fromhex(g['bytes']))]==bytes.fromhex(g['bytes'])
assert words==999 and len(j['full_listings'])==28
print('PASS_ORIGINAL999_WORDS_28_FULL_EE_LISTINGS_AND3_XBOX_LISTINGS')
