"""Publish hash-bound refinement metadata; keep C, assembly and objects private."""
from collections import Counter
from fractions import Fraction
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re

from summarize_compiler_matching import failure_detail

ROOT = Path(__file__).resolve().parents[2]
SAMPLE_SHA = '52a07ad180e1b05c889a72a6f04a8938cb02da5cdc6296431c8f612f359d3f59'


def load(path):
    return json.loads(path.read_bytes())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-root', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--publish', action='store_true')
    args = parser.parse_args()
    private = args.private_root.resolve()
    assert private.is_relative_to(ROOT / 'build/oracle-telemetry')
    assert sha(private / 'SAMPLE.json') == SAMPLE_SHA
    sample = load(private / 'SAMPLE.json')
    pack_path = private.parent / 'revised-private-originals.json'
    assert sha(pack_path) == '04d476ee36d69f1db0bd51a5ae609e2d959ff63c6404e0f20a6347935a19577f'
    originals = {row['address']: row for row in load(pack_path)['sample']}
    frozen_driver = sha(ROOT / 'tools/research/run_compiler_refinement.py')
    selected = {row['address']: row for row in sample['sample']}
    assert len(selected) == 15
    functions = []
    cells = []
    control = None
    for key in [*selected, '00156a80']:
        folder = args.work / 'attempts' / key
        session = load(folder / 'SESSION.json')
        assert session['status'] == 'CLOSED'
        attempts = [(path, load(path)) for path in sorted(folder.glob('a[0-9][0-9]/RESULT.json'))]
        source_versions = set()
        final = []
        history = []
        for path, attempt in attempts:
            source = path.parent / ('func_' + key.upper() + '.c')
            assert sha(source) == attempt['candidate_sha256']
            source_versions.add(sha(source))
            assert attempt['status'] == 'COMPLETE' and attempt['sample_sha256'] == SAMPLE_SHA
            history.append(dict(attempt=path.parent.name, candidate_sha256=sha(source),
                driver_sha256=attempt['driver_sha256'], seconds=attempt['seconds'],
                source_revision=attempt['source_revision'], note=attempt['note'], cells=len(attempt['results']),
                statuses=dict(Counter(row['status'] for row in attempt['results'])),
                raw_driver_archived=(path.parent / 'DRIVER.py').exists()))
            if attempt['driver_sha256'] != frozen_driver:
                continue
            assert sha(path.parent / 'DRIVER.py') == frozen_driver
            assert sha(path.parent / 'STRICT_CHECKER.py') == attempt['strict_checker_sha256']
            if key != '00156a80':
                assert not re.search(rb'M2C_|INCLUDE_ASM|GLOBAL_ASM|\b(?:__asm__|asm)\s*\(|g_ram|c\.r\[', source.read_bytes())
            for row in attempt['results']:
                assert row['candidate_sha256'] == sha(source)
                assert row['original_sha256'] == originals[key]['original_body_sha256']
                value = dict(row, attempt=path.parent.name, driver_sha256=frozen_driver)
                value['failure_category'], value['diagnostic'] = failure_detail(row)
                if row['status'] in ('BYTE_EXACT', 'MISMATCH'):
                    assert row['raw_body_and_relocations_equal_postprocessing']
                    assert sha(path.parent / ('cell_' + f"{attempt['results'].index(row):03d}") /
                               ('func_' + key.upper() + '.o')) == row['object_sha256']
                    if row['status'] == 'BYTE_EXACT':
                        assert row['candidate_resolved_sha256'] == row['original_sha256']
                        assert row['candidate_bytes'] == row['original_bytes']
                        assert not row['raw_word_diff']
                final.append(value)
        assert final and len(source_versions) <= 12
        assert any(len(attempt['results']) == 104 and attempt['driver_sha256'] == frozen_driver for _, attempt in attempts)
        exact = [row for row in final if row['status'] == 'BYTE_EXACT']
        standard = [row for row in exact if row['flags'].startswith(('-O3', '-O4'))]
        compared = [row for row in final if row['status'] in ('BYTE_EXACT', 'MISMATCH')]
        assert compared, 'retain unanswered cases explicitly'
        best = max(compared, key=lambda row: Fraction(
            max(row['original_bytes'], row['candidate_bytes']) // 4 - len(row['raw_word_diff']),
            max(row['original_bytes'], row['candidate_bytes']) // 4))
        fit = max(compared, key=lambda row: Fraction(row['kit_matched'], max(row['kit_target'], row['kit_candidate'])))
        item = dict(address=key, classification='BYTE_EXACT_STANDARD' if standard else 'BYTE_EXACT_O0_ONLY' if exact else 'MISMATCH',
            source_versions=len(source_versions), attempt_count=len(attempts),
            compiler_cells_all_attempts=sum(len(attempt['results']) for _, attempt in attempts),
            final_cells=len(final), final_statuses=dict(Counter(row['status'] for row in final)),
            dedicated_session_seconds=session['elapsed_seconds'], session=session, history=history,
            selected_source_sha256=best['candidate_sha256'], original_sha256=best['original_sha256'],
            exact_cells=[{field: row.get(field) for field in ('compiler', 'flags', 'g', 'assembler', 'assembler_sha256',
                'candidate_sha256', 'object_sha256', 'candidate_resolved_sha256', 'attempt')} for row in exact],
            best_comparison={field:best.get(field) for field in ('compiler', 'flags', 'g', 'assembler',
                'assembler_sha256', 'status', 'object_sha256', 'candidate_sha256', 'candidate_resolved_sha256',
                'original_bytes', 'candidate_bytes', 'raw_word_diff', 'instruction_diff')},
            best_comparison_basis='Aligned raw-word agreement; not instruction edit distance or equivalence.',
            nearest_instruction_fit={field:fit.get(field) for field in ('compiler','flags','g','assembler',
                'status','kit_matched','kit_target','kit_candidate','instruction_diff','raw_word_diff')},
            nearest_instruction_fit_limit='Diagnostic only: symbol spelling can lower this score even when raw relocated bytes match.')
        if key == '00156a80':
            assert exact
            control = item
            continue
        assert item['dedicated_session_seconds'] <= 600
        item.update(module=selected[key]['module'], instructions=selected[key]['instructions'],
                    observation=selected[key]['observation'])
        functions.append(item)
        cells.extend(final)
    counts = dict(Counter(row['classification'] for row in functions))
    uniform = Counter((row['compiler'],row['flags'],row['g'],row['assembler']) for row in cells
                      if row['status']=='BYTE_EXACT' and row['flags'].startswith(('-O3','-O4')))
    maximum = max(uniform.values())
    result = dict(status='COMPLETE', sample_sha256=SAMPLE_SHA, sample=sample,
        frozen_driver_sha256=frozen_driver, functions=functions, positive_control=control,
        counts=counts, denominator=15,
        standard_exact=sum(row['classification'] == 'BYTE_EXACT_STANDARD' for row in functions),
        including_O0_exact=sum(row['classification'].startswith('BYTE_EXACT') for row in functions),
        standard_uniform_cell_maximum=dict(exact=maximum,denominator=15,
            cells=[dict(compiler=c,flags=f,g=g,assembler=a)for(c,f,g,a),n in uniform.items()if n==maximum],
            limit='The 10/15 union uses different PS2 compiler families; no uniform 10/15 claim.'),
        final_cell_count=len(cells), final_statuses=dict(Counter(row['status'] for row in cells)),
        interpretation='Supports a bounded subsystem matching-decompilation pilot, not a catalogue-wide rate or native-port acceptance.',
        limits=['Purposive sample, shared prior inspection and harness setup excluded from dedicated session timers.',
            'Early attempts have driver hashes only; acceptance uses fresh final rechecks with archived exact driver, C and raw objects.',
            'Callback/global/type declarations are matching-source hypotheses, not recovered original C declarations or admitted native ABI.',
            'No original translation-unit boundary or MSL runtime archive fingerprint recovery was performed.',
            'Compiler package and flags need not be uniquely identified by a matching body.',
            'No native pointer-width, object lifetime, C defined-behavior, ownership or production integration admission.'],
        ownership_delta=0, transformed_delta=0, native_delta=0, native_acceptance='OPEN')
    output = ROOT / 'research/runs' if args.publish else private
    table = output / 'compiler_refinement_cells.csv'
    fields = ['module', 'address', 'compiler', 'flags', 'g', 'assembler', 'assembler_sha256', 'attempt',
              'candidate_sha256', 'object_sha256', 'candidate_resolved_sha256', 'original_sha256', 'status',
              'failure_category', 'diagnostic', 'raw_body_and_relocations_equal_postprocessing', 'seconds']
    with table.open('w', encoding='utf8', newline='') as stream:
        writer = csv.DictWriter(stream, fields, lineterminator='\n', extrasaction='ignore')
        writer.writeheader()
        writer.writerows(cells)
    result['table'] = dict(path=table.relative_to(ROOT).as_posix(), sha256=sha(table), rows=len(cells))
    if args.publish:
        path = ROOT / 'research/runs/renderer-scope.json'
        scope = load(path)
        previous = scope.get('compiler_matching_refinement_20261126', {})
        for field in ('amendments', 'supporting_receipts', 'independent_review'):
            if field in previous:result[field] = previous[field]
        scope['compiler_matching_refinement_20261126'] = result
        path.write_text(json.dumps(scope, indent=2) + '\n', encoding='utf8', newline='\n')
    else:
        (private / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8', newline='\n')
    print(json.dumps(dict(status=result['status'], standard_exact=result['standard_exact'],
        including_O0_exact=result['including_O0_exact'], denominator=15, counts=counts, final_cell_count=len(cells),
        table=result['table'])))


if __name__ == '__main__':
    main()
