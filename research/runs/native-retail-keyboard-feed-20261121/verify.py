"""Native keyboard queue and actual platform archive integration. No game/endpoints."""
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
        impl=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-keyboard-feed-07/IMPLEMENTATION_CORRECTED.json'
        source=json.loads(impl.read_text(encoding='utf-8'))['sources']
        c.require(all(sha(path)==value for path,value in source.items()),'reviewed current source identity')
        source[str(Path(__file__).resolve())]=sha(__file__)
        preserve={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
        c.require(all(sha(path)==value for path,value in preserve.items()),'protected source unchanged')
        write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),working_tree=True,sources=source,compiler=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),cache_sha256=sha(BUILD/'CMakeCache.txt'),scope='Native keyboard event decoder and real platform archive build plus affected CPU/file regressions; no game or physical endpoint'))
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
        c.execute('build.json',[CMAKE,'--build',BUILD,'--target',*[row[0] for row in clients],'mka_native_retail_platform','-j2'])
        receipts=[]
        fixture_content=OUT/'owned-content';fixture_content.mkdir()
        disposable=OUT/'disposable-package-errors';disposable.mkdir()
        for name,footer,symbol in clients:
            exe=BUILD/(name+'.exe')
            args=[]
            if name=='test_native_retail_keyboard':args=[keyboard_table]
            elif name=='test_native_retail_bound_owners':args=[fixture_content]
            elif name=='test_native_retail_package':args=[asset.parent,manifest_root/'package-members.tsv']
            elif name=='test_native_retail_package_errors':args=[disposable]
            elif name=='test_native_retail_static_constructors':args=[fixture_content,manifest_root/'static-labels.tsv',manifest_root/'hash-vectors.tsv']
            run=c.execute(name+'.json',[exe,*args])
            c.require(run['stdout'].strip()==footer and not run['stderr'],'full fixture assertions '+name)
            c.symbols(name+'-symbols',exe,run['executable_sha256'],[symbol],True)
            receipts.append(dict(target=name,sha256=run['executable_sha256']))
        archive=BUILD/'libmka_native_retail_startup.a'
        c.symbols('archive',archive,sha(archive),['mka::retail::main_binding::construct_retail_dispatch_object(','mka::retail::main_binding::construct_retail_record_owners(','mka::retail::main_binding::construct_retail_aux_dispatch_object(','mka::retail::main_binding::construct_retail_audio_statistics(','mka::retail::main_binding::clear_retail_mode_stack(','mka::retail::main_binding::enter_retail_mode_zero(','mka::retail::main_binding::read_retail_top_mode(','mka::retail::main_binding::initialize_retail_package_stage(','mka::retail::main_binding::construct_retail_two_service_objects(','mka::retail::main_binding::construct_retail_empty_binding(','mka::retail::main_binding::construct_retail_key_action_records(','mka::retail::main_binding::construct_retail_first_paired_key_records(','mka::retail::main_binding::construct_retail_second_paired_key_records(','mka::retail::main_binding::construct_retail_first_handler_records(','mka::retail::main_binding::construct_retail_second_handler_records(','mka::retail::main_binding::initialize_retail_action_service('],False)
        keyboard_archive=BUILD/'libmka_native_retail_keyboard.a'
        c.symbols('keyboard-archive',keyboard_archive,sha(keyboard_archive),['mka::retail::NativeRetailKeyboard::publish_utf8(','mka::retail::publish_retail_keyboard_sdl_event('],False)
        knm=c.execute('keyboard-native-only-symbols.json',[BIN/'llvm-nm.exe','-C',keyboard_archive])
        c.require(not re.search(r' U (?:SDL_|.*(?:ps2::|g_ram|g_spr|Cpu::))',knm['stdout']),'keyboard decoder uses no SDL endpoints or guest backend')
        platform_archive=BUILD/'libmka_native_retail_platform.a'
        pnm=c.execute('platform-keyboard-callers.json',[BIN/'llvm-nm.exe','-C',platform_archive])
        c.require(all(name in pnm['stdout'] for name in [' T mka::retail::NativeRetailPlatform::keyboard_events(', ' U mka::retail::publish_retail_keyboard_sdl_event(', ' U mka::retail::NativeRetailKeyboard::create(']),'actual platform owns and feeds queue')
        nm=c.execute('link-obligations.json',[BIN/'llvm-nm.exe','-C',archive])
        missing=set(re.findall(r' U mka::retail::main_binding::(\w+)\(',nm['stdout']))-set(re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(',nm['stdout']))
        prior=json.loads((F/'research/runs/native-retail-input-history-20261121/validation-01/verification.json').read_text(encoding='utf-8'))
        expected=set(prior['remaining_integration_symbols'])-{'register_retail_initialized_main_record','register_retail_existing_service_cleanup'}
        c.require(missing==expected,'exact remaining integration symbols, never missing original retail functions')
        argv=list(map(str,[CMAKE,'--build',BUILD,'--target','test_native_startup_missing_bindings','-j2']))
        proc=subprocess.run(argv,cwd=F,env=c.ENV,text=True,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
        write('incomplete-entry-link.json',dict(argv=argv,exit=proc.returncode,stdout=proc.stdout,stderr=proc.stderr,missing=sorted(missing),executable_sha256=sha(CMAKE)))
        c.require(proc.returncode!=0 and all(('main_binding::'+name+'(') in (proc.stdout+proc.stderr) for name in missing),'actual complete entry still refuses every unresolved integration symbol')
        c.require(not (BUILD/'test_native_startup_missing_bindings.exe').exists(),'no incomplete game delivered')
        c.require(all(sha(path)==value for path,value in source.items()),'source stable through execution')
        c.require(all(sha(path)==value for path,value in preserve.items()),'protected source stable through execution')
        c.require((asset_before.st_size,asset_before.st_mtime_ns,asset_before.st_ino)==(asset.stat().st_size,asset.stat().st_mtime_ns,asset.stat().st_ino),'original asset metadata unchanged through read-only checks')
        result=dict(status='PASS_NATIVE_KEYBOARD_FEED_CPU_FILE_CHECKS',sources=source,fixtures=receipts,archive_sha256=sha(archive),keyboard_archive_sha256=sha(keyboard_archive),platform_archive_sha256=sha(platform_archive),source_key_table_sha256=sha(keyboard_table),independent_manifest_sha256=manifest['manifest_sha256'],source_asset_sha256=manifest['asset_sha256'],remaining_integration_symbols=sorted(missing),complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,limits=['No native scene keyboard0x40A receiver, actual action producer/participant binding, sample/volume or profile consumer','No actual host-to-retail role assignment/poll','No main-menu/combat or complete native route acceptance'])
        write('verification.json',result)
        print(json.dumps({k:result[k] for k in ['status','complete_game_entry_linkable','game_executed','physical_endpoints_opened']}))
    except Exception as e:
        write('failure.json',dict(status='FAILED',error=type(e).__name__,message=str(e)))
        raise
