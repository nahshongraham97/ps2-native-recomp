"""Compile and execute native Wave/ownership policy only; never launch a game."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, subprocess, sys

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
R = P / 'research/runs/native-retail-startup-20261120/native-heap/role-accounting'
spec = importlib.util.spec_from_file_location('_wave_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
         bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native Release without legacy game units')
    prior_path = F / 'research/runs/native-heap-20261120/validation-03/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_HEAP_OWNERSHIP_AND_REAL_HOST_LIFETIME_CUTS' and
         len(prior['source_after_sha256']) == 720 and len(prior['fixtures']) == 2, 'actual published prior certificate')
    before_record = json.loads((R / 'before/provenance.json').read_bytes())
    need(isinstance(before_record['source_sha256'], dict) and len(before_record['source_sha256']) == 5, 'actual five-file snapshot schema')
    changed = {str(F / name) for name in before_record['source_sha256']}
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched) == 715 and all(sha(n) == h for n, h in untouched.items()), '715 prior inputs unchanged')
    for name, digest in before_record['source_sha256'].items():
        need(sha(R / 'before' / name) == digest == prior['source_after_sha256'][str(F / name)], 'actual original before bytes')
    review_path = R / 'SOURCE_REVIEW.json'; review = json.loads(review_path.read_bytes())
    need(review['status'] == 'SOURCE_REVIEW_NATIVE_WAVE_ALLOCATION_AND_HEAP_ROLES_VERIFIED' and
         review['source_only'] is True and isinstance(review['input_sha256'], dict) and len(review['input_sha256']) == 18,
         'actual current source review schema')
    need(all(sha(n) == h for n, h in review['input_sha256'].items()), 'actual reviewed code/source unchanged')
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(map(pathlib.Path, review['input_sha256']))
    paths.update(q for q in R.rglob('*') if q.is_file())
    paths.update([prior_path, pathlib.Path(__file__).resolve()])
    authored = [F / name for name in ['cmake/native_game.cmake', 'runtime/include/mka/native_heap.hpp',
        'runtime/src/native/native_heap.cpp', 'tests/test_native_heap_owner.cpp', 'tests/test_native_heap_inflight.cpp',
        'runtime/include/mka/wave_allocation.hpp', 'runtime/src/native/wave_allocation.cpp', 'tests/test_wave_allocation.cpp']]
    paths.update(authored)
    for path in authored:
        data = path.read_bytes()
        need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in data.splitlines()), 'authored LF/EOF: ' + str(path))
    before = {str(q): sha(q) for q in sorted(paths)}
    for name, digest in c.PRESERVE.items(): need(sha(F / name) == digest, 'protected IOP bytes')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, unchanged_prior_pins=len(untouched), compiler_sha256=sha(c.BIN / 'clang++.exe'),
        scope='Actual native allocation transactions, roles and request/diagnostic ownership; retail producer remains unbound'))
    cases = [
        ('heap-owner', 'test_native_heap_owner',
         'PASS_NATIVE_HEAP_OWNER: 4 groups real aligned storage/roles/request guards/routing/lifetime/concurrency; no retail initialization or source callback claim',
         ['mka::memory::NativeHeapOwner::retain_request', 'mka::memory::NativeHeapRequest::release']),
        ('heap-inflight', 'test_native_heap_inflight',
         'PASS_NATIVE_HEAP_INFLIGHT: real host allocation success/failure and deletion cuts; test-only linker gates',
         ['mka::memory::NativeHeapOwner::allocate', 'mka::memory::NativeAllocation::release']),
        ('wave-policy', 'test_native_wave_allocation',
         'PASS_NATIVE_WAVE_ALLOCATION: 8 groups source-ordered native routes/ownership/diagnostics; test-only host faults, no retail producer execution',
         ['mka::memory::NativeWaveAllocator::allocate', 'mka::memory::NativeHeapOwner::retain_request'])]
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', *[x[1] for x in cases], '-j2'])
    fixtures = []
    for label, target, footer, required in cases:
        print('RUN ' + label, flush=True)
        run = c.execute(label + '.json', [c.BUILD / (target + '.exe')])
        need(run['stdout'].strip() == footer, 'complete assertions: ' + label)
        audit = c.symbols(label + '-symbols', c.BUILD / (target + '.exe'), run['executable_sha256'], required, True)
        fixtures.append(dict(component=label, target=target, executed_target_sha256=run['executable_sha256'], audit=audit))
    archives = []
    for target, required in [('libmka_native_heap.a', ['mka::memory::NativeHeapOwner::retain_request']),
                             ('libmka_native_wave_allocation.a', ['mka::memory::NativeWaveAllocator::allocate'])]:
        archive = c.BUILD / target
        archives.append(c.symbols(target + '-symbols', archive, sha(archive), required, False))
    raw = c.execute('wave-raw-wrapper-symbols.json', [c.BIN / 'llvm-nm.exe', '--defined-only', c.BUILD / 'test_native_wave_allocation.exe'])
    need('__wrap__ZnwySt11align_val_t' in raw['stdout'], 'actual test-only allocation wrapper definition')
    ninja = c.BUILD / 'build.ninja'; text = ninja.read_text(encoding='utf-8'); rules = []
    for label, target, _, _ in cases:
        match = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
        need(match and all(s not in match.group() for s in ['ps2runtime', 'source_oracle']), 'native-only actual linkage')
        need(('--wrap=' in match.group()) == (label != 'heap-owner'), 'fault gates limited to their isolated fixtures')
        rules.append(dict(target=target, actual_build_rule=match.group()))
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rules=rules, archives=archives))
    need(before == {n: sha(n) for n in before}, 'measured inputs stable through execution')
    for name, digest in c.PRESERVE.items(): need(sha(F / name) == digest, 'protected IOP bytes after execution')
    write('verification.json', dict(status='PASS_NATIVE_WAVE_TRANSACTIONS_AND_ACTUAL_OWNER_ACCOUNTING',
        source_after_sha256=before, unchanged_prior_pins=len(untouched), fixtures=fixtures,
        frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], limits=[
            'Actual native transaction policy/ownership and test-only allocation faults; no original retail producer execution',
            'Actual native startup, overlay readiness/loading/typed live-object retirement and full12DA40 integration remain OPEN',
            'No source guest reservations/addresses/caps/byte charges or state-machine substitutes',
            'Diagnostic component supports same-thread reentry; no concurrent sink/atomic cross-owner snapshot claim',
            'No game/oracle/physical GPU/audio/input endpoints, drivers/models/settings or user cards/saves/keys touched']))
    print(json.dumps(dict(status='PASS_NATIVE_WAVE_TRANSACTIONS_AND_ACTUAL_OWNER_ACCOUNTING', fixtures=len(fixtures), pins=len(before))), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-[0-9]+', sys.argv[1]): raise SystemExit('usage: verify.py validation-NN')
    c.OUT = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    c.OUT.mkdir(exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try: verify()
    except Exception as error:
        c.write('aborted.json', dict(status='NATIVE_WAVE_ALLOCATION_CHECK_FAILED', error=str(error)))
        raise
