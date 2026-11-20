"""Compile actual startup/owner-group components; no game or device execution."""
import csv, ctypes, datetime, hashlib, importlib.util, json, os, pathlib, re, struct, subprocess, sys

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'native-heap/retail-owner-group'
spec = importlib.util.spec_from_file_location('_owners_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
         bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release')
    prior_path = F / 'research/runs/native-wave-allocation-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_WAVE_TRANSACTIONS_AND_ACTUAL_OWNER_ACCOUNTING' and
         len(prior['source_after_sha256']) == 749 and len(prior['fixtures']) == 3, 'actual prior schema')
    old = json.loads((R / 'before/provenance.json').read_bytes())
    need(isinstance(old['source_sha256'], dict) and len(old['source_sha256']) == 5, 'five exact pre-integration snapshots')
    for name, digest in old['source_sha256'].items():
        need(sha(R / 'before' / name) == digest == prior['source_after_sha256'][str(F / name)], 'actual prior source bytes')
    changed = {str(F / name) for name, digest in old['source_sha256'].items() if sha(F / name) != digest}
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(changed) == 4 and len(untouched) == 745 and all(sha(n) == h for n, h in untouched.items()), '745 prior inputs unchanged')
    review_path = R / 'FINAL_SOURCE_REVIEW.json'; review = json.loads(review_path.read_bytes())
    need(review['status'] == 'SOURCE_REVIEW_NATIVE_RETAIL_MEMORY_OWNER_GROUP_AND_FAULT_FIXTURE_VERIFIED' and
         review['source_only'] is True and isinstance(review['input_sha256'], dict) and len(review['input_sha256']) == 14,
         'actual successor review schema')
    need(all(sha(n) == h for n, h in review['input_sha256'].items()), 'actual reviewed code/evidence pins')
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(map(pathlib.Path, review['input_sha256']))
    paths.update(q for q in R.rglob('*') if q.is_file())
    paths.update([prior_path, pathlib.Path(__file__).resolve()])
    original = P / 'research/inputs/SLUS_214.10'; elf = original.read_bytes()
    need(sha(original) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original source ELF')
    phoff, = struct.unpack_from('<I', elf, 28); size, count = struct.unpack_from('<HH', elf, 42)
    segments = [struct.unpack_from('<8I', elf, phoff + i * size) for i in range(count)]
    joined = 0
    for report_path, status in [(R / 'SOURCE.json', 'RETAIL_MAIN_HEAP_GROUP_ORDER_AND_POLICY_SOURCE_VERIFIED'),
                                (S / 'MAIN_AFTER_MEMORY_SOURCE.json', 'RETAIL_MAIN_AFTER_MEMORY_ORDER_AND_PRODUCER_EDGES_VERIFIED')]:
        report = json.loads(report_path.read_bytes())
        need(report['status'] == status and report['source_only'] is True and isinstance(report['input_sha256'], dict) and
             isinstance(report['primary_exports'], list), 'actual source export producer schema')
        paths.update([report_path, report_path.with_suffix('.md')])
        need(all(sha(n) == h for n, h in report['input_sha256'].items()), 'original source evidence stable')
        paths.update(map(pathlib.Path, report['input_sha256']))
        for export in report['primary_exports']:
            q = pathlib.Path(export['path']); need(sha(q) == export['sha256'], 'primary word export bytes'); paths.add(q)
            rows = list(csv.DictReader(q.read_text(encoding='utf-8-sig').splitlines()))
            need(len(rows) == export['words'] and {'va', 'file_offset', 'le32'}.issubset(rows[0]), 'actual word-row schema/count')
            for row in rows:
                va, offset, word = (int(row[k], 16) for k in ['va', 'file_offset', 'le32'])
                mapped = [s[1] + va - s[2] for s in segments if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
                need(mapped == [offset] and struct.unpack_from('<I', elf, offset)[0] == word, 'original PT_LOAD bytes')
                joined += 1
    need(joined == 1222, '669 memory group plus553 following-main source words')
    authored = [F / n for n in ['cmake/native_game.cmake', 'recomp/native_startup_codegen.cpp',
        'tests/test_native_startup_codegen.cpp', 'tests/test_native_retail_startup.cpp',
        'runtime/include/mka/retail_memory_owners.hpp', 'runtime/src/native/retail_memory_owners.cpp',
        'tests/test_retail_memory_owners.cpp', 'tests/test_retail_memory_owner_faults.cpp']]
    paths.update(authored)
    for q in authored:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in b.splitlines()), 'authored LF/EOF')
    before = {str(q): sha(q) for q in sorted(paths)}
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'unrelated IOP bytes unchanged')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, unchanged_prior_pins=len(untouched), primary_words_rejoined=joined,
        compiler_sha256=sha(c.BIN / 'clang++.exe'), scope='Actual generated state owns native heap group; full entry remains unresolved'))
    cases = [
        ('owner-group', 'test_native_retail_memory_owners', [],
         'PASS_NATIVE_RETAIL_MEMORY_OWNERS: 4 groups ordered identities/policies/real storage; owner-group phase only, no full retail main',
         ['mka::retail::NativeRetailMemoryOwners::create', 'mka::memory::NativeWaveAllocator::allocate']),
        ('owner-faults', 'test_native_retail_memory_owner_faults', [], None,
         ['mka::retail::NativeRetailMemoryOwners::bind_wave_initial']),
        ('startup-source', 'test_native_startup_codegen', [original],
         'PASS native retail startup source identity, ordered prerequisites and owned emission; no game boot',
         ['recomp::emit_native_startup_closure']),
        ('startup-runtime', 'test_native_retail_startup', [c.OUT / 'startup-owned'],
         'PASS generated retail startup owns actual arguments/content/memory group; failed initialization is unpublished and full entry link remains blocked',
         ['mka::retail::initialize_retail_memory_owners', 'mka::retail::NativeRetailMemoryOwners::create', 'mka::content::NativeContentVolume::open_resource'])]
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', *[x[1] for x in cases], 'mka_native_retail_startup', '-j2'])
    fixtures = []; fault_counts = None
    for label, target, args, footer, required in cases:
        print('RUN ' + label, flush=True)
        run = c.execute(label + '.json', [c.BUILD / (target + '.exe'), *args])
        if footer: need(run['stdout'].strip() == footer, 'complete actual assertions: ' + label)
        else:
            match = re.fullmatch(r'PASS_NATIVE_RETAIL_MEMORY_OWNER_FAULTS: create=([1-9][0-9]*) bind=([1-9][0-9]*) forced ordinary allocation cuts; owner-group/binding rollback only, no retail startup/game', run['stdout'].strip())
            need(match is not None, 'actual completed dynamic fault count footer')
            fault_counts = dict(create=int(match[1]), bind=int(match[2]))
        audit = c.symbols(label + '-symbols', c.BUILD / (target + '.exe'), run['executable_sha256'], required, True)
        fixtures.append(dict(component=label, target=target, executed_target_sha256=run['executable_sha256'], audit=audit))
    generated_key = hashlib.sha256((sha(F / 'recomp/native_startup_codegen.cpp') + ':' + sha(F / 'recomp/native_closure_codegen.cpp')).encode()).hexdigest()
    current = c.BUILD / ('native-retail-startup-' + generated_key)
    generated = {str(current / n): sha(current / n) for n in ['native_retail_startup.hpp', 'native_retail_startup.cpp', 'native_retail_entry.cpp']}
    archive = c.BUILD / 'libmka_native_retail_startup.a'
    archive_audit = c.symbols('startup-archive', archive, sha(archive), ['mka::retail::initialize_retail_memory_owners'], False)
    undefined = c.execute('startup-required-bindings.json', [c.BIN / 'llvm-nm.exe', '-u', '-C', archive])
    required = ['initialize_native_retail_crt', 'finalize_native_retail_pre_main', 'initialize_native_retail_platform', 'continue_native_retail_main']
    need(all('mka::retail::' + n in undefined['stdout'] for n in required), 'four actual production bindings remain required')
    args = list(map(str, [c.CMAKE, '--build', c.BUILD, '--target', 'test_native_startup_missing_bindings', '-j2']))
    result = subprocess.run(args, cwd=F, env=c.ENV, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    write('missing-bindings-link.json', dict(argv=args, exit=result.returncode, stdout=result.stdout, stderr=result.stderr,
        expected_failure=True, required_native_bindings=required, tool_sha256=sha(c.CMAKE)))
    need(result.returncode != 0 and all(n in result.stdout + result.stderr for n in required), 'incomplete entry still refused by linker')
    ninja = c.BUILD / 'build.ninja'; text = ninja.read_text(encoding='utf-8'); rules = []
    for _, target, _, _, _ in cases:
        match = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
        need(match and all(s not in match.group() for s in ['ps2runtime', 'source_oracle', '--wrap=']), 'native-only actual linkage')
        rules.append(dict(target=target, actual_build_rule=match.group()))
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rules=rules, archive=archive_audit))
    need(before == {n: sha(n) for n in before}, 'all measured inputs stable through build and execution')
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved IOP after execution')
    write('verification.json', dict(status='PASS_NATIVE_RETAIL_OWNER_GROUP_AND_STARTUP_OWNERSHIP', source_after_sha256=before,
        unchanged_prior_pins=len(untouched), primary_words_rejoined=joined, fixtures=fixtures,
        actual_fault_counts=fault_counts, generated_runtime_sha256=generated, missing_production_bindings=required,
        frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], limits=[
            'Actual native owner-group and generated subordinate startup component only; full entry cannot link',
            'Wave readiness supplied explicitly by fixture; actual profile memory/reset/fixed/MPEG/middleware/overlay/input/main remain OPEN',
            'ordinary allocation faults are fixture-induced; no arbitrary PS2 capacity or fake arena/device success',
            'No game/oracle/physical GPU/audio/input, drivers/local models/settings or user card/save/key operations; no frozen corpus rerun']))
    print(json.dumps(dict(status='PASS_NATIVE_RETAIL_OWNER_GROUP_AND_STARTUP_OWNERSHIP', fixtures=len(fixtures), pins=len(before), faults=fault_counts)), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-[0-9]+', sys.argv[1]): raise SystemExit('usage: verify.py validation-NN')
    c.OUT = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    c.OUT.mkdir(exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try: verify()
    except Exception as error:
        c.write('aborted.json', dict(status='NATIVE_RETAIL_OWNER_GROUP_CHECK_FAILED', error=str(error)))
        raise
