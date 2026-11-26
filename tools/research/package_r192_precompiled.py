"""Seal the pinned R192 EE cache as a manifest-checked, precompiled DLL package.

This copies already compiled artifacts. It never loads DLLs or launches a game.
The source harvest and its static PE metadata must match their publication pins.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid

PACKAGE_SHA = '1957949806ff214a3d298d14fea04e04a733e73ec342afc4d5ef0f18b64a9927'
METADATA_SHA = 'c2bc1df3d1f7e6734a0cb4677e77d45954d81d4685589936368f9e50218203b7'
HARVEST_SHA = '8ddf50297d2f4490785051035da7e4da9f5ac852c0f6b298d4e082ca542e3bf3'
COMPILER_SHA = 'ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe'
STAMP = 'A2659FA9BD846661'
CPU_SIZE = 1456
HOST_SIZE = 328
COUNT = 3569


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def pinned(path, expected):
    data = path.read_bytes()
    require(digest(data) == expected, 'pin_mismatch:' + str(path))
    return data


def validate_entries(package, metadata, harvest):
    require(package['status'] == 'STATIC_PE_CACHE_METADATA_COMPLETE', 'incomplete_package_metadata')
    require(metadata['status'] == 'STATIC_PE_CACHE_METADATA_COMPLETE' and not metadata['errors'],
            'incomplete_PE_decode')
    records = {Path(row['artifact']['copy']).name: row for row in metadata['EE']}
    require(len(records) == COUNT, 'wrong_EE_metadata_count')
    entries = []
    for entry in package['artifacts']:
        if entry['domain'] != 'ee':
            continue
        relative = Path(entry['file'])
        require(relative.parts[:3] == ('copies', 'game', 'ee-live') and len(relative.parts) == 4,
                'invalid_package_path')
        require(relative.suffix == '.dll' and relative.name.startswith('a2659fa9-'), 'invalid_DLL_basename')
        source = harvest / relative
        require(source.resolve().parent == (harvest / 'copies/game/ee-live').resolve(), 'cache_path_escape')
        require(not source.is_symlink(), 'symlink_DLL_refused')
        row = records.get(relative.name)
        require(row is not None, 'missing_PE_record')
        require(entry['stamp'] == row['stamp'] == STAMP, 'wrong_stamp')
        rank = 0 if 'q' in relative.name[9:] else 1
        require(entry['rank'] == row['rank'] == rank, 'wrong_rank')
        require(entry['record_count'] == len(row['modules']) > 0, 'wrong_module_count')
        require(entry['sha256'] == row['artifact']['sha256'] and entry['size'] == row['artifact']['size'],
                'metadata_artifact_mismatch')
        require('b005000048010000' in row['init_bytes'], 'unjoined_Cpu_Host_layout')
        entries.append((relative.name, source, entry))
    entries.sort(key=lambda row: row[0])
    require(len(entries) == COUNT and len({row[0] for row in entries}) == COUNT, 'wrong_unique_DLL_count')
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--harvest', required=True, type=Path)
    parser.add_argument('--destination', required=True, type=Path)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--abi-include', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    require(not args.output.exists(), 'receipt_directory_must_be_fresh')
    require(not args.destination.exists(), 'destination_must_be_fresh_never_overwrite')
    package = json.loads(pinned(args.harvest / 'PACKAGE-03.json', PACKAGE_SHA))
    metadata = json.loads(pinned(args.harvest / 'METADATA-03.json', METADATA_SHA))
    harvest = json.loads(pinned(args.harvest / 'RESULT.json', HARVEST_SHA))
    entries = validate_entries(package, metadata, args.harvest)
    pinned(args.compiler, COMPILER_SHA)
    # Inspect exact harvested headers, not a replacement structure authored for this query.
    include = args.abi_include.resolve()
    require(include == (args.harvest/'copies/game/ee-live/include').resolve(), 'unproved_ABI_include')
    headers = []
    harvested_hashes = {Path(row['copy']).resolve(): row['sha256'] for row in harvest['inventory']}
    for path in sorted(include.rglob('*.hpp')):
        require(not path.is_symlink(), 'symlink_ABI_header_refused')
        require(path in harvested_hashes, 'unrecorded_ABI_header')
        pinned(path, harvested_hashes[path])
        headers.append({'path': str(path), 'sha256': harvested_hashes[path]})
    require(headers, 'missing_ABI_headers')
    query = ('#include "ps2/ee_live_abi.hpp"\n'
             'static_assert(sizeof(ps2::Cpu)==1456);\n'
             'static_assert(sizeof(ps2::EeLiveHost)==328);\n'
             'ps2::Cpu cpu_layout; ps2::EeLiveHost host_layout;\n')
    command = [str(args.compiler), '-std=c++20', '-fsyntax-only', '-I'+str(include),
               '-Xclang', '-fdump-record-layouts', '-x', 'c++', '-']
    checked = subprocess.run(command, input=query, text=True, capture_output=True)
    require(checked.returncode == 0, 'Cpu_or_EeLiveHost_static_assert_failed:'+checked.stderr)
    args.output.mkdir(parents=True, exist_ok=False)
    layout = args.output/'ABI-layout.log'
    layout.write_text(checked.stdout+checked.stderr, encoding='utf-8', newline='\n')
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    staging = args.destination.with_name(args.destination.name+'.staging-'+uuid.uuid4().hex)
    staging.mkdir(exist_ok=False)
    receipts = []
    for name, source, entry in entries:
        before = source.stat()
        data = pinned(source, entry['sha256'])
        require(len(data) == entry['size'], 'DLL_size_changed')
        after = source.stat()
        require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), 'source_changed_during_read')
        target = staging/name
        with target.open('xb') as stream:
            require(stream.write(data) == len(data), 'short_DLL_write')
            stream.flush()
            os.fsync(stream.fileno())
        require(digest(target.read_bytes()) == entry['sha256'], 'copied_DLL_hash_mismatch')
        receipts.append({'basename': name, 'sha256': entry['sha256'], 'rank': entry['rank'],
                         'record_count': entry['record_count'], 'size': len(data), 'source': str(source.resolve())})
    text = f'R192_AOT_EE_V1 {STAMP} {CPU_SIZE} {HOST_SIZE} {COUNT}\n'
    text += ''.join(f"{row['sha256']} {row['rank']} {row['record_count']} {row['basename']}\n" for row in receipts)
    manifest = staging/'manifest.txt'
    with manifest.open('x', encoding='ascii', newline='\n') as stream:
        require(stream.write(text) == len(text), 'short_manifest_write')
        stream.flush()
        os.fsync(stream.fileno())
    require(manifest.read_bytes() == text.encode('ascii'), 'manifest_roundtrip_failed')
    require(not args.destination.exists(), 'concurrent_destination_creator_refused')
    # Windows rename refuses an existing destination. No source/cache file is changed.
    staging.rename(args.destination)
    result = {
        'status': 'BUILD_PACKAGE_PASS', 'scope': 'Already compiled EE DLL package; no DLL execution or gameplay/native-acceptance claim.',
        'tool': {'path': str(Path(__file__).resolve()), 'sha256': digest(Path(__file__).read_bytes())},
        'harvest': str(args.harvest.resolve()), 'package_metadata_sha256': PACKAGE_SHA,
        'harvest_manifest_sha256': HARVEST_SHA,
        'static_PE_metadata_sha256': METADATA_SHA, 'destination': str(args.destination.resolve()),
        'manifest': str((args.destination/'manifest.txt').resolve()), 'manifest_sha256': digest(text.encode('ascii')),
        'stamp': STAMP, 'Cpu_size': CPU_SIZE, 'EeLiveHost_size': HOST_SIZE, 'DLL_count': len(receipts),
        'ranks': {'normal': sum(row['rank'] == 1 for row in receipts), 'quick': sum(row['rank'] == 0 for row in receipts)},
        'ABI_check': {'command': command, 'stdin': query, 'compiler_sha256': COMPILER_SHA,
                      'exit_code': checked.returncode, 'headers': headers, 'layout_log_sha256': digest(layout.read_bytes())},
        'artifacts': receipts,
    }
    target = args.output/'BUILD_PACKAGE.json'
    target.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target.resolve()), 'sha256': digest(target.read_bytes()),
                      'manifest_sha256': result['manifest_sha256'], 'DLL_count': len(receipts)}))


if __name__ == '__main__':
    main()
