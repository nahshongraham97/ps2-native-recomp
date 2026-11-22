from pathlib import Path
import json,hashlib,gzip,struct
out=Path(__file__).parent
j=json.loads((out/'SOURCE_JOIN.json').read_text());b=Path(j['elf']).read_bytes()
assert hashlib.sha256(b).hexdigest()==j['sha256']
phoff=struct.unpack_from('<I',b,28)[0];size,num=struct.unpack_from('<HH',b,42)
loads=[struct.unpack_from('<8I',b,phoff+i*size) for i in range(num)]
count=0
for name,v in j['functions'].items():
 raw=gzip.decompress((out/'ee'/(name+'.gz')).read_bytes())
 assert hashlib.sha256(raw).hexdigest()==v['sha256']
 lines=raw.decode().splitlines();assert len(lines)==v['words']
 for line in lines:
  a,h,ins=line.split('\t',2);a=int(a,16)
  p=next(p for p in loads if p[0]==1 and p[2]<=a<p[2]+p[4])
  assert b[p[1]+a-p[2]:p[1]+a-p[2]+4]==bytes.fromhex(h)
  if ins=='UNDECODED':assert h=='00000000'
  count+=1
assert count==712
for v in json.loads((out/'ARCHIVES.json').read_text()).values():
 raw=(out/v['archive']).read_bytes()
 assert hashlib.sha256(raw).hexdigest()==v['archive_sha256']
 assert hashlib.sha256(gzip.decompress(raw)).hexdigest()==v['original_sha256']
print('PASS_ORIGINAL712_WORDS_AND44_RAW_ARCHIVE_PAIRS_SOURCE_ONLY')
