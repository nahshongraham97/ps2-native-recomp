"""Read-only whole-disc search for the original structured MPG upload format.

This is an offline input-discovery pass, not proof of launchability, unit identity,
all possible upload formats, generated patches or whole-game equivalence.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import mmap
import re
import struct

from extract_r192_elf_vu import payload, require, sha
from inventory_r192_code_inputs import ISO_SHA

ROOT = Path(__file__).resolve().parents[2]
# DMA RET/END, zero address/tag words, then MPG. The QWC and MPG arguments
# remain variable. Filtering/reading is bounded separately by each disc file.
HEADER = re.compile(rb"..\x00[\x60\x70]\x00{8}...[\x4a\xca]", re.DOTALL)


def stream_sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 << 20), b''):
            h.update(block)
    return h.hexdigest()


def scan_file(data, start, size):
    """Return every matching aligned upload plus explicit parse refusals."""
    require(0 <= start <= len(data) and 0 <= size <= len(data)-start,
            'disc_file_bounds')
    rows, refused = [], []
    for match in HEADER.finditer(data, start, start+size):
        at = match.start()
        if (at-start) % 16:
            continue
        qwc = struct.unpack_from('<I', data, at)[0] & 65535
        end = at+16+qwc*16
        if end > start+size:
            refused.append({'file_offset': at-start, 'reason': 'DMA_crosses_disc_file'})
            continue
        raw = bytes(data[at:end])
        try:
            image, origin = payload(raw, 0)
        except ValueError as error:
            refused.append({'file_offset': at-start, 'reason': str(error)})
            continue
        bitmap = bytearray(2048)
        for span in origin['spans']:
            for pc in range(span['VU_offset'], span['VU_offset']+span['size'], 8):
                bitmap[pc//8] = 1
        rows.append({'file_offset': at-start, 'ISO_offset': at, 'bytes': len(raw),
                     'raw_sha256': hashlib.sha256(raw).hexdigest(),
                     'image_sha256': hashlib.sha256(image).hexdigest(),
                     'bitmap_sha256': hashlib.sha256(bitmap).hexdigest(),
                     'uploaded_pairs': sum(bitmap),
                     'fits_VU0_4KiB_address_domain': not any(bitmap[512:]),
                     'spans': [{**s, 'file_offset': at-start+s['file_offset'],
                                'ISO_offset': at+s['file_offset']} for s in origin['spans']],
                     'commands': origin['commands'], '_image': image, '_bitmap': bitmap})
    return rows, refused


def self_test():
    good = struct.pack('<8I', 0x60000001, 0, 0, 0x4a020000,
                       0x8000033c, 0x400002ff, 0x8000033c, 0x2ff)
    rows, bad = scan_file(good, 0, len(good))
    require(len(rows) == 1 and not bad and rows[0]['uploaded_pairs'] == 2,
            'original_shape_positive')
    # Two top-level files: the payload cannot borrow bytes from its neighbour.
    rows, bad = scan_file(good, 0, 24)
    require(not rows and len(bad) == 1 and bad[0]['reason'] == 'DMA_crosses_disc_file',
            'cross_file_borrow_accepted')
    rows, bad = scan_file(b'X'+good, 0, len(good)+1)
    require(not rows and not bad, 'unaligned_header_accepted')
    mutated = good[:12]+struct.pack('<I', 0x4a0207ff)+good[16:]
    rows, bad = scan_file(mutated, 0, len(mutated))
    require(not rows and len(bad) == 1 and bad[0]['reason'] == 'VU_code_extent',
            'destination_overflow_accepted')
    # Both files must be independently searched; matching an earlier file does
    # not terminate the search or treat later content as absent.
    two = good+b'\0'*32+good
    found = [row for at, size in [(0, 32), (32, 64)]
             for row in scan_file(two, at, size)[0]]
    require([r['ISO_offset'] for r in found] == [0, 64], 'later_file_ignored')
    return ['original_shape', 'cross_file_borrow_rejected',
            'unaligned_header_excluded', 'VU_extent_overflow_rejected',
            'later_file_searched']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--expected-inventory-sha', required=True)
    parser.add_argument('--known-generation', type=Path, required=True)
    parser.add_argument('--expected-known-sha', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT/'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.inventory) == args.expected_inventory_sha, 'disc_inventory_changed')
    require(sha(args.known_generation) == args.expected_known_sha, 'known_generation_changed')
    inventory = json.loads(args.inventory.read_text())
    require(inventory['status'] == 'READ_ONLY_KNOWN_CONTAINER_CODE_CENSUS'
            and inventory['ISO_sha256'] == ISO_SHA, 'known_disc_inventory_required')
    known = json.loads(args.known_generation.read_text())
    require(known['status'] == 'ALL_UPLOADED_ENTRY_ENUMERATION_COMPLETE',
            'original_upload_generation_required')
    known_pairs = {(r['image_sha256'], r['uploaded_bitmap_sha256']) for r in known['records']}
    before = args.iso.stat()
    require(stream_sha(args.iso) == ISO_SHA and before.st_size == inventory['ISO_bytes'], 'original_ISO_pin')
    disc_source = ROOT/'scripts/disc.py'
    spec = importlib.util.spec_from_file_location('r192_vu_disc_reader', disc_source)
    disc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(disc)
    image = disc.Image(str(args.iso))
    try:
        files = list(image.files())
    finally:
        image.file.close()
    require(len(files) == inventory['disc_files'] and len({f[0] for f in files}) == len(files),
            'full_disc_file_denominator')
    pins = {str(path.resolve()): sha(path) for path in
            [Path(__file__), Path(__file__).with_name('extract_r192_elf_vu.py'),
             Path(__file__).with_name('inventory_r192_code_inputs.py'), disc_source,
             args.inventory, args.known_generation]}
    out.mkdir(parents=True)
    tests = self_test()
    rows, refusals, scanned = [], [], []
    unique = set()
    with args.iso.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        for name, _, sector, size in files:
            start = sector*disc.SECTOR
            found, bad = scan_file(data, start, size)
            scanned.append({'path': name, 'ISO_offset': start, 'bytes': size,
                            'matched': len(found), 'refused': len(bad)})
            for row in found:
                pair = row['image_sha256'], row['bitmap_sha256']
                row['path'] = name
                row['already_in_original_ELF_generation'] = pair in known_pairs
                # Identify the narrowest container span from the independent
                # structural census. A matching range is not a callable root.
                owners = [c for c in inventory['SEC_sections']+inventory['containers']
                          if c['ISO_offset'] <= row['ISO_offset']
                          and row['ISO_offset']+row['bytes'] <= c['ISO_offset']+c['bytes']]
                owner = min(owners, key=lambda c: c['bytes']) if owners else None
                row['narrowest_container'] = ({k: owner[k] for k in ['path', 'ISO_offset', 'bytes']}
                                              if owner else None)
                stem = row['image_sha256']+'-'+row['bitmap_sha256']
                if pair not in unique:
                    (out/(stem+'.image.bin')).write_bytes(row['_image'])
                    (out/(stem+'.bitmap')).write_bytes(row['_bitmap'])
                unique.add(pair)
                del row['_image'], row['_bitmap']
                rows.append(row)
            refusals.extend({'path': name, **row} for row in bad)
    require(all(pair in unique for pair in known_pairs), 'earlier_original_upload_missing_from_disc')
    after = args.iso.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns)
            and stream_sha(args.iso) == ISO_SHA, 'original_ISO_changed')
    require(pins == {p: sha(Path(p)) for p in pins}, 'input_changed')
    result = {'status': 'WHOLE_DISC_STRUCTURED_MPG_SCAN_COMPLETE', 'ISO_sha256': ISO_SHA,
              'input_pins': pins, 'controls': tests, 'scanned_files': scanned,
              'disc_file_count': len(scanned), 'bytes_scanned': sum(f['bytes'] for f in scanned),
              'matched_payload_instances': len(rows), 'unique_image_bitmap_pairs': len(unique),
              'known_original_ELF_pairs': len(known_pairs),
              'new_pairs': len(unique-known_pairs), 'payloads': rows, 'refusals': refusals,
              'limits': ['Only the exact structured DMA RET/END plus MPG upload format is recognized.',
                         'MPG records do not identify VU0 versus VU1 or prove a launch root.',
                         'Mixed command chains, raw bus uploads, runtime patches, compressed payloads and generated uploads remain separate.',
                         'No game/card/runtime changes, ownership/native credit or universal coverage claim.']}
    target = out/'RESULT.json'
    target.write_text(json.dumps(result, indent=2)+'\n', newline='\n')
    print(json.dumps({k: result[k] for k in ['status', 'disc_file_count', 'bytes_scanned',
                                           'matched_payload_instances', 'unique_image_bitmap_pairs',
                                           'known_original_ELF_pairs', 'new_pairs']}
                     | {'parse_refusals': len(refusals), 'result_sha256': sha(target)}))


if __name__ == '__main__':
    main()
