"""Source-format index joined to actual decoded pictures; CPU/file fixtures only."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys
FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_syntax_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)
OUT = pathlib.Path(__file__).resolve().parent/'validation-02'
ASSETS = FORK/'games/mkarmageddon/disc-files/MOVIEPS2'
LIBS = PARENT.parent/'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
EVIDENCE = PARENT/'research/runs/native-movie-ingress-20261119'

def verify_reviews():
    """Actual frozen review APIs only; read-only and safe before build/output."""
    reviews = {'MOVIE_NATIVE_SOURCE_SYNTAX_INDEX_SOURCE_REVIEW.json': ('SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_EXECUTED', 'a002a52ea265e3663dded296470b13add2421f4b4c9aafa6a631837b30132a82', 'pins'), 'MOVIE_NATIVE_SOURCE_SYNTAX_DECODER_SOURCE_REVIEW.json': ('SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_BUILT', 'bcbfab496f8db5139b29ee9091a363e3f429539e45ae472fdfcb27a2e1e64028', 'current_sha256'), 'MOVIE_NATIVE_SOURCE_SYNTAX_CORPUS_SOURCE_REVIEW.json': ('SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_EXECUTED', 'd1a0f164d890ac249f270974275a9f5d8c24ba101613fcd339356ce951de6db6', 'pins'), 'MOVIE_NATIVE_SOURCE_SYNTAX_INDEX_ORACLE_SOURCE_REVIEW.json': ('SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_EXECUTED', 'df463aefb49f064f9d1811faac204d2a3de2d5288f10b09a76463e919081c0b8', 'pins')}
    for name,(status,digest,pin_field) in reviews.items():
        common.require(common.sha(EVIDENCE/name)==digest,'exact frozen peer review identity')
        review=json.loads((EVIDENCE/name).read_bytes())
        common.require(review['status']==status,'exact peer source-review scope/status')
        pins=review[pin_field]
        common.require(type(pins) is dict and bool(pins),'explicit nonempty reviewed source map')
        for path,digest in pins.items():common.require(common.sha(path)==digest,'reviewed source identity '+path)
    return reviews


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    reviews = verify_reviews()
    OUT.mkdir(parents=True, exist_ok=False)
    files = list((FORK/'runtime/include/mka').glob('*.hpp'))
    files += list((FORK/'runtime/src/native').rglob('*.cpp'))+list((FORK/'runtime/src/native').rglob('*.inc'))
    tests = ['tests/test_movie_source_syntax_index.cpp', 'tests/test_movie_source_syntax_corpus.cpp',
        'tests/movie_source_syntax_wire.hpp', 'tests/native_test_sha256.hpp',
        'tests/test_movie_source_observations.cpp', 'tests/test_movie_source_failure.cpp',
        'tests/test_movie_decode.cpp', 'tests/test_movie_source.cpp', 'tests/native_adx_test_observer.hpp',
        'tests/data/movie_native_adx_source_oracle.hpp', 'tests/data/movie_decode_oracle.hpp', 'cmake/native_audio.cmake']
    files += [FORK/x for x in common.NAMES+tests]
    files += [COMMON, pathlib.Path(__file__).resolve(), common.BIN/'clang++.exe']
    files += [EVIDENCE/x for x in reviews]
    files += [PARENT/'tools/research'/x for x in ['verify_movie_source_syntax_index.py',
        'verify_movie_header_units.py', 'verify_native_adx_audio_assets.py', 'verify_movie_source_observations.py']]
    require(sha(PARENT/'tools/research/verify_movie_source_observations.py') ==
        '38936f9e14603218e59e424a3b5b95cfbf6d1c1e6e0153b5e20b1331d3974c3c', 'immutable ingress regression checker')
    files += [LIBS/x for x in ['avutil-59.dll', 'avcodec-61.dll', 'avformat-61.dll']]
    saved = FORK/'research/runs/native-movie-source-20261119/validation-01/assets.tsv'
    rows = [x.split('\t') for x in saved.read_text().splitlines()]
    require(len(rows) == 38 and all(len(x) == 4 for x in rows), 'original FOUR-column factory manifest')
    catalog = OUT/'assets.tsv'
    with catalog.open('x',encoding='utf-8',newline='\n') as output:output.write(''.join(x[0]+'\t'+x[0]+'\n' for x in rows))
    files += [saved,catalog]+[ASSETS/x[0] for x in rows]
    files = list(dict.fromkeys(files)); before = {str(x):sha(x) for x in files}
    for path in files:
        if path.suffix in ['.cpp','.hpp','.py','.cmake','.inc']:require(b'\r\r' not in path.read_bytes(), 'no doubled CR')
    for name,digest in common.PRESERVE.items():require(sha(FORK/name) == digest, 'preserved dirty IOP')
    cache_path = common.BUILD/'CMakeCache.txt'; cache = cache_path.read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M), 'native-only Release')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_before_sha256=before,
        compiler_path=str(common.BIN/'clang++.exe'),compiler_sha256=sha(common.BIN/'clang++.exe'),
        cache_before_sha256=sha(cache_path),scope='Source syntax and actual decoded-picture binding only; no endpoint or SDK admission'))
    targets = ['test_native_movie_source_syntax_index','test_native_movie_source_syntax_corpus',
        'test_native_movie_source_observations','test_native_movie_source_failure','test_native_movie_decode','test_native_movie_source','mka_native_media']
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',*targets,'-j2'])
    runs=[]
    cases=[('syntax-unit',targets[0],[],
        'PASS immutable source syntax pictures; no SDK admission or selected-output association'),
        ('syntax-corpus',targets[1],[LIBS,ASSETS,catalog],
        'PASS native source syntax38 and exact decoded-picture attachments; no SDK admission or playback'),
        ('observation-regression',targets[2],[LIBS,ASSETS,catalog],
        'PASS native source observations38 raw prefixes/output/program-end; no header admission or timeline binding'),
        ('source-failure',targets[3],[LIBS,ASSETS/'LOGON.SFD'],
        'PASS native decoder both-open source rejection and terminal truncation; no output/race-authentication claim'),
        ('owned-decoder',targets[4],[LIBS,ASSETS,OUT/'decode-owned','--catalog'],
        'PASS native filesystem/MPEG2/ADX decoder ownership and actual media output; no game/A-V-sync/presentation/fidelity claim'),
        ('retail-source-factory',targets[5],[LIBS,ASSETS,saved,OUT/'source-owned'],None)]
    for label,target,args,footer in cases:
        print('RUN '+label,flush=True)
        result=common.execute(label+'.json',[common.BUILD/(target+'.exe'),*args])
        if footer:require(result['stdout'].splitlines()[-1] == footer,'complete fixture footer')
        else:
            summary=json.loads(result['stdout'].splitlines()[-1]);require(summary['kind']=='complete' and summary['assets']==38 and summary['full_goldens']==3 and summary['first_video_only']==35 and summary['missing_requests']==4,'complete retail source factory')
        required=['mka::video::NativeMovieSourceSyntaxIndex::']
        if label!='syntax-unit':required+=['mka::video::NativeMovieDecoder::']
        audit=common.symbols(label+'-symbols',common.BUILD/(target+'.exe'),result['executable_sha256'],required,True)
        runs.append(dict(component=label,executed_target_sha256=result['executable_sha256'],audit=audit))
    print('RUN independent original38 source-syntax comparison',flush=True)
    common.execute('syntax-oracle-execution.json',[sys.executable,'-O',PARENT/'tools/research/verify_movie_source_syntax_index.py',ASSETS,OUT/'syntax-corpus.json',OUT/'syntax-oracle.json'])
    syntax=json.loads((OUT/'syntax-oracle.json').read_bytes())
    require(syntax['status']=='PASS_ORIGINAL38_SOURCE_SYNTAX_AND_ATTACHMENT_MEMBERSHIP' and syntax['assets']==38 and syntax['pictures']==65181 and syntax['cross_pes_prefixes']==90 and syntax['absent_pts']==18 and syntax['negative_checks']==39,'complete source-syntax comparison')
    print('RUN independent unchanged video/audio/source comparison',flush=True)
    common.execute('observation-oracle-execution.json',[sys.executable,'-O',PARENT/'tools/research/verify_movie_source_observations.py',OUT/'observation-regression.json',ASSETS,OUT/'observation-oracle.json'])
    observed=json.loads((OUT/'observation-oracle.json').read_bytes())
    require(observed['status']=='PASS_ORIGINAL_ASSET_NATIVE_SOURCE_OBSERVATIONS' and observed['assets']==38 and observed['decoded_video_pictures']==65181 and observed['negative_checks']==52,'complete original ingress/media comparison')
    archive=common.BUILD/'libmka_native_media.a'
    audit=common.symbols('archive-symbols',archive,sha(archive),['mka::video::NativeMovieSourceSyntaxIndex::','mka::video::NativeMovieDecoder::'],False)
    final_cache=cache_path.read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$',final_cache,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',final_cache,re.M), 'final native-only Release cache')
    require(before == {name:sha(name) for name in before}, 'stable source/DLL/asset/review identities through verification')
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved dirty IOP after verification')
    write('verification.json',dict(status='PASS_SCOPED_NATIVE_SOURCE_SYNTAX_ACTUAL_DECODER_ORIGINAL38',source_after_sha256=before,
        fixtures=runs,native_archive_sha256=sha(archive),archive_audit=audit,cache_after_sha256=sha(cache_path),
        limits=['Source-format records, exact decoded-picture attachment membership, native ADX and three YUV goldens only',
        'ID3 quant/field/composite paths are synthetic only; no original38 ID3',
        'No SDK current admission/selected work-record/default userdata callback/clock or complete player/game acceptance',
        'Archive compiled/inspected only; no local model/server/game/context/audio endpoint/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_SOURCE_SYNTAX_ACTUAL_DECODER_ORIGINAL38',source_pins=len(before),fixtures=len(runs))),flush=True)
