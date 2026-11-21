"""Execute the existing retail selector with its generated typed field load."""
import hashlib, importlib.util, json, re
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
OUT = H / 'validation-01'
D = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-typed-consumer-28'
spec = importlib.util.spec_from_file_location('checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = OUT
if __name__ == '__main__':
    OUT.mkdir(exist_ok=False)
    try:
        frozen = json.loads((D / 'INTEGRATION.json').read_text(encoding='utf-8'))
        source = dict(frozen['sources'])
        source[str(Path(__file__).resolve())] = c.sha(__file__)
        c.require(all(c.sha(p) == h for p, h in source.items()), 'exact frozen two-file consumer source delta')
        prior_path = F / 'research/runs/native-typed-retail-20261121/validation-02/verification.json'
        c.require(c.sha(prior_path) == '8d1d48e7c3ac60babea430dd1d8e29944c0c1975fa142974d048671a990269ed', 'prior typed checkpoint')
        prior = json.loads(prior_path.read_text(encoding='utf-8'))
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', 'test_native_retail_input_selection', '-j2'])
        target = 'test_native_retail_input_selection'
        receipt = c.execute(target, [c.BUILD / (target + '.exe')])
        c.require(receipt['stdout'].strip() == 'PASS native physical-role source candidate selection, controls, diagnostics and ownership' and
                  not receipt['stderr'], 'actual existing candidate branch/lifetime/fault assertions')
        symbols = c.execute('actual-selector-symbols', [c.BIN / 'llvm-nm.exe', '-C', c.BUILD / (target + '.exe')])
        for symbol in ['mka::retail::NativeRetailInputSelection::select_physical(',
                       'mka::retail::generated::retail_scalar_mode_get_185cb0(',
                       'mka::retail::NativeRetailGameSession::scalar_mode(']:
            c.require(symbol in symbols['stdout'], 'actual selector/generated/session linkage: ' + symbol)
        c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_',
                               symbols['stdout']), 'no guest/device/offline compiler in actual selector consumer')
        unchanged = []
        for row in prior['client_identities'] + prior['unchanged_prior_binary_identities']:
            if row['target'] == target:
                continue
            c.require(c.sha(c.BUILD / (row['target'] + '.exe')) == row['sha256'], 'unaffected dated binary identity')
            unchanged.append(row)
        archive = c.BUILD / 'libmka_native_retail_startup.a'
        nm = c.execute('actual-main-obligations', [c.BIN / 'llvm-nm.exe', '-C', archive])
        missing = set(re.findall(r' U mka::retail::main_binding::(\w+)\(', nm['stdout'])) - set(
            re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(', nm['stdout']))
        c.require(missing == set(prior['remaining_integration_symbols']), 'same exact39 open actual main integrations')
        negative = c.execute('incomplete-entry-link', [c.CMAKE, '--build', c.BUILD, '--target',
            'test_native_startup_missing_bindings', '-j2'], expected=None)
        c.require(all('main_binding::' + name + '(' in negative['stdout'] + negative['stderr'] for name in missing)
            and not (c.BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'no incomplete entry executable')
        c.require(all(c.sha(p) == h for p, h in source.items()), 'source stable through actual affected client')
        c.require(all(c.sha(p) == h for p, h in prior['protected_inputs'].items()), 'protected inputs unchanged')
        result = dict(schema='native_typed_scalar_consumer_verification_v1', status='PASS_ACTUAL_NATIVE_SELECTOR_TYPED_FIELD',
            sources=source, executed_client=dict(target=target, sha256=receipt['executable_sha256']),
            unchanged_prior_binary_identities=unchanged, prior_verification_sha256=c.sha(prior_path),
            main_archive_sha256=c.sha(archive), typed_archive_sha256=c.sha(c.BUILD / 'libmka_native_typed_retail.a'),
            remaining_integration_symbols=sorted(missing), complete_game_entry_linkable=False,
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=['281930 is a direct original field load; generated getter is source-equivalent reuse, not a claimed original call edge.',
                    'Actual native selector component only; full Main/host input/task/render/audio remain open.',
                    '40 previous binary identities remain unchanged and were not reexecuted.'])
        c.save('verification.json', result)
        print(json.dumps({k: result[k] for k in ['status', 'game_executed', 'complete_game_entry_linkable']}))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
