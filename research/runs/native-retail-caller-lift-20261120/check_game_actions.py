"""Native original game action state integration; no devices or game execution."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_native_caller_receipts',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv)!=2 or not re.fullmatch(r'validation-game-actions-[0-9]+',sys.argv[1]):raise SystemExit('usage: check_game_actions.py validation-game-actions-NN')
    out=pathlib.Path(__file__).resolve().parent/sys.argv[1]
    common.OUT=out;common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    require,sha,write=common.require,common.sha,common.write
    records=PARENT/'research/runs/native-movie-ingress-20261119'
    files=[FORK/x for x in ['runtime/include/mka/game_action_input.hpp','runtime/src/native/game_action_input.cpp','runtime/include/mka/controller_input.hpp','runtime/src/native/controller_input.cpp','tests/test_game_action_input.cpp','CMakeLists.txt','cmake/native_game.cmake']]
    files += [records/x for x in ['NATIVE_GAME_ACTION_INPUT_SOURCE.md','NATIVE_GAME_ACTION_INPUT_SOURCE.json','NATIVE_GAME_ACTION_INPUT_IMPLEMENTATION.md','NATIVE_GAME_ACTION_INPUT_IMPLEMENTATION.json']]
    files += [COMMON,pathlib.Path(__file__).resolve(),common.BIN/'clang++.exe',PARENT/'research/inputs/SLUS_214.10']
    before={str(x):sha(x) for x in files}
    require(before[str(PARENT/'research/inputs/SLUS_214.10')]=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f','actual source image')
    cache=(common.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M),'native-only Release cache')
    out.mkdir(exist_ok=False)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_before_sha256=before,scope='Native source-derived game action state using actual ControllerInput publications; no window/device/game/decoder'))
    try:
        common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target','test_native_game_action_input','-j2'])
        target=common.BUILD/'test_native_game_action_input.exe'
        result=common.execute('game-actions.json',[target])
        require(result['stdout'].splitlines()[-1]=='PASS native game action input ordered mappings/poll edges/LOGON skip; no guest ABI or task suspension notifications','complete source-state assertions')
        audit=common.symbols('game-actions-symbols',target,result['executable_sha256'],['mka::input::NativeGameActionInput::','mka::input::ControllerInput::'],True)
        require(before=={name:sha(name) for name in before},'stable source/proof/compiler pins')
        for path,h in common.PRESERVE.items():require(sha(FORK/path)==h,'preserved unrelated IOP')
        write('verification.json',dict(status='PASS_NATIVE_GAME_ACTION_INPUT',source_after_sha256=before,executable_sha256=result['executable_sha256'],audit=audit,limits=['Native source state and actual host-owner publications only; no physical input/window/game','Explicit selected participant order and mutable mappings; no permanent default button guessing','Game scheduler/task suspension notifications and actual caller binding remain OPEN','No decoder/media content verification or local model/driver/settings action']))
        print(json.dumps(dict(status='PASS_NATIVE_GAME_ACTION_INPUT',pins=len(before))),flush=True)
    except Exception as error:
        write('aborted.json',dict(status='NATIVE_GAME_ACTION_INPUT_CHECK_FAILED',error=str(error)));raise
