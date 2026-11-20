"""Single formatting-corrected fixture rebuild; retain20 verified unchanged clients."""
import ctypes,datetime,hashlib,importlib.util,json,os,pathlib,re,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119';RUN=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('_output_format_original',RUN/'verify.py');original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original);c=original.c

def verify():
 need,sha,write=c.require,c.sha,c.write
 prior_path=RUN/'validation-01/verification.json';prior=json.loads(prior_path.read_bytes());need(prior['status']=='PASS_NATIVE_LOGICAL_OUTPUT_AND_SHARED_CLOCK_ACTIVATION' and len(prior['fixtures'])==21 and len(prior['source_after_sha256'])==511,'actual historical execution schema')
 fixture=F/'tests/test_program_output_activation.cpp';old=RUN/'before-whitespace-correction/test_program_output_activation.cpp'
 need(old.read_bytes()==fixture.read_bytes()+b'\n' and sha(old)==prior['source_after_sha256'][str(fixture)],'only terminal LF removed; exact old bytes retained')
 need(all(sha(n)==h for n,h in prior['source_after_sha256'].items() if n!=str(fixture)),'all other original511 inputs current')
 review_path=R/'NATIVE_OUTPUT_ACTIVATION_SOURCE_REVIEW_02_20261120.json';review=json.loads(review_path.read_bytes());need(review['status']=='SOURCE_REVIEW_NATIVE_OUTPUT_ACTIVATION_VERIFIED' and review['source_only'] is True and len(review['input_sha256'])==11,'actual corrected11input source review')
 need(all(sha(n)==h for n,h in review['input_sha256'].items()),'corrected source review current')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']};paths.update([prior_path,old,RUN/'before-whitespace-correction/.gitattributes',F/'.gitattributes',RUN/'publication-attempt-01.json',pathlib.Path(__file__).resolve(),review_path,review_path.with_suffix('.md')])
 before={str(q):sha(q) for q in sorted(paths)}
 for n in original.CHANGED+original.NEW:
  data=(F/n).read_bytes();need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n') and all(line==line.rstrip(b' \t') for line in data.splitlines()),'full authored whitespace preflight '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'protected IOP')
 baseline={x['component']:x for x in prior['fixtures']};need(len(baseline)==21,'distinct original clients')
 for x in prior['fixtures']:need(sha(c.BUILD/(x['target']+'.exe'))==x['executed_target_sha256'],'unchanged prior binary before rebuild '+x['component'])
 archive=c.BUILD/'libmka_native_sound_resources.a';archive_before=sha(archive)
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_before_sha256=before,original_verification_sha256=sha(prior_path),scope='Only finalLF removed from activationfixture; one actual rebuild/rerun,20 unchanged receipts explicitly reused',native_archive_before_sha256=archive_before,compiler_sha256=sha(c.BIN/'clang++.exe')))
 c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target','test_native_program_output_activation','-j2'])
 need(sha(archive)==archive_before,'production archive unchanged by fixture-only rebuild')
 fixtures=[]
 for old_fixture in prior['fixtures']:
  label,target=old_fixture['component'],old_fixture['target'];exe=c.BUILD/(target+'.exe');old_run_path=RUN/'validation-01'/(label+'.json');old_run=json.loads(old_run_path.read_bytes())
  if label=='output-activation':
   run=c.execute(label+'.json',old_run['argv']);need(run['stdout']==old_run['stdout'],'exact full7group corrected assertions');audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],old_fixture['audit']['required_defined_symbols'],True);reused=False
  else:
   need(sha(exe)==old_fixture['executed_target_sha256']==old_run['executable_sha256']==old_run['executable_after_sha256'] and old_run['exit']==0,'unchanged executable and passed original receipt '+label)
   run=dict(old_run,reused_from=str(old_run_path),reused_receipt_sha256=sha(old_run_path));write(label+'.json',run)
   for suffix in ['-symbols.json','-symbols-target.json']:
    q=RUN/'validation-01'/(label+suffix);record=json.loads(q.read_bytes());write(label+suffix,dict(record,reused_from=str(q),reused_receipt_sha256=sha(q)))
   audit=old_fixture['audit'];reused=True
  fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit,execution_reused=reused,execution_origin=str(old_run_path) if reused else str(c.OUT/(label+'.json'))))
 link=RUN/'validation-01/linkage-audit.json';linkage=json.loads(link.read_bytes());need(sha(c.BUILD/'build.ninja')==linkage['ninja_sha256'],'unchanged actual native linkage');write('linkage-audit.json',dict(linkage,reused_from=str(link),reused_receipt_sha256=sha(link)))
 need(before=={n:sha(n) for n in before},'all correction/current inputs stable')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'frozen decoder unchanged')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'protected IOP after correction')
 write('verification.json',dict(status='PASS_NATIVE_LOGICAL_OUTPUT_AND_SHARED_CLOCK_ACTIVATION',source_after_sha256=before,unchanged_prior_pins=486,fixtures=fixtures,executed_this_run=1,reused_unchanged_executions=20,original_verification_sha256=sha(prior_path),native_archive_before_sha256=archive_before,native_archive_after_sha256=sha(archive),frozen_decoder_certificate_sha256=sha(frozen),limits=prior['limits']+['Validation01 is preserved before-formatting evidence; validation02 is current','One whitespace-corrected fixture rebuilt/rerun;20 prior executions explicitly reused only after exact current source/binary checks','Raw before-correction snapshots alone are exempted from whitespace normalization/checks; production source/checkers unchanged']))
 print(json.dumps(dict(status='PASS_NATIVE_LOGICAL_OUTPUT_AND_SHARED_CLOCK_ACTIVATION',fixtures=21,pins=len(before),untouched=486,executed_now=1,reused=20)),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify_whitespace_correction.py validation-NN')
 c.OUT=RUN/sys.argv[1];c.OUT.mkdir(exist_ok=False);c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_OUTPUT_FORMATTING_CORRECTION_CHECK_FAILED',error=str(error)));raise
