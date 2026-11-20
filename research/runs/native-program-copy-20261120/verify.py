"""Actual native per-play copies/outputs; frozen decoder; no physical/game."""
import ast,ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119'
BASE=F/'research/runs/native-runtime-preparation-20261120/verify.py'
spec=importlib.util.spec_from_file_location('_program_copy_prior_cases',BASE);e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e);c=e.c
CHANGED=['runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp','runtime/include/mka/program_executor.hpp','runtime/src/native/program_executor.cpp','cmake/native_sound_resources.cmake']
NEW=['runtime/include/mka/program_copy.hpp','runtime/src/native/program_copy.cpp','tests/test_program_copy.cpp','tests/test_program_copy_failure.cpp']
def verify():
 need,sha,write=c.require,c.sha,c.write
 cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig');need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'actual native-only Release')
 prior_path=F/'research/runs/native-runtime-preparation-20261120/validation-01/verification.json';prior=json.loads(prior_path.read_bytes())
 need(prior['status']=='PASS_NATIVE_BANK_RUNTIME_PREPARATION_OWNERSHIP' and len(prior['source_after_sha256'])==415 and len(prior['fixtures'])==15,'actual prior schema')
 changed={str(F/n) for n in CHANGED};untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed};need(all(sha(n)==h for n,h in untouched.items()),'untouched prior data/code/evidence')
 review_path=R/'NATIVE_PROGRAM_COPY_FINAL_SOURCE_REVIEW_20261120.json';review=json.loads(review_path.read_bytes())
 need(review['status']=='SOURCE_REVIEW_NATIVE_PROGRAM_COPY_NO_KNOWN_BLOCKER' and set(review['input_sha256'])=={str(F/n) for n in CHANGED+NEW},'actual nine-input final review')
 need(all(sha(n)==h for n,h in review['input_sha256'].items()),'review current')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']};paths.update(F/n for n in CHANGED+NEW)
 proof_path=R/'NATIVE_PROGRAM_COPY_ROOT_WORDS_20261120.json';proof=json.loads(proof_path.read_bytes())
 need(proof['status']=='NORMAL_CLONE_AND_ORDINARY_SCHEDULE_RAW_WORD_ROWS_VERIFIED' and proof['word_rows']==1149 and len(proof['exports'])==13,'bounded original clone/schedule proof')
 paths.update([pathlib.Path(__file__).resolve(),BASE,prior_path,review_path,review_path.with_suffix('.md'),proof_path,R/'NATIVE_PROGRAM_COPY_PLAN_20261120.md'])
 for stem in ['NATIVE_NORMAL_PLAY_INSTANCE_SOURCE_20261120','NATIVE_NORMAL_PROGRAM_ACTIVATION_SOURCE_20261120']:
  q=R/(stem+'.json');d=json.loads(q.read_bytes());need(d['source_only'] is True and isinstance(d['input_sha256'],dict),'actual source review schema');paths.update([q,q.with_suffix('.md')])
  for n,h in d['input_sha256'].items():
   q=pathlib.Path(n)
   if n in changed:
    q=R/'native-program-copy-before-20261120'/q.relative_to(F)
   need(sha(q)==h,'preserved original native/source input');paths.add(q)
 for export in proof['exports']:
  q=pathlib.Path(export['path']);need(sha(q)==export['sha256'],'actual raw original export');paths.add(q)
 before={str(q):sha(q) for q in sorted(paths)}
 for n in CHANGED+NEW:need(b'\r' not in (F/n).read_bytes(),'authored LF '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'preserved dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),scope='Actual perplay command/donor/output ownership and ordinary reset/deadline plans; no output admission or game'))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
 a=F/'research/runs/native-program-admission-20261120/verify.py';b=F/'research/runs/native-deferred-program-work-20261120/verify.py'
 cases=[
  ('copy','test_native_program_copy',[ms2],r'PASS_NATIVE_PROGRAM_COPY: actual ANNOUNCR copies/schedules/independent native outputs/semantic cleanup and pure command arithmetic; no source activation/callback/heard fidelity claim'),
  ('copy-failure','test_native_program_copy_failure',[ms2],r'PASS native program copy allocation rollback: [0-9]+ failure sites; actual voice failure/retry; test-only allocator wrap'),
  ('runtime','test_native_runtime_preparation',[ms2],r'PASS_NATIVE_RUNTIME_PREPARATION: 5 actual bank runtime ownership cases; no callback/clone/output/voice/fidelity claim')]+e.source_cases(a,ms2,ms4)
 cases += [case for case in e.source_cases(b,ms2,ms4) if case[0] in ['work','cache','preparation','program']]
 need(len(cases)==17 and len({x[1] for x in cases})==17,'seventeen actual affected targets')
 c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[x[1] for x in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True);exe=c.BUILD/(target+'.exe');run=c.execute(label+'.json',[exe,*args])
  need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete fixture assertions '+label)
  if label.startswith('copy'):required=['mka::sound::NativeProgramCopy::clone']
  elif label in ['program','playback','reservation','owner-gaps']:required=['mka::sound::sample_start_operands']
  else:required=['mka::sound::PreparedBank::acquire_runtime_program']
  audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],required,True)
  fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M);need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only actual build rule')
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group());need(len(wraps)=={'copy-failure':1,'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'designated test-only wraps '+label)
  if label=='copy-failure':need(wraps==['_Znwy'],'only actual allocating new symbol injected')
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
 need(before=={n:sha(n) for n in before},'current source/review/tool/asset stable')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'decoder unchanged')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'dirty IOP preserved after execution')
 write('verification.json',dict(status='PASS_NATIVE_PROGRAM_COPY_AND_ORDINARY_SCHEDULE_PRIMITIVES',source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),limits=['Actual private normal copies/native voice ownership/reset/deadline primitives, not output admission/identity-list/source callback or whole game','Explicit source-compatible clock input is not a sampled clock or actual30000 activation receipt','Native start failure retains applied operands but not output; not complete retail error equivalence','Replacement/forcedstop1/alternate scheduling remain named unbound','Changed donor metadata justifies one344bank policy pass; no decoder or unrelated numeric corpus rerun','No game/physical endpoints/local model/server/driver/card/save/key/settings operation']))
 print(json.dumps(dict(status='PASS_NATIVE_PROGRAM_COPY_AND_ORDINARY_SCHEDULE_PRIMITIVES',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];c.OUT.mkdir(exist_ok=False);c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_PROGRAM_COPY_CHECK_FAILED',error=str(error)));raise
