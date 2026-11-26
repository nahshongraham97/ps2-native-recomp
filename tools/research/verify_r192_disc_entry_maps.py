"""Falsify source-entry admission using authenticated generated NTGUI copies."""
from pathlib import Path
import argparse
import json
import re
import shutil

import compile_r192_remaining_disc_images as compiler
from extract_r192_elf_vu import require, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--proof', type=Path, required=True)
    parser.add_argument('--expected-proof-sha', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(compiler.ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.proof) == args.expected_proof_sha, 'generation_receipt_changed')
    proof = json.loads(args.proof.read_text())
    require(proof['status'] == 'OFFLINE_DISC_IMAGE_GENERATION_PASS', 'successful_source_generation_required')
    files = proof['generated_files']['NTGUI']
    source = Path(files[0]['path']).parent
    require({p.name for p in source.iterdir() if p.is_file()} == {Path(r['path']).name for r in files}, 'generated_file_set_changed')
    for pin in files:
        require(sha(Path(pin['path'])) == pin['sha256'], 'generated_file_changed')
    entry = int(proof['NTGUI_ELF_entry'], 16)
    producer = Path(compiler.__file__)
    pins = {str(p.resolve()): sha(p) for p in [Path(__file__), producer, args.proof]}
    out.mkdir(parents=True)
    positive = compiler.entry_map(source, entry)
    require(positive == proof['NTGUI_entry_map'] and positive['ELF_entry_mapped_to_actual_label'], 'original_entry_map_changed')
    require(not compiler.entry_map(source, entry + 2)['ELF_entry_mapped_to_actual_label'], 'unmapped_ELF_entry_admitted')
    cases = [{'case': 'authenticated_original_entry_map', 'passed': True, 'measurement': positive},
             {'case': 'unmapped_ELF_entry', 'passed': True}]
    first = next(p for p in sorted(source.glob('unit_*.cpp')) if re.search(r'case \d+u: goto L_[0-9a-fA-F]+;', p.read_text()))
    for name, expected in [('wrong-array-size', 'compiled_entry_array_extent'),
                           ('wrong-target', 'compiled_entry_target_changed'),
                           ('dropped-case', 'compiled_entry_target_changed')]:
        directory = out / name
        shutil.copytree(source, directory)
        target = directory / first.name
        text = target.read_text()
        if name == 'wrong-array-size':
            text, n = re.subn(r'(const std::uint16_t \w+\[)(\d+)(\])', lambda m: m[1] + str(int(m[2]) + 1) + m[3], text, count=1)
        elif name == 'wrong-target':
            text, n = re.subn(r'(case \d+u: goto L_)([0-9a-fA-F]+)(;)', lambda m: m[1] + f'{int(m[2],16)^4:08X}' + m[3], text, count=1)
        else:
            text, n = re.subn(r'case \d+u: goto L_[0-9a-fA-F]+;', '', text, count=1)
        require(n == 1, 'fixture_seam_changed')
        target.write_text(text, newline='\n')
        try:
            compiler.entry_map(directory, entry)
        except ValueError as error:
            require(str(error) == expected, 'wrong_refusal:' + str(error))
            cases.append({'case': name, 'passed': True, 'refusal': str(error), 'changed_source_sha256': sha(target)})
        else:
            raise ValueError('planted_invalid_map_accepted:' + name)
    require(pins == {p: sha(Path(p)) for p in pins}, 'verification_input_changed')
    for pin in files:
        require(sha(Path(pin['path'])) == pin['sha256'], 'original_generated_source_changed')
    result = {'status': 'PASS_DISC_ENTRY_MAP_FALSIFICATION', 'input_pins': pins, 'cases': cases,
              'scope': 'Source entry-map/root guards only; no original/game/network execution or native credit.'}
    target = out / 'RESULT.json'
    target.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'result_sha256': sha(target)}))


if __name__ == '__main__':
    main()
