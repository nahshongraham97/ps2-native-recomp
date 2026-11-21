"""Native owner integration checks. Never opens game/physical endpoints."""
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
        impl=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/constructor-integration-03/IMPLEMENTATION.json'
        source=json.loads(impl.read_text(encoding='utf-8'))['sources']
        c.require(all(sha(path)==value for path,value in source.items()),'reviewed current source identity')
        source[str(Path(__file__).resolve())]=sha(__file__)
        preserve={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
        c.require(all(sha(path)==value for path,value in preserve.items()),'protected source unchanged')
        write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),working_tree=True,sources=source,compiler=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),cache_sha256=sha(BUILD/'CMakeCache.txt'),scope='CPU ownership and serialized role data only; no game or physical endpoint'))
        clients=[
          ('test_native_retail_dispatch_owners','PASS native prefix copy/failure/lifetime checks; no message parser, game or endpoint','mka::retail::NativeRetailPrefixDispatchDescriptor::'),
          ('test_native_retail_record_owners','PASS native named-record ownership/failure/growth checks; no profile provider, game or endpoint','mka::retail::NativeRetailRecordOwners::'),
          ('test_native_retail_input_roles','PASS native four-role initialization and original action table; no assignment, polling, game or endpoint','mka::retail::NativeRetailInputRoles::'),
          ('test_native_retail_scene_owners','PASS native scene constructor/gates/structural ownership; no populated retail effects or game/device','mka::retail::NativeRetailSceneEventGraph::'),
          ('test_native_retail_middleware_work','PASS native middleware transfer/retry/ticket/lifetime checks; test ports only, no retail jobs or game/device','mka::retail::NativeRetailMiddlewareDispatcher::')]
        c.execute('build.json',[CMAKE,'--build',BUILD,'--target',*[row[0] for row in clients],'-j2'])
        receipts=[]
        for name,footer,symbol in clients:
            exe=BUILD/(name+'.exe');run=c.execute(name+'.json',[exe])
            c.require(run['stdout'].strip()==footer and not run['stderr'],'full fixture assertions '+name)
            c.symbols(name+'-symbols',exe,run['executable_sha256'],[symbol],True)
            receipts.append(dict(target=name,sha256=run['executable_sha256']))
        archive=BUILD/'libmka_native_retail_startup.a'
        c.symbols('archive',archive,sha(archive),['mka::retail::main_binding::construct_retail_dispatch_object(','mka::retail::main_binding::construct_retail_record_owners(','mka::retail::main_binding::construct_retail_aux_dispatch_object('],False)
        nm=c.execute('link-obligations.json',[BIN/'llvm-nm.exe','-C',archive])
        missing=set(re.findall(r' U mka::retail::main_binding::(\w+)\(',nm['stdout']))-set(re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(',nm['stdout']))
        prior=json.loads((F/'research/runs/native-retail-constructors-20261120/validation-01/verification.json').read_text(encoding='utf-8'))
        expected=set(prior['missing_bodies'])-{'construct_retail_dispatch_object','construct_retail_scene_records','construct_retail_aux_dispatch_object','initialize_retail_controller_stage'}
        expected.add('initialize_retail_package_stage')
        c.require(missing==expected,'exact remaining integration symbols, never missing original retail functions')
        argv=list(map(str,[CMAKE,'--build',BUILD,'--target','test_native_startup_missing_bindings','-j2']))
        proc=subprocess.run(argv,cwd=F,env=c.ENV,text=True,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
        write('incomplete-entry-link.json',dict(argv=argv,exit=proc.returncode,stdout=proc.stdout,stderr=proc.stderr,missing=sorted(missing),executable_sha256=sha(CMAKE)))
        c.require(proc.returncode!=0 and all(('main_binding::'+name+'(') in (proc.stdout+proc.stderr) for name in missing),'actual complete entry still refuses every unresolved integration symbol')
        c.require(not (BUILD/'test_native_startup_missing_bindings.exe').exists(),'no incomplete game delivered')
        c.require(all(sha(path)==value for path,value in source.items()),'source stable through execution')
        c.require(all(sha(path)==value for path,value in preserve.items()),'protected source stable through execution')
        result=dict(status='PASS_NATIVE_RETAIL_OWNERS_CPU_CHECKS',sources=source,fixtures=receipts,archive_sha256=sha(archive),remaining_integration_symbols=sorted(missing),complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,limits=['No async profile provider/message consumer','No actual host-to-retail role assignment/poll','No main-menu/combat or complete native route acceptance'])
        write('verification.json',result)
        print(json.dumps({k:result[k] for k in ['status','complete_game_entry_linkable','game_executed','physical_endpoints_opened']}))
    except Exception as e:
        write('failure.json',dict(status='FAILED',error=type(e).__name__,message=str(e)))
        raise
