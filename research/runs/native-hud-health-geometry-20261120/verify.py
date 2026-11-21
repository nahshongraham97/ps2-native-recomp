"""Source-pinned native HUD position writes against existing saved data; no game."""
import csv, ctypes, datetime, importlib.util, json, os, pathlib, re, struct, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'hud-health-geometry'
spec = importlib.util.spec_from_file_location('_hud_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
         bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release cache')
    prior_path = F / 'research/runs/native-class-tier-allocation-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_CLASS_TIER_DIRECT_AND_OUTER_POLICY' and
         len(prior['source_after_sha256']) == 1006 and len(prior['fixtures']) == 1, 'actual previous certificate schema')
    originals = json.loads((R / 'before-root-integration/provenance.json').read_bytes())['files']
    need(len(originals) == 4, 'exact four root snapshots')
    snapshots = {}
    for name, info in originals.items():
        need(sha(info['before_path']) == info['sha256'], 'exact root before bytes')
        snapshots[(name, info['sha256'])] = pathlib.Path(info['before_path'])
    changed = {n for n, h in prior['source_after_sha256'].items() if sha(n) != h}
    need(changed == {str(F / 'cmake/native_game.cmake'), str(S / 'CLASS_CALLER_SOURCE.md')},
         'only HUD build target and corrected priority label changed prior pins')
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched) == 1004 and all(sha(n) == h for n, h in untouched.items()), '1004 untouched prior inputs')
    resolutions = []
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    for name in changed:
        expected = prior['source_after_sha256'][name]
        need((name, expected) in snapshots, 'exact previous version remains available')
        paths.add(snapshots[(name, expected)])
        resolutions.append(dict(original_path=name, expected_sha256=expected,
                                retained_path=str(snapshots[(name, expected)])))
    paths.add(prior_path)
    paths.update(q for q in R.rglob('*') if q.is_file())
    paths.add(pathlib.Path(__file__).resolve())
    reports = [
        (S / 'TASK_ASSOCIATION_SOURCE.json', 'BOUNDED_TASK_ASSOCIATION_PRIORITY_FLOAT_BODY_AND_LIFETIME_SOURCE_VERIFIED_NATIVE_BODY_OPEN', 'raw_exports'),
        (S / 'TASK_BODY_SOURCE.json', 'RETAIL_TASK_2003_HEALTH_PRESENTATION_BODY_AND_EFFECT_BOUNDARIES_VERIFIED', 'primary_exports'),
        (R / 'SOURCE_POLICY_REVIEW.json', 'SOURCE_REVIEW_RETAIL_HEALTH_HUD_GEOMETRY_POLICY_VERIFIED', 'primary_exports'),
        (R / 'CODE_REVIEW.json', 'SOURCE_REVIEW_NATIVE_HUD_HEALTH_GEOMETRY_VERIFIED', None)]
    original = P / 'research/inputs/SLUS_214.10'
    elf = original.read_bytes()
    need(sha(original) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original retail ELF')
    phoff, = struct.unpack_from('<I', elf, 28)
    phsize, phcount = struct.unpack_from('<HH', elf, 42)
    segments = [struct.unpack_from('<8I', elf, phoff + i * phsize) for i in range(phcount)]
    joined = 0
    for path, status, export_key in reports:
        d = json.loads(path.read_bytes())
        need(d['status'] == status and d['source_only'] is True and isinstance(d['input_sha256'], dict), 'actual source review schema')
        paths.update([path, path.with_suffix('.md')])
        for name, digest in d['input_sha256'].items():
            need(sha(name) == digest, 'current exact report input: ' + name)
            paths.add(pathlib.Path(name))
        if export_key:
            need(isinstance(d[export_key], list), 'actual export array')
            for export in d[export_key]:
                q = pathlib.Path(export['path'])
                need(sha(q) == export['sha256'], 'exact raw word export')
                paths.add(q)
                rows = list(csv.DictReader(q.read_text(encoding='utf-8-sig').splitlines()))
                need(len(rows) == export['words'] and bool(rows), 'complete primary word count')
                fields = rows[0]
                va_key = 'virtual_address' if 'virtual_address' in fields else 'va'
                word_key = 'word_le32' if 'word_le32' in fields else 'le32'
                need({va_key, word_key, 'file_offset'}.issubset(fields), 'actual producer CSV schema')
                for row in rows:
                    va, offset, word = (int(row[k], 16) for k in [va_key, 'file_offset', word_key])
                    mapped = [s[1] + va - s[2] for s in segments if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
                    need(mapped == [offset] and struct.unpack_from('<I', elf, offset)[0] == word, 'original mapped instruction word')
                    joined += 1
    need(joined == 2378, 'all2378 task/body/logical coordinate words rejoined')
    capture_path = R / 'ORACLE_CAPTURE.json'
    oracle = json.loads(capture_path.read_bytes())
    need(oracle['status'] == 'SAVED_NATIVE_HUD_FULL_HEALTH_NUMERIC_ORACLE_VERIFIED' and
         oracle['saved_state_only'] is True and len(oracle['captures']) == 3 and isinstance(oracle['input_sha256'], dict), 'actual saved capture schema')
    for name, digest in oracle['input_sha256'].items():
        need(sha(name) == digest, 'unchanged original oracle/source artifact')
        paths.add(pathlib.Path(name))
    fmt = oracle['format']
    need(fmt['version'] == 8 and fmt['cpu_pod_bytes'] == 1456 and fmt['ee_ram_file_offset'] == 1476, 'actual source checked state format')
    table_path = R / 'ORACLE_EXPECTATIONS.tsv'
    rows = list(csv.DictReader(table_path.read_text(encoding='utf-8-sig').splitlines(), delimiter='\t'))
    need(len(rows) == 24 and len({(x['frame'], x['side'], x['vertex']) for x in rows}) == 24, '24 distinct captured vertices')
    expected_rows = []
    for capture in oracle['captures']:
        q = pathlib.Path(capture['path'])
        need(sha(q) == capture['sha256'], 'exact existing saved state')
        paths.add(q)
        b = q.read_bytes()
        need(b[:8] == b'PS2STATE' and len(b) == capture['file_bytes'] and
             struct.unpack_from('<I', b, 8)[0] == 8 and
             struct.unpack_from('<Q', b, fmt['frame_file_offset'])[0] == capture['frame'], 'saved header/frame identity')
        for address, bits in capture['globals'].items():
            need(struct.unpack_from('<I', b, fmt['ee_ram_file_offset'] + int(address, 16))[0] == int(bits, 16), 'captured original game inputs')
        for quad in capture['quads']:
            need(quad['saved_generation'] == quad['live_generation'] and len(quad['vertex']) == 4, 'captured live pair and four vertices')
            for i, vertex in enumerate(quad['vertex']):
                need(tuple(int(x, 16) for x in vertex['bits']) == struct.unpack_from('<II', b, vertex['file_offset']), 'original saved XY bytes')
                expected_rows.append(dict(frame=str(capture['frame']), origin=str(int(capture['globals']['0064C440'], 16)),
                    logical_width=str(int(capture['globals']['0064DEF8'], 16)), displayed_left_bits=capture['globals']['0064E354'],
                    displayed_right_bits=capture['globals']['0064E358'], side=quad['side'], vertex=str(i),
                    x=str(vertex['xy'][0]), y=str(vertex['xy'][1]), x_bits=vertex['bits'][0], y_bits=vertex['bits'][1], dirty=str(quad['dirty'])))
    need(rows == expected_rows, 'external numeric oracle is exactly captured data, no formula-generated expectations')
    authored = [F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/hud_health_geometry.hpp',
        'runtime/src/native/hud_health_geometry.cpp', 'tests/test_hud_health_geometry.cpp']]
    paths.update(authored)
    for q in authored:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and
             all(x == x.rstrip(b' \t') for x in b.splitlines()), 'authored LF/EOF')
    before = {str(q): sha(q) for q in sorted(paths)}
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved IOP bytes')
    write('historical-input-resolution.json', dict(status='EXACT_HUD_PRIOR_VERSIONS_RESOLVED', resolutions=resolutions,
        no_expected_hash_replacement=True))
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, untouched_prior_pins=1004, primary_words_rejoined=joined,
        compiler_sha256=sha(c.BIN / 'clang++.exe'), scope='Native paired HUD position ownership; no game or physical endpoint'))
    target = 'test_native_hud_health_geometry'
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', target, '-j2'])
    run = c.execute('hud-health-geometry.json', [c.BUILD / (target + '.exe'), table_path])
    need(run['stdout'].strip() == 'PASS_NATIVE_HUD_HEALTH_GEOMETRY: 3 groups actual paired position writes/dirty/preserved channels/lifetimes; saved full-health oracle frames=3; no task/resource producer/GPU or full game claim', 'actual complete fixture and all3 captured comparisons')
    required = ['mka::retail::update_hud_health_geometry', 'mka::retail::NativeHudPositionGeometry::snapshot',
                'mka::retail::NativeHudHealthObject::retire']
    audit = c.symbols('hud-symbols', c.BUILD / (target + '.exe'), run['executable_sha256'], required, True)
    archive = c.BUILD / 'libmka_native_hud_health_geometry.a'
    archive_audit = c.symbols('hud-archive', archive, sha(archive), required, False)
    ninja = c.BUILD / 'build.ninja'
    text = ninja.read_text(encoding='utf-8')
    rule = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
    need(rule and all(x not in rule.group() for x in ['ps2runtime', 'source_oracle', 'mka_native_media', 'mka_native_audio']), 'actual isolated native CPU linkage')
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rule=rule.group(), archive=archive_audit))
    need(before == {n: sha(n) for n in before}, 'source and saved evidence unchanged through actual execution')
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_NATIVE_HUD_PAIRED_GEOMETRY_AND_SAVED_FULL_HEALTH', source_after_sha256=before,
        unchanged_prior_pins=1004, primary_words_rejoined=joined, captured_frames=3, captured_vertices=24,
        historical_resolution_count=len(resolutions),
        fixtures=[dict(component='hud-health-geometry', target=target, executed_target_sha256=run['executable_sha256'], audit=audit)],
        frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], limits=[
            'Native paired position leaf only; no task2003 body/association/resource producer or GPU submission integration',
            'Three full-health saved numeric states only; no fractional accumulator or rendered comparison',
            'Ordinary finite host float grouping; not EE accumulator bit fidelity',
            'Int64 logical coordinate domain is native extension, not source signed32 conversion parity',
            'No game, oracle execution, physical devices, model/settings, drivers or user-state writes']))
    print(json.dumps(dict(status='PASS_NATIVE_HUD_PAIRED_GEOMETRY_AND_SAVED_FULL_HEALTH', pins=len(before), fixtures=1,
        primary_words=joined, captured_frames=3, captured_vertices=24)), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
    c.OUT.mkdir(parents=True, exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try:
        verify()
    except Exception as e:
        c.write('failure.json', dict(status='NATIVE_HUD_VERIFICATION_FAILED', error=repr(e)))
        raise
