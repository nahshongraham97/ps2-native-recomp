"""Check the real native Main save-service join, without launching a game."""
import ctypes
import importlib.util
import json
import re
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
F = H.parents[2]
P = F.parents[2]
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s = importlib.util.spec_from_file_location('native_client_receipts',
    F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(s)
s.loader.exec_module(c)
c.OUT = H / (sys.argv[1] if len(sys.argv) > 1 else 'main-validation-01')

def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        prior_path = H / 'validation-01/verification.json'
        prior = json.loads(prior_path.read_text(encoding='utf-8'))
        c.require(prior['status'] == 'PASS_NATIVE_SAVE_PROFILE_DEPENDENCY_COMPONENTS' and
                  isinstance(prior['executed_clients'], list), 'actual component receipt schema')
        sources = dict(prior['sources'])
        cmake = str(F / 'cmake/native_game.cmake')
        c.require(all(c.sha(p) == h for p, h in sources.items() if p != cmake), 'all unchanged component pins')
        superseded = {cmake: dict(previous=sources[cmake], current=c.sha(cmake),
            reason='Add actual Main save-service source/link and connected client')}
        added = ['cmake/native_game.cmake', 'cmake/native_media.cmake', 'recomp/native_startup_codegen.cpp',
            'runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main.cpp',
            'runtime/src/native/retail_main_state.cpp', 'runtime/src/native/retail_save_startup.cpp',
            'runtime/src/native/retail_memory_owners.cpp', 'runtime/include/mka/retail_memory_owners.hpp',
            'runtime/include/mka/native_user_paths.hpp', 'runtime/src/native/native_user_paths.cpp',
            'tests/test_retail_save_startup.cpp', 'tests/test_retail_game_records_main.cpp',
            'tests/test_retail_pad_control.cpp', 'tests/test_native_retail_startup.cpp',
            'tests/test_native_startup_codegen.cpp', 'tests/test_native_startup_missing_bindings.cpp',
            'research/runs/native-retail-save-profile-20261121/check_main.py']
        sources.update({str(F / name): c.sha(F / name) for name in added})
        protected = prior['protected_inputs']
        c.require(all(c.sha(p) == h for p, h in protected.items()), 'protected inputs before')
        c.save('inputs.json', dict(schema='native_save_main_inputs_v1', sources=sources,
            protected_inputs=protected, prior_path=str(prior_path), prior_sha256=c.sha(prior_path),
            superseded=superseded))
        clients = {
            'test_native_retail_save_startup': [],
            'test_native_retail_game_records_main': [P / 'research/inputs/modules'],
            'test_native_retail_pad_control': [],
            'test_native_retail_startup': [c.OUT / 'exclusive-startup-content'],
            'test_native_startup_codegen': [P / 'research/inputs/SLUS_214.10']}
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *clients, '-j2'])
        current = []
        retained_main = []
        for target, args in clients.items():
            earlier = H / 'main-validation-02' / ('run-' + target + '.json')
            if c.OUT.name != 'main-validation-02' and target in [
                    'test_native_retail_save_startup', 'test_native_retail_game_records_main',
                    'test_native_retail_pad_control'] and earlier.exists():
                previous = json.loads(earlier.read_text(encoding='utf-8'))
                binary = c.BUILD / (target + '.exe')
                c.require(previous['exit'] == 0 and previous['stdout'].startswith('PASS') and
                    not previous['stderr'] and previous['executable_sha256'] ==
                    previous['executable_after_sha256'] == c.sha(binary), 'exact unchanged Main client execution')
                retained_main.append(dict(target=target, sha256=c.sha(binary),
                    receipt=str(earlier), receipt_sha256=c.sha(earlier)))
                continue
            r = c.execute('run-' + target, [c.BUILD / (target + '.exe'), *args])
            c.require(r['stdout'].startswith('PASS') and not r['stderr'], 'client assertions: ' + target)
            current.append(dict(target=target, sha256=r['executable_sha256'], receipt='run-' + target + '.json'))
        retained = []
        for row in prior['executed_clients']:
            c.require(c.sha(c.BUILD / (row['target'] + '.exe')) == row['sha256'], 'unchanged component binary')
            receipt = H / 'validation-01' / row['receipt']
            r = json.loads(receipt.read_text(encoding='utf-8'))
            c.require(r['exit'] == 0 and r['executable_sha256'] == r['executable_after_sha256'] == row['sha256'],
                      'retained exact component execution')
            retained.append(dict(row, receipt=str(receipt), receipt_sha256=c.sha(receipt)))
        archive = c.BUILD / 'libmka_native_retail_startup.a'
        nm = c.execute('startup-symbols', [c.BIN / 'llvm-nm.exe', '-C', archive])
        c.require(' T mka::retail::main_binding::initialize_retail_saved_profile_storage(' in nm['stdout'],
                  'real storage service definition compiled into production archive')
        required = set(prior['remaining_main_integrations_from_unchanged_archive'])
        required.remove('initialize_retail_game_scene_records')
        required.add('initialize_retail_saved_profile_records')
        c.require(len(required) == 28, 'explicit remaining record obligation, not an integration-count reduction')
        refused = c.execute('actual-entry-link', [c.CMAKE, '--build', c.BUILD,
            '--target', 'test_native_startup_missing_bindings', '-j2'], expected=None)
        missing = re.findall(r'undefined symbol: ([^\n]+)', refused['stdout'] + refused['stderr'])
        names = {re.search(r'main_binding::(\w+)\(', symbol).group(1) for symbol in missing
                 if re.search(r'main_binding::(\w+)\(', symbol)}
        c.require(len(missing) == len(names) == 28 and names == required and
                  not (c.BUILD / 'test_native_startup_missing_bindings.exe').exists(),
                  'actual complete entry still refuses only the exact 28 required Main effects')
        c.require(all(c.sha(p) == h for p, h in sources.items()), 'source stable through checks')
        c.require(all(c.sha(p) == h for p, h in protected.items()), 'protected inputs after')
        result = dict(schema='native_save_main_verification_v1',
            status='PASS_MAIN_NATIVE_SAVE_SERVICE_CONNECTED_CHECKS', sources=sources,
            protected_inputs=protected, executed_clients=current, retained_main_clients=retained_main,
            retained_component_clients=retained,
            prior_component_record=str(prior_path), prior_component_sha256=c.sha(prior_path),
            changed_build_registration=superseded, startup_archive_sha256=c.sha(archive),
            remaining_integration_symbols=sorted(names), complete_game_entry_linkable=False,
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=['Actual Main service readiness joined before still-unimplemented saved-profile records.',
                    'Default Windows known-folder resolution branch compiled only; fixture uses owned native storage.',
                    'Complete profile gameplay schema, menu effects and later startup/frame/input/render/audio remain open.',
                    'No full-game, physical input, movie/render or sound acceptance.'])
        c.save('verification.json', result)
        print(json.dumps(dict(status=result['status'], fresh_clients=len(current),
            retained_main_clients=len(retained_main),
            retained_components=len(retained), required_main_effects=len(names), game_executed=False)))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', type=type(error).__name__, message=str(error)))
        raise

if __name__ == '__main__': main()
