"""Actual native tier transactions with versioned source corrections; no game."""
import csv, ctypes, datetime, importlib.util, json, os, pathlib, re, struct, subprocess, sys

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'class-tier-allocation'
spec = importlib.util.spec_from_file_location('_tier_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
         bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release')
    prior_path = F / 'research/runs/native-allocation-class-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_DYNAMIC_ALLOCATION_CLASS_AND_RETAIL_PROFILE_SELECTION' and
         len(prior['source_after_sha256']) == 923 and len(prior['fixtures']) == 2, 'actual previous certificate')
    ledger_path = R / 'CURRENT_SOURCE_RECONCILIATION.json'; ledger = json.loads(ledger_path.read_bytes())
    need(ledger['status'] == 'CURRENT_CLASS_AND_PROFILE_SOURCE_REPORTS_RECONCILED' and
         isinstance(ledger['versions'], list) and len(ledger['versions']) == 4 and
         isinstance(ledger['historical_input_redirections'], list), 'actual correction ledger schema')
    snapshots = {}
    for item in ledger['versions']:
        need(sha(item['path']) == item['current_sha256'] and sha(item['archived_path']) == item['previous_sha256'], 'exact old/new report versions')
        snapshots[(item['path'], item['previous_sha256'])] = pathlib.Path(item['archived_path'])
    old = json.loads((R / 'before-root-integration/provenance.json').read_bytes())
    need(isinstance(old['source_sha256'], dict) and len(old['source_sha256']) == 3, 'three exact pre-integration code snapshots')
    for name, digest in old['source_sha256'].items():
        q = R / 'before-root-integration' / name
        need(sha(q) == digest == prior['source_after_sha256'][str(F / name)], 'exact original code bytes')
        snapshots[(str(F / name), digest)] = q
    resolutions = []
    def resolve(name, digest, producer):
        q = pathlib.Path(name)
        if sha(q) == digest: return q
        need((name, digest) in snapshots, 'unrecognized changed evidence: ' + name)
        retained = snapshots[(name, digest)]
        need(sha(retained) == digest, 'preserved historical input bytes')
        resolutions.append(dict(producer=producer, original_path=name, expected_sha256=digest, retained_path=str(retained)))
        return retained
    changed = {name for name, digest in prior['source_after_sha256'].items() if sha(name) != digest}
    expected_changed = {str(F / name) for name in old['source_sha256']}
    expected_changed.update(str(S / n) for n in ['PROFILE_MEMORY_SOURCE.md', 'PROFILE_MEMORY_SOURCE.json'])
    need(changed == expected_changed and len(changed) == 5, 'three code declarations/targets and two corrected profile documents only')
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched) == 918 and all(sha(n) == h for n, h in untouched.items()), '918 unchanged prior inputs')
    for name in changed: resolve(name, prior['source_after_sha256'][name], 'previous component certificate')
    current_profile = json.loads((S / 'PROFILE_MEMORY_SOURCE.json').read_bytes())
    old_profile = json.loads(resolve(str(S / 'PROFILE_MEMORY_SOURCE.json'),
        prior['source_after_sha256'][str(S / 'PROFILE_MEMORY_SOURCE.json')], 'profile data comparison').read_bytes())
    need(all(current_profile[k] == old_profile[k] for k in ['roles', 'selected_profiles', 'raw_exports', 'word_count']),
         'profile/class data unchanged by semantic label correction')
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(snapshots.values()); paths.update(q for q in R.rglob('*') if q.is_file())
    paths.update([prior_path, pathlib.Path(__file__).resolve()])
    reports = [(R / 'SOURCE_REVIEW.json', 'SOURCE_REVIEW_NATIVE_CLASS_TIER_ALLOCATION_VERIFIED'),
               (R / 'CORRECTION.json', 'CLASS_CALLER_AND_PROFILE_OWNER_LABEL_CORRECTION_SOURCE_VERIFIED'),
               (R / 'CORRECTED_DESIGN_REVIEW.json', 'CORRECTED_CLASS_TIER_ALLOCATION_DESIGN_SOURCE_VERIFIED'),
               (S / 'CLASS_CALLER_SOURCE.json', 'BOUNDED_CLASS_CALLER_HEADERS_ROUTES_AND_REFERENCE_LIFETIME_SOURCE_VERIFIED_NATIVE_BINDINGS_OPEN')]
    source_report = None
    for path, status in reports:
        d = json.loads(path.read_bytes())
        need(d['status'] == status and d['source_only'] is True and isinstance(d['input_sha256'], dict), 'actual source report schema')
        paths.update([path, path.with_suffix('.md')])
        for name, digest in d['input_sha256'].items(): paths.add(resolve(name, digest, str(path)))
        if path.name == 'CLASS_CALLER_SOURCE.json': source_report = d
    review = json.loads((R / 'SOURCE_REVIEW.json').read_bytes())
    need(len(review['input_sha256']) == 23, 'actual final source review pin count')
    # This report's implementation pins are current, not resolved to old code.
    for name, digest in review['input_sha256'].items():
        if name.startswith(str(F)) and 'before-' not in name: need(sha(name) == digest, 'current reviewed implementation pin')
    original = P / 'research/inputs/SLUS_214.10'; elf = original.read_bytes()
    need(sha(original) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original source ELF')
    phoff, = struct.unpack_from('<I', elf, 28); size, count = struct.unpack_from('<HH', elf, 42)
    segments = [struct.unpack_from('<8I', elf, phoff + i * size) for i in range(count)]
    need(isinstance(source_report['raw_exports'], list), 'actual corrected raw export schema')
    joined = 0
    for export in source_report['raw_exports']:
        q = pathlib.Path(export['path']); need(sha(q) == export['sha256'], 'actual raw caller export bytes'); paths.add(q)
        rows = list(csv.DictReader(q.read_text(encoding='utf-8-sig').splitlines()))
        need(len(rows) == export['words'] and {'virtual_address', 'file_offset', 'word_le32'}.issubset(rows[0]), 'actual caller row schema/count')
        for row in rows:
            va, offset, word = (int(row[k], 16) for k in ['virtual_address', 'file_offset', 'word_le32'])
            mapped = [s[1] + va - s[2] for s in segments if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
            need(mapped == [offset] and struct.unpack_from('<I', elf, offset)[0] == word, 'original PT_LOAD words')
            joined += 1
    need(joined == 1288, 'all1288 original caller words')
    authored = [F / name for name in ['runtime/include/mka/native_heap.hpp', 'runtime/include/mka/wave_allocation.hpp',
        'cmake/native_game.cmake', 'runtime/include/mka/class_tier_allocation.hpp',
        'runtime/src/native/class_tier_allocation.cpp', 'tests/test_class_tier_allocation.cpp']]
    paths.update(authored)
    for q in authored:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in b.splitlines()), 'authored LF/EOF')
    before = {str(q): sha(q) for q in sorted(paths)}
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved unrelated IOP bytes')
    write('historical-input-resolution.json', dict(status='EXACT_HISTORICAL_VERSIONS_RESOLVED', resolutions=resolutions,
        no_expected_hash_replacement=True, profile_data_unchanged=True))
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, unchanged_prior_pins=len(untouched), primary_words_rejoined=joined,
        compiler_sha256=sha(c.BIN / 'clang++.exe'), scope='Corrected native tier policy only; real profile/overlay/object generation still unbound'))
    target = 'test_native_class_tier_allocation'
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', target, '-j2'])
    run = c.execute('tier-policy.json', [c.BUILD / (target + '.exe')])
    need(run['stdout'].strip() == 'PASS_NATIVE_CLASS_TIER_ALLOCATION: 5 groups exact direct/outer routes and actual ownership; test-only host faults, no object/profile/reset or retail startup claim', 'complete actual tier assertions')
    audit = c.symbols('tier-policy-symbols', c.BUILD / (target + '.exe'), run['executable_sha256'],
        ['mka::memory::NativeTierAllocator::allocate', 'mka::memory::NativeWaveAllocator::allocate', 'mka::memory::NativeAllocationClass::allocate'], True)
    archive = c.BUILD / 'libmka_native_class_tier_allocation.a'
    archive_audit = c.symbols('tier-archive', archive, sha(archive), ['mka::memory::NativeTierAllocator::allocate'], False)
    raw = c.execute('raw-tier-wrapper.json', [c.BIN / 'llvm-nm.exe', '--defined-only', c.BUILD / (target + '.exe')])
    need('__wrap__ZnwySt11align_val_t' in raw['stdout'], 'actual isolated host fault wrapper')
    ninja = c.BUILD / 'build.ninja'; text = ninja.read_text(encoding='utf-8')
    match = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
    need(match and all(s not in match.group() for s in ['ps2runtime', 'source_oracle']) and
         match.group().count('--wrap=') == 1 and '--wrap=_ZnwySt11align_val_t' in match.group(), 'actual native-only isolated fixture linkage')
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rule=match.group(), archive=archive_audit))
    need(before == {n: sha(n) for n in before}, 'measured code/evidence stable throughout verification')
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_NATIVE_CLASS_TIER_DIRECT_AND_OUTER_POLICY', source_after_sha256=before,
        unchanged_prior_pins=len(untouched), primary_words_rejoined=joined,
        fixtures=[dict(component='tier-policy', target=target, executed_target_sha256=run['executable_sha256'], audit=audit)],
        historical_resolution_count=len(resolutions), frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], limits=[
            'Supported six-class scope requires known-present overlay and enabled overflow; not all standalone Wave configurations',
            'Actual allocation policy only; original object header/generation/body, profile/reset/overlay producer and full game remain OPEN',
            'Test-only aligned allocation faults; same-thread reentry and async observation, no simultaneous callback/concurrency equivalence',
            'No MPEG terminal implementation: corrected terminal target is actual direct Wave overlay',
            'No game/oracle/physical endpoints/drivers/models/settings/user state or frozen corpus operations']))
    print(json.dumps(dict(status='PASS_NATIVE_CLASS_TIER_DIRECT_AND_OUTER_POLICY', fixtures=1, pins=len(before), historical_resolutions=len(resolutions))), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-[0-9]+', sys.argv[1]): raise SystemExit('usage: verify.py validation-NN')
    c.OUT = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    c.OUT.mkdir(exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try: verify()
    except Exception as error:
        c.write('aborted.json', dict(status='NATIVE_CLASS_TIER_CHECK_FAILED', error=str(error)))
        raise
