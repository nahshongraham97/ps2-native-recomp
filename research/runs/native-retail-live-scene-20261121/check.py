"""Compile the actual scene/menu/profile transition; no game or endpoints."""
import ctypes
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
F = H.parents[2]
P = F.parents[2]
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C / 'native-retail-live-scene-70/root02'
s = importlib.util.spec_from_file_location('native_scene_cpu_receipts',
    F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(s)
s.loader.exec_module(c)
c.OUT = H / (sys.argv[1] if len(sys.argv) > 1 else 'validation-01')


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous = F / 'research/runs/native-retail-profile-state-20261121/validation-01/verification.json'
        c.require(c.sha(previous) == '8eeb9f0619a0a2befc400719d6949cf4b72d970cc746378ec418041392b9c8a8',
            'exact prior connected profile result')
        prior = load(previous)
        c.require(prior['schema'] == 'native_profile_state_connected_verification_v1' and
            isinstance(prior['sources'], dict) and isinstance(prior['remaining_integration_symbols'], list),
            'actual prior producer schema')
        manifest = D / 'IMPLEMENTATION.json'
        c.require(c.sha(manifest) == 'a4fcd61c92998047caf31d6a20ff382bd16d26b0235d1b0ffdabe9f2e6731ef0',
            'exact unbuilt integration source freeze')
        authored = load(manifest)
        c.require(authored['schema'] == 'native_live_scene_profile_transition_integration_v1',
            'actual integration manifest schema')
        sources = {str(F / name): pin for name,pin in authored['native_sources_sha256'].items()}
        changes = {}
        for name,pin in prior['sources'].items():
            current = c.sha(name)
            if current != pin:
                c.require(name in sources and sources[name] == current,
                    'prior changed input explicitly frozen: ' + name)
                changes[name] = dict(previous_sha256=pin, current_sha256=current)
            else:
                sources.setdefault(name, pin)
        sources[str(Path(__file__))] = c.sha(__file__)
        for name in ['runtime/src/native/retail_main_state.cpp',
            'runtime/src/native/retail_platform.cpp','runtime/src/native/retail_game_session_startup.cpp']:
            sources.setdefault(str(F/name),c.sha(F/name))
        evidence = {str(manifest): c.sha(manifest)}
        if (D/'SCOPE_CORRECTION.json').exists():
            evidence[str(D/'SCOPE_CORRECTION.json')] = c.sha(D/'SCOPE_CORRECTION.json')
        for scope in ['native-retail-live-scene-70/source-agent01',
            'native-retail-profile-transition-71/source-agent01',
            'native-retail-profile-transition-71/author02',
            'native-retail-live-scene-70/review-object02']:
            for path in (C / scope).glob('*'):
                if path.is_file(): evidence[str(path)] = c.sha(path)
        for path in D.glob('GHIDRA_*.json'):
            evidence[str(path)] = c.sha(path)
        protected = prior['protected_inputs']
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(p) == pin for p,pin in group.items()), 'stable inputs before compilation')
        elf = (P / 'research/inputs/SLUS_214.10').read_bytes()
        phoff = struct.unpack_from('<I', elf, 28)[0]
        size,count = struct.unpack_from('<HH', elf, 42)
        segments = [struct.unpack_from('<8I',elf,phoff+i*size) for i in range(count)]

        def read(address,length):
            matches = [x for x in segments if x[0] == 1 and x[2] <= address and address+length <= x[2]+x[4]]
            c.require(len(matches) == 1, 'original file-backed PT_LOAD range')
            x = matches[0]
            at = x[1]+address-x[2]
            return elf[at:at+length]

        def word(address):
            return struct.unpack('<I',read(address,4))[0]

        def join(value):
            count = 0
            if isinstance(value,dict):
                address = value.get('address')
                expected = value.get('le32',value.get('word'))
                if address is not None and expected is not None:
                    address = int(address,16) if isinstance(address,str) else address
                    expected = int(expected,16) if isinstance(expected,str) else expected
                    c.require(word(address) == expected, 'original word ' + hex(address))
                    count += 1
                for item in value.values(): count += join(item)
            elif isinstance(value,list):
                count += sum(join(item) for item in value)
            return count

        raw70 = C / 'native-retail-live-scene-70/source-agent01/RAW_SOURCE.json'
        raw71 = C / 'native-retail-profile-transition-71/source-agent01/RAW_SOURCE.json'
        c.require(c.sha(raw70) == 'ff04d351b1c4d8027365570054f039bb0528267ef0ccb76586fd81561d8dca2d' and
            c.sha(raw71) == '65d0252a285ea1ebaad826dddf3757b432126bfb77eb12266913fc73868e9bec',
            'exact original source contracts')
        joins = {str(path): join(load(path)) for path in [raw70,raw71]}
        c.require(all(joins.values()) and joins[str(raw71)] == 70, 'complete source word joins')
        for name,address in [('menu_vtable_59fe40',0x59fe40),('other_vtable_5ae9e0',0x5ae9e0)]:
            data = bytes.fromhex(load(raw70)['data'][name])
            c.require(read(address,len(data)) == data, 'original scene vtable data')
        for address in [0x794d74,0x794d78]:
            c.require(any(x[0] == 1 and x[2]+x[4] <= address and address+4 <= x[2]+x[5]
                for x in segments), 'source reference has loader-zero birth')
        program = load(D / 'GHIDRA_PROGRAM.json')
        c.require(program['sha256'] == protected[str(P/'research/inputs/SLUS_214.10')] and
            program['language_id'] == 'r5900:LE:32:default' and program['processor'] == 'MIPS-R5900',
            'actual existing Ghidra target identity')
        listing_counts = {}
        for name,expected in [('002F18F0',11),('002F1920',31),('0018A260',70)]:
            listing = load(D / ('GHIDRA_'+name+'.json'))
            c.require(not listing['has_more'] and len(listing['items']) == listing['total'] == expected,
                'complete actual EE listing')
            for item in listing['items']:
                c.require(read(int(item['address'],16),4) == bytes.fromhex(item['bytes']),
                    'actual EE listing matches original instruction')
            listing_counts[name] = expected
        c.require(word(0x18a33c) == 0xac470000 and word(0x18a34c) == 0xac600000 and
            word(0x18a328) == 0x2508ffff, 'selected/opposite stores and wrapped counter decrement')
        c.save('inputs.json',dict(sources=sources,evidence=evidence,protected_inputs=protected,
            explicit_prior_source_changes=changes,original_word_joins=joins,actual_ee_listing_counts=listing_counts,
            prior_path=str(previous),prior_sha256=c.sha(previous)))
        modules = P / 'research/inputs/modules'
        clients = {
            'test_native_retail_live_scene_reference':[],
            'test_native_retail_profile_scene_object':[],
            'test_native_retail_profile_transition':[],
            'test_native_retail_profile_menu':[],
            'test_native_retail_profile_aggregate_main':[modules],
            'test_native_retail_game_session':[],
            'test_native_retail_game_records_main':[modules],
            'test_native_retail_profile_preferences':[modules],
            'test_native_retail_profile_resource_ids':[modules],
            'test_native_retail_save_startup':[]}
        reuse = len(sys.argv) > 2 and sys.argv[2] == 'reuse-passed-validation-01'
        passed = H / 'validation-01'
        if reuse:
            c.require(c.sha(passed/'inputs.json') == 'a093739d85c43b20a98000df5ce5045c35e638b0b9aed06f9010370af2140257' and
                c.sha(passed/'build.json') == '33bf8d5b398fe76f3517eda98aa9dcb0b7b44c1419def0bbf5e8eb6c2b77a394' and
                c.sha(passed/'failure.json') == '8ed1f3d78be7bc02519a1f82f528fdc8f98f66ff89a9aca1ef692e33e4ac3e3a',
                'exact preserved passed-build/failed-wrapper evidence')
            measured = load(passed/'inputs.json')
            for group in ['sources','evidence','protected_inputs']:
                for path,pin in measured[group].items():
                    if path == str(Path(__file__)):
                        c.require(c.sha(D/'before-scope-correction/check.py') == pin,
                            'exact prior checker preserved')
                    else:
                        c.require(c.sha(path) == pin,'unchanged passed-build input: ' + path)
            c.require(load(passed/'build.json')['exit'] == 0,'actual prior successful compilation')
        else:
            c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*clients,'-j2'])
        executions = []
        for target,args in clients.items():
            if reuse:
                path = passed/('run-'+target+'.json')
                r = load(path)
                c.require(r['exit'] == 0 and r['argv'] == list(map(str,[c.BUILD/(target+'.exe'),*args])) and
                    r['executable_sha256'] == r['executable_after_sha256'] == c.sha(c.BUILD/(target+'.exe')),
                    'exact passed current executable/arguments: ' + target)
                r = dict(r,reused_from=str(path),reused_record_sha256=c.sha(path))
                c.save('run-'+target+'.json',r)
            else:
                r = c.execute('run-'+target,[c.BUILD/(target+'.exe'),*args])
            quiet = target in ['test_native_retail_live_scene_reference','test_native_retail_profile_resource_ids']
            c.require(not r['stderr'] and (r['stdout'] == '' if quiet else r['stdout'].startswith('PASS')),
                'actual client assertions: ' + target)
            executions.append(dict(target=target,sha256=r['executable_sha256'],receipt='run-'+target+'.json'))
        native_platform_imports = {}
        for target in list(clients)[:5]:
            r = c.execute('symbols-'+target,[c.BIN/'llvm-nm.exe','-C',c.BUILD/(target+'.exe')])
            c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b',
                r['stdout']), 'no guest/device/compiler symbols: ' + target)
            native_platform_imports[target] = sorted(set(re.findall(r'\bSDL_[A-Za-z0-9_]+',r['stdout'])))
            if target in ['test_native_retail_live_scene_reference','test_native_retail_profile_scene_object']:
                c.require(not native_platform_imports[target], 'pure Session fixture has no SDL linkage')
        c.save('native-platform-linkage.json',dict(native_sdl_symbols=native_platform_imports,
            scope='Native imports are linkage evidence, not endpoint execution. Main construction and fixture call paths inspected.',
            correction_record=str(D/'SCOPE_CORRECTION.json'),correction_sha256=c.sha(D/'SCOPE_CORRECTION.json')))
        required = set(authored['production_requirements_open'])
        r = c.execute('required-native-effects-link',[c.CMAKE,'--build',c.BUILD,
            '--target','test_native_profile_required_effects','-j2'],expected=None)
        effects = re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
        c.require(len(required) == len(effects) == 3 and
            all(any('::'+name+'(' in symbol for symbol in effects) for name in required) and
            not (c.BUILD/'test_native_profile_required_effects.exe').exists(),
            'exact three remaining strong production scene/actor effects')
        r = c.execute('actual-entry-link',[c.CMAKE,'--build',c.BUILD,
            '--target','test_native_startup_missing_bindings','-j2'],expected=None)
        undefined = re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
        names = {m.group(1) for symbol in undefined if
            (m := re.search(r'main_binding::(\w+)\(',symbol))}
        c.require(names == set(prior['remaining_integration_symbols']) and len(undefined) == len(names) == 27 and
            not (c.BUILD/'test_native_startup_missing_bindings.exe').exists(), 'exact remaining Main link requirements')
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(p) == pin for p,pin in group.items()), 'stable inputs after current checks')
        result = dict(schema='native_live_scene_profile_transition_connected_verification_v1',
            status='PASS_NATIVE_LIVE_SCENE_PROFILE_TRANSITION_CPU_CHECKS',sources=sources,evidence=evidence,
            protected_inputs=protected,executed_clients=executions,original_word_joins=joins,
            client_executions_reused_from_current_passed_build=reuse,native_platform_imports=native_platform_imports,
            actual_ee_listing_counts=listing_counts,explicit_prior_source_changes=changes,
            prior_path=str(previous),prior_sha256=c.sha(previous),
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            session_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_game_session.a'),
            remaining_integration_symbols=sorted(names),required_profile_effects=sorted(required),
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,
            frames=None,audio=None,limits=[
                'Real live scene lookup and18A260 now run in actual menu/aggregate clients on retained Session.',
                'Real scene producers,2F1630,18B0D0,secret camera cleanup and full171B10 remain open.',
                'Tests provide only explicitly named missing action/18B0D0/actor bodies; production has no defaults.',
                'No accepted boot/menu/combat/physical input/render/movie/audio or whole-game evidence.'])
        c.save('verification.json',result)
        print(json.dumps(dict(status=result['status'],fresh_clients=len(executions),
            remaining_main=len(names),required_scene_actor_effects=len(required),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',type=type(error).__name__,message=str(error)))
        raise


if __name__ == '__main__':
    main()
