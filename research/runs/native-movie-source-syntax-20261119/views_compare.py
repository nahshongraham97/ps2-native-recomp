"""Compare immutable measured native receipts with the view-based source oracle."""
import ctypes,datetime,importlib.util,json,os,pathlib,re,sys
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
ROOT=pathlib.Path(__file__).resolve().parent
BASE=ROOT/'validation-02'
OUT=ROOT/'views-comparison-01'
EVIDENCE=PARENT/'research/runs/native-movie-ingress-20261119'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_views_receipts',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
ASSETS=FORK/'games/mkarmageddon/disc-files/MOVIEPS2'

if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT=OUT;common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    require,sha,write=common.require,common.sha,common.write
    review=EVIDENCE/'MOVIE_NATIVE_SOURCE_SYNTAX_ORACLE_VIEWS_SOURCE_REVIEW.json'
    require(sha(review)=='60de31b292204c315164b8c35a8af1fb4f3ab7719a471a75d548dcac5fc31ba1','exact reviewed two-line change/fixture')
    for name,digest in json.loads(review.read_bytes())['pins'].items():require(sha(name)==digest,'exact view source review')
    original=json.loads((BASE/'provenance.json').read_bytes())
    before=original['source_before_sha256'].copy()
    require(before=={name:sha(name) for name in before},'same sources/DLLs/assets as measured native fixtures')
    for path in [pathlib.Path(__file__).resolve(),review,BASE/'provenance.json',
        PARENT/'tools/research/verify_movie_source_syntax_index_views.py',PARENT/'tools/research/test_movie_header_oracle_views.py']:
        before[str(path)]=sha(path)
    require(sha(FORK/'research/runs/native-movie-source-syntax-20261119/validate.py')==
        '38fe54b87fc969583a82ea1e6f4333a5dea30bae1568b50c781752c60fbb3934','active runner remains unchanged')
    for name in ['syntax-oracle.json','syntax-oracle-execution.json','observation-oracle.json','observation-oracle-execution.json','wrapper-aborted.json']:
        before[str(BASE/name)]=sha(BASE/name)
    media_plan={'native_value': 13, 'program_end': 3, 'schema_or_absence': 4, 'missing_field': 18, 'receipt': 8, 'coverage_footer': 3, 'json': 3}
    for label in ['syntax','observation']:
        done=json.loads((BASE/(label+'-oracle-execution.json')).read_bytes())
        require(type(done['exit']) is int and done['exit']==0 and done['executable_sha256']==done['executable_after_sha256']==sha(done['argv'][0]),'completed independent original checker execution')
        expected=PARENT/'tools/research'/('verify_movie_source_syntax_index.py' if label=='syntax' else 'verify_movie_source_observations.py')
        native_receipt=BASE/('syntax-corpus.json' if label=='syntax' else 'observation-regression.json')
        result_path=BASE/(label+'-oracle.json')
        tail=[ASSETS,native_receipt,result_path] if label=='syntax' else [native_receipt,ASSETS,result_path]
        require(done['argv']==[str(sys.executable),'-O',str(expected),*map(str,tail)],'exact completed original checker command')
        completed=json.loads(result_path.read_bytes());native=json.loads(native_receipt.read_bytes())
        require(completed['checker_sha256']==sha(expected),'actual completed checker result producer')
        status='PASS_ORIGINAL38_SOURCE_SYNTAX_AND_ATTACHMENT_MEMBERSHIP' if label=='syntax' else 'PASS_ORIGINAL_ASSET_NATIVE_SOURCE_OBSERVATIONS'
        require(completed['status']==status and completed['assets']==38,'exact completed original comparison status')
        receipt_field='receipt_sha256' if label=='syntax' else 'native_receipt_sha256'
        target_field='executable_sha256' if label=='syntax' else 'native_executable_sha256'
        require(completed[receipt_field]==sha(native_receipt) and
            completed[target_field]==native['executable_sha256']==native['executable_after_sha256']==sha(native['argv'][0]),'actual completed native receipt/target joins')
        before[str(pathlib.Path(done['argv'][0]))]=sha(done['argv'][0])
    fixtures=[]
    for label in ['syntax-unit','syntax-corpus','observation-regression','source-failure','owned-decoder','retail-source-factory']:
        path=BASE/(label+'.json');d=json.loads(path.read_bytes())
        require(type(d['exit']) is int and d['exit']==0 and d['executable_sha256']==d['executable_after_sha256']==sha(d['argv'][0]),'same passed native fixture target')
        audit=BASE/(label+'-symbols-target.json');a=json.loads(audit.read_bytes())
        require(a['execution_applicable'] is True and not a['forbidden'] and a['audited_target_expected_sha256']==a['executed_target_sha256']==a['target_before_sha256']==a['target_after_sha256']==d['executable_sha256'],'same executed-target source audit')
        fixtures.append(dict(component=label,receipt=str(path),receipt_sha256=sha(path),executed_target_sha256=d['executable_sha256'],audit=str(audit),audit_sha256=sha(audit)))
        tool_receipt=BASE/(label+'-symbols.json');nm=json.loads(tool_receipt.read_bytes())
        require(type(nm['exit']) is int and nm['exit']==0 and
            nm['argv']==[str(common.BIN/'llvm-nm.exe'),'-C','--defined-only',d['argv'][0]] and
            nm['executable_sha256']==nm['executable_after_sha256']==a['tool_sha256']==sha(nm['argv'][0]),'saved nm tool/target execution joins')
        forbidden=[line for line in nm['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b',line)]
        require(not forbidden and all(symbol in nm['stdout'] for symbol in a['required_defined_symbols']),'actual saved symbol output guards')
        require(a['target']==d['argv'][0],'actual audit target path')
        for item in [path,audit,pathlib.Path(d['argv'][0]),tool_receipt,pathlib.Path(nm['argv'][0])]:before[str(item)]=sha(item)
    build_receipt=BASE/'build.json';build=json.loads(build_receipt.read_bytes())
    require(type(build['exit']) is int and build['exit']==0 and build['argv'][0]==str(common.CMAKE) and
        build['executable_sha256']==build['executable_after_sha256']==sha(common.CMAKE),'saved build tool execution identity')
    for item in [build_receipt,common.CMAKE]:before[str(item)]=sha(item)
    cache=common.BUILD/'CMakeCache.txt';text=cache.read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$',text,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M),'current native-only Release')
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved dirty IOP')
    OUT.mkdir(exist_ok=False)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_before_sha256=before,
        measured_native_provenance_sha256=sha(BASE/'provenance.json'),fixtures=fixtures,
        scope='Reuse6 unchanged measured CPU/file fixtures; no rebuild/native reexecution; new view oracle only'))
    print('RUN bytes/view focused equivalence',flush=True)
    equiv=common.execute('view-equivalence.json',[sys.executable,'-O',PARENT/'tools/research/test_movie_header_oracle_views.py',ASSETS])
    same=json.loads(equiv['stdout']);require(same['status']=='PASS_FROZEN_HEADER_BYTES_VIEW_EQUIVALENCE' and same['original_unit_variants']==8 and same['synthetic_cases']==3,'full focused equivalence plan')
    print('RUN optimized original38 source-syntax comparison',flush=True)
    common.execute('syntax-oracle-execution.json',[sys.executable,'-O',PARENT/'tools/research/verify_movie_source_syntax_index_views.py',ASSETS,BASE/'syntax-corpus.json',OUT/'syntax-oracle.json'])
    syntax=json.loads((OUT/'syntax-oracle.json').read_bytes())
    require(syntax['status']=='PASS_ORIGINAL38_SOURCE_SYNTAX_AND_ATTACHMENT_MEMBERSHIP' and syntax['assets']==38 and syntax['pictures']==65181 and syntax['cross_pes_prefixes']==90 and syntax['absent_pts']==18 and syntax['negative_checks']==39,'exact original38 source comparison')
    original_syntax=json.loads((BASE/'syntax-oracle.json').read_bytes())
    for field in ['expected_rows_sha256','expected_attachment_memberships_sha256','pictures','cross_pes_prefixes','absent_pts','negative_checks']:
        require(syntax[field]==original_syntax[field],'complete original38 bytes/view source record equivalence')
    observation=json.loads((BASE/'observation-oracle.json').read_bytes())
    groups=observation['negative_checks']
    require(type(groups) is dict and groups.keys()==media_plan.keys() and all(type(n) is int for n in groups.values()) and groups==media_plan,'exact categorized original media negative plan')
    require(observation['status']=='PASS_ORIGINAL_ASSET_NATIVE_SOURCE_OBSERVATIONS' and observation['assets']==38 and observation['decoded_video_pictures']==65181,'exact passed original media/source regression')
    require(observation['native_receipt_sha256']==sha(BASE/'observation-regression.json') and syntax['receipt_sha256']==sha(BASE/'syntax-corpus.json'),'exact compared native receipt identities')
    write('original-comparison-reuse.json',dict(status='PASS_COMPLETED_ORIGINAL_CHECKERS_REUSED_WITH_EXACT_VIEWS_EQUIVALENCE',
        original_syntax_sha256=sha(BASE/'syntax-oracle.json'),media_source_sha256=sha(BASE/'observation-oracle.json'),media_negative_groups=groups,
        syntax_negative_checks=syntax['negative_checks'],scope='No original checker rerun or native reexecution; original38 fields/record hashes exactly match view implementation'))
    archive=common.BUILD/'libmka_native_media.a';audit=common.symbols('archive-symbols',archive,sha(archive),['mka::video::NativeMovieSourceSyntaxIndex::','mka::video::NativeMovieDecoder::'],False)
    require(before=={name:sha(name) for name in before},'sources/fixtures/binaries/inputs stable through new comparison')
    final=cache.read_text(encoding='utf-8-sig');require(re.search(r'^PS2_GENERATED:PATH=$',final,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',final,re.M),'final native-only Release')
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after comparison')
    write('verification.json',dict(status='PASS_SCOPED_NATIVE_SOURCE_SYNTAX_VIEW_ORACLE_ORIGINAL38',source_after_sha256=before,fixtures=fixtures,
        native_archive_sha256=sha(archive),archive_audit=audit,cache_after_sha256=sha(cache),
        limits=['Six measured native fixtures reused by exact receipts/currenttarget/audit joins; no reexecution or rebuild',
        'Readonly buffer-view original38 result equivalence; both original v1/checkers passed; old wrapper failed only on result-schema aggregation',
        'Native source-format records/decoded attachment membership, native PCM and3 YUV goldens; no SDK admission/currentwork/clock/player/game claim',
        'No game/context/audioendpoint/localmodel/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_SOURCE_SYNTAX_VIEW_ORACLE_ORIGINAL38',source_pins=len(before),native_fixtures_reused=6,negative_checks=91)),flush=True)
