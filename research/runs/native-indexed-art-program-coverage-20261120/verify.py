"""Actual native indexed pixels and pre-effect sound coverage; no game/devices."""
import ctypes, datetime, hashlib, importlib.util, json, os, pathlib, re, struct, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'health-task-effects-03'
spec = importlib.util.spec_from_file_location('_indexed_coverage_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release')
    prior_path = F / 'research/runs/native-art-section-data-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_ART_SECTION_DATA_FIVE_ACTUAL_VARIANTS' and len(prior['source_after_sha256']) == 1240, 'actual prior certificate')
    snapshots = {}
    for relative in ['before-root-integration/provenance.json', 'program-coverage/before/provenance.json',
                     'program-coverage/before-owner-integration/provenance.json']:
        d = json.loads((R / relative).read_bytes())
        need(isinstance(d['files'], dict), 'actual before-version schema')
        for name, item in d['files'].items():
            need(sha(item['archived_path']) == item['sha256'], 'exact archived bytes')
            snapshots[(name, item['sha256'])] = pathlib.Path(item['archived_path'])
    resolutions = []
    def resolve(name, digest, producer):
        q = pathlib.Path(name)
        if sha(q) == digest:
            return q
        need((name, digest) in snapshots, 'unrecognized changed evidence: ' + name)
        q = snapshots[(name, digest)]
        need(sha(q) == digest, 'retained exact expected version')
        resolutions.append(dict(producer=producer, original_path=name, expected_sha256=digest, retained_path=str(q)))
        return q
    changed = {n for n, h in prior['source_after_sha256'].items() if sha(n) != h}
    expected = {str(F / n) for n in ['cmake/native_game.cmake', 'runtime/include/mka/program_executor.hpp',
        'runtime/src/native/program_executor.cpp', 'runtime/src/native/playback_owner.cpp',
        'tests/test_program_executor.cpp', 'tests/test_program_output_activation.cpp']}
    need(changed == expected, 'only agreed target and five sound files changed prior inputs')
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched) == 1234 and all(sha(n) == h for n, h in untouched.items()), '1234 unchanged prior pins')
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    for name in changed:
        paths.add(resolve(name, prior['source_after_sha256'][name], 'previous native component'))
    paths.update(q for q in R.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    paths.update([prior_path, pathlib.Path(__file__).resolve()])
    for relative, status in [('indexed-sprite/CODE_REVIEW.json', 'SOURCE_REVIEW_NATIVE_INDEXED_SPRITE_DATA_VERIFIED'),
                             ('program-coverage/CODE_REVIEW.json', 'SOURCE_REVIEW_NATIVE_PROGRAM_EARLY_COVERAGE_VERIFIED')]:
        q = R / relative
        d = json.loads(q.read_bytes())
        need(d['status'] == status and d['source_only'] is True and isinstance(d['input_sha256'], dict), 'actual final source review schema')
        for name, digest in d['input_sha256'].items():
            need(sha(name) == digest, 'current reviewed code/evidence identity')
            paths.add(pathlib.Path(name))
    reports = ['AUDIO_EXIT_SOURCE', 'CONTEXT_UPDATE_PASSES_SOURCE', 'TEXTURE_SOURCE']
    for stem in reports:
        q = S / 'health-task-effects-02' / (stem + '.json')
        d = json.loads(q.read_bytes())
        need(d['source_only'] is True and isinstance(d['input_sha256'], dict), 'actual source predecessor schema')
        paths.update([q, q.with_suffix('.md')])
        for name, digest in d['input_sha256'].items():
            paths.add(resolve(name, digest, str(q)))
    texture_path = S / 'health-task-effects-02/TEXTURE_SOURCE.json'
    texture = json.loads(texture_path.read_bytes())
    need(texture['status'] == 'RETAIL_FIGHTINGART_INDEXED_TEXTURE_LAYOUT_SOURCE_VERIFIED' and len(texture['samples']) == 2, 'actual texture source schema')
    archive_path = pathlib.Path(texture['archive']['path'])
    asset_ranges = []
    with archive_path.open('rb') as file:
        for sample in texture['samples']:
            file.seek(sample['archive_record_offset'])
            data = file.read(sample['record_size'])
            need(hashlib.sha256(data).hexdigest() == sample['record_sha256'], 'actual source texture bytes')
            asset_ranges.append(dict(path=str(archive_path), offset=sample['archive_record_offset'], length=sample['record_size'], sha256=sample['record_sha256']))
    table = c.OUT / 'indexed-source.tsv'
    lines = [str(len(texture['samples']))]
    for sample in texture['samples']:
        section_offset = sample['archive_record_offset'] - texture['archive']['offset']
        prefix = bytes.fromhex(sample['sprite_prefix_hex'])
        width, height = struct.unpack_from('<ii', prefix, 5 + prefix[4])
        lines.append(' '.join(map(str, [sample['name'], width, height, section_offset + sample['encoded_start_in_record'], sample['encoded_size']])))
    table.open('x', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    paths.add(table)
    ms2 = F / 'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2'
    need(sha(ms2) == prior['source_after_sha256'][str(ms2)] == '74b7affaf2ec29052b39f531788ff12148f9870e79dde953e701c63e6e948d5d', 'actual pinned sound archive')
    authored = [F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/indexed_sprite_data.hpp',
        'runtime/src/native/indexed_sprite_data.cpp', 'tests/test_indexed_sprite_data.cpp',
        'runtime/include/mka/program_executor.hpp', 'runtime/src/native/program_executor.cpp',
        'runtime/src/native/playback_owner.cpp', 'tests/test_program_executor.cpp', 'tests/test_program_output_activation.cpp']]
    paths.update(authored)
    for q in authored:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and all(x == x.rstrip(b' \t') for x in b.splitlines()), 'authored LF/EOF')
    before = {str(q): sha(q) for q in sorted(paths)}
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'protected IOP source')
    write('historical-input-resolution.json', dict(status='EXACT_INDEXED_COVERAGE_PRIOR_VERSIONS_RESOLVED', resolutions=resolutions, no_expected_hash_replacement=True))
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, asset_ranges=asset_ranges, compiler_sha256=sha(c.BIN / 'clang++.exe'),
        scope='Native raw-channel asset decode and early sound coverage/owner preservation only; no game/device'))
    output = c.OUT / 'decoded-owned'
    cases = [
        ('indexed', 'test_native_indexed_sprite_data', [output, archive_path.parent, archive_path.name,
            291459072, 6823936, texture['archive']['offset'] - 291459072, texture['archive']['extent'], table]),
        ('executor', 'test_native_program_executor', [ms2]),
        ('activation', 'test_native_program_output_activation', [ms2]),
        ('program-copy', 'test_native_program_copy', [ms2]),
        ('playback-owner', 'test_native_playback_owner', [ms2]),
        ('reservation', 'test_native_playback_reservation', [ms2]),
        ('reservation-gaps', 'test_native_playback_reservation_gaps', [ms2])]
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', *[x[1] for x in cases], '-j2'])
    fixtures = []
    for name, target, args in cases:
        run = c.execute(name + '.json', [c.BUILD / (target + '.exe'), *args])
        need('PASS' in run['stdout'] and 'FAIL' not in run['stdout'], 'complete component success receipt')
        if name == 'indexed':
            need(run['stdout'].strip() == 'PASS_NATIVE_INDEXED_SPRITE_DATA: actual decoded sprites=2 with malformed and serialized-alignment coverage; stored pixels only, no renderer or game claim', 'actual two-sprite decoder case')
        if name == 'executor':
            need(run['stdout'].strip() == 'PASS actualannouncrtimeline/ownedclip/exactoperands/startACK/stop/earlyreachablecoverage/unreachablepadding; no mix/audio execution', 'actual early coverage executor case')
        if name == 'activation':
            need(run['stdout'].strip() == 'PASS_NATIVE_PROGRAM_OUTPUT_ACTIVATION: 8 actual role/clock/replacement/coverage groups; ordinary native output only', 'actual incumbent/clock preservation group')
        required = ['mka::content::decode_indexed_sprite'] if name == 'indexed' else ['mka::sound::validate_program_coverage']
        audit = c.symbols(name + '-symbols', c.BUILD / (target + '.exe'), run['executable_sha256'], required, True)
        fixtures.append(dict(component=name, target=target, executed_target_sha256=run['executable_sha256'], audit=audit))
    decoded = []
    for sample in texture['samples']:
        q = output / (sample['name'] + '.rgba')
        need(q.stat().st_size == 1024 and sha(q) == sample['software_layout_rgba_raw_sha256'], 'all actual decoded stored-channel bytes match scoped source reference')
        decoded.append(dict(name=sample['name'], path=str(q), bytes=1024, sha256=sha(q)))
    for name, required in [('libmka_native_indexed_sprite_data.a', ['mka::content::decode_indexed_sprite']),
                           ('libmka_native_sound_resources.a', ['mka::sound::validate_program_coverage'])]:
        lib = c.BUILD / name
        c.symbols(name + '-archive', lib, sha(lib), required, False)
    ninja = c.BUILD / 'build.ninja'
    text = ninja.read_text(encoding='utf-8')
    rules = []
    for _, target, _ in cases:
        rule = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
        need(rule and all(x not in rule.group() for x in ['ps2runtime', 'source_oracle']), 'actual native-only component linkage')
        rules.append(rule.group())
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rules=rules))
    need(before == {n: sha(n) for n in before}, 'all measured inputs stable through execution')
    for pin in asset_ranges:
        with pathlib.Path(pin['path']).open('rb') as file:
            file.seek(pin['offset'])
            need(hashlib.sha256(file.read(pin['length'])).hexdigest() == pin['sha256'], 'original texture input unchanged')
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'protected IOP after verification')
    write('verification.json', dict(status='PASS_NATIVE_INDEXED_ART_AND_PRE_EFFECT_PROGRAM_COVERAGE', source_after_sha256=before,
        unchanged_prior_pins=1234, historical_resolution_count=len(resolutions), asset_ranges=asset_ranges,
        fixtures=fixtures, decoded_images=decoded, frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], limits=[
            'Stored source channels for two known 16x16 indexed assets only; no alpha normalization, GPU publication or render parity',
            'Implemented executor kinds1/7 only; unsupported operations now refuse early without incumbent/clock effects',
            'No flag1 or kind2/6 execution support added; context update/retirement and complete native startup remain open',
            'No game, physical endpoint, model/settings, driver or original input writes; no unchanged codec/corpus runs']))
    print(json.dumps(dict(status='PASS_NATIVE_INDEXED_ART_AND_PRE_EFFECT_PROGRAM_COVERAGE', fixtures=len(fixtures),
        pins=len(before), decoded_images=2, unchanged_prior_pins=1234)), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
    c.OUT.mkdir(parents=True, exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try:
        verify()
    except Exception as e:
        c.write('failure.json', dict(status='NATIVE_INDEXED_COVERAGE_VERIFICATION_FAILED', error=repr(e)))
        raise
