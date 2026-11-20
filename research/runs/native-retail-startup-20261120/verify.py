"""Source-produced retail startup/native files; never launch the game or devices."""
import ctypes, datetime, hashlib, importlib.util, json, os, pathlib, re, subprocess, sys

F=pathlib.Path(__file__).resolve().parents[3]
P=F.parents[2]
R=P/'research/runs/native-retail-startup-20261120'
spec=importlib.util.spec_from_file_location('_startup_evidence',F/'research/runs/native-movie-kernel-policies-20261119/validate.py')
c=importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need,sha,write=c.require,c.sha,c.write
    cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and
        bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'native Release empty generated input')
    prior_path=F/'research/runs/native-runtime-work-20261120/validation-01/verification.json'
    prior=json.loads(prior_path.read_bytes())
    need(prior['status']=='PASS_NATIVE_PRIVATE_RUNTIME_WORK_AND_SERVICE_RETIREMENT'
        and len(prior['source_after_sha256'])==567,'actual prior schema')
    snapshot=R/'before'
    old=json.loads((snapshot/'provenance.json').read_bytes())
    need(len(old['source_sha256'])==7,'seven changed files with exact working snapshots')
    changed={str(F/n) for n in old['source_sha256']}
    untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched)==560 and all(sha(n)==h for n,h in untouched.items()),'560 unchanged prior inputs')
    review_path=R/'SOURCE_REVIEW.json'
    review=json.loads(review_path.read_bytes())
    need(review['status']=='SOURCE_REVIEW_NATIVE_RETAIL_STARTUP_AND_CONTENT_VERIFIED'
        and review['source_only'] is True and len(review['input_sha256'])==15,'actual source review schema')
    need(all(sha(n)==h for n,h in review['input_sha256'].items()),'reviewed current source bytes')
    paths={pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(pathlib.Path(n) for n in review['input_sha256'])
    paths.update([prior_path,pathlib.Path(__file__).resolve(),review_path,review_path.with_suffix('.md'),
        R/'PLAN.md',R/'IMPLEMENTATION.md',R/'TARGET_CORRECTION.md',R/'BUILD_CORRECTION.md',R/'DIRECTORY_ACCESS_CORRECTION.md',snapshot/'provenance.json',F/'tests/test_native_file.cpp',
        F/'tests/test_native_closure_codegen.cpp',F/'tests/test_content_volume.cpp'])
    need(sha(F/'tests/test_content_volume.cpp')=='2b1c42d6aa3e96fb14e241f5955155916bcc25edc95bf96bb921cb1b4b28b2ed',
        'existing sector fixture preserved, not confused with native owner')
    for name,digest in old['source_sha256'].items():
        need(sha(snapshot/name)==digest==prior['source_after_sha256'][str(F/name)],'exact prior bytes '+name)
        paths.add(snapshot/name)
    for stem,status in [('SOURCE','BOUNDED_RETAIL_STARTUP_SOURCE_ORDER_AND_OWNED_STATE_VERIFIED'),
                        ('SOURCE_RESOURCE_NAMES','BOUNDED_STARTUP_RESOURCE_NAME_ROUTES_VERIFIED')]:
        q=R/(stem+'.json');d=json.loads(q.read_bytes())
        need(d['status']==status and d['source_only'] is True and isinstance(d['input_sha256'],dict)
            and isinstance(d['exports'],list),'actual original source producer schema')
        paths.update([q,q.with_suffix('.md')])
        for name,digest in d['input_sha256'].items():
            path=pathlib.Path(name)
            if name in changed:path=snapshot/path.relative_to(F)
            need(sha(path)==digest,'current or exact historical source input')
            paths.add(path)
        for export in d['exports']:
            path=pathlib.Path(export['path']);need(sha(path)==export['sha256'],'primary raw export')
            paths.add(path)
    old_logon=c.BUILD/('native-retail-logon-'+sha(snapshot/'recomp/native_closure_codegen.cpp'))
    frozen_logon={name:sha(old_logon/name) for name in ['native_logon.hpp','native_logon.cpp']}
    paths.update(old_logon/name for name in frozen_logon)
    before={str(q):sha(q) for q in sorted(paths)}
    for name in review['input_sha256']:
        data=pathlib.Path(name).read_bytes()
        need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n')
            and all(line==line.rstrip(b' \t') for line in data.splitlines()),'authored LF/EOF '+name)
    for name,digest in c.PRESERVE.items():need(sha(F/name)==digest,'protected dirty IOP')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),
        source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),
        scope='Actual source-generated subordinate startup/native files; incomplete entry must not link'))
    original=P/'research/inputs/SLUS_214.10'
    cases=[
        ('volume','test_native_content_volume_owner',[c.OUT/'volume-owned'],
         'PASS_NATIVE_CONTENT_VOLUME_OWNER: opened identity/retail keys/containment/moves/root replacement/sparse64 files; no startup/schema claim',
         ['mka::content::NativeContentVolume::open_resource','mka::content::NativeFile::opened_regular_file']),
        ('startup-source','test_native_startup_codegen',[original],
         'PASS native retail startup source identity, ordered prerequisites and owned emission; no game boot',
         ['recomp::emit_native_startup_closure']),
        ('startup-runtime','test_native_retail_startup',[c.OUT/'startup-owned'],
         'PASS generated retail startup leaves use owned arguments and actual native volume; full entry link remains blocked',
         ['mka::retail::initialize_retail_resource_prefix','mka::retail::derive_retail_boot_directory','mka::content::NativeContentVolume::open_resource']),
        ('logon-emitter','test_native_closure_codegen',[original,c.OUT/'logon-owned'],
         'PASS strict typed LOGON source recipe and owned native emission; runtime chain not executed',
         ['recomp::emit_native_logon_closure']),
        ('native-file','test_native_file',[],'native file tests passed',['mka::content::NativeFile::open'])]
    c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[x[1] for x in cases],
        'mka_native_retail_startup','mka_native_logon','-j2'])
    fixtures=[]
    for label,target,args,footer,required in cases:
        print('RUN '+label,flush=True)
        run=c.execute(label+'.json',[c.BUILD/(target+'.exe'),*args])
        need(run['stdout'].splitlines()[-1]==footer,'complete assertions '+label)
        audit=c.symbols(label+'-symbols',c.BUILD/(target+'.exe'),run['executable_sha256'],required,True)
        fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
    need(all(sha(c.OUT/'logon-owned'/name)==digest for name,digest in frozen_logon.items()),
        'unchanged generated LOGON runtime bytes without any decoder rerun')
    native_logon=c.BUILD/('native-retail-logon-'+sha(F/'recomp/native_closure_codegen.cpp'))
    need(all(sha(native_logon/name)==digest for name,digest in frozen_logon.items()),'actual CMake generated LOGON unchanged')
    generated_key=hashlib.sha256((sha(F/'recomp/native_startup_codegen.cpp')+':'+
        sha(F/'recomp/native_closure_codegen.cpp')).encode()).hexdigest()
    current=c.BUILD/('native-retail-startup-'+generated_key)
    generated={str(current/name):sha(current/name) for name in ['native_retail_startup.hpp','native_retail_startup.cpp','native_retail_entry.cpp']}
    archive=c.BUILD/'libmka_native_retail_startup.a'
    audit=c.symbols('startup-archive',archive,sha(archive),['mka::retail::run_native_retail'],False)
    undefined=c.execute('startup-required-bindings.json',[c.BIN/'llvm-nm.exe','-u','-C',archive])
    required=['initialize_native_retail_crt','finalize_native_retail_pre_main','initialize_native_retail_platform','continue_native_retail_main']
    need(all('mka::retail::'+name in undefined['stdout'] for name in required),'four required native production bindings')
    # Expected failure is its own receipt, never mislabeled as a successful build.
    args=list(map(str,[c.CMAKE,'--build',c.BUILD,'--target','test_native_startup_missing_bindings','-j2']))
    result=subprocess.run(args,cwd=F,env=c.ENV,text=True,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
    write('missing-bindings-link.json',dict(argv=args,exit=result.returncode,stdout=result.stdout,stderr=result.stderr,
        expected_failure=True,required_native_bindings=required,tool_sha256=sha(c.CMAKE)))
    need(result.returncode!=0 and all(name in result.stdout+result.stderr for name in required),
        'incomplete native entry refused by linker for all exact obligations')
    ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
    for _,target,_,_,_ in cases:
        match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
        need(match is not None and all(x not in match.group() for x in ['ps2runtime','source_oracle','--wrap=']),
            'native-only actual linkage '+target)
        rules.append(dict(target=target,actual_build_rule=match.group()))
    write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules,archive=audit))
    need(before=={n:sha(n) for n in before},'source/evidence/tools stable during run')
    frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'offline decoder frozen')
    for name,digest in c.PRESERVE.items():need(sha(F/name)==digest,'preserved dirty IOP after verification')
    volume=json.loads((c.OUT/'volume.json').read_bytes())
    write('verification.json',dict(status='PASS_NATIVE_RETAIL_STARTUP_LEAVES_AND_OWNED_CONTENT',source_after_sha256=before,
        unchanged_prior_pins=len(untouched),fixtures=fixtures,generated_runtime_sha256=generated,
        unchanged_logon_sha256=frozen_logon,missing_production_bindings=required,
        volume_capability_notes=[line for line in volume['stdout'].splitlines() if line.startswith('SKIP')],
        frozen_decoder_certificate_sha256=sha(frozen),limits=[
            'Actual generated prefix/argv0 leaf and native file destination; full entry cannot link until four production obligations implemented',
            'General2209E0 resource producer, CRT/platform continuation, native input/render/audio/save and full-game acceptance remain OPEN',
            'Windows directory anchoring/handle containment; no unrestricted OS mutation or nonWindows containment proof',
            'No game launch, GPU/audio/input endpoint, driver/model/settings or user card/save/key operation; no movie corpus rerun']))
    print(json.dumps(dict(status='PASS_NATIVE_RETAIL_STARTUP_LEAVES_AND_OWNED_CONTENT',fixtures=len(fixtures),pins=len(before))),flush=True)


if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
    c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1]
    c.OUT.mkdir(exist_ok=False)
    c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
    try:verify()
    except Exception as e:c.write('aborted.json',dict(status='NATIVE_RETAIL_STARTUP_CHECK_FAILED',error=str(e)));raise
