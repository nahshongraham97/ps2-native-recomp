"""Read-only original IRX/official SDK joins for the thbase1.2 ordinal42 API."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import struct

import generate_r192_iop_aot as original

W = original.W
require = original.require
sha = original.file_sha
SDK_COMMIT = 'd4b9943bd4a58dbe69800bf4ec9d81640df5bc5b'
SDK_FILES = {
    'iop/system/threadman/src/exports.tab': 'aaebc7b7395b20aacd9d2125346c0d2fcbad895ace1a590ce27b31a03c97c1af',
    'iop/system/threadman/include/xthbase.h': '62faaa305adca53f9b3187b68ee799a8c8dfd92487ce5bebc0992eb37a8f2037',
    'iop/system/threadman/src/thbase.c': '3d734f3b1fce515875d9f13dc4a36a309fbabe39b45ef2d328bd35ac224c9dd5',
    'iop/kernel/include/kerr.h': '8977f2f47b0e9fa10340c0d07163f01aaa5bd60d3866be9aca0929b058067f89',
}
BODY = [0x27BDFFE8, 0xAFB00010, 0x3C100000, 0x8E107080, 0xAFBF0014, 0x0C001ACC,
        0, 0x14400002, 0x2402FF9C, 0x8602000E, 0x8FBF0014, 0x8FB00010, 0x03E00008, 0x27BD0018]


def prove(domain_path, sdk_path):
    require(sha(domain_path) == 'ef311498267533dd6d2e7ca9175d568255bded60ea97fcef81f89bc4de2c29b6', 'original_domain_pin_mismatch')
    domain = json.loads(domain_path.read_text())
    sdk = json.loads(sdk_path.read_text())
    require(sdk['commit'] == SDK_COMMIT and {r['path'] for r in sdk['files']} == set(SDK_FILES), 'official_SDK_identity_mismatch')
    for item in sdk['files']:
        require(item['sha256'] == SDK_FILES[item['path']] and sha(Path(item['copy'])) == item['sha256'], 'official_SDK_byte_pin_mismatch')
        require(item['url'] == 'https://raw.githubusercontent.com/ps2dev/ps2sdk/' + SDK_COMMIT + '/' + item['path'], 'official_SDK_URL_mismatch')
    irx = original.load_tool('thbase42_irx', W / 'scripts/iop_irx_imports.py')
    records = domain['records']
    definitions, imports = [], []
    for row in records:
        if row['alias'] not in ('MODULES/DNAS310.IMG!THREADMAN', 'NETGUI/IOPMODS/IOPRP300.IMG!THREADMAN',
                                'MODULES/MSL.IRX', 'MODULES/SDRDRV.IRX'):
            continue
        source = Path(row['original_copy']); raw = source.read_bytes()
        require(original.sha(raw) == row['original_sha256'], 'original_IRX_changed')
        image, load, name = irx._parse_elf_load_image(raw)
        relocations = irx.scan_relocations(raw, len(image))
        def word(at): return original.u32(image, at)
        def span(start, end):
            require(0 <= start < end <= load['file_size'], 'original_instruction_span_outside_load')
            data = bytes(image[start:end])
            require(raw[load['file_offset'] + start:load['file_offset'] + end] == data, 'ELF_file_image_byte_join_failed')
            return {'image_start': f'{start:08X}', 'image_end_exclusive': f'{end:08X}',
                    'ELF_file_offset': load['file_offset'] + start, 'sha256': original.sha(data),
                    'words': [{'image_offset': f'{at:08X}', 'file_offset': load['file_offset'] + at,
                               'word': f'{word(at):08X}'} for at in range(start, end, 4)]}
        common = {'alias': row['alias'], 'original': {'path': str(source), 'sha256': row['original_sha256']},
                  'address_form': 'IRX image offset; file offset=PT_LOAD.file_offset+offset; runtime VA=physical load base+offset', 'load': load}
        if 'THREADMAN' in row['alias']:
            header = 0x67B0; slot = header + 20 + 42 * 4; target = word(slot)
            require(word(header) == 0x41C00000 and image[header + 12:header + 20].rstrip(b'\0') == b'thbase' and
                    word(header + 8) & 0xFFFF == 0x0102, 'original_thbase_export_header_mismatch')
            require(target == 0x1EE8 and any(r['offset'] == slot and r['type'] == 2 for r in relocations), 'original_ordinal42_pointer_relocation_missing')
            require([word(target + 4 * i) for i in range(len(BODY))] == BODY, 'original_priority_method_words_mismatch')
            q = [entry for table in irx.scan_import_tables(image) if table['library'] == 'intrman'
                 for entry in table['imports'] if entry['ordinal'] == 23 and entry['image_offset'] == 0x6B30]
            require(len(q) == 1 and word(0x6B30) == 0x03E00008 and word(0x6B34) == 0x24000017, 'original_QueryIntrContext_import_join_missing')
            require(any(r['offset'] == 0x1EFC and r['type'] == 4 for r in relocations), 'original_QueryIntrContext_call_relocation_missing')
            definitions.append(dict(common, export_header_image_offset=f'{header:08X}', version='0102', ordinal=42,
                pointer_slot_image_offset=f'{slot:08X}', original_target=f'{target:08X}', pointer_slot_word=f'{word(slot):08X}',
                body=span(target, target + 4 * len(BODY)), current_thread_pointer_offset='00007080',
                current_thread_pointer_relocations=[r for r in relocations if r['offset'] in (0x1EF0, 0x1EF4)],
                QueryIntrContext_import_offset='00006B30', return_load={'offset': '00001F0C', 'word': '8602000E', 'signed_bits': 16, 'field_offset': 14},
                interrupt_result={'delay_offset': '00001F08', 'word': '2402FF9C', 'signed_value': -100},
                scanner_limit='The historical export scanner stops at null slot3. Slot42 is independently joined by the original thbase header, bounded table address, R_MIPS32 relocation and original body; no missing code was excluded from all-PC compilation.'))
        else:
            entries = [(table, entry) for table in irx.scan_import_tables(image) if table['library'] == 'thbase'
                       for entry in table['imports'] if entry['ordinal'] == 42]
            if not entries:
                imports.append(dict(common, status='DOES_NOT_IMPORT_ORDINAL42')); continue
            require(len(entries) == 1 and entries[0][0]['version'] & 0xFFFF == 0x0102, 'retail_thbase_import_version_mismatch')
            table, entry = entries[0]; stub = entry['image_offset']
            require(word(stub) == 0x03E00008 and word(stub + 4) == 0x2400002A, 'original_ordinal42_stub_mismatch')
            callers = []
            for at in range(0, row['text_size'], 4):
                value = word(at)
                if value >> 26 in (2, 3) and (value & 0x03FFFFFF) << 2 == stub:
                    require(any(r['offset'] == at and r['type'] == 4 for r in relocations), 'retail_import_call_relocation_missing')
                    callers.append({'call_offset': f'{at:08X}', 'word': f'{value:08X}',
                                    'kind': 'JAL' if value >> 26 == 3 else 'J',
                                    'context': span(max(0, at - 24), min(row['text_size'], at + 56))})
            imports.append(dict(common, status='ORIGINAL_IMPORT_AND_DIRECT_CALLERS_VERIFIED', version='0102', ordinal=42,
                                stub=span(stub, stub + 8), original_direct_callers=callers,
                                observed_snapshot_join={'base': '000A1A00', 'stub_VA': '000A1B98', 'caller_VA': '000AC498', 'RA': '000AC4A0',
                                                        'proved': stub == 0x198 and any(c['call_offset'] == '0000AA98' for c in callers)} if row['alias'] == 'MODULES/MSL.IRX' else None))
    require(len(definitions) == 2 and len(imports) == 2, 'required_original_domain_join_missing')
    require(next(x for x in imports if x['alias'] == 'MODULES/MSL.IRX')['observed_snapshot_join']['proved'], 'actual_MSL_refusal_caller_not_original_joined')
    return {'status': 'ORIGINAL_AND_OFFICIAL_SDK_ROLE_VERIFIED', 'native_credit': 0, 'SDK': sdk,
            'original_definitions': definitions, 'original_retail_imports': imports,
            'contract': {'name': 'GetThreadCurrentPriority', 'library': 'thbase', 'version': '0102', 'ordinal': 42,
                'arguments': [], 'return': 'signed32 in IOP V0; original current priority is LH signed16 from TCB+14',
                'interrupt_context': 'QueryIntrContext true -> -100 (KE_ILLEGAL_CONTEXT)',
                'normal_context': 'existing native current-thread priority, proved within signed16 domain',
                'no_running_thread': 'unproved state: refuse by name, never substitute zero'},
            'limits': ['No whole-game pass or native credit from identification.',
                       'Frozen N formerly returned zero through an unbound import; service connection is a source-supported correction, not equivalence to that fallback.',
                       'Guest stack spills of the original method are reference frame transport, not a fabricated native TCB layout.'],
            'next': 'Compare original method through actual reference execute with native typed query, error/state/width controls, then disposable owned saved-state/game replay.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', type=Path, required=True); parser.add_argument('--sdk-pins', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True); args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    report = prove(args.domain, args.sdk_pins)
    report['producer'] = {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__))}
    args.output.mkdir(parents=True, exist_ok=False); path = args.output / 'RESULT.json'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(path), 'sha256': sha(path), 'status': report['status']}))


if __name__ == '__main__': main()
