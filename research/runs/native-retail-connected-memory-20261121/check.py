"""Required connected startup memory checks; no game or physical endpoints."""
import importlib.util
from pathlib import Path
import json

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = H / (c.sys.argv[1] if len(c.sys.argv) > 1 else 'validation-01')

if __name__ == '__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        prior = json.loads((F / 'research/runs/native-retail-records-workspace-20261121/validation-03/verification.json').read_text())
        paths = ['runtime/include/mka/native_heap.hpp', 'runtime/src/native/native_heap.cpp',
                 'runtime/include/mka/retail_memory_owners.hpp', 'runtime/src/native/retail_memory_owners.cpp',
                 'tests/test_native_heap_owner.cpp', 'tests/test_native_heap_inflight.cpp',
                 'tests/test_retail_memory_owners.cpp', 'runtime/include/mka/wave_allocation.hpp',
                 'runtime/src/native/wave_allocation.cpp', 'cmake/native_game.cmake']
        sources = {str(F / name): c.sha(F / name) for name in paths}
        protected = prior['protected_inputs']
        def stable():
            for group in [sources, protected]:
                c.require(all(c.sha(path) == pin for path, pin in group.items()), 'stable memory/protected input bytes')
        stable()
        c.save('source-preflight.json', dict(sources=sources, protected_inputs=protected))
        targets = ['test_native_heap_owner', 'test_native_heap_inflight', 'test_native_retail_memory_owners']
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *targets, '-j2'])
        clients = []
        for target in targets:
            receipt = c.execute(target, [c.BUILD / (target + '.exe')])
            c.require('PASS' in receipt['stdout'] and not receipt['stderr'] and 'reused_from' not in receipt,
                      'fresh affected memory checks ' + target)
            clients.append(dict(target=target, sha256=receipt['executable_sha256']))
        stable()
        c.save('verification.json', dict(schema='native_connected_memory_checks_v1',
            status='PASS_REQUIRED_NATIVE_MEMORY_RESET', sources=sources, protected_inputs=protected,
            executed_clients=clients, game_executed=False, physical_endpoints_opened=False,
            limits=['Only directly affected native memory checks; production Main bridge not complete.',
                    'Actual kind0 ownership subtree reset only; no kind1 requested-tag reset claim.',
                    'No game executable, frames, audio or gameplay acceptance.']))
        print('PASS_REQUIRED_NATIVE_MEMORY_RESET: 3 fresh clients; complete game integration remains open')
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
