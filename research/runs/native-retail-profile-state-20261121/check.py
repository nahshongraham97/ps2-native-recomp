"""Build/check the connected native profile state; never run a game or endpoint."""
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
s = importlib.util.spec_from_file_location('native_cpu_receipts',
    F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(s)
s.loader.exec_module(c)
c.OUT = H / (sys.argv[1] if len(sys.argv) > 1 else 'validation-01')

def load(path): return json.loads(path.read_text(encoding='utf-8'))

def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous = F / 'research/runs/native-retail-save-profile-20261121/main-validation-03/verification.json'
        c.require(c.sha(previous) == 'e9fd260d1d5dcea9a22d68938d26a793f8c04623694b743199ce901a949accae', 'exact prior record')
        prior = load(previous)
        c.require(prior['schema'] == 'native_save_main_verification_v1' and
                  isinstance(prior['remaining_integration_symbols'], list), 'prior actual schema')
        current = load(C / 'native-retail-profile-settings-69/root01/IMPLEMENTATION.json')
        c.require(current['schema'] == 'native_profile_preferences_and_connected_aggregate_69_author_v1', 'current root author schema')
        sources = dict(current['sources'])
        evidence = {}
        for name in ['native-retail-profile-settings-69/root01/IMPLEMENTATION.json',
                     'native-retail-profile-menu-67/author01/IMPLEMENTATION.json',
                     'native-retail-profile-resource-ids-68/author02/OWNER_CORRECTION.json']:
            evidence[str(C / name)] = c.sha(C / name)
        menu = load(C / 'native-retail-profile-menu-67/author01/IMPLEMENTATION.json')
        c.require(menu['schema'] == 'native_retail_profile_menu_driver_implementation_v1', 'menu producer schema')
        sources.update({str(F / p): h for p,h in menu['files_sha256'].items()})
        ids = load(C / 'native-retail-profile-resource-ids-68/author02/OWNER_CORRECTION.json')
        c.require(ids['schema'] == 'native_retail_profile_resource_ids_owner_correction_v1', 'ID owner correction schema')
        for name, pin in ids['current_sha256'].items():
            root = C / 'native-retail-profile-resource-ids-68' if name.startswith('author02/') else F
            sources[str(root / name)] = pin
        changed = {}
        for name, old in prior['sources'].items():
            now = c.sha(name)
            if now != old:
                c.require(name in sources, 'prior pinned change is explicitly authored: ' + name)
                changed[name] = dict(previous_sha256=old, current_sha256=now)
            else: sources.setdefault(name, old)
        additions = ['tests/test_retail_game_session.cpp', 'tests/test_retail_game_records_main.cpp',
            'tests/test_retail_save_startup.cpp', 'runtime/src/native/retail_main_control.hpp',
            'research/runs/native-retail-profile-state-20261121/check.py']
        sources.update({str(F / name): c.sha(F / name) for name in additions})
        c.require(all(c.sha(p) == h for p,h in sources.items()), 'all current author pins before build')
        protected = prior['protected_inputs']
        c.require(all(c.sha(p) == h for p,h in protected.items()), 'protected inputs before')
        elf = (P / 'research/inputs/SLUS_214.10').read_bytes()
        phoff = struct.unpack_from('<I', elf, 28)[0]
        size, count = struct.unpack_from('<HH', elf, 42)
        segments = [struct.unpack_from('<8I', elf, phoff+i*size) for i in range(count)]
        def read(address, length):
            parts = [s for s in segments if s[0] == 1 and s[2] <= address and address+length <= s[2]+s[4]]
            c.require(len(parts) == 1, 'actual original file-backed mapping')
            s = parts[0]
            at = s[1] + address - s[2]
            return elf[at:at+length]
        def join(value):
            n = 0
            if isinstance(value, dict):
                address = value.get('address', value.get('source_address'))
                word = value.get('le32', value.get('word'))
                if address is not None and word is not None:
                    at = int(address,16) if isinstance(address,str) else address
                    expected = int(word,16) if isinstance(word,str) else word
                    c.require(struct.unpack('<I', read(at,4))[0] == expected, 'original instruction/data word: ' + hex(at))
                    n += 1
                for item in value.values(): n += join(item)
            elif isinstance(value,list): n += sum(join(x) for x in value)
            return n
        joined = {}
        for name in ['native-retail-profile-menu-67/source-agent01/RAW_SOURCE.json',
            'native-retail-profile-resource-ids-68/source-agent01/RAW_SOURCE.json',
            'native-retail-profile-settings-69/source-agent01/RAW_SOURCE.json',
            'native-retail-profile-settings-69/source-agent01/CONSUMER_WORDS.json',
            'native-retail-profile-aggregate-61/source-agent01/RAW_SOURCE.json']:
            path = C / name
            evidence[str(path)] = c.sha(path)
            joined[name] = join(load(path))
            c.require(joined[name] > 0, 'actual original words joined for each source producer')
        c.require(read(0x5be650,88).hex() ==
            '0000403f0000403f0000403f0000403f0000403f0000803f0200000002000000020000005a0000000300000001000000000000000000000000000000320000003200000032000000320000003200000055000000ffffffff',
            'all original preference image bytes')
        c.require(struct.unpack('<I',read(0x122920,4))[0] == 0x0c0a5598, 'actual Main call295660')
        c.save('inputs.json',dict(sources=sources,evidence=evidence,protected_inputs=protected,
            explicit_prior_source_changes=changed,original_word_joins=joined,
            prior_path=str(previous),prior_sha256=c.sha(previous)))
        modules = P / 'research/inputs/modules'
        clients = {
            'test_native_retail_profile_preferences':[modules],
            'test_native_retail_profile_menu':[],
            'test_native_retail_profile_resource_ids':[modules],
            'test_native_retail_profile_aggregate_main':[modules],
            'test_native_retail_game_records_main':[modules],
            'test_native_retail_game_session':[],
            'test_native_retail_save_startup':[]}
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*clients,'-j2'])
        executions = []
        for target,args in clients.items():
            r = c.execute('run-'+target,[c.BUILD/(target+'.exe'),*args])
            c.require(not r['stderr'] and (r['stdout'] == '' if target ==
                'test_native_retail_profile_resource_ids' else r['stdout'].startswith('PASS')),
                'actual client assertions: '+target)
            executions.append(dict(target=target,sha256=r['executable_sha256'],receipt='run-'+target+'.json'))
        missing_effects = c.execute('required-native-effects-link',[c.CMAKE,'--build',c.BUILD,
            '--target','test_native_profile_required_effects','-j2'],expected=None)
        effect_symbols = re.findall(r'undefined symbol: ([^\n]+)', missing_effects['stdout']+missing_effects['stderr'])
        required = set(menu['required_undefined_production_children']) | {'retire_matching_retail_secret_camera_from_446f80'}
        c.require(len(effect_symbols) == 5 and all(any(('::'+name+'(') in symbol for name in required)
            for symbol in effect_symbols) and all(any(('::'+name+'(') in symbol for symbol in effect_symbols)
            for name in required) and not (c.BUILD/'test_native_profile_required_effects.exe').exists(),
            'all five actual scene/actor effects remain strong production requirements')
        entry = c.execute('actual-entry-link',[c.CMAKE,'--build',c.BUILD,'--target',
            'test_native_startup_missing_bindings','-j2'],expected=None)
        undefined = re.findall(r'undefined symbol: ([^\n]+)',entry['stdout']+entry['stderr'])
        expected = set(prior['remaining_integration_symbols']) - {'initialize_retail_scene_service'}
        names = {re.search(r'main_binding::(\w+)\(',x).group(1) for x in undefined if re.search(r'main_binding::(\w+)\(',x)}
        c.require(names == expected and len(undefined) == len(names) == 27 and
            not (c.BUILD/'test_native_startup_missing_bindings.exe').exists(), 'actual current27 remaining Main effects')
        c.require(all(c.sha(p) == h for p,h in sources.items()), 'current source stable through checks')
        c.require(all(c.sha(p) == h for p,h in evidence.items()), 'evidence stable through checks')
        c.require(all(c.sha(p) == h for p,h in protected.items()), 'protected inputs after')
        result = dict(schema='native_profile_state_connected_verification_v1',
            status='PASS_NATIVE_PROFILE_STATE_CONNECTED_CPU_CHECKS',sources=sources,evidence=evidence,
            protected_inputs=protected,executed_clients=executions,original_word_joins=joined,
            prior_record=str(previous),prior_sha256=c.sha(previous),explicit_prior_source_changes=changed,
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            session_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_game_session.a'),
            remaining_integration_symbols=sorted(names),required_profile_effects=sorted(required),
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['Actual Main295660 current preference initializer; native aliases and external encoding verified.',
                'Aggregate/menu and resource-ID algorithms use explicit test-only scene/actor effects; production children remain undefined.',
                'Full171B10 parent/child lifecycle, remaining runtime fields, staging transfer and live64E318 producer remain open.',
                'No accepted boot/menu/combat/physical input/render/movie/audio or whole-game proof.'])
        c.save('verification.json',result)
        print(json.dumps(dict(status=result['status'],fresh_clients=len(executions),remaining_main=len(names),
            required_scene_actor_effects=len(required),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',type=type(error).__name__,message=str(error)))
        raise

if __name__ == '__main__':main()
