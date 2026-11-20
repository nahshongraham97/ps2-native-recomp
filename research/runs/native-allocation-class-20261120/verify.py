"""Native payload/profile evidence with independent source data; never boot a game."""
import csv, ctypes, datetime, importlib.util, json, os, pathlib, re, struct, subprocess, sys

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'allocation-class'
spec = importlib.util.spec_from_file_location('_class_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
         bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release')
    prior_path = F / 'research/runs/native-retail-memory-owners-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_RETAIL_OWNER_GROUP_AND_STARTUP_OWNERSHIP' and
         len(prior['source_after_sha256']) == 835 and len(prior['fixtures']) == 4, 'actual prior schema')
    old = json.loads((R / 'before/provenance.json').read_bytes())
    need(isinstance(old['source_sha256'], dict) and set(old['source_sha256']) == {'cmake/native_game.cmake'}, 'one exact changed CMake snapshot')
    need(sha(R / 'before/native_game.cmake') == old['source_sha256']['cmake/native_game.cmake'] ==
         prior['source_after_sha256'][str(F / 'cmake/native_game.cmake')], 'actual prior CMake bytes')
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n != str(F / 'cmake/native_game.cmake')}
    need(len(untouched) == 834 and all(sha(n) == h for n, h in untouched.items()), '834 prior inputs unchanged')
    review_path = R / 'SOURCE_REVIEW.json'; review = json.loads(review_path.read_bytes())
    need(review['status'] == 'SOURCE_REVIEW_NATIVE_ALLOCATION_CLASS_AND_PROFILE_VERIFIED' and review['source_only'] is True and
         isinstance(review['input_sha256'], dict) and len(review['input_sha256']) == 14, 'actual source review schema')
    need(all(sha(n) == h for n, h in review['input_sha256'].items()), 'reviewed implementation unchanged')
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(map(pathlib.Path, review['input_sha256']))
    paths.update(q for q in R.rglob('*') if q.is_file())
    paths.update([prior_path, pathlib.Path(__file__).resolve()])
    original = P / 'research/inputs/SLUS_214.10'; elf = original.read_bytes()
    need(sha(original) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original ELF')
    phoff, = struct.unpack_from('<I', elf, 28); size, count = struct.unpack_from('<HH', elf, 42)
    segments = [struct.unpack_from('<8I', elf, phoff + i * size) for i in range(count)]
    joined = 0; profile = None
    producers = [(S / 'PROFILE_MEMORY_SOURCE.json', 'BOUNDED_PROFILE_MEMORY_OWNERS_AND_RESET_SOURCE_VERIFIED_NATIVE_FIXED_POLICIES_OPEN',
                  'raw_exports', 'virtual_address', 'word_le32'),
                 (S / 'FIXED_CLASS_SOURCE.json', 'RETAIL_FIXED_CLASS_STORAGE_AND_PFX_LIVE_COUNT_SOURCE_VERIFIED',
                  'primary_exports', 'va', 'le32')]
    for path, status, exports_key, address_key, word_key in producers:
        d = json.loads(path.read_bytes())
        need(d['status'] == status and d['source_only'] is True and isinstance(d['input_sha256'], dict) and
             isinstance(d[exports_key], list), 'actual per-producer source schema')
        need(all(sha(n) == h for n, h in d['input_sha256'].items()), 'original source report inputs')
        paths.update([path, path.with_suffix('.md')]); paths.update(map(pathlib.Path, d['input_sha256']))
        for export in d[exports_key]:
            q = pathlib.Path(export['path']); need(sha(q) == export['sha256'], 'primary raw export identity'); paths.add(q)
            rows = list(csv.DictReader(q.read_text(encoding='utf-8-sig').splitlines()))
            need(len(rows) == export['words'] and {address_key, 'file_offset', word_key}.issubset(rows[0]), 'actual original rows/count')
            for row in rows:
                va, offset, word = (int(row[k], 16) for k in [address_key, 'file_offset', word_key])
                mapped = [s[1] + va - s[2] for s in segments if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
                need(mapped == [offset] and struct.unpack_from('<I', elf, offset)[0] == word, 'actual original PT_LOAD words')
                joined += 1
        if exports_key == 'raw_exports': profile = d
    need(joined == 2376, '1654 profile plus722 fixed-class primary words')
    need(profile and isinstance(profile['selected_profiles'], dict) and set(profile['selected_profiles']) == {str(i) for i in range(14)} and
         len(profile['roles']) == 13, 'actual profile schema: dictionary14/roles13')
    callbacks = {'00310C90': 1, '00310BC0': 2, '00310AF0': 3, '00310A20': 4, '00310950': 5, '003108D0': 6}
    rows = ['13']
    for i, role in enumerate(profile['roles']):
        need(role['source_kind'] == 2 and role['source_alignment_flags_word_10'] == 4, 'verified class alignment source code4 only')
        callback = role['allocation_callback']
        need(callback is None or callback in callbacks, 'actual supported source callback metadata')
        rows.append(f"{i + 1} {role['profile_word_index']} {role['source_block_bytes']} 16 {0 if callback is None else callbacks[callback]}")
    for i in range(14):
        values = profile['selected_profiles'][str(i)]
        need(isinstance(values, list) and len(values) == 18 and values[7] is None and
             all(v is None if j == 7 else isinstance(v, int) and 0 <= v <= 0xffffffff for j, v in enumerate(values)), 'original selected store extent/unwritten field')
        rows.append(str(i) + ' ' + ' '.join('unwritten' if v is None else str(v) for v in values))
    oracle_path = c.OUT / 'source-profile.tsv'
    oracle_path.open('x', encoding='utf-8', newline='\n').write('\n'.join(rows) + '\n')
    write('profile-oracle.json', dict(source_report=str(S / 'PROFILE_MEMORY_SOURCE.json'),
        source_report_sha256=sha(S / 'PROFILE_MEMORY_SOURCE.json'), target=str(oracle_path), target_sha256=sha(oracle_path),
        descriptors=13, original_profiles=14, source_only=True, note='Original source facts, never parsed from candidate implementation'))
    paths.add(oracle_path)
    authored = [F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/allocation_class.hpp',
        'runtime/src/native/allocation_class.cpp', 'tests/test_allocation_class.cpp',
        'runtime/include/mka/allocation_profile.hpp', 'runtime/src/native/allocation_profile.cpp', 'tests/test_allocation_profile.cpp']]
    paths.update(authored)
    for q in authored:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(line == line.rstrip(b' \t') for line in b.splitlines()), 'authored LF/EOF')
    before = {str(q): sha(q) for q in sorted(paths)}
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved unrelated IOP bytes')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, unchanged_prior_pins=len(untouched), primary_words_rejoined=joined,
        compiler_sha256=sha(c.BIN / 'clang++.exe'), scope='Actual native class storage and profile metadata only; no full game producer'))
    cases = [('class-storage', 'test_native_allocation_class', [],
              'PASS_NATIVE_ALLOCATION_CLASS: 3 groups actual class payload/ownership/fit/signed PFX predicate; no callback/object/reset or retail startup claim',
              ['mka::memory::NativeAllocationClass::allocate', 'mka::memory::checked_pfx_headroom']),
             ('class-profile', 'test_native_class_profile', [oracle_path],
              'PASS_NATIVE_CLASS_PROFILE: 20 selectors/13 classes match original stores; actual dynamic PFX storage and signed game predicate, no reset/producer/game claim',
              ['mka::retail::select_native_class_profile', 'mka::memory::NativeAllocationClass::allocate'])]
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', *[x[1] for x in cases], '-j2'])
    fixtures = []
    for label, target, args, footer, required in cases:
        print('RUN ' + label, flush=True)
        run = c.execute(label + '.json', [c.BUILD / (target + '.exe'), *args])
        need(run['stdout'].strip() == footer, 'complete assertions: ' + label)
        audit = c.symbols(label + '-symbols', c.BUILD / (target + '.exe'), run['executable_sha256'], required, True)
        fixtures.append(dict(component=label, target=target, executed_target_sha256=run['executable_sha256'], audit=audit))
    ninja = c.BUILD / 'build.ninja'; text = ninja.read_text(encoding='utf-8'); rules = []
    for _, target, _, _, _ in cases:
        match = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
        need(match and all(s not in match.group() for s in ['ps2runtime', 'source_oracle', '--wrap=']), 'actual native-only linkage')
        rules.append(dict(target=target, actual_build_rule=match.group()))
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rules=rules))
    need(before == {n: sha(n) for n in before}, 'all measured source/evidence stable through verification')
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_NATIVE_DYNAMIC_ALLOCATION_CLASS_AND_RETAIL_PROFILE_SELECTION',
        source_after_sha256=before, unchanged_prior_pins=len(untouched), primary_words_rejoined=joined, fixtures=fixtures,
        frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], independent_profile_oracle_sha256=sha(oracle_path), limits=[
            'Actual direct native payload/metadata and signed game predicate only; no callbacks/object generation/body/reset/full game readiness',
            'Real production class binding requires dedicated immutable role/schema/owner contracts; declaring arbitrary PFX is not a producer',
            'PFX configured game headroom retained independently of dynamic allocation capacity; no slot/free-list/header/guest arena model',
            'Class selector omits record initialization and remaining/unwritten profile effects; full182670 remains OPEN',
            'No game/oracle/physical endpoints, driver/model/settings or user state touched; frozen decoder/corpus not rerun']))
    print(json.dumps(dict(status='PASS_NATIVE_DYNAMIC_ALLOCATION_CLASS_AND_RETAIL_PROFILE_SELECTION', fixtures=len(fixtures), pins=len(before))), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-[0-9]+', sys.argv[1]): raise SystemExit('usage: verify.py validation-NN')
    c.OUT = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    c.OUT.mkdir(exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try: verify()
    except Exception as error:
        c.write('aborted.json', dict(status='NATIVE_ALLOCATION_CLASS_CHECK_FAILED', error=str(error)))
        raise
