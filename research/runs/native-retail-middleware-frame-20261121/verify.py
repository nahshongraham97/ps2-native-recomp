"""Real native main middleware pump, persistent time/callback and control transfer; CPU/file checks only."""
import ctypes,datetime,hashlib,importlib.util,json,os,re,subprocess,sys
from pathlib import Path
F=Path(__file__).resolve().parents[3]
P=F.parents[2]
HERE=Path(__file__).resolve().parent
OUT=HERE/(sys.argv[1] if len(sys.argv)>1 else 'validation-01')
BUILD=F/'build_native_services_only_20261119-01'
BIN=P.parent/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=P.parent/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(name,data):
    with (OUT/name).open('x',encoding='utf-8',newline='\n') as stream:json.dump(data,stream,indent=2);stream.write('\n')
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(exist_ok=False)
    spec=importlib.util.spec_from_file_location('checks',F/'research/runs/native-movie-kernel-policies-20261119/validate.py')
    c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
    c.OUT=OUT;c.ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    try:
        cache=(BUILD/'CMakeCache.txt').read_bytes().decode('utf-8-sig').replace('\r\n','\n')
        c.require(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M),'no legacy generated game')
        c.require(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M),'Release')
        impl=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-middleware-time-18/INTEGRATION.json'
        source=json.loads(impl.read_text(encoding='utf-8'))['sources']
        c.require(all(sha(path)==value for path,value in source.items()),'reviewed current source identity')
        source[str(Path(__file__).resolve())]=sha(__file__)
        preserve={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
        c.require(all(sha(path)==value for path,value in preserve.items()),'protected source unchanged')
        write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),working_tree=True,sources=source,compiler=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),cache_sha256=sha(BUILD/'CMakeCache.txt'),scope='Actual native source-order middleware main pump, persistent monotonic time/callback, real native reentry control transfer, file startup/rollback, package and affected CPU/file regressions; no game/endpoints'))
        manifest_root=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/package-and-constructors-05/input-manifests'
        manifest=json.loads((manifest_root/'PROVENANCE.json').read_text(encoding='utf-8'))
        c.require(manifest['package_rows']==245 and manifest['static_label_rows']==44 and manifest['hash_vectors']==16,'actual independent manifest schemas')
        c.require(all(sha(path)==digest for path,digest in manifest['manifest_sha256'].items()),'independent expectation manifests pinned')
        asset=Path(manifest['asset']);asset_before=asset.stat()
        c.require(asset_before.st_size==manifest['asset_size'] and asset_before.st_mtime_ns==manifest['asset_mtime_ns'],'read-only original asset identity')
        with asset.open('rb') as stream:c.require(hashlib.file_digest(stream,'sha256').hexdigest()==manifest['asset_sha256'],'full original asset SHA')
        keyboard_table=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-keyboard-feed-07/source-key-table.bin'
        c.require(keyboard_table.stat().st_size==740 and sha(keyboard_table)=='16ae56231aea9d26c3a3f6108c7365ab475618556da7ca2fbf6303a054fba12d','independent original raw key table')
        clients=[
          ('test_native_retail_participants','PASS native participant birth and menu reconstruction; no game or endpoint','mka::retail::NativeRetailParticipants::create_first_two('),
          ('test_native_retail_input_assignments','PASS native source assignment protocol; no host route, game or endpoint','mka::retail::NativeRetailInputAssignments::bind_direct('),
          ('test_native_retail_keyboard','PASS native keyboard queue and SDL event decoding; physical UI/game open','mka::retail::publish_retail_keyboard_sdl_event('),
          ('test_native_retail_action_history','PASS native retail action journal selective reset; producer/game open','mka::retail::NativeRetailActionHistory::reset('),
          ('test_native_retail_package','PASS native original package all245 metadata/byte comparisons and range checks; no game or endpoint','mka::retail::NativeRetailPackage::'),
          ('test_native_retail_package_errors','PASS native package malformed/disposable file cases; no game or endpoint','mka::retail::NativeRetailPackage::'),
          ('test_native_retail_static_constructors','PASS native static constructors all44 source keys/observer/lifetime checks; runtime handlers/game/endpoints unexercised','mka::retail::main_binding::construct_retail_key_action_records('),
          ('test_native_retail_mode_state','PASS native retail mode history source edge/failure checks; no game or endpoint','mka::retail::NativeRetailModeState::'),
          ('test_native_retail_audio_statistics','PASS native retail audio statistics finite/integer/failure checks; no sample producer, game or endpoint','mka::retail::NativeRetailAudioStatistics::'),
          ('test_native_retail_bound_owners','PASS actual native main constructor/mode bindings and allocation failures; no game or endpoint','mka::retail::main_binding::read_retail_top_mode('),
          ('test_native_retail_dispatch_owners','PASS native prefix copy/failure/lifetime checks; no message parser, game or endpoint','mka::retail::NativeRetailPrefixDispatchDescriptor::'),
          ('test_native_retail_record_owners','PASS native named-record ownership/failure/growth checks; no profile provider, game or endpoint','mka::retail::NativeRetailRecordOwners::'),
          ('test_native_retail_input_roles','PASS native four-role initialization and original action table; no assignment, polling, game or endpoint','mka::retail::NativeRetailInputRoles::'),
          ('test_native_retail_scene_owners','PASS native scene constructor/gates/structural ownership; no populated retail effects or game/device','mka::retail::NativeRetailSceneEventGraph::'),
          ('test_native_retail_middleware_work','PASS native middleware transfer/retry/ticket/lifetime checks; test ports only, no retail jobs or game/device','mka::retail::NativeRetailMiddlewareDispatcher::')]
        oracle=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-random-12/reference-01/reference-vectors.tsv'
        c.require(sha(oracle)=='4077398fe5d5a4917c97c58c4204f31f4a8a1579a44d25aa4be8a26aed951f94','independent frozen original-AOT random oracle')
        clients += [
          ('test_native_retail_main_settings','', 'mka::retail::NativeRetailMainSettings::effective_frame_rate('),
          ('test_native_retail_random','independent random oracle rows: 20252','mka::retail::NativeRetailRandom::next('),
          ('test_native_retail_game_session','', 'mka::retail::NativeRetailGameSession::create('),
          ('test_native_startup_codegen','PASS native retail startup source identity, ordered prerequisites and owned emission; no game boot','recomp::emit_native_startup_closure('),
          ('test_native_retail_startup','PASS generated retail startup owns actual arguments/content/memory group; failed initialization is unpublished and full entry link remains blocked','mka::retail::initialize_retail_memory_owners('),
          ('test_native_retail_crt','PASS production native CRT ownership/lifecycle, deferred work and callbacks; no game or device','mka::retail::NativeRetailCrt::create('),
          ('test_native_retail_main_timing','PASS native main shared-clock stopwatches and source parsed setting; no game/device','mka::retail::NativeRetailMainTiming::')]
        profile_rows=F/'research/runs/native-allocation-class-20261120/validation-01/source-profile.tsv'
        old_profile=json.loads((F/'research/runs/native-allocation-class-20261120/validation-01/profile-oracle.json').read_text(encoding='utf-8'))
        c.require(sha(profile_rows)==old_profile['target_sha256'],'independent actual18-word profile rows')
        clients += [
          ('test_native_retail_file_paths','PASS retail native source aliases and owned file route','mka::retail::NativeRetailFilePaths::open('),
          ('test_native_retail_profile_owners','', 'mka::retail::NativeRetailProfileOwners::create_initial_storage('),
          ('test_native_class_profile','PASS_NATIVE_CLASS_PROFILE: 20 selectors/13 classes and game-pool/companion counts match original stores; actual dynamic PFX storage and signed game predicate, no reset/producer/game claim','mka::retail::select_native_class_profile('),
          ('test_native_retail_memory_owners','PASS_NATIVE_RETAIL_MEMORY_OWNERS: 4 groups ordered identities/policies/real storage; owner-group phase only, no full retail main','mka::retail::NativeRetailMemoryOwners::create('),
          ('test_native_retail_memory_owner_faults','', 'mka::retail::NativeRetailMemoryOwners::create('),
          ('test_native_allocation_class','PASS_NATIVE_ALLOCATION_CLASS: 3 groups actual class payload/ownership/fit/signed PFX predicate; no callback/object/reset or retail startup claim','mka::memory::NativeAllocationClass::create(')]
        clients += [
          ('test_native_retail_file_startup','', 'mka::retail::main_binding::initialize_retail_middleware_files('),
          ('test_native_content_volume_owner','PASS_NATIVE_CONTENT_VOLUME_OWNER: opened identity/retail keys/containment/moves/root replacement/sparse64 files; no startup/schema claim','mka::content::NativeContentVolume::open_resource(')]
        clients += [
          ('test_native_retail_middleware_frame','PASS native middleware frame time and typed callback component','mka::retail::NativeRetailMiddlewareFrame::invoke_after_dispatcher('),
          ('test_native_retail_middleware_main','PASS actual native middleware main pump, source completion order and real main control transfer; game open','mka::retail::main_binding::pump_retail_middleware(')]
        c.execute('build.json',[CMAKE,'--build',BUILD,'--target',*[row[0] for row in clients],'mka_native_retail_platform','-j2'])
        receipts=[]
        fixture_content=OUT/'owned-content';fixture_content.mkdir()
        disposable=OUT/'disposable-package-errors';disposable.mkdir()
        for name,footer,symbol in clients:
            exe=BUILD/(name+'.exe')
            args=[]
            if name in ['test_native_retail_file_startup','test_native_retail_middleware_main']:args=[fixture_content]
            elif name=='test_native_content_volume_owner':args=[OUT/'volume-owner-content']
            elif name=='test_native_class_profile':args=[profile_rows]
            elif name=='test_native_retail_random':args=[oracle]
            elif name=='test_native_startup_codegen':args=[P/'research/inputs/SLUS_214.10']
            elif name=='test_native_retail_startup':args=[OUT/'exclusive-startup-content']
            elif name=='test_native_retail_crt':args=[fixture_content]
            elif name=='test_native_retail_keyboard':args=[keyboard_table]
            elif name=='test_native_retail_bound_owners':args=[fixture_content]
            elif name=='test_native_retail_package':args=[asset.parent,manifest_root/'package-members.tsv']
            elif name=='test_native_retail_package_errors':args=[disposable]
            elif name=='test_native_retail_static_constructors':args=[fixture_content,manifest_root/'static-labels.tsv',manifest_root/'hash-vectors.tsv']
            run=c.execute(name+'.json',[exe,*args])
            if name=='test_native_content_volume_owner':
                actual_lines=run['stdout'].splitlines()
                c.require(not run['stderr'] and actual_lines in [
                    [footer],['SKIP symlink creation: 1314',footer]],'exact volume client success with explicit known privilege skip')
                symlink_creation_exercised=actual_lines==[footer]
            elif name=='test_native_retail_file_startup':
                c.require(re.fullmatch(r'PASS native middleware file startup: [1-9][0-9]* allocation cuts, 12 native files, 65 native children; source frame body covered separately; callback producers/game remain open',run['stdout'].strip()) and not run['stderr'],'actual native file startup/fault exhaustion/growth/teardown client')
            elif name=='test_native_retail_profile_owners':
                c.require(re.fullmatch(r'PASS native retail profile initial class storage: [1-9][0-9]* allocation cuts, 13 Wave children; reset/callback/profile effects open',run['stdout'].strip()) and not run['stderr'],'actual class graph fault/lifetime fixture footer')
            elif name=='test_native_retail_memory_owner_faults':
                c.require(re.fullmatch(r'PASS_NATIVE_RETAIL_MEMORY_OWNER_FAULTS: create=[1-9][0-9]* bind=[1-9][0-9]* forced ordinary allocation cuts; owner-group/binding rollback only, no retail startup/game',run['stdout'].strip()) and not run['stderr'],'actual group allocation fault fixture footer')
            elif name=='test_native_retail_main_settings':
                c.require(run['stdout'].strip()=='PASS native main scalar settings and shared game-control flag; game and endpoints open' and not run['stderr'],'settings assertion footer')
            elif name=='test_native_retail_game_session':
                c.require(re.fullmatch(r'PASS native persistent session: [1-9][0-9]* creation allocation fault cuts, same-owner settings/PRNG/auxiliary, lifetime and thread failures; no game or endpoint\n',run['stdout']) and not run['stderr'],'session allocation/thread/lifetime assertion footer')
            else:c.require(run['stdout'].strip()==footer and not run['stderr'],'full fixture assertions '+name)
            c.symbols(name+'-symbols',exe,run['executable_sha256'],[symbol],True)
            receipts.append(dict(target=name,sha256=run['executable_sha256']))
        archive=BUILD/'libmka_native_retail_startup.a'
        c.symbols('archive',archive,sha(archive),['mka::retail::main_binding::construct_retail_dispatch_object(','mka::retail::main_binding::construct_retail_record_owners(','mka::retail::main_binding::construct_retail_aux_dispatch_object(','mka::retail::main_binding::construct_retail_audio_statistics(','mka::retail::main_binding::clear_retail_mode_stack(','mka::retail::main_binding::enter_retail_mode_zero(','mka::retail::main_binding::read_retail_top_mode(','mka::retail::main_binding::initialize_retail_package_stage(','mka::retail::main_binding::construct_retail_two_service_objects(','mka::retail::main_binding::construct_retail_empty_binding(','mka::retail::main_binding::construct_retail_key_action_records(','mka::retail::main_binding::construct_retail_first_paired_key_records(','mka::retail::main_binding::construct_retail_second_paired_key_records(','mka::retail::main_binding::construct_retail_first_handler_records(','mka::retail::main_binding::construct_retail_second_handler_records(','mka::retail::main_binding::initialize_retail_action_service(','mka::retail::main_binding::reset_retail_auxiliary_words(','mka::retail::main_binding::observe_retail_frame_role(','mka::retail::main_binding::observe_retail_action_row(','mka::retail::NativeRetailInputAssignments::bind_direct(','mka::retail::NativeRetailParticipants::create_first_two('],False)
        c.symbols('real-file-startup-bindings',archive,sha(archive),['mka::retail::main_binding::initialize_retail_middleware_files(','mka::retail::main_binding::shutdown_retail_middleware_files(','mka::retail::NativeRetailMiddlewareDispatcher::validate_attachment(','mka::retail::NativeRetailMiddlewareWorkGroup::validate_current_thread('],False)
        frame_archive=BUILD/'libmka_native_retail_middleware_frame.a'
        c.symbols('actual-native-middleware-pump',archive,sha(archive),['mka::retail::main_binding::pump_retail_middleware(','mka::retail::reenter_native_retail_main('],False)
        c.symbols('native-middleware-frame',frame_archive,sha(frame_archive),['mka::retail::NativeRetailMiddlewareFrame::sample(','mka::retail::NativeRetailMiddlewareFrame::invoke_after_dispatcher('],False)
        fnm=c.execute('frame-native-only-symbols.json',[BIN/'llvm-nm.exe','-C',frame_archive])
        c.require(not re.search(r' U (?:SDL_|.*(?<![A-Za-z0-9_])(?:(?:ps2|Cpu|spu2|sif|gs)::|(?:g_ram|g_spr)\b))',fnm['stdout']),'native frame has no guest/device dependency')
        paths_archive=BUILD/'libmka_native_retail_file_paths.a'
        profile_archive=BUILD/'libmka_native_retail_profile_owners.a'
        c.symbols('alias-archive',paths_archive,sha(paths_archive),['mka::retail::NativeRetailFilePaths::create(','mka::retail::NativeRetailFilePaths::open('],False)
        c.symbols('profile-archive',profile_archive,sha(profile_archive),['mka::retail::NativeRetailProfileOwners::create_initial_storage(','mka::retail::NativeRetailProfileOwners::payload_leaf('],False)
        c.require('native_disc_key' not in (F/'runtime/src/native/retail_package.cpp').read_text(encoding='utf-8'),'no hardcoded alias-bypass package key')
        game_archive=BUILD/'libmka_native_retail_game_session.a'
        c.symbols('game-session-archive',game_archive,sha(game_archive),['mka::retail::NativeRetailGameSession::create(','mka::retail::NativeRetailMainSettings::effective_frame_rate(','mka::retail::NativeRetailRandom::next('],False)
        gnm=c.execute('game-session-native-only-symbols.json',[BIN/'llvm-nm.exe','-C',game_archive])
        c.require(not re.search(r' U (?:SDL_|.*(?<![A-Za-z0-9_])(?:(?:ps2|Cpu|spu2|sif|gs)::|(?:g_ram|g_spr)\b))',gnm['stdout']),'session library has no device/guest/register dependency')
        c.symbols('main-settings-bindings',archive,sha(archive),['mka::retail::initialize_native_retail_game_session(','mka::retail::main_binding::read_retail_profile_selector(','mka::retail::main_binding::read_retail_frame_rate(','mka::retail::main_binding::retail_timing_override_active(','mka::retail::main_binding::read_retail_startup_seed_component(','mka::retail::main_binding::seed_retail_startup_random_state('],False)
        keyboard_archive=BUILD/'libmka_native_retail_keyboard.a'
        c.symbols('keyboard-archive',keyboard_archive,sha(keyboard_archive),['mka::retail::NativeRetailKeyboard::publish_utf8(','mka::retail::publish_retail_keyboard_sdl_event('],False)
        knm=c.execute('keyboard-native-only-symbols.json',[BIN/'llvm-nm.exe','-C',keyboard_archive])
        c.require(not re.search(r' U (?:SDL_|.*(?:ps2::|g_ram|g_spr|Cpu::))',knm['stdout']),'keyboard decoder uses no SDL endpoints or guest backend')
        platform_archive=BUILD/'libmka_native_retail_platform.a'
        pnm=c.execute('platform-keyboard-callers.json',[BIN/'llvm-nm.exe','-C',platform_archive])
        c.require(all(name in pnm['stdout'] for name in [' T mka::retail::NativeRetailPlatform::keyboard_events(', ' U mka::retail::publish_retail_keyboard_sdl_event(', ' U mka::retail::NativeRetailKeyboard::create(']),'actual platform owns and feeds queue')
        nm=c.execute('link-obligations.json',[BIN/'llvm-nm.exe','-C',archive])
        missing=set(re.findall(r' U mka::retail::main_binding::(\w+)\(',nm['stdout']))-set(re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(',nm['stdout']))
        prior=json.loads((F/'research/runs/native-retail-file-startup-20261121/validation-02/verification.json').read_text(encoding='utf-8'))
        expected=set(prior['remaining_integration_symbols'])-{'pump_retail_middleware'}
        c.require(missing==expected,'exact remaining integration symbols, never missing original retail functions')
        argv=list(map(str,[CMAKE,'--build',BUILD,'--target','test_native_startup_missing_bindings','-j2']))
        proc=subprocess.run(argv,cwd=F,env=c.ENV,text=True,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
        write('incomplete-entry-link.json',dict(argv=argv,exit=proc.returncode,stdout=proc.stdout,stderr=proc.stderr,missing=sorted(missing),executable_sha256=sha(CMAKE)))
        c.require(proc.returncode!=0 and all(('main_binding::'+name+'(') in (proc.stdout+proc.stderr) for name in missing),'actual complete entry still refuses every unresolved integration symbol')
        c.require(not (BUILD/'test_native_startup_missing_bindings.exe').exists(),'no incomplete game delivered')
        c.require(all(sha(path)==value for path,value in source.items()),'source stable through execution')
        c.require(all(sha(path)==value for path,value in preserve.items()),'protected source stable through execution')
        c.require((asset_before.st_size,asset_before.st_mtime_ns,asset_before.st_ino)==(asset.stat().st_size,asset.stat().st_mtime_ns,asset.stat().st_ino),'original asset metadata unchanged through read-only checks')
        result=dict(status='PASS_NATIVE_RETAIL_MIDDLEWARE_FRAME_CPU_FILE_CHECKS',sources=source,fixtures=receipts,archive_sha256=sha(archive),frame_archive_sha256=sha(frame_archive),alias_archive_sha256=sha(paths_archive),profile_archive_sha256=sha(profile_archive),profile_oracle_sha256=sha(profile_rows),game_session_archive_sha256=sha(game_archive),random_oracle_sha256=sha(oracle),keyboard_archive_sha256=sha(keyboard_archive),platform_archive_sha256=sha(platform_archive),source_key_table_sha256=sha(keyboard_table),independent_manifest_sha256=manifest['manifest_sha256'],source_asset_sha256=manifest['asset_sha256'],symlink_creation_exercised=symlink_creation_exercised,remaining_integration_symbols=sorted(missing),complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,limits=['Windows symlink creation was not exercised if symlink_creation_exercised is false; the client explicitly reported privilege1314, not full containment coverage','Actual source-order main middleware pump and native time/typed callback body are real; original callback producer and general async file clients remain unresolved. Native frame nonowningly retains its CRT, which must outlive it; ordinary Startup field order enforces this lifetime.','No constant gate implementation: actual source live gate/task/render producers remain unbound. Host-to-retail physical sample/selection/poll and complete resource/task initialization remain open','No main-menu/combat or complete native route acceptance'])
        write('verification.json',result)
        print(json.dumps({k:result[k] for k in ['status','complete_game_entry_linkable','game_executed','physical_endpoints_opened']}))
    except Exception as e:
        write('failure.json',dict(status='FAILED',error=type(e).__name__,message=str(e)))
        raise
