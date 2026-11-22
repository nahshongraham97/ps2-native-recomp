from pathlib import Path
import subprocess,json,hashlib,gzip
r=Path.cwd();d=r/'research/runs/parallel-renderer-plugin-0401-20261122';v=d/'validation-06';files=json.loads((v/'SOURCE_FILES.json').read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
keys=list(files);out=subprocess.check_output(['git','cat-file','--batch'],input=('\n'.join(':'+p for p in keys)+'\n').encode());at=0;index={};pairs={}
for p in keys:
 end=out.index(b'\n',at);header=out[at:end].split();assert header[-2]==b'blob',(p,header);n=int(header[-1]);at=end+1;b=out[at:at+n];at+=n+1;raw=(r/p).read_bytes();assert sha(raw)==files[p];assert raw.replace(b'\r\n',b'\n')==b or raw==b,p;index[p]=sha(b)
 if sha(raw)!=sha(b):pairs[p]={'raw_sha256':sha(raw),'index_sha256':sha(b)}
assert at==len(out)
(v/'COMMITTED_SOURCE_FILES.json').write_bytes((json.dumps(index,indent=2)+'\n').encode());(v/'SOURCE_NEWLINE_PAIRS.json').write_bytes((json.dumps(pairs,indent=2)+'\n').encode())
for x in json.loads((d/'ARCHIVES.json').read_text()):
 b=(d/x['archive']).read_bytes();assert sha(b)==x['archive_sha256'];raw=gzip.decompress(b);assert len(raw)==x['raw_bytes'] and sha(raw)==x['raw_sha256']
for p,h in json.loads((v/'ADDITIONAL_PINS.json').read_text()).items():assert sha(Path(p).read_bytes())==h,p
receipt=json.loads((v/'VERIFICATION.json').read_text());core=receipt['core'];assert sha(Path(core['argv'][0]).read_bytes())==core['executable_sha256'];assert sha(gzip.decompress((v/'core.log.gz').read_bytes()))==core['log_sha256'];assert len(receipt['all_required'])==62
(d/'DELIVERY.json').write_bytes((json.dumps({'base':'feb05096e43fb0ae37bac6407d2567569ddb03e0','source_files':len(files),'newline_only_pairs':len(pairs),'source_manifest_sha256':sha((v/'SOURCE_FILES.json').read_bytes()),'index_manifest_sha256':sha((v/'COMMITTED_SOURCE_FILES.json').read_bytes()),'verification_sha256':sha((v/'VERIFICATION.json').read_bytes()),'source_join_sha256':sha((d/'SOURCE_JOIN.json').read_bytes()),'manual_pins_sha256':sha((d/'MANUAL_PINS.json').read_bytes()),'final_core_sha256':core['executable_sha256'],'scope':'Persistent typed0401 module constructor/destructor,setter,dispatch,retirement only. Four exact native math kernels required. No full engine/game acceptance.'},indent=2)+'\n').encode());print('PASS_DELIVERY_AUTH',len(files),len(pairs))
