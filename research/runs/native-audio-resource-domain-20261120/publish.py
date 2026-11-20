"""Publish only the reviewed native sound resource checkpoint; fail closed."""
import hashlib,json,pathlib,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2]
R=P/'research/runs/native-movie-ingress-20261119';RUN=F/'research/runs/native-audio-resource-domain-20261120'
def sha(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def git(root,*args,raw=False):
 result=subprocess.check_output(['git',*args],cwd=root)
 return result if raw else result.decode('utf-8').strip()
def run(root,*args):subprocess.run(['git',*args],cwd=root,check=True,stdout=subprocess.DEVNULL)
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def lines(root,*args):return set(git(root,*args,raw=True).decode().strip('\0').split('\0'))-{''}
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json'
require(sha(inventory)=='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6','preserved inventory')
for path,digest in preserved.items():require(sha(F/path)==digest,'preserved IOP')
V=RUN/'validation-04/verification.json';v=json.loads(V.read_bytes())
require(v['status']=='PASS_NATIVE_AUDIO_RESOURCE_DOMAIN_INTEGRATION' and len(v['fixtures'])==12,'verification schema/status')
require(all(sha(path)==digest for path,digest in v['source_after_sha256'].items()),'current exact verified source pins')
for name,status in [('NATIVE_AUDIO_RESOURCE_EXCEPTION_CLOSURE_20261120.json','PASS_LIMITED_CORRECTION_REVIEW'),
('NATIVE_AUDIO_RESOURCE_EXECUTION_REVIEW_20261120.json','SAVED_VALIDATION_04_IDENTITIES_AND_LINKAGE_VERIFIED')]:
 d=json.loads((R/name).read_bytes());require(d['review_status']==status,'final review status')
 for row in d['inputs']:
  path=pathlib.Path(row['path']);path=path if path.is_absolute() else P/path
  require(sha(path)==row['sha256'],'review input '+str(path))
require(git(F,'rev-parse','HEAD')=='d10a54b040cbd47c37fa965a5871e86ebc77ae53','expected fork base')
require(git(P,'rev-parse','HEAD')=='a972fbb3dbc648e80df778b2b4bb31d32da9e806','expected parent base')
require(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','main branches')
require(not lines(F,'diff','--cached','--name-only','-z') and not lines(P,'diff','--cached','--name-only','-z'),'no unrelated staged files')
base=['CMakeLists.txt','cmake/native_sound_resources.cmake','runtime/include/mka/audio_resource_domain.hpp',
'runtime/src/native/audio_resource_domain.cpp','tests/test_audio_resource_domain.cpp','tests/test_audio_resource_exception.cpp',
'runtime/include/mka/sound_archive.hpp','runtime/src/native/sound_archive.cpp','runtime/include/mka/stream_source.hpp',
'runtime/src/native/stream_source.cpp','runtime/include/mka/audio_banks.hpp','runtime/src/native/audio_banks.cpp',
'runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp','runtime/include/mka/audio_bank_protocol.hpp',
'runtime/src/native/audio_bank_protocol.cpp','runtime/include/mka/audio_bank_service.hpp','runtime/src/native/audio_bank_service.cpp',
'runtime/include/mka/sample_payload.hpp']
paths={F/name for name in base}
paths.update(path for path in RUN.rglob('*') if path.is_file() and path.suffix in {'.json','.md','.py'} and '__pycache__' not in path.parts)
fork_names={path.relative_to(F).as_posix() for path in paths}
require(lines(F,'diff','--name-only','-z')-fork_names==set(preserved),'only preserved dirty IOP outside scoped source')
def publish(root,names,message):
 names=sorted(names)
 require(all((root/name).is_file() for name in names),'approved files exist')
 # Arguments are passed directly; no shell interpolation or broad staging.
 subprocess.run(['git','-c','core.autocrlf=false','add','--',*names],cwd=root,check=True,stdout=subprocess.DEVNULL)
 staged=lines(root,'diff','--cached','--name-only','-z')
 require(staged and staged <= set(names),'staged set is approved changed set')
 # Approved unchanged paths need not be staged; any approved changed/new path must be.
 changed=lines(root,'diff','HEAD','--name-only','-z') & set(names)
 require(staged==changed,'all approved changed paths staged')
 run(root,'diff','--cached','--check')
 for name in staged:require(git(root,'cat-file','blob',':'+name,raw=True)==(root/name).read_bytes(),'staged bytes '+name)
 run(root,'commit','-m',message)
 head=git(root,'rev-parse','HEAD')
 run(root,'push','origin','main')
 require(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'remote main equals published head')
 return head,len(staged)
fork_head,fork_count=publish(F,fork_names,'Own native sound resource barriers and safe movie publication lifetimes')
# Parent owns the published source-commit identity, avoiding circular commit pins.
for name in ['PORT_STATUS.md','ROADMAP.md']:
 path=P/name;text=path.read_text(encoding='utf-8')
 marker='Native sound resource-domain implementation passes all12 affected executables'
 require(text.count(marker)==1,'current result owner paragraph')
 text=text.replace(marker,'Published fork `'+fork_head+'`: native sound resource-domain implementation passes all12 affected executables',1)
 if name=='PORT_STATUS.md':
  marker='| Current native source checkpoint | Published fork '
  require(text.count(marker)==1,'source checkpoint row')
  text=text.replace(marker,'| Current native source checkpoint | Published fork `'+fork_head+'`: native sound-resource domain/actual immutable PCM publication and safe final backing retirement;12 affected fixtures pass (328 stable pins,287 unchanged prior pins), injected failure and concurrency coverage; full LOGON surround/renderer/host output/game acceptance OPEN. Prior published fork ',1)
  old='| Offline native sound work | Owned bank/program/command/resource lookup, raw normal-sample reader and mono data codec; records below | Standalone source/byte checks only; no live sound, mixer, hardware or whole-game parity |'
  require(old in text,'affected sound status row')
  text=text.replace(old,'| Native sound components | Owned bank/program/resource lookup and codecs, native voice/mix fixtures, shared preparation/backing owner and actual PCM publication/retirement;12 affected fixtures pass | Native source/software component evidence only; live sound identity, physical output, hardware/full-game parity remain OPEN |',1)
 path.write_text(text,encoding='utf-8',newline='\n')
result=R/'NATIVE_AUDIO_RESOURCE_DOMAIN_RESULTS_20261120.md'
result.write_text('Published fork source checkpoint: `'+fork_head+'`. This is a native component checkpoint, not an accepted game binary.\n\n'+result.read_text(),encoding='utf-8',newline='\n')
receipt=dict(status='NATIVE_AUDIO_RESOURCE_COMPONENT_PUBLISHED',fork_commit=fork_head,
verification_sha256=sha(V),fixtures=12,source_pins=328,unchanged_prior_pins=287,
limits=['No game/physical endpoint evidence or full-game acceptance; see scoped records'],preserved_dirty_sha256=preserved)
(R/'NATIVE_AUDIO_RESOURCE_PUBLICATION_20261120.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
reports=['NATIVE_AUDIO_RESOURCE_BINDING_REVIEW_20261120','NATIVE_AUDIO_RESOURCE_IMPLEMENTATION_REVIEW_20261120',
'NATIVE_AUDIO_RESOURCE_FINAL_PEER_20261120','NATIVE_AUDIO_RESOURCE_PEER_CLOSURE_20261120',
'NATIVE_AUDIO_RESOURCE_OWNER_CLOSURE_20261120','NATIVE_AUDIO_RESOURCE_EXCEPTION_CLOSURE_20261120',
'NATIVE_AUDIO_RESOURCE_EXECUTION_REVIEW_20261120']
parent_paths={P/name for name in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
parent_paths.update(R/(name+suffix) for name in reports for suffix in ['.md','.json'])
parent_paths.update(R/name for name in ['NATIVE_AUDIO_RESOURCE_DOMAIN_PLAN_20261120.md','NATIVE_AUDIO_RESOURCE_DOMAIN_RESULTS_20261120.md',
'NATIVE_AUDIO_RESOURCE_PREBUILD_CORRECTIONS_20261120.md','NATIVE_AUDIO_RESOURCE_PUBLICATION_20261120.json'])
parent_paths.update(path for path in (R/'native-audio-resource-domain-before-20261120').rglob('*') if path.is_file())
parent_names={path.relative_to(P).as_posix() for path in parent_paths}
require(lines(P,'diff','--name-only','-z')-parent_names=={'research/doc-audit-20261118/inventory.json'},'only preserved inventory outside parent docs')
parent_head,parent_count=publish(P,parent_names,'Record verified native sound resource ownership checkpoint and remaining movie binding')
for path,digest in preserved.items():require(sha(F/path)==digest,'IOP still preserved after publication')
require(sha(inventory)=='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6','inventory still preserved')
require(all(sha(path)==digest for path,digest in v['source_after_sha256'].items()),'verified source unchanged after commits')
print(json.dumps(dict(status='PUBLISHED_SCOPED_NATIVE_AUDIO_RESOURCE_CHECKPOINT',fork_commit=fork_head,parent_commit=parent_head,
fork_files=fork_count,parent_files=parent_count,fixtures=12,verification_sha256=sha(V))),flush=True)
