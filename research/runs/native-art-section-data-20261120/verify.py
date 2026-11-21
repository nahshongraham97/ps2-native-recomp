"""Bounded actual retail art data through native files; no game/device execution."""
import csv, ctypes, datetime, hashlib, importlib.util, json, os, pathlib, re, struct, subprocess

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120'
R = S / 'health-task-effects-01'
A = R / 'art-section'
spec = importlib.util.spec_from_file_location('_art_evidence',
    F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
         bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release')
    prior_path = F / 'research/runs/native-hud-health-geometry-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_HUD_PAIRED_GEOMETRY_AND_SAVED_FULL_HEALTH' and
         len(prior['source_after_sha256']) == 1127 and len(prior['fixtures']) == 1, 'actual prior schema')
    ledger = json.loads((S / 'hud-health-geometry/RESULT_DOC_VERSIONS.json').read_bytes())
    need(ledger['status'] == 'HUD_DRAFT_NOTES_RECONCILED_WITH_EXECUTION_RESULT' and len(ledger['versions']) == 2, 'actual historical note schema')
    snapshots = {}
    for item in ledger['versions']:
        need(sha(item['path']) == item['current_sha256'] and sha(item['archived_path']) == item['previous_sha256'], 'exact note versions')
        snapshots[(item['path'], item['previous_sha256'])] = pathlib.Path(item['archived_path'])
    initial = json.loads((A / 'before-root-integration/provenance.json').read_bytes())['files']
    cmake = str(F / 'cmake/native_game.cmake')
    need(sha(initial[cmake]['archived_path']) == initial[cmake]['sha256'] == prior['source_after_sha256'][cmake], 'exact original target declarations')
    snapshots[(cmake, initial[cmake]['sha256'])] = pathlib.Path(initial[cmake]['archived_path'])
    resolutions = []
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    changed = {n for n, h in prior['source_after_sha256'].items() if sha(n) != h}
    need(changed == {x[0] for x in snapshots} and len(changed) == 3, 'only one target file and two dated note versions changed')
    for name in changed:
        digest = prior['source_after_sha256'][name]
        need((name, digest) in snapshots and sha(snapshots[(name, digest)]) == digest, 'exact old measured version retained')
        paths.add(snapshots[(name, digest)])
        resolutions.append(dict(original_path=name, expected_sha256=digest, retained_path=str(snapshots[(name, digest)])))
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched) == 1124 and all(sha(n) == h for n, h in untouched.items()), '1124 untouched prior pins')
    paths.update([prior_path, pathlib.Path(__file__).resolve()])
    paths.update(q for q in R.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    reports = [
        ('AUDIO_SOURCE.json', 'HEALTH_CUE_9194D_BANK_PROGRAM_GAIN_AND_READY_RECORD_SOURCE_VERIFIED_ACTIVATION_BINDINGS_OPEN', 'raw_exports'),
        ('AUDIO_RECEIVER_SOURCE.json', 'HEALTH_CUE_COMMAND36_SELECTOR_PRIORITY_FLAGS_RECEIVER_JOIN_VERIFIED', 'raw_exports'),
        ('AUDIO_FLAG1_SOURCE.json', 'HEALTH_CUE_FLAG0001_ORDINARY_KIND7_FORCED_RETIREMENT_SOURCE_BOUNDARY_VERIFIED_NATIVE_COORDINATOR_OPEN', 'raw_exports'),
        ('GEOMETRY_SOURCE.json', 'RETAIL_HEALTH_HUD_RESOURCE_AND_QUAD_CREATION_SOURCE_VERIFIED', 'primary_exports'),
        ('SECTION_RESOURCE_SOURCE.json', 'RETAIL_FIGHTINGART_SECTION_AND_SPRITE_SERIALIZATION_SOURCE_VERIFIED', 'primary_exports')]
    joined = 0
    for name, status, key in reports:
        q = R / name
        d = json.loads(q.read_bytes())
        need(d['status'] == status and d['source_only'] is True and isinstance(d['input_sha256'], dict), 'actual source producer schema')
        for n, h in d['input_sha256'].items():
            need(sha(n) == h, 'source report exact current input')
            paths.add(pathlib.Path(n))
        candidates = [pathlib.Path(n) for n in d['input_sha256'] if n.endswith('SLUS_214.10') or n.lower().endswith('msl.irx')]
        original = next(x for x in candidates if x.name.lower() == ('msl.irx' if 'RECEIVER' in name or 'FLAG1' in name else 'slus_214.10'))
        data = original.read_bytes()
        phoff, = struct.unpack_from('<I', data, 28)
        size, count = struct.unpack_from('<HH', data, 42)
        segs = [struct.unpack_from('<8I', data, phoff + i * size) for i in range(count)]
        for export in d[key]:
            raw = pathlib.Path(export['path'])
            need(sha(raw) == export['sha256'], 'exact primary row export')
            rows = list(csv.DictReader(raw.read_text(encoding='utf-8-sig').splitlines()))
            need(len(rows) == export['words'] and bool(rows), 'complete source words')
            vk = next(x for x in ['original_va', 'virtual_address', 'va'] if x in rows[0])
            wk = next(x for x in ['word_le32', 'le32'] if x in rows[0])
            for row in rows:
                va, offset, word = (int(row[k], 16) for k in [vk, 'file_offset', wk])
                mapped = [s[1] + va - s[2] for s in segs if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
                need(mapped == [offset] and struct.unpack_from('<I', data, offset)[0] == word, 'original ELF/IRX mapped words')
                joined += 1
    need(joined == 3197, 'all3197 source words')
    review = json.loads((A / 'CODE_REVIEW.json').read_bytes())
    need(review['status'] == 'SOURCE_REVIEW_NATIVE_ART_SECTION_DATA_VERIFIED' and review['source_only'] is True and
         isinstance(review['input_sha256'], dict), 'actual independent code review schema')
    for n, h in review['input_sha256'].items():
        need(sha(n) == h, 'exact reviewed current input')
        paths.add(pathlib.Path(n))
    source = json.loads((R / 'SECTION_RESOURCE_SOURCE.json').read_bytes())
    archive = source['archive']
    archive_path = pathlib.Path(archive['path'])
    need(archive_path.stat().st_size == archive['file_bytes'], 'actual archive length')
    expectations = []
    asset_pins = []
    with archive_path.open('rb') as file:
        header = file.read(20)
        need(hashlib.sha256(header).hexdigest() == archive['header_sha256'], 'original archive header')
        words = struct.unpack('<5I', header)
        need(words[3] == archive['directory_offset'], 'actual archive directory offset')
        file.seek(words[3])
        directory, strings = file.read(words[2] * 12), file.read(words[4])
        need(hashlib.sha256(directory).hexdigest() == archive['directory_sha256'] and
             hashlib.sha256(strings).hexdigest() == archive['strings_sha256'], 'exact bounded archive metadata')
        name_at, member_at, member_size = struct.unpack_from('<3I', directory, archive['directory_index'] * 12)
        need(strings[name_at:].split(b'\0', 1)[0].decode('ascii') == archive['member_name'] and
             member_at == archive['member_offset'] and member_size == archive['member_extent'], 'actual member record join')
        for variant in source['variants']:
            need(variant['archive_offset'] == member_at + variant['member_offset'] and
                 variant['member_offset'] + variant['extent'] <= member_size, 'selected member bounds')
            file.seek(variant['archive_offset'])
            data = file.read(variant['extent'])
            digest = hashlib.sha256(data).hexdigest()
            need(digest == variant['sha256'] and list(struct.unpack_from('<7I', data)) == variant['header'], 'actual locale section bytes/header')
            lines = [str(variant['header'][4]) + ' ' + str(len(variant['resources']))]
            offsets = [struct.unpack_from('<I', data, 28 + i * 16 + 4)[0] for i in range(variant['header'][4])]
            names = 28 + variant['header'][4] * 16
            for entry in variant['resources']:
                kind, payload, opaque, name_offset = struct.unpack_from('<4I', data, 28 + entry['index'] * 16)
                need(kind == entry['kind'] and payload == entry['offset'] and name_offset == entry['name_offset'] and
                     data[names + name_offset:].split(b'\0', 1)[0].decode('ascii') == entry['name'], 'actual named record')
                end = min([x for x in offsets if x > payload] + [len(data)])
                need(hashlib.sha256(data[payload:payload + entry['extent']]).hexdigest() == entry['sha256'], 'measured resource snapshot bytes')
                dims = payload + 5 + data[payload + 4]
                width, height = struct.unpack_from('<ii', data, dims)
                need(width == entry['width'] and height == entry['height'] and dims + 8 < end, 'actual signed dimensions and bounded texture envelope')
                lines.append(' '.join(map(str, [entry['name'], entry['index'], kind, payload, opaque, name_offset,
                    width, height, dims + 8, end - dims - 8])))
            table = c.OUT / ('source-' + str(variant['selector_offset']) + '.tsv')
            table.open('x', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
            paths.add(table)
            expectations.append((variant, table))
            asset_pins.append(dict(path=str(archive_path), offset=variant['archive_offset'], length=variant['extent'], sha256=digest))
    authored = [F / n for n in ['cmake/native_game.cmake', 'runtime/include/mka/art_section_data.hpp',
        'runtime/src/native/art_section_data.cpp', 'tests/test_art_section_data.cpp']]
    paths.update(authored)
    for q in authored:
        b = q.read_bytes()
        need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and all(x == x.rstrip(b' \t') for x in b.splitlines()), 'authored LF/EOF')
    before = {str(q): sha(q) for q in sorted(paths)}
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'protected IOP source')
    write('historical-input-resolution.json', dict(status='EXACT_ART_PRIOR_VERSIONS_RESOLVED', resolutions=resolutions, no_expected_hash_replacement=True))
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, asset_ranges=asset_pins, compiler_sha256=sha(c.BIN / 'clang++.exe'),
        scope='Native serialized art data and exact source audio gaps; no runtime texture readiness or game'))
    target = 'test_native_art_section_data'
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', target, '-j2'])
    fixtures = []
    for variant, table in expectations:
        index = variant['selector_offset']
        run = c.execute('locale-' + str(index) + '.json', [c.BUILD / (target + '.exe'), c.OUT / ('owned-' + str(index)),
            archive_path.parent, archive_path.name, archive['member_offset'], archive['member_extent'], variant['member_offset'], variant['extent'], table])
        need(run['stdout'].strip() == 'PASS_NATIVE_ART_SECTION_DATA: bounded owned SEC ingress and malformed cases; actual encoded sprites=2; no texture readiness or game claim', 'actual complete fixture with real source data')
        fixtures.append(dict(component='locale-' + str(index), target=target, executed_target_sha256=run['executable_sha256']))
    need(len({x['executed_target_sha256'] for x in fixtures}) == 1, 'all5 locale executions use exact same executable')
    required = ['mka::content::NativeArtSectionData::read', 'mka::content::NativeArtSectionData::find_sprite', 'mka::content::NativeContentFile::read']
    audit = c.symbols('art-symbols', c.BUILD / (target + '.exe'), fixtures[0]['executed_target_sha256'], required, True)
    lib = c.BUILD / 'libmka_native_art_section_data.a'
    libaudit = c.symbols('art-archive', lib, sha(lib), required[:2], False)
    ninja = c.BUILD / 'build.ninja'
    rule = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', ninja.read_text(encoding='utf-8'), re.M)
    need(rule and all(x not in rule.group() for x in ['ps2runtime', 'source_oracle', 'mka_native_media', 'mka_native_audio']), 'actual isolated native file linkage')
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rule=rule.group(), archive=libaudit))
    need(before == {n: sha(n) for n in before}, 'measured source unchanged')
    with archive_path.open('rb') as file:
        for pin in asset_pins:
            file.seek(pin['offset'])
            need(hashlib.sha256(file.read(pin['length'])).hexdigest() == pin['sha256'], 'actual source ranges unchanged after executions')
    for n, h in c.PRESERVE.items(): need(sha(F / n) == h, 'protected IOP after verification')
    write('verification.json', dict(status='PASS_NATIVE_ART_SECTION_DATA_FIVE_ACTUAL_VARIANTS', source_after_sha256=before,
        unchanged_prior_pins=1124, primary_words_rejoined=joined, asset_ranges=asset_pins, fixtures=fixtures, executable_audit=audit,
        historical_resolution_count=3, frozen_decoder_certificate_sha256=prior['frozen_decoder_certificate_sha256'], limits=[
            'Bounded serialized directory/bitmap metadata and actual encoded bytes only; not decoded GPU textures or loaded game resources',
            'Five measured fightingart locales only; original game locale selection remains separate',
            'Audio wirefields verified; required flag1 cleanup/context/output/envelope/bank publication remain open, no gate weakened',
            'No game, oracle execution, physical endpoint, model/settings, drivers or original state writes']))
    print(json.dumps(dict(status='PASS_NATIVE_ART_SECTION_DATA_FIVE_ACTUAL_VARIANTS', pins=len(before), original_words=joined,
        executions=len(fixtures), actual_encoded_sprites=10)), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
    c.OUT.mkdir(parents=True, exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try:
        verify()
    except Exception as e:
        c.write('failure.json', dict(status='NATIVE_ART_SECTION_VERIFICATION_FAILED', error=repr(e)))
        raise
