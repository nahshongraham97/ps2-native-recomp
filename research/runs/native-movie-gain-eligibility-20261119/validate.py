"""Native PCM, ordinary eligibility and gain-tail checks; no device/game/model."""
import ctypes,datetime,hashlib,importlib.util,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
NAMES=['runtime/include/mka/movie_playback.hpp','runtime/src/native/movie_playback.cpp',
    'runtime/include/mka/movie_audio_submission.hpp','runtime/include/mka/movie_audio_progress.hpp',
    'runtime/src/native/movie_audio_progress.cpp','tests/test_movie_playback.cpp',
    'runtime/include/mka/movie_frame_eligibility.hpp','runtime/src/native/movie_frame_eligibility.cpp',
    'tests/test_movie_frame_eligibility.cpp','runtime/include/mka/movie_eligibility_numeric.hpp',
    'runtime/src/native/movie_eligibility_numeric.cpp','runtime/include/mka/movie_pcm_mix.hpp',
    'runtime/src/native/movie_pcm_mix.cpp','runtime/src/native/generated/movie_pcm_mix_tables.inc',
    'tests/test_movie_pcm_mix.cpp','runtime/src/native/movie_gain.cpp','runtime/src/native/movie_control.cpp',
    'runtime/src/native/movie_color.cpp','runtime/src/native/movie_gpu.cpp',
    'CMakeLists.txt','cmake/native_media.cmake']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def req(ok,message):
    if not ok:raise RuntimeError(message)
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(name,x):
    with (OUT/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(x,f,indent=2);f.write('\n')
def run(name,args):
    args=list(map(str,args));before=sha(args[0]);r=subprocess.run(args,cwd=FORK,env=ENV,
        capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    x=dict(argv=args,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
        executable_sha256=before,executable_after_sha256=sha(args[0]))
    write(name,x);req(r.returncode==0 and sha(args[0])==before,name+' failed');return x
def symbols(name,target,expected,required,executed):
    before=sha(target);record=run(name,[BIN/'llvm-nm.exe','-C','--defined-only',target])
    forbidden=[s for s in record['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b',s)]
    req(not forbidden and all(s in record['stdout'] for s in required),'native symbol definitions')
    req(before==sha(target)==expected,'exact symbol target')
    x=dict(target=str(target),target_before_sha256=before,target_after_sha256=sha(target),
        audited_target_expected_sha256=expected,execution_applicable=executed,
        tool_sha256=record['executable_sha256'],forbidden=forbidden,required_defined_symbols=required)
    if executed:x['executed_target_sha256']=expected
    write(name.replace('.json','-target.json'),x);return x
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3);OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    req(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)),'no generated game')
    req(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'Release')
    movie=PARENT/'research/inputs/modules/MOVIER.ovl'
    cri=PARENT/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/modules/cri_adxi.irx'
    generator=PARENT/'tools/research/generate_movie_pcm_mix_tables.py'
    checker=PARENT/'tools/research/verify_movie_pcm_mix_trace.py'
    paths=[*[FORK/n for n in NAMES],movie,cri,generator,checker,pathlib.Path(__file__).resolve()]
    before={str(p):sha(p) for p in paths}
    for p in paths:
        if p.suffix in ['.cpp','.hpp','.py','.inc']:req(b'\r\r\n' not in p.read_bytes(),'no doubled-CR source '+str(p))
    for n,h in PRESERVE.items():req(sha(FORK/n)==h,'preserve '+n)
    # Exact in-memory generator replay. Never rewrites its original include.
    spec=importlib.util.spec_from_file_location('_pcm_tables',generator)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    generated=module.generate(movie.read_bytes(),cri.read_bytes())
    req(generated==(FORK/'runtime/src/native/generated/movie_pcm_mix_tables.inc').read_bytes(),'exact table generator reproduction')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=before,preserved_dirty_sha256=PRESERVE,
        compiler_path=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),
        cache_before_sha256=sha(BUILD/'CMakeCache.txt'),generated_include_sha256=hashlib.sha256(generated).hexdigest(),
        scope='Explicit stateless PCM/ordinary clock policy/gain-tail owner; fake endpoints only; no game/device/local model'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_frame_eligibility',
        'test_native_movie_playback','test_native_movie_pcm_mix','mka_native_media','-j2'])
    ordinary=run('eligibility.json',[BUILD/'test_native_movie_frame_eligibility.exe'])
    req(ordinary['stdout'].strip()=='PASS pure ordinary frame eligibility; no clock/target producer or player binding','ordinary assertions complete')
    stream=run('playback.json',[BUILD/'test_native_movie_playback.exe',movie])
    req(stream['stdout'].strip()=='PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding','gain/reentry assertions complete')
    pcm=run('pcm.json',[BUILD/'test_native_movie_pcm_mix.exe','--dump-tables'])
    run('pcm-oracle-execution.json',[sys.executable,'-O',checker,OUT/'pcm.json',OUT/'pcm-oracle.json'])
    oracle=json.loads((OUT/'pcm-oracle.json').read_bytes())
    req(oracle['executable_sha256']==pcm['executable_sha256'],'actual PCM checker target')
    req([oracle[k] for k in ['gain_rows','pan_rows','pan_grid_rows','sample_grid_rows']]==[1000,31,7967,2816],'complete PCM result plan')
    audits=[]
    for label,record,target,required in [
        ('eligibility-symbols.json',ordinary,'test_native_movie_frame_eligibility.exe',['mka::video::NativeMovieFrameEligibility::']),
        ('playback-symbols.json',stream,'test_native_movie_playback.exe',['mka::video::NativeMovieStreamPlayback::']),
        ('pcm-symbols.json',pcm,'test_native_movie_pcm_mix.exe',['mka::video::mix_movie_pcm('])]:
        audits.append(symbols(label,BUILD/target,record['executable_sha256'],required,True))
    archive=BUILD/'libmka_native_media.a'
    audits.append(symbols('archive-symbols.json',archive,sha(archive),
        ['mka::video::NativeMovieFrameEligibility::','mka::video::mix_movie_pcm(',
         'mka::video::NativeMovieStreamPlayback::'],False))
    req(before=={str(p):sha(p) for p in paths},'source unchanged during verification')
    for n,h in PRESERVE.items():req(sha(FORK/n)==h,'preserve after '+n)
    write('verification.json',dict(status='PASS_SCOPED_NATIVE_MOVIE_GAIN_PCM_AND_ORDINARY_ELIGIBILITY',
        source_after_sha256=before,eligibility_fixture_sha256=ordinary['executable_sha256'],
        playback_fixture_sha256=stream['executable_sha256'],pcm_fixture_sha256=pcm['executable_sha256'],
        native_archive_sha256=sha(archive),pcm_oracle=oracle,symbol_audits=audits,
        cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['Conditional generic stateless DSP/ordinary policy and fake-owner checks only',
                'No initial profile/speaker/mode, real audio, selected target/clock, metadata/end, timing or full movie proof',
                'No runtime device, game, driver/settings or owner local-model access']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_MOVIE_GAIN_PCM_AND_ORDINARY_ELIGIBILITY',
        eligibility_fixture_sha256=ordinary['executable_sha256'],playback_fixture_sha256=stream['executable_sha256'],
        pcm_fixture_sha256=pcm['executable_sha256'],pcm_plan_rows=11814,negative_checks=oracle['negative_checks'])))
