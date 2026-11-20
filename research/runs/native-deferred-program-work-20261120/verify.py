"""Corrected native sample-cache policy and owned fresh work; no device/game."""
import ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2]
RECORDS=PARENT/'research/runs/native-movie-ingress-20261119'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_deferred_work_evidence',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
CHANGED=['runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp',
 'tests/test_bank_preparation.cpp','cmake/native_sound_resources.cmake']
NEW=['tests/test_deferred_program_work.cpp','tests/test_deferred_cache_policy.cpp']
def verify():
    require,sha,write=common.require,common.sha,common.write
    cache=common.BUILD/'CMakeCache.txt';text=cache.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$',text,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M)),'native-only Release cache')
    prior_path=FORK/'research/runs/native-sound-play-reservation-20261120/validation-01/verification.json'
    prior=json.loads(prior_path.read_bytes())
    require(prior['status']=='PASS_NATIVE_SOUND_PLAY_CONTEXT_RESERVATION_AND_SERVICE_GAPS' and len(prior['fixtures'])==3,'actual prior schema')
    changed={str(FORK/name) for name in CHANGED}
    untouched={name:digest for name,digest in prior['source_after_sha256'].items() if name not in changed}
    require(all(sha(name)==digest for name,digest in untouched.items()),'untouched prior code/evidence/assets')
    source_path=RECORDS/'NATIVE_DEFERRED_PROGRAM_FINAL_SOURCE_20261120.json'
    source=json.loads(source_path.read_bytes())
    require(source['status']=='FINAL_DEFERRED_PROGRAM_SOURCE_CORRECTIONS_VERIFIED' and source['current_inputs_stable_at_report'] is True,'actual final source review')
    require(all(sha(PARENT/n)==h for n,h in source['current_inputs_at_review_start'].items()),'reviewed current native source')
    fixture_path=RECORDS/'NATIVE_DEFERRED_PROGRAM_FIXTURE_REVIEW_20261120.json'
    fixture=json.loads(fixture_path.read_bytes())
    require(fixture['status']=='SOURCE_REVIEW_NO_CONCRETE_FIXTURE_BLOCKER' and isinstance(fixture['input_sha256'],dict),'actual fixture review schema')
    require(all(sha(n)==h for n,h in fixture['input_sha256'].items()),'reviewed fixture inputs')
    proof_path=RECORDS/'NATIVE_SAMPLE_CACHE_BRANCH_ROOT_CHECK_20261120.json'
    proof=json.loads(proof_path.read_bytes())
    require(proof['status']=='ORIGINAL_FLAG40_PRESENT_SKIPS_REFERENCE_DECREMENT_VERIFIED','original branch polarity')
    require(sha(PARENT/proof['module'])==proof['module_sha256'],'exact original module')
    paths={pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(FORK/n for n in CHANGED+NEW)
    paths.update([COMMON,pathlib.Path(__file__).resolve(),prior_path,source_path,source_path.with_suffix('.md'),
     fixture_path,fixture_path.with_suffix('.md'),proof_path,
     RECORDS/'NATIVE_DEFERRED_CANDIDATE_CACHE_BRANCH_CORRECTION_20261120.json',
     RECORDS/'NATIVE_DEFERRED_CANDIDATE_POLICY_CORRECTION_20261120.json'])
    before={str(p):sha(p) for p in sorted(paths)}
    for n in CHANGED+NEW:require(b'\r' not in (FORK/n).read_bytes(),'authored LF '+n)
    for n,h in common.PRESERVE.items():require(sha(FORK/n)==h,'preserved dirty IOP')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
      fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
      parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
      source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(common.BIN/'clang++.exe'),
      scope='Changed normal cache ownership and actual fresh program work; transitive native preparation/Service/playback clients, no physical endpoint'))
    ms2=FORK/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
    require(str(ms2) in before and str(ms4) in before,'actual pinned source assets')
    cases=[
     ('work','test_native_deferred_program_work',[ms2],r'PASS_NATIVE_DEFERRED_PROGRAM_WORK: 5 actual ANNOUNCR lifecycle cases; CPU-only, no runtime callback or hardware fidelity claim'),
     ('cache','test_native_deferred_cache_policy',[common.OUT/'cache-owned'],r'PASS native sample cache flag40 polarity/occurrence ownership/PCM separation/private candidate cleanup; explicit source-format fixtures only'),
     ('exceptions','test_native_audio_resource_exception',[ms2],r'PASS injected native preparation exceptions/whole-candidate cleanup/exact-token retries/failure completion; no production fault switch'),
     ('domain','test_native_audio_resource_domain',[ms2,common.OUT/'domain-owned'],r'PASS native audio resource barriers/shared backings/private PCM publication/unload; no game or physical audio'),
     ('banks','test_native_audio_banks',[ms2,ms4],r'PASS audio bank host ownership, exact real lookup and retained source bytes; no preparation/playback claim'),
     ('protocol','test_native_audio_bank_protocol',[ms2],r'PASS verified audio bank framing and explicit host token binding; no preparation/completion/playback'),
     ('preparation','test_native_bank_preparation',[ms2,common.OUT/'preparation-owned'],r'PASS [0-9]+ actualbanks [0-9]+eager [0-9]+deferred [0-9]+skipprecreation; nativepreparation only nocallback/playback'),
     ('service','test_native_audio_bank_service',[ms2,common.OUT/'service-owned'],r'PASS nativebank sync/async completion ordering/exacttokens/eagerfailure; no transport/game/playback'),
     ('program','test_native_program_executor',[ms2],r'PASS actualannouncrtimeline/ownedclip/exactoperands/startACK/stop/unsupportedcontrols; no mix/audio execution'),
     ('playback','test_native_playback_owner',[ms2],r'PASS actualservice/fulltoken/deferredannouncr/HostVoiceACK/kind7/ownerstop/64voices/mix/refusedactiveunload; no boot/device/fidelity claim'),
     ('reservation','test_native_playback_reservation',[ms2],r'PASS actual ANNOUNCR pending play reservation/context identity/registry growth/nonzero concurrent voice/resource domain; no device/game/PCM fidelity claim'),
     ('gaps','test_native_playback_reservation_gaps',[ms2],r'PASS native playback reservation pre/post-service gaps/64 slot growth/unpublished rollback/exact-bank and close uncertainty; injected CPU fixture only')]
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',*[x[1] for x in cases],'-j2'])
    fixtures=[]
    for label,target,args,footer in cases:
        print('RUN '+label,flush=True)
        exe=common.BUILD/(target+'.exe');run=common.execute(label+'.json',[exe,*args])
        require(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete assertions '+label)
        audit=common.symbols(label+'-symbols',exe,run['executable_sha256'],['mka::sound::PreparedProgramWork::build'],True)
        fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
    ninja=common.BUILD/'build.ninja';text=ninja.read_text();rules=[]
    for label,target,_,_ in cases:
        match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
        require(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only target link')
        require(('--wrap=' in match.group())==(label in ['gaps','exceptions']),'only designated fault fixtures wrap')
        rules.append(dict(target=target,actual_build_rule=match.group()))
    write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
    require(before=={n:sha(n) for n in before},'stable source/tool/evidence/asset pins')
    frozen=FORK/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    require(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'movie decoder stays frozen')
    for n,h in common.PRESERVE.items():require(sha(FORK/n)==h,'IOP preserved after execution')
    write('verification.json',dict(status='PASS_NATIVE_DEFERRED_PROGRAM_WORK_AND_CORRECTED_SAMPLE_CACHE',
      source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),
      limits=['Actual fresh-data preparation cancellation/lifetime, not complete source runtime descriptor/callback coordination',
       'Service async admission lease, source cached-program branch, global queue callback/retirement and command12 remain OPEN',
       'Sticky cache test retains an eager program through bank ownership; no isolated proof beyond stated source model',
       'Changed full native preparation justified one existing344-bank policy pass; no movie decoder or unchanged numeric corpus rerun',
       'No game/physical device/model/server/driver/user-data action or accepted retail run']))
    print(json.dumps(dict(status='PASS_NATIVE_DEFERRED_PROGRAM_WORK_AND_CORRECTED_SAMPLE_CACHE',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
    common.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];common.OUT.mkdir(exist_ok=False)
    common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    try:verify()
    except Exception as error:common.write('aborted.json',dict(status='NATIVE_DEFERRED_PROGRAM_CHECK_FAILED',error=str(error)));raise
