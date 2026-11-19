"""Actual native source/audio adapters; read-only media and positive-zero PCM."""
import ctypes,datetime,hashlib,json,os,pathlib,re,subprocess
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
LIBS=WORKSPACE/'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
ASSETS=FORK/'games/mkarmageddon/disc-files/MOVIEPS2'
CATALOG=FORK/'research/runs/native-movie-source-20261119/validation-01/assets.tsv'
NAMES=['runtime/include/mka/movie_playback.hpp','runtime/src/native/movie_playback_adapters.cpp',
    'runtime/include/mka/audio_device.hpp','runtime/src/native/audio_device.cpp',
    'runtime/include/mka/movie_source.hpp','runtime/src/native/movie_source.cpp',
    'runtime/include/mka/movie_decode.hpp','runtime/src/native/movie_decode.cpp',
    'runtime/src/native/movie_assets.cpp','runtime/src/native/native_file.cpp',
    'runtime/src/native/movie_catalog.cpp','runtime/src/native/movie_pes_index.cpp',
    'runtime/src/native/mpeg2_timing.cpp','tests/test_movie_playback_adapters.cpp',
    'tests/test_native_audio_device.cpp','tests/native_test_sha256.hpp',
    'cmake/native_media.cmake','cmake/native_audio.cmake','CMakeLists.txt']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def require(ok,message):
    if not ok:raise RuntimeError(message)
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(name,x):
    with (OUT/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(x,f,indent=2);f.write('\n')
def run(name,args):
    args=list(map(str,args));before=sha(args[0])
    r=subprocess.run(args,cwd=FORK,env=ENV,capture_output=True,text=True,
        creationflags=subprocess.CREATE_NO_WINDOW)
    x=dict(argv=args,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
        executable_sha256=before,executable_after_sha256=sha(args[0]))
    write(name,x);require(r.returncode==0 and sha(args[0])==before,name+' failed');return x
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3);OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)),'no generated game')
    require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'Release')
    require(sha(ASSETS/'LOGON.SFD')=='cf49fb014d51f20fee7727a01e845cc7303be63fe0af2902fb3eadf371a843af','exact LOGON')
    paths=[*[FORK/n for n in NAMES],CATALOG,ASSETS/'LOGON.SFD',
        *[LIBS/n for n in ['avformat-61.dll','avcodec-61.dll','avutil-59.dll']],pathlib.Path(__file__).resolve()]
    before={str(p):sha(p) for p in paths}
    for n,h in PRESERVE.items():require(sha(FORK/n)==h,'preserve '+n)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=before,preserved_dirty_sha256=PRESERVE,
        compiler_path=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),
        cache_before_sha256=sha(BUILD/'CMakeCache.txt'),
        scope='Actual native adapters; two LOGON source owners first audio/video only; positive-zero WASAPI; no game/GPU/audible media/settings'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_playback_adapters','test_native_audio_device','-j2'])
    audio=BUILD/'test_native_audio_device.exe'
    inspect=run('inspect.json',[audio,'--inspect','console'])
    inventory=json.loads(inspect['stdout']);require(inventory['mode']=='inspect_only_no_stream','fresh read-only discovery')
    require(inventory['default_failure']['kind']==0,'actual console endpoint exists')
    chosen=[e for e in inventory['active_render_endpoints'] if e['default_for_role'] and e['endpoint_id_utf8']==inventory['default_endpoint_id_utf8']]
    require(len(chosen)==1,'exact observed role endpoint')
    selected=chosen[0]
    require(selected['mix_failure']['kind']==0 and selected['encoding']=='ieee_float' and
        selected['container_bits']==32 and selected['valid_bits']==32 and selected['sample_rate']>0 and
        selected['channels']>0 and selected['channel_mask']>0,'actual float32 endpoint layout')
    write('selected-host-metadata.json',dict(scope='Observed console default for deliberate zero-only component; not persistent game configuration',endpoint=selected))
    args=[selected['endpoint_id_utf8'],selected['sample_rate'],selected['channels'],selected['channel_mask'],1000000,'exact']
    silent=run('audio-format-silent.json',[audio,'--silent',*args])
    require('PASS opt-in silent-only WASAPI lifecycle;' in silent['stdout'],'same-open getter lifecycle fixture completion')
    fixture=BUILD/'test_native_movie_playback_adapters.exe'
    execution=run('execution.json',[fixture,'--silent',LIBS,ASSETS,CATALOG,*args])
    lines=execution['stdout'].splitlines()
    require(len(lines)==3 and lines[-1]=='PASS actual native source and zero-only audio adapters; no movie playback or retail policy binding','exact adapter completion')
    source=json.loads(lines[0]);observed=json.loads(lines[1])
    require(source['source']=='LOGON.SFD' and source['scope']=='first_video_and_audio_only_two_source_owners','bounded source scope')
    require(observed['audio']=='actual_owned_adapter_positive_zero_only' and
        [observed['rate'],observed['channels'],observed['mask']]==[selected['sample_rate'],selected['channels'],selected['channel_mask']], 'actual client format join')
    require(observed['first']==dict(first=0,frames=observed['capacity_frames'],epoch=0) and
        observed['after_reset']==dict(first=0,frames=1,epoch=1),'observed receipt/reset literals')
    target_before=sha(fixture)
    symbols=run('symbols.json',[BIN/'llvm-nm.exe','-C','--defined-only',fixture])
    forbidden=[s for s in symbols['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b',s)]
    required=['mka::video::own_movie_playback_source(', 'mka::video::own_movie_playback_audio(', 'mka::sound::NativeAudioDevice::stream_format(']
    require(not forbidden and all(s in symbols['stdout'] for s in required),'native adapter symbol scope')
    require(target_before==sha(fixture)==execution['executable_sha256'],'executed/audited target join')
    write('symbol-target.json',dict(target=str(fixture),target_before_sha256=target_before,target_after_sha256=sha(fixture),
        executed_target_sha256=execution['executable_sha256'],execution_applicable=True,
        tool_sha256=symbols['executable_sha256'],forbidden=forbidden,required_defined_symbols=required))
    require(before=={str(p):sha(p) for p in paths},'source/media/library changed during run')
    for n,h in PRESERVE.items():require(sha(FORK/n)==h,'preserve after '+n)
    write('verification.json',dict(status='PASS_SCOPED_ACTUAL_NATIVE_MOVIE_ADAPTERS_ZERO_ONLY',source_after_sha256=before,
        fixture_sha256=execution['executable_sha256'],execution_receipt_sha256=sha(OUT/'execution.json'),
        source_observation=source,audio_observation=observed,forbidden=forbidden,
        cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['First LOGON video/audio ownership forwarding only, not independent decoder/full-media parity',
                'Positive-zero PCM only; actual decoded media never queued to endpoint',
                'No GPU, complete movie, retail policies, A/V sync, acoustic completion or game acceptance']))
    print(json.dumps(dict(status='PASS_SCOPED_ACTUAL_NATIVE_MOVIE_ADAPTERS_ZERO_ONLY',fixture_sha256=execution['executable_sha256'],source=source,audio=observed)))
