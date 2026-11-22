import pathlib,json,hashlib,gzip
r=pathlib.Path(__file__).resolve().parents[3];e=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
for a in json.loads((e/'ARCHIVES.json').read_text()):
 b=(r/a['archive_path']).read_bytes();assert sha(b)==a['archive_sha256'];assert sha(gzip.decompress(b))==a['raw_sha256']
for a in json.loads((e/'ARTIFACTS.json').read_text()):assert sha((r/a['path']).read_bytes())==a['sha256'],a['path']
for a in json.loads((e/'validation-05/SOURCE_FILES.json').read_text()):
 b=(r/a['path']).read_bytes();assert sha(b)==a['sha256'] or sha(b.replace(b"\r\n",b"\n"))==a['lf_sha256'],a['path']
for a in json.loads((e/'CLIENTS.json').read_text()):assert sha(pathlib.Path(a['binary']).read_bytes())==a['sha256']
print('PASS source/artifact/archive/private final binary identities; component scope only')
