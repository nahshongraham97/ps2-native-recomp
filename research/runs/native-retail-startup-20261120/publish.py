"""Publish measured native retail startup/file ownership, retaining failed evidence."""
import ast, hashlib, json, pathlib, subprocess
F=pathlib.Path(__file__).resolve().parents[3]
P=F.parents[2]
RUN=pathlib.Path(__file__).resolve().parent
R=P/'research/runs/native-retail-startup-20261120'
tree=ast.parse((F/'research/runs/native-service-runtime-20261120/publish.py').read_text())
defs=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'sha','need','git','names','publish'}]
assert len(defs)==5
exec(compile(ast.Module(body=defs,type_ignores=[]),'checked-publish-functions','exec'),globals())


def validate():
    v=json.loads((RUN/'validation-04/verification.json').read_bytes())
    need(v['status']=='PASS_NATIVE_RETAIL_STARTUP_LEAVES_AND_OWNED_CONTENT' and
        len(v['source_after_sha256'])==651 and len(v['fixtures'])==5 and v['unchanged_prior_pins']==560,
        'actual measured startup evidence schema')
    for name,digest in v['source_after_sha256'].items():need(sha(name)==digest,'current measured input '+name)
    for name,digest in v['generated_runtime_sha256'].items():need(sha(name)==digest,'actual generated source')
    for stem,status in [('SOURCE_REVIEW','SOURCE_REVIEW_NATIVE_RETAIL_STARTUP_AND_CONTENT_VERIFIED'),
        ('EXECUTION_REVIEW','SAVED_NATIVE_RETAIL_STARTUP_EXECUTION_VERIFIED'),
        ('CLOSURE','NATIVE_RETAIL_STARTUP_LEAVES_AND_CONTENT_RESULTS_JOINED')]:
        d=json.loads((R/(stem+'.json')).read_bytes())
        need(d['status']==status and isinstance(d['input_sha256'],dict),'actual review schema '+stem)
        for name,digest in d['input_sha256'].items():need(sha(name)==digest,'current reviewed input '+name)
    for fixture in v['fixtures']:
        need(sha(F/'build_native_services_only_20261119-01'/(fixture['target']+'.exe'))==fixture['executed_target_sha256'],
            'actual executed binary identity')
    return v


def check_parent_text(files):
    snapshot=json.loads((R/'before/provenance.json').read_bytes())['source_sha256']
    raw={R/'before'/name:digest for name,digest in snapshot.items()
        if name in {'runtime/include/mka/native_file.hpp','runtime/src/native/native_file.cpp'}}
    for q in files:
        if q in raw:
            need(sha(q)==raw[q],'preserve exact original measured CRLF source')
            continue
        b=q.read_bytes();need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
            all(line==line.rstrip(b' \t') for line in b.splitlines()),'parent source/evidence LF/EOF '+str(q))


for root,base in [(F,'b83418702c56ce937a6d66f1d85ae45f89cbe21e'),(P,'3ab5a9a4246efb31fe2e27d67e96e84252e081b2')]:
    need(git(root,'branch','--show-current')=='main' and git(root,'rev-parse','HEAD')==base,'expected current shared checkout')
    need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
protected={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for name,digest in protected.items():need(sha(F/name)==digest,'preserved separate worker IOP commit')
inventory=P/'research/doc-audit-20261118/inventory.json'
inventory_digest='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
need(sha(inventory)==inventory_digest,'preserved broad inventory')
v=validate()
check_parent_text({P/'PORT_STATUS.md',P/'ROADMAP.md',P/'.gitattributes'}|
    {q for q in R.rglob('*') if q.is_file()})
source=json.loads((R/'SOURCE_REVIEW.json').read_bytes())['input_sha256']
files={pathlib.Path(name) for name in source}
for q in RUN.rglob('*'):
    if not q.is_file() or '__pycache__' in q.parts:continue
    parts=q.relative_to(RUN).parts
    if any(part.endswith('-owned') for part in parts):continue
    if q.relative_to(RUN).as_posix()=='validation-03/root-access-probe-02/results.json':continue
    if q.suffix in {'.py','.json','.md'} or any(part.startswith('before-') for part in parts):files.add(q)
for q in files:
    b=q.read_bytes();need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
        all(line==line.rstrip(b' \t') for line in b.splitlines()),'authored source/report LF/EOF '+str(q))
approved={q.relative_to(F).as_posix() for q in files}
need(not (names(F,'diff','--name-only','-z')-approved),'only authored fork tracked edits')
head,fcount=publish(F,approved,'Lift retail startup state and add an owned native content volume')
prefix='Published fork '+chr(96)+head+chr(96)+': '
for name in ['PORT_STATUS.md','ROADMAP.md']:
    q=P/name;s=q.read_text()
    marker='Actual native retail startup leaves and owned content are implemented; validation-04 passes5 CPU/file clients'
    need(s.count(marker)==1,'single current startup result')
    s=s.replace(marker,prefix+marker[0].lower()+marker[1:])
    s=s.replace('Independent saved execution review passes; component publication pending.','Independent saved execution review passes; this component is published.')
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork '
        need(s.count(marker)==1,'single current checkpoint')
        s=s.replace(marker,'| Current native source checkpoint | '+prefix+
            'source-generated retail startup leaves and instance-owned sector-free file service;5 native CPU/file clients pass '+
            '(651 stable pins,560 untouched prior), expected linker refusal for4 unimplemented entry bindings. '+
            'Actual symlink escape not exercised; complete startup/input/render/audio/save/game acceptance OPEN. Prior published fork ',1)
    q.write_bytes(s.rstrip('\n').encode()+b'\n')
receipt=R/'PUBLICATION.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_RETAIL_STARTUP_COMPONENT_PUBLISHED',fork_commit=head,
        verification_sha256=sha(RUN/'validation-04/verification.json'),fixtures=5,source_pins=651,
        unchanged_prior_pins=560,missing_production_bindings=v['missing_production_bindings'],
        volume_capability_notes=v['volume_capability_notes'],separate_worker_iop_sha256=protected,
        limits=['Native startup leaves and native content only, not bootable or accepted game',
            'Exact failed artifacts retained; normalized Win32 diagnostic rows published, original CRLF probe remains local',
            'Offline decoder unchanged; no physical endpoint or game launch']),out,indent=2)
    out.write('\n')
files={P/'PORT_STATUS.md',P/'ROADMAP.md',P/'.gitattributes'}
files.update(q for q in R.rglob('*') if q.is_file())
check_parent_text(files)
approved={q.relative_to(P).as_posix() for q in files}
need(names(P,'diff','--name-only','-z')-approved<={'research/doc-audit-20261118/inventory.json'},'only authored parent tracked edits')
phead,pcount=publish(P,approved,'Record native retail startup checks and remaining initialization dependencies')
validate()
for name,digest in protected.items():need(sha(F/name)==digest,'IOP unchanged after publication')
need(sha(inventory)==inventory_digest,'inventory unchanged after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_RETAIL_STARTUP_AND_DOCS',fork_commit=head,parent_commit=phead,
    fork_files=fcount,parent_files=pcount,fixtures=5,pins=651)),flush=True)
