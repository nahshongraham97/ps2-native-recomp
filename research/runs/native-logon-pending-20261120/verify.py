"""Actual AOT LOGON pending/retirement integration; deterministic endpoints only."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, subprocess, sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2]
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_pending_logon_evidence',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
CHANGED=['cmake/native_game.cmake','recomp/native_closure_codegen.cpp',
 'runtime/include/mka/frontend_scheduler.hpp','runtime/src/native/frontend_scheduler.cpp',
 'runtime/include/mka/logon_frontend_task.hpp','runtime/src/native/logon_frontend_task.cpp',
 'tests/test_native_closure_codegen.cpp','tests/test_native_logon_runtime.cpp','tests/test_logon_frontend_task.cpp']
NEW=['runtime/include/mka/logon_surround.hpp','tests/logon_surround_probe.hpp','tests/test_logon_pending.cpp']

def verify():
    require,sha,write=common.require,common.sha,common.write
    cache=common.BUILD/'CMakeCache.txt';text=cache.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$',text,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M)), 'native-only Release cache')
    prior_path=FORK/'research/runs/native-movie-gpu-drain-20261120/validation-01/verification.json'
    prior=json.loads(prior_path.read_bytes())
    require(prior['status']=='PASS_NATIVE_MOVIE_GPU_COMPLETION_CPU_AND_WGL_BUILD' and isinstance(prior['source_after_sha256'],dict),'actual prior schema')
    changed={str(FORK/name) for name in CHANGED}
    untouched={name:digest for name,digest in prior['source_after_sha256'].items() if name not in changed}
    require(all(sha(name)==digest for name,digest in untouched.items()),'all untouched prior pins')
    paths={pathlib.Path(name) for name in prior['source_after_sha256']}
    paths.update(FORK/name for name in CHANGED+NEW)
    paths.update([COMMON,pathlib.Path(__file__).resolve(),prior_path])
    before={str(path):sha(path) for path in sorted(paths)}
    for name in CHANGED+NEW:require(b'\r' not in (FORK/name).read_bytes(),'authored LF '+name)
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved dirty IOP')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
      fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
      parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
      source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(common.BIN/'clang++.exe'),
      scope='AOT LOGON and actual native task/scheduler; explicit surround probes and deterministic output ports, no physical endpoint'))
    libraries=PARENT.parent/'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    assets=FORK/'games/mkarmageddon/disc-files/MOVIEPS2';overlay=PARENT/'research/inputs/modules/MOVIER.ovl'
    cases=[
      ('emitter','test_native_closure_codegen',[PARENT/'research/inputs/SLUS_214.10'],
       'PASS strict typed LOGON source recipe and owned native emission; runtime chain not executed',[]),
      ('scheduler','test_native_frontend_scheduler',[],
       'PASS native frontend owned queue/live ordering/blocked handoff/lifetime; no game scheduler bootstrap or hardware tick',
       ['mka::game::NativeFrontendScheduler::dispatch']),
      ('pending','test_native_logon_pending',[],
       'PASS pending native frontend retirement/no-allocation cancel/exact retry/shutdown; CPU scheduler only, no movie decode or endpoint',
       ['mka::game::NativeFrontendScheduler::poll_retirements','mka::game::NativeFrontendScheduler::retry_retirement']),
      ('caller','test_native_logon_runtime',[libraries,assets,'LOGON.SFD',overlay],
       'PASS generated native LOGON actual Service/input-task suspension/restore/identity; deterministic ports not full game effects',
       ['mka::native_logon::NativeLogonState::retry_cleanup','mka::native_logon::NativeLogonState::poll']),
      ('frontend','test_native_logon_frontend',[libraries,assets,'LOGON.SFD',overlay],
       'PASS native queued LOGON with owned geometry/hierarchy/material tasks; deterministic ports and explicit world/frontend probes',
       ['mka::native_logon::NativeLogonFrontendTask::retire','mka::game::NativeFrontendScheduler::dispatch'])]
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',*[x[1] for x in cases],'-j2'])
    fixtures=[]
    for label,target,args,footer,symbols in cases:
        print('RUN '+label,flush=True)
        exe=common.BUILD/(target+'.exe');run=common.execute(label+'.json',[exe,*args])
        require(run['stdout'].splitlines()[-1]==footer,'complete fixture '+label)
        # Emitter is an OFFLINE compiler/source checker, never candidate runtime.
        if label=='emitter':audit=dict(scope='offline source compiler only',target_sha256=sha(exe))
        else:audit=common.symbols(label+'-symbols',exe,run['executable_sha256'],symbols,True)
        fixtures.append(dict(component=label,executed_target_sha256=run['executable_sha256'],audit=audit))
    require(before=={name:sha(name) for name in before},'stable source/tools/asset/evidence pins')
    frozen=FORK/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    require(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'decoder stays frozen')
    generated=list(common.BUILD.glob('native-retail-logon-*/native_logon.cpp'))
    current=[x for x in generated if x.parent.name=='native-retail-logon-'+sha(FORK/'recomp/native_closure_codegen.cpp')]
    require(len(current)==1,'exact source-keyed actual AOT output')
    generated_pins={str(x):sha(x) for x in [current[0],current[0].with_suffix('.hpp')]}
    ninja=common.BUILD/'build.ninja';rules=[];text=ninja.read_text()
    for _,target,_,_,_ in cases[1:]:
        match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
        require(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native target links')
        rules.append(dict(target=target,actual_build_rule=match.group()))
    write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules,generated_source_sha256=generated_pins))
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after execution')
    write('verification.json',dict(status='PASS_NATIVE_LOGON_PENDING_AND_RETAINED_TASK_CLEANUP',
      source_after_sha256=before,unchanged_prior_pins=len(untouched),generated_source_sha256=generated_pins,fixtures=fixtures,
      frozen_decoder_certificate_sha256=sha(frozen),
      limits=['Required native surround work remains an explicit dependency; fixture probes do not implement source315680 or whole renderer handoff',
       'Only affected actual AOT/task/scheduler scenarios, not full-game or hardware acceptance',
       'No decoder-only/unchanged numeric corpus rerun; no game/model/server/driver/physical endpoint action']))
    print(json.dumps(dict(status='PASS_NATIVE_LOGON_PENDING_AND_RETAINED_TASK_CLEANUP',fixtures=len(fixtures),pins=len(before))),flush=True)

if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
    common.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];common.OUT.mkdir(exist_ok=False)
    common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    try:verify()
    except Exception as error:common.write('aborted.json',dict(status='NATIVE_LOGON_PENDING_CHECK_FAILED',error=str(error)));raise
