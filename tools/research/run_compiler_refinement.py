"""Record private, manually refined PS2 C candidates against frozen originals.

No source synthesis, acceptance operation, oracle run, or credit modification.
Generated/game-derived sources and objects stay in the private work directory.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import time

SAMPLE_SHA = '52a07ad180e1b05c889a72a6f04a8938cb02da5cdc6296431c8f612f359d3f59'
PACK_SHA = '04d476ee36d69f1db0bd51a5ae609e2d959ff63c6404e0f20a6347935a19577f'
KIT_COMMIT = '3ae9a231d887f187617e25756c7fa866703fa5a1'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf8', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=['begin', 'attempt', 'recheck', 'close'])
    parser.add_argument('--sample', type=Path, required=True)
    parser.add_argument('--pack', type=Path, required=True)
    parser.add_argument('--kit', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--address', required=True)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--note', required=True)
    parser.add_argument('--compiler', help='Comma-separated labels; default all 26 MW labels')
    parser.add_argument('--optimization', default='-O3,-O4')
    parser.add_argument('--g', default='0,8')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--assembler', choices=['toolkit', 'bundled'], default='toolkit')
    args = parser.parse_args()
    assert sha(args.sample) == SAMPLE_SHA and sha(args.pack) == PACK_SHA
    sample = json.loads(args.sample.read_bytes())
    pack = json.loads(args.pack.read_bytes())
    key = args.address.lower().removeprefix('0x')
    admitted = {row['address'] for row in sample['sample']} | {'00156a80'}
    assert key in admitted and len(sample['sample']) == 15
    assert 1 <= args.jobs <= 4
    assert shutil.disk_usage('/mnt/c').free >= 12 * (1 << 30)
    oracle = subprocess.run(['/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe',
        '-NoProfile', '-Command',
        "Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(ps2run|pcsx2|xemu)' } | ForEach-Object { $_.Id }"],
        capture_output=True, text=True, check=True)
    assert not oracle.stdout.strip(), 'oracle process present; compiler work refused'
    assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=args.kit, text=True).strip() == KIT_COMMIT
    root = args.work.resolve()
    folder = root / 'attempts' / key
    folder.mkdir(parents=True, exist_ok=True)
    clock = folder / 'SESSION.json'
    if args.stage == 'begin':
        assert not clock.exists(), 'preserve prior session'
        save(clock, dict(status='OPEN', started_epoch=time.time(),
            started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), note=args.note,
            clock_scope='Dedicated refinement session; shared harness/input inspection and final cross-build sweep are separate.'))
        print('BEGUN', key)
        return
    session = json.loads(clock.read_bytes())
    assert session['status'] == 'OPEN' or args.stage == 'recheck'
    if args.stage == 'close':
        session.update(status='CLOSED', stopped_epoch=time.time(),
            elapsed_seconds=time.time() - session['started_epoch'], close_note=args.note)
        save(clock, session)
        print('CLOSED', key, round(session['elapsed_seconds'], 3))
        return
    assert args.candidate and args.candidate.resolve().is_relative_to(root / 'work')
    source = args.candidate.read_bytes()
    assert not re.search(rb'INCLUDE_ASM|GLOBAL_ASM|\b(?:__asm__|asm)\s*\(|g_ram|c\.r\[', source), 'assembly/register/memory fallback refused'
    if key == '00156a80':
        assert source == (Path('/opt/mkar-decomp/experiment-01/work') / args.candidate.name).read_bytes(), 'control source changed'
    else:
        assert b'M2C_' not in source, 'refined source must use typed fields'
    previous = sorted(folder.glob('a[0-9][0-9]/RESULT.json'))
    revisions = {json.loads(path.read_bytes())['candidate_sha256'] for path in previous}
    candidate_sha = hashlib.sha256(source).hexdigest()
    if args.stage == 'recheck':
        assert candidate_sha in revisions, 'closed-session recheck may not introduce source'
    assert len(revisions | {candidate_sha}) <= 12, 'source revision limit'
    # Rechecking an already tested candidate across all builds is a separate
    # measurement, not extra author refinement time or a new source revision.
    if candidate_sha not in revisions:
        assert time.time() - session['started_epoch'] <= 600, 'dedicated refinement deadline'
    attempt = folder / f'a{len(previous)+1:02d}'
    assert not attempt.exists()
    attempt.mkdir()
    shutil.copyfile(Path(__file__), attempt / 'DRIVER.py')
    shutil.copyfile(Path(__file__).with_name('compiler_matching_linux.py'), attempt / 'STRICT_CHECKER.py')
    candidate = attempt / args.candidate.name
    candidate.write_bytes(source)
    row = next(row for row in pack['sample'] if row['address'] == key)
    original = bytes.fromhex(row['raw_hex'])
    assert hashlib.sha256(original).hexdigest() == row['original_body_sha256']
    name = 'func_' + key.upper()
    sys.path.insert(0, str(args.kit))
    import dk
    spec = importlib.util.spec_from_file_location('strict_original_checker', Path(__file__).with_name('compiler_matching_linux.py'))
    checker = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checker)
    base_run = dk.run

    def bounded_run(command, *positional, **keywords):
        if Path(str(command[0])).name == 'cc.sh':
            command = ['timeout', '--signal=TERM', '--kill-after=5', '60', *command]
        return base_run(command, *positional, **keywords)

    dk.run = bounded_run
    os.environ['DK_WARM'] = '0'
    tc = Path(os.environ['DK_TOOLCHAIN'])
    versions = args.compiler.split(',') if args.compiler else ['mwcps2-' + path.name for path in sorted((tc / 'mwcps2').iterdir()) if path.is_dir()]
    options = args.optimization.split(',')
    gs = [int(g) for g in args.g.split(',')]
    assert all(opt in ('-O0', '-O1', '-O2', '-O3', '-O4') for opt in options)
    assert all(g in (0, 8) for g in gs)
    cells = [(version, opt + (f' -sdatathreshold {g}' if version.startswith('mwcps2-') else ''), g)
             for version in versions for opt in options for g in gs]
    started = time.time()

    def worker(worker_index):
        work = root / 'workers' / str(worker_index)
        if not work.exists():
            work.mkdir(parents=True)
            for part in ('include', 'asm', 'config', 'tools'):
                shutil.copytree(root / part, work / part, symlinks=True)
            shutil.copyfile(root / 'dk.json', work / 'dk.json')
            (work / 'src').mkdir()
        project = dk.Project(work)
        wrapper = work / 'tools/cc.sh'
        original_wrapper = (root / 'tools/cc.sh').read_text()
        marker = '    python3 "$ROOT/tools/pad_functions.py" "$OUT_ABS"'
        assert original_wrapper.count(marker) == 1
        capture_wrapper = original_wrapper.replace(marker,
            '    cp "$OUT_ABS" "$OUT_ABS.compiler.o"\n' + marker)
        asm_marker = 'mipsel-none-elf-as -EL "-G$G" -march=r5900 -mabi=eabi -no-pad-sections \\\n    -I "$ROOT" -I "$ROOT/include" -o "$OUT" "$TMP/out.s"'
        assert original_wrapper.count(asm_marker) == 1
        bundled_wrapper = capture_wrapper.replace(asm_marker,
            'cp "$TMP/out.s" "$OUT.compiler.s"\n'
            'if [ -n "${DK_REFINE_AS:-}" ]; then\n'
            '    "$DK_REFINE_AS" -EL "-G$G" -o "$OUT" "$TMP/out.s"\n'
            'else\n    ' + asm_marker + '\nfi')
        assert wrapper.read_text() in (original_wrapper, capture_wrapper, bundled_wrapper)
        capture_wrapper = bundled_wrapper
        wrapper.write_text(capture_wrapper)
        project._asm = {path.stem: path for path in (work / 'asm').rglob('*.s')}
        results = []
        for index in range(worker_index, len(cells), args.jobs):
            version, flags, g = cells[index]
            assert shutil.disk_usage('/mnt/c').free >= 12 * (1 << 30)
            dest = attempt / f'cell_{index:03d}'
            dest.mkdir()
            item = dict(compiler=version, flags=flags, g=g, module=row['module'], address=key,
                original_sha256=row['original_body_sha256'], candidate_sha256=candidate_sha)
            item['assembler'] = 'compiler-internal' if version.startswith('mwcps2-') else args.assembler
            begin = time.monotonic()
            try:
                if version == '3.2-030210-beta2':
                    item.update(status='COMPILER_UNAVAILABLE', reason='Previously verified cygwin_internal failure/hang; not retried.')
                else:
                    assembler = tc / 'ee-gcc' / version / 'bin/ee-as'
                    if args.assembler == 'bundled' and not version.startswith('mwcps2-'):
                        if not assembler.is_file():
                            item.update(status='ASSEMBLER_UNAVAILABLE', reason='No bundled ee-as in this compiler package')
                            save(dest / 'RESULT.json', item)
                            results.append((index, item))
                            continue
                        project.cfg['refine_assembler'] = str(assembler)
                        item['assembler_sha256'] = sha(assembler)
                    else:
                        project.cfg.pop('refine_assembler', None)
                    original_env = project.env
                    def cell_env(**over):
                        env = original_env(**over)
                        if args.assembler == 'bundled' and not version.startswith('mwcps2-'):
                            env['DK_REFINE_AS'] = str(assembler)
                        else:
                            env.pop('DK_REFINE_AS', None)
                        return env
                    project.env = cell_env
                    raw_object = work / 'build/try' / name / (name + '.o.compiler.o')
                    raw_object.unlink(missing_ok=True)
                    try:
                        score, error = dk.try_func(project, name, candidate, gcc=version, cflags=flags, g=g)
                    finally:
                        project.env = original_env
                    if score is None:
                        item.update(status='COMPILER_ERROR', reason=error)
                    else:
                        item.update(kit_matched=score[0], kit_target=score[1], kit_candidate=score[2],
                                    kit_soft=score[3], instruction_diff=score[4])
                        processed = work / 'build/try' / name / (name + '.o')
                        obj = raw_object if version.startswith('mwcps2-') else processed
                        assert obj.exists(), 'raw compiler object capture missing'
                        preserved = dest / (name + '.o')
                        shutil.copyfile(obj, preserved)
                        item['object_sha256'] = sha(preserved)
                        item['postprocessed_object_sha256'] = sha(processed)
                        raw, relocs = checker.object_bytes(preserved, name)
                        processed_body, processed_relocs = checker.object_bytes(processed, name)
                        assert raw == processed_body and relocs == processed_relocs, 'toolkit changed function body or relocations'
                        item['raw_body_and_relocations_equal_postprocessing'] = True
                        linked = checker.relocate(raw, relocs, pack['main_gp'] if row['module'] == 'SLUS_214.10' else None, int(key, 16))
                        item.update(status='BYTE_EXACT' if linked == original else 'MISMATCH',
                            candidate_resolved_sha256=hashlib.sha256(linked).hexdigest(),
                            candidate_bytes=len(linked), original_bytes=len(original), relocations=relocs,
                            raw_word_diff=[dict(index=i // 4, original=original[i:i+4].hex(), candidate=linked[i:i+4].hex())
                                for i in range(0, max(len(original), len(linked)), 4) if original[i:i+4] != linked[i:i+4]])
            except (Exception, SystemExit) as error:
                item.update(status='CHECK_REFUSED', reason=repr(error))
            item['seconds'] = time.monotonic() - begin
            save(dest / 'RESULT.json', item)
            results.append((index, item))
        return results

    with ThreadPoolExecutor(max_workers=args.jobs) as workers:
        results = sorted(pair for group in workers.map(worker, range(args.jobs)) for pair in group)
    record = dict(status='COMPLETE', address=key, module=row['module'], note=args.note,
        candidate_sha256=candidate_sha, source_revision=len(revisions | {candidate_sha}),
        started_epoch=started, stopped_epoch=time.time(), seconds=time.time()-started,
        session_elapsed_seconds=time.time()-session['started_epoch'], jobs=args.jobs,
        assembler=args.assembler,
        driver_sha256=sha(Path(__file__)), strict_checker_sha256=sha(Path(__file__).with_name('compiler_matching_linux.py')),
        sample_sha256=SAMPLE_SHA, original_pack_sha256=PACK_SHA, kit_commit=KIT_COMMIT,
        results=[item for _, item in results], ownership_delta=0, credit_delta=0)
    save(attempt / 'RESULT.json', record)
    exact = [item for _, item in results if item['status'] == 'BYTE_EXACT']
    scored = [item for _, item in results if 'kit_matched' in item]
    best = max(scored, key=lambda x: x['kit_matched']/max(x['kit_target'], x['kit_candidate'])) if scored else None
    print(json.dumps(dict(address=key, attempt=attempt.name, source_revision=record['source_revision'], cells=len(cells),
        exact_cells=len(exact), best={k:best[k] for k in ('compiler','flags','g','status','kit_matched','kit_target','kit_candidate','instruction_diff') if k in best} if best else None,
        errors=[item.get('reason') for _, item in results if item['status'] in ('COMPILER_ERROR','CHECK_REFUSED')][:1])))


if __name__ == '__main__':
    main()
