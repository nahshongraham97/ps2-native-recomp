"""Native play reservations; actual file/PCM, explicit fault wrappers, no device."""
import ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2]
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_sound_reservation_evidence',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
CHANGED=['runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp',
 'tests/test_playback_owner.cpp','cmake/native_sound_resources.cmake']
NEW=['tests/test_playback_reservation.cpp','tests/test_playback_reservation_gaps.cpp']
def verify():
    require,sha,write=common.require,common.sha,common.write
    cache=common.BUILD/'CMakeCache.txt';text=cache.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$',text,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M)),'native-only Release cache')
    prior_path=FORK/'research/runs/native-logon-pending-20261120/validation-03/verification.json'
    prior=json.loads(prior_path.read_bytes())
    require(prior['status']=='PASS_NATIVE_LOGON_PENDING_AND_RETAINED_TASK_CLEANUP' and len(prior['fixtures'])==5,'actual prior schema/scope')
    changed={str(FORK/name) for name in CHANGED}
    untouched={name:digest for name,digest in prior['source_after_sha256'].items() if name not in changed}
    require(all(sha(name)==digest for name,digest in untouched.items()),'unchanged prior pins')
    review_path=PARENT/'research/runs/native-movie-ingress-20261119/NATIVE_SOUND_PLAY_RESERVATION_FINAL_SOURCE_20261120.json'
    # Inspect this producer's actual schema before choosing fields; the reviewed
    # current files are also pinned here independently of its narrative status.
    review=json.loads(review_path.read_bytes())
    require(review['review_status']=='PASS_BOUNDED_RESERVATION_CONTEXT_LIFETIME_SOURCE_REVIEW' and review['source_only'] is True and isinstance(review['inputs'],list),'actual current source review schema/status')
    require(all(sha(PARENT/row['path'])==row['sha256'] for row in review['inputs']),'current exact reviewed source')
    paths={pathlib.Path(name) for name in prior['source_after_sha256']}
    paths.update(FORK/name for name in CHANGED+NEW)
    paths.update([COMMON,pathlib.Path(__file__).resolve(),prior_path,review_path,review_path.with_suffix('.md')])
    before={str(path):sha(path) for path in sorted(paths)}
    for name in CHANGED+NEW:require(b'\r' not in (FORK/name).read_bytes(),'authored LF '+name)
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved dirty IOP')
    asset=FORK/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2'
    require(str(asset) in before,'actual original asset is already hash-pinned')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
      fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
      parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
      source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(common.BIN/'clang++.exe'),
      cache_before_sha256=sha(cache),scope='Native playback context/reservations with actual ANNOUNCR PCM and explicit fixture-only Service gap injection'))
    cases=[
      ('playback','test_native_playback_owner',
       'PASS actualservice/fulltoken/deferredannouncr/HostVoiceACK/kind7/ownerstop/64voices/mix/refusedactiveunload; no boot/device/fidelity claim'),
      ('publication','test_native_playback_reservation',
       'PASS actual ANNOUNCR pending play reservation/context identity/registry growth/nonzero concurrent voice/resource domain; no device/game/PCM fidelity claim'),
      ('gaps','test_native_playback_reservation_gaps',
       'PASS native playback reservation pre/post-service gaps/64 slot growth/unpublished rollback/exact-bank and close uncertainty; injected CPU fixture only')]
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',*[row[1] for row in cases],'-j2'])
    fixtures=[]
    for label,target,footer in cases:
        print('RUN '+label,flush=True)
        exe=common.BUILD/(target+'.exe');run=common.execute(label+'.json',[exe,asset])
        require(run['stdout'].splitlines()[-1]==footer,'complete assertions '+label)
        audit=common.symbols(label+'-symbols',exe,run['executable_sha256'],
         ['mka::sound::NativePlaybackOwner::play','mka::sound::NativePlaybackOwner::create_sound_context'],True)
        fixtures.append(dict(component=label,executed_target_sha256=run['executable_sha256'],audit=audit))
    # Wrapping exists ONLY on the explicit fault-injection target.
    ninja=common.BUILD/'build.ninja';text=ninja.read_text();rules=[]
    for label,target,_ in cases:
        match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
        require(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native target linkage')
        require(('--wrap=' in match.group())==(label=='gaps'),'fixture-only Service wrappers')
        rules.append(dict(target=target,actual_build_rule=match.group()))
    write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
    require(before=={name:sha(name) for name in before},'stable current source/tools/asset/review pins')
    frozen=FORK/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    require(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'decoder stays frozen')
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'IOP preserved after execution')
    write('verification.json',dict(status='PASS_NATIVE_SOUND_PLAY_CONTEXT_RESERVATION_AND_SERVICE_GAPS',
      source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,
      frozen_decoder_certificate_sha256=sha(frozen),
      limits=['Actual source-native deferred job cancellation, command12 forced cleanup1 and global public handle revocation remain unimplemented',
       'Explicit native context identity is not yet bound to retail bootstrap/current-context producer',
       'File/PCM/ownership/normal mix tests and injected CPU gaps, not physical audio, waveform/timing parity or full-game acceptance',
       'No game/model/server/driver/user-data action; no decoder-only or unchanged sound corpus rerun']))
    print(json.dumps(dict(status='PASS_NATIVE_SOUND_PLAY_CONTEXT_RESERVATION_AND_SERVICE_GAPS',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
    common.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];common.OUT.mkdir(exist_ok=False)
    common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    try:verify()
    except Exception as error:common.write('aborted.json',dict(status='NATIVE_SOUND_PLAY_RESERVATION_CHECK_FAILED',error=str(error)));raise
