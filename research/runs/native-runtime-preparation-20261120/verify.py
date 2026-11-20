"""Bank-owned native runtime cache/semantic refs; no decoder/game/endpoint work."""
import ast,ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119'
COMMON=F/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_runtime_owner_evidence',COMMON);c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
CHANGED=['runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp','cmake/native_sound_resources.cmake']
NEW=['runtime/include/mka/runtime_preparation.hpp','tests/test_runtime_preparation.cpp']
def source_cases(path,ms2,ms4):
 tree=ast.parse(path.read_text());fun=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='verify')
 node=next(n for n in fun.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='cases' for t in n.targets))
 scope=dict(ms2=ms2,ms4=ms4,c=c,common=c);exec(compile(ast.Module(body=[node],type_ignores=[]),'frozen-case-definitions','exec'),scope);return scope['cases']
def verify():
 need,sha,write=c.require,c.sha,c.write
 cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig');need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'native Release empty generated game')
 prior_path=F/'research/runs/native-program-admission-20261120/validation-02/verification.json';prior=json.loads(prior_path.read_bytes())
 need(prior['status']=='PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION' and len(prior['source_after_sha256'])==379 and len(prior['fixtures'])==10,'actual prior schema')
 changed={str(F/n) for n in CHANGED};untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed};need(all(sha(n)==h for n,h in untouched.items()),'untouched prior source/evidence/assets')
 review_path=R/'NATIVE_RUNTIME_PREPARATION_SOURCE_REVIEW_20261120.json';review=json.loads(review_path.read_bytes())
 need(review['status']=='SOURCE_REVIEW_NATIVE_RUNTIME_CACHE_OWNER_VERIFIED' and set(review['input_sha256'])=={str(F/n) for n in CHANGED+NEW},'actual reviewed five input schema')
 need(all(sha(n)==h for n,h in review['input_sha256'].items()),'actual reviewed final source')
 proof_path=R/'NATIVE_RUNTIME_PREPARATION_ROOT_WORDS_20261120.json';proof=json.loads(proof_path.read_bytes());need(proof['status']=='BOUNDED_RUNTIME_CONSTRUCTOR_RELEASE_AND_ACTIVATION_RAW_ROWS_VERIFIED' and proof['word_rows']==1189 and len(proof['exports'])==17,'bounded original source proof')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']};paths.update(F/n for n in CHANGED+NEW)
 paths.update([pathlib.Path(__file__).resolve(),COMMON,prior_path,review_path,review_path.with_suffix('.md'),proof_path,R/'NATIVE_RUNTIME_PREPARATION_PLAN_20261120.md'])
 for stem in ['NATIVE_RUNTIME_PREPARATION_OWNER_SOURCE_20261120','NATIVE_RUNTIME_ACTIVATION_SOURCE_20261120']:
  q=R/(stem+'.json');d=json.loads(q.read_bytes());need(d['source_only'] is True and isinstance(d['input_sha256'],dict),'actual historical original source schema')
  paths.update([q,q.with_suffix('.md')])
  for name,digest in d['input_sha256'].items():
   original=pathlib.Path(name)
   if name in changed:
    before=R/'native-runtime-preparation-before-20261120'/original.relative_to(F)
    need(sha(before)==digest,'exact preimplementation native source');paths.add(before)
   else:need(sha(original)==digest,'unchanged original source input');paths.add(original)
 for export in proof['exports']:
  q=pathlib.Path(export['path']);need(sha(q)==export['sha256'],'original raw word export');paths.add(q)
 before={str(q):sha(q) for q in sorted(paths)}
 for n in CHANGED+NEW:need(b'\r' not in (F/n).read_bytes(),'authored LF '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'preserved dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),scope='Changed real bank-owned runtime/cache/reference ownership, no activation or physical game'))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
 a=F/'research/runs/native-program-admission-20261120/verify.py';b=F/'research/runs/native-deferred-program-work-20261120/verify.py'
 cases=[('runtime','test_native_runtime_preparation',[ms2],r'PASS_NATIVE_RUNTIME_PREPARATION: 5 actual bank runtime ownership cases; no callback/clone/output/voice/fidelity claim')]+source_cases(a,ms2,ms4)
 cases += [case for case in source_cases(b,ms2,ms4) if case[0] in ['work','cache','preparation','program']]
 need(len(cases)==15 and len({x[1] for x in cases})==15,'fifteen actual affected targets')
 c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[x[1] for x in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True);exe=c.BUILD/(target+'.exe');run=c.execute(label+'.json',[exe,*args])
  need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete fixture assertions '+label)
  audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],['mka::sound::PreparedBank::acquire_runtime_program'],True)
  fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M);need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only actual target')
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group());need(len(wraps)=={'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'only designated fault wraps '+label)
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
 need(before=={n:sha(n) for n in before},'source/tool/review/asset stable')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'decoder frozen')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'dirty IOP preserved after execution')
 write('verification.json',dict(status='PASS_NATIVE_BANK_RUNTIME_PREPARATION_OWNERSHIP',source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),limits=['Actual constructor/cache/semantic reference ownership, not full runtime activation/clone/callback/source worker or game acceptance','Exported immutable PreparedProgram retains occurrences; last semantic release proves runtime cache detachment only','Eager constructors run on private bank candidate, not within individual program leases','Changed core bank preparation justifies existing344bank policy check; movie decoder/corpus and unrelated numeric tests not rerun','No physical device/game/model/server/driver/card/save/key/settings operation']))
 print(json.dumps(dict(status='PASS_NATIVE_BANK_RUNTIME_PREPARATION_OWNERSHIP',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];c.OUT.mkdir(exist_ok=False);c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_RUNTIME_PREPARATION_CHECK_FAILED',error=str(error)));raise
