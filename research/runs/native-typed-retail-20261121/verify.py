"""Offline compiler and compiled native clients; never launches the game."""
import ctypes, hashlib, json, os, re, subprocess, sys
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / (sys.argv[1] if len(sys.argv) > 1 else 'validation-01')
BUILD = F / 'build_native_services_only_20261119-01'
BIN = P.parent / 'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE = P.parent / 'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
ENV = dict(os.environ, PATH=str(BIN) + os.pathsep + os.environ['PATH'])

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def save(name, value):
    with (OUT / name).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def execute(name, argv, expected=0):
    argv = list(map(str, argv))
    before = sha(argv[0])
    prior_receipt = HERE / 'validation-01' / (name + '.json')
    if name in ['test_native_retail_task_registry', 'test_native_typed_codegen'] and OUT.name != 'validation-01' and prior_receipt.exists():
        prior = json.loads(prior_receipt.read_text(encoding='utf-8'))
        if prior['argv'] == argv and prior['exit'] == 0 and prior['executable_sha256'] == before == prior['executable_after_sha256']:
            prior['reused_from'] = str(prior_receipt)
            prior['reused_record_sha256'] = sha(prior_receipt)
            save(name + '.json', prior)
            return prior
    proc = subprocess.run(argv, cwd=F, env=ENV, capture_output=True, text=True,
                          creationflags=subprocess.CREATE_NO_WINDOW)
    receipt = dict(argv=argv, exit=proc.returncode, stdout=proc.stdout,
                   stderr=proc.stderr, executable_sha256=before,
                   executable_after_sha256=sha(argv[0]))
    save(name + '.json', receipt)
    require(before == receipt['executable_after_sha256'], 'tool/executable changed: ' + name)
    require((proc.returncode == expected) if expected is not None else proc.returncode != 0,
            'unexpected exit: ' + name)
    return receipt

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(exist_ok=False)
    try:
        previous = F / 'research/runs/native-retail-input-selection-20261121/validation-01/verification.json'
        require(sha(previous) == '988a18ca7e291f8ef5c9bcd1b948f66ca5fb622fe8772c3b6230044835abeb69',
                'exact prior verified checkpoint')
        prior = json.loads(previous.read_text(encoding='utf-8'))
        save('schema-preflight.json', {k: dict(type=type(v).__name__,
            size=len(v) if isinstance(v, (list, dict)) else None) for k, v in prior.items()})
        require(isinstance(prior['sources'], dict) and isinstance(prior['fixtures'], list) and
                isinstance(prior['remaining_integration_symbols'], list), 'prior schema types')
        source = dict(prior['sources'])
        require(all(sha(path) == expected for path, expected in source.items()),
                'all prior pinned source inputs unchanged')
        added = ['CMakeLists.txt', 'cmake/native_typed.cmake',
            'recomp/native_typed_ir.hpp', 'recomp/native_typed_ir.cpp',
            'recomp/native_typed_codegen.hpp', 'recomp/native_typed_codegen.cpp',
            'recomp/native_typed_main.cpp', 'recomp/r5900.hpp', 'recomp/r5900.cpp',
            'recomp/native_closure_codegen.hpp', 'recomp/native_closure_codegen.cpp',
            'runtime/include/mka/retail_task_registry.hpp', 'runtime/src/native/retail_task_registry.cpp',
            'tests/test_retail_task_registry.cpp', 'tests/test_native_typed_codegen.cpp',
            'tests/test_native_typed_retail.cpp', 'research/runs/native-typed-retail-20261121/verify.py']
        source.update({str(F / path): sha(F / path) for path in added})
        protected = {str(F / 'runtime/src/iop/mcserv.cpp'): 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
            str(F / 'runtime/src/iop/msl_native.cpp'): '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',
            str(P / 'research/doc-audit-20261118/inventory.json'): '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6',
            str(P / 'research/inputs/SLUS_214.10'): 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'}
        require(all(sha(path) == expected for path, expected in protected.items()), 'protected inputs before')
        clients = ['test_native_retail_task_registry', 'test_native_typed_codegen', 'test_native_typed_retail']
        execute('build', [CMAKE, '--build', BUILD, '--target', *clients, '-j2'])
        elf = P / 'research/inputs/SLUS_214.10'
        fixtures = []
        for target in clients:
            args = [elf, HERE / 'validation-01/fixture-generated'] if target == 'test_native_typed_codegen' else []
            receipt = execute(target, [BUILD / (target + '.exe'), *args])
            require(not receipt['stderr'], 'client error output: ' + target)
            if target == 'test_native_typed_retail':
                require(receipt['stdout'].startswith('PASS compiled typed retail AOT:'), 'generated runtime assertions')
            elif target == 'test_native_retail_task_registry':
                require(re.fullmatch(r'PASS native retail ordered task registry: [1-9][0-9]* publication allocation cuts; actual task producers/dispatch/main gate open\n', receipt['stdout']), 'registry assertions')
            else:
                require(not receipt['stdout'], 'offline fixture success is quiet')
            fixtures.append(dict(target=target, sha256=receipt['executable_sha256']))
        generated_dirs = list(BUILD.glob('native-typed-retail-*'))
        matching = [d for d in generated_dirs if all((d / name).read_bytes() ==
            (HERE / 'validation-01/fixture-generated' / name).read_bytes() for name in
            ('native_typed_retail.hpp', 'native_typed_retail.cpp'))]
        require(len(matching) == 1, 'one exact source-generated implementation')
        generated = matching[0]
        execute('incremental-compiler', [BUILD / 'native_typed_recomp.exe', elf, generated])
        generated_sha = {name: sha(generated / name) for name in ('native_typed_retail.hpp', 'native_typed_retail.cpp')}
        archived = BUILD / 'libmka_native_typed_retail.a'
        archive_before = sha(archived)
        cpp = generated / 'native_typed_retail.cpp'
        original = cpp.read_bytes()
        original_stat = cpp.stat()
        (OUT / 'generated-before-tamper.cpp').write_bytes(original)
        try:
            cpp.write_bytes(original + b'\n// deliberate verification-only tamper\n')
            refused = execute('tampered-generated-build', [CMAKE, '--build', BUILD,
                '--target', 'test_native_typed_retail', '-j2'], expected=None)
            require('existing generated native source differs' in refused['stdout'] + refused['stderr'],
                    'always-run authenticating prerequisite refused newer tampered generated source')
            require(sha(archived) == archive_before, 'tampered generated source never reached archive')
        finally:
            cpp.write_bytes(original)
            os.utime(cpp, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        execute('restored-generated-build', [CMAKE, '--build', BUILD,
            '--target', 'test_native_typed_retail', '-j2'])
        require(sha(archived) == archive_before and all(sha(generated / n) == h for n, h in generated_sha.items()),
                'restored exact output/archive identity')
        symbols = {}
        for target in ['test_native_retail_task_registry', 'test_native_typed_retail']:
            receipt = execute(target + '-symbols', [BIN / 'llvm-nm.exe', '-C', BUILD / (target + '.exe')])
            require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_',
                                  receipt['stdout']), 'no runtime guest/device/compiler symbols: ' + target)
            if target == 'test_native_typed_retail':
                require('mka::retail::generated::retail_scalar_mode_get_185cb0(' in receipt['stdout'] and
                        'mka::retail::NativeRetailGameSession::scalar_mode(' in receipt['stdout'],
                        'actual generated code and native owner linked')
            symbols[target] = sha(BUILD / (target + '.exe'))
        unchanged = []
        for row in prior['fixtures']:
            require(sha(BUILD / (row['target'] + '.exe')) == row['sha256'],
                    'prior current binary changed: ' + row['target'])
            unchanged.append(row)
        main_archive = BUILD / 'libmka_native_retail_startup.a'
        require(sha(main_archive) == prior['archive_sha256'], 'unchanged actual main archive')
        nm = execute('main-link-obligations', [BIN / 'llvm-nm.exe', '-C', main_archive])
        missing = set(re.findall(r' U mka::retail::main_binding::(\w+)\(', nm['stdout'])) - set(
            re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(', nm['stdout']))
        require(missing == set(prior['remaining_integration_symbols']), 'exact actual remaining main bindings')
        refused = execute('incomplete-entry-link', [CMAKE, '--build', BUILD, '--target',
            'test_native_startup_missing_bindings', '-j2'], expected=None)
        require(all('main_binding::' + n + '(' in refused['stdout'] + refused['stderr'] for n in missing)
                and not (BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'full entry fails closed')
        require(all(sha(path) == expected for path, expected in source.items()), 'source stable through checks')
        require(all(sha(path) == expected for path, expected in protected.items()), 'protected inputs after')
        result = dict(schema='native_typed_retail_cpu_verification_v1',
            status='PASS_TYPED_AOT_AND_TASK_REGISTRY_CPU_CHECKS', sources=source,
            client_identities=fixtures,
            new_executions=[r for r in fixtures if not json.loads((OUT / (r['target'] + '.json')).read_text(encoding='utf-8')).get('reused_from')],
            reused_component_executions=[r for r in fixtures if json.loads((OUT / (r['target'] + '.json')).read_text(encoding='utf-8')).get('reused_from')],
            unchanged_prior_binary_identities=unchanged,
            prior_verification_path=str(previous), prior_verification_sha256=sha(previous),
            generated_directory=str(generated), generated_sha256=generated_sha,
            typed_archive_sha256=sha(archived), registry_archive_sha256=sha(BUILD / 'libmka_native_retail_task_registry.a'),
            compiler_tool_sha256=sha(BUILD / 'native_typed_recomp.exe'),
            protected_inputs=protected, remaining_integration_symbols=sorted(missing),
            complete_game_entry_linkable=False, game_executed=False,
            physical_endpoints_opened=False, frames=None, audio=None,
            limits=['Only original scalar getter/setter are generated; general CFG/calls/memory contracts stay closed.',
                    'Task lookup/retirement only; actual task bodies, dispatch/reset/Main gate unbound.',
                    'Prior38 binaries are unchanged identities, not new executions or new game proof.',
                    'No main menu, combat, visual, physical input or audio acceptance.'])
        save('verification.json', result)
        print(json.dumps({k: result[k] for k in ['status', 'game_executed', 'complete_game_entry_linkable']}))
    except Exception as error:
        save('failure.json', dict(status='FAILED', type=type(error).__name__, message=str(error)))
        raise
