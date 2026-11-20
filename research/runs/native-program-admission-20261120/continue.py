"""Continue unchanged native binaries after target-specific audit wrapper failure."""
import ast,ctypes,hashlib,importlib.util,json,os,pathlib,re,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('_original_admission_verifier',RUN/'verify.py')
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e);c=e.c
ctypes.windll.kernel32.SetErrorMode(3)
if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: continue.py validation-NN')
c.OUT=RUN/sys.argv[1];c.OUT.mkdir(exist_ok=False)
c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
def verify():
 need,sha,write=c.require,c.sha,c.write;old=RUN/'validation-01'
 aborted=json.loads((old/'aborted.json').read_bytes());need(aborted['error']=='scoped native symbols','specific saved wrapper failure')
 prov=json.loads((old/'provenance.json').read_bytes());pins=prov['source_before_sha256']
 plan=P/'research/runs/native-movie-ingress-20261119/NATIVE_PROGRAM_ADMISSION_PLAN_20261120.md'
 oldplan=P/'research/runs/native-movie-ingress-20261119/native-program-admission-before-20261120/plan-before-run01-wrapper-note.md'
 need(sha(oldplan)==pins[str(plan)],'exact original plan before status note')
 need(all(sha(n)==h for n,h in pins.items() if n!=str(plan)),'unchanged native inputs and reviewed original verifier')
 current={n:sha(n) for n in pins};current[str(oldplan)]=sha(oldplan);current[str(pathlib.Path(__file__).resolve())]=sha(__file__)
 current[str(old/'provenance.json')]=sha(old/'provenance.json');current[str(old/'aborted.json')]=sha(old/'aborted.json')
 tree=ast.parse((RUN/'verify.py').read_text());fun=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='verify')
 assignment=next(n for n in fun.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='cases' for t in n.targets))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4');scope=dict(ms2=ms2,ms4=ms4,c=c)
 exec(compile(ast.Module(body=[assignment],type_ignores=[]),'saved-original-cases','exec'),scope);cases=scope['cases']
 need(len(cases)==10,'ten declared affected targets')
 build=json.loads((old/'build.json').read_bytes());need(build['exit']==0,'original all-target build passed')
 current[str(old/'build.json')]=sha(old/'build.json')
 fixtures=[]
 for label,target,args,footer in cases:
  exe=c.BUILD/(target+'.exe');required=['mka::sound::'+{'banks':'AudioBankSession::admit_deferred_normal','protocol':'AudioBankProtocol::admit_deferred_normal'}.get(label,'AudioBankService::admit_deferred_program')]
  if label in ['admission','gates','banks']:
   runpath=old/(label+'.json');run=json.loads(runpath.read_bytes());nm_path=old/(label+'-symbols.json');nm=json.loads(nm_path.read_bytes())
   need(run['exit']==0 and run['argv'][0]==str(exe) and sha(exe)==run['executable_sha256']==run['executable_after_sha256'],'exact saved executed target '+label)
   need(nm['exit']==0 and nm['argv']==[str(c.BIN/'llvm-nm.exe'),'-C','--defined-only',str(exe)] and nm['executable_sha256']==nm['executable_after_sha256']==sha(c.BIN/'llvm-nm.exe'),'actual saved inspection tool/target '+label)
   forbidden=[line for line in nm['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b',line)]
   need(not forbidden and all(s in nm['stdout'] for s in required),'actual per-target required native symbol '+label)
   audit=dict(target=str(exe),target_before_sha256=sha(exe),target_after_sha256=sha(exe),audited_target_expected_sha256=run['executable_sha256'],execution_applicable=True,executed_target_sha256=run['executable_sha256'],tool_sha256=sha(c.BIN/'llvm-nm.exe'),forbidden=forbidden,required_defined_symbols=required,saved_execution=str(runpath),saved_inspection=str(nm_path))
   write(label+'-symbols-target.json',audit);current[str(runpath)]=sha(runpath);current[str(nm_path)]=sha(nm_path)
   location=str(runpath)
  else:
   print('RUN '+label,flush=True);run=c.execute(label+'.json',[exe,*args]);audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],required,True);location=str(c.OUT/(label+'.json'))
  need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'actual assertions '+label)
  fixtures.append(dict(component=label,target=target,execution_record=location,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
  need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only actual target')
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group())
  need(len(wraps)=={'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'only exact designated test wraps')
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
 need(current=={n:sha(n) for n in current},'current source/asset/tool/receipt pins stable')
 prior=json.loads((F/'research/runs/native-deferred-program-work-20261120/validation-01/verification.json').read_bytes())
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'decoder stays frozen')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'dirty IOP preserved')
 write('verification.json',dict(status='PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION',source_after_sha256=current,
  unchanged_prior_pins=prov['unchanged_prior_pins'],fixtures=fixtures,saved_executions_reused=3,new_executions=7,new_builds=0,
  original_build_receipt=str(old/'build.json'),frozen_decoder_certificate_sha256=sha(frozen),
  prior_wrapper_failure=dict(receipt=str(old/'aborted.json'),reason='Service symbol incorrectly required for bank-only target; actual Session symbol verified, original record preserved'),
  limits=['Actual owned fresh data admission, not source scheduler/callback/runtime descriptor/command12 or full game',
   'Named same-bank busy overlap is not verified source unload wait/callback equivalence',
   'Unexpected unload mutation quarantined, explicit whole Session close remains possible',
   'No hardware endpoints, game, local model/server, driver, save/key/card action; no decoder/whole-bank corpus rerun']))
 print(json.dumps(dict(status='PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION',fixtures=len(fixtures),pins=len(current),untouched=prov['unchanged_prior_pins'])),flush=True)
try:verify()
except Exception as error:c.write('aborted.json',dict(status='NATIVE_PROGRAM_ADMISSION_CONTINUATION_FAILED',error=str(error)));raise
