"""Inspect sealed R192 cache DLL tables without loading or executing a DLL.

Inputs are an explicit pinned, owner-preserved harvest inventory and its copies.
The supported init shapes are the measured Clang O0/O3/AVX and GCC forms.
No game, live process, source cache or persistent card is accessed.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct

DEFAULT_INVENTORY_SHA = '8ddf50297d2f4490785051035da7e4da9f5ac852c0f6b298d4e082ca542e3bf3'

def sha(d):
    return hashlib.sha256(d).hexdigest()

def req(b, s):
    if not b:
        raise ValueError(s)

def u32(d, o):
    return struct.unpack_from('<I', d, o)[0]

def u64(d, o):
    return struct.unpack_from('<Q', d, o)[0]

class PE:

    def __init__(self, d):
        self.d = d
        p = u32(d, 60)
        req(d[p:p + 4] == b'PE\x00\x00', 'PE_signature')
        n = struct.unpack_from('<H', d, p + 6)[0]
        z = struct.unpack_from('<H', d, p + 20)[0]
        op = p + 24
        req(struct.unpack_from('<H', d, op)[0] == 523, 'PE32plus')
        self.base = u64(d, op + 24)
        self.sections = []
        for i in range(n):
            at = op + z + i * 40
            vs, va, sz, raw = struct.unpack_from('<IIII', d, at + 8)
            self.sections.append((va, sz, raw))
        ex = u32(d, op + 112)
        at = self.off(ex)
        _, _, _, _, _, _, nf, nn, fa, na, oa = struct.unpack_from('<IIHHIIIIIII', d, at)
        self.exports = {}
        for i in range(nn):
            name = self.string(self.base + u32(d, self.off(na) + 4 * i))
            ordn = struct.unpack_from('<H', d, self.off(oa) + 2 * i)[0]
            self.exports[name] = u32(d, self.off(fa) + 4 * ordn)

    def off(self, rva, size=1):
        for va, sz, raw in self.sections:
            if 0 <= rva - va <= sz - size:
                return raw + rva - va
        raise ValueError(f'PE_unmapped:{rva:x}/{size}')

    def get(self, va, size):
        return self.d[self.off(va - self.base, size):self.off(va - self.base, size) + size]

    def string(self, va):
        at = self.off(va - self.base)
        end = self.d.index(0, at)
        req(end - at < 1024, 'long_string')
        return self.d[at:end].decode('ascii')

    def table(self, kind):
        init = self.exports[f'ps2_{kind}_live_init']
        at = self.off(init)
        code = self.d[at:at + 256]
        matches = list(re.finditer(b'\\xc7[\\x00\\x02](.{4})\\x48\\x8d\\x05(.{4})', code, re.S))
        if matches:
            req(len(matches) == 1, 'ambiguous_init_table')
            m = matches[0]
            count = struct.unpack('<I', m[1])[0]
            target = self.base + init + m.start() + 13 + struct.unpack('<i', m[2])[0]
        else:
            matches = list(re.finditer(b'\\x48\\x8d\\x05(.{4})\\xc7\\x02(.{4})\\xc3', code, re.S))
            req(len(matches) == 1, 'unsupported_GCC_init_table')
            m = matches[0]
            count = struct.unpack('<I', m[2])[0]
            target = self.base + init + m.start() + 7 + struct.unpack('<i', m[1])[0]
        req(0 < count < 20000, 'invalid_table_count')
        stampat = self.off(self.exports[f'ps2_{kind}_live_stamp'])
        stamp = self.d[stampat:stampat + 11]
        req(stamp[:2] == b'H\xb8' and stamp[-1:] == b'\xc3', 'unsupported_stamp_shape')
        return (target, count, f"{struct.unpack_from('<Q', stamp, 2)[0]:016X}", code[:m.end() + 20].hex())

def decode_ee(d):
    pe = PE(d)
    table, count, stamp, init = pe.table('ee')
    mods = []
    for i in range(count):
        at = pe.off(table - pe.base + 48 * i, 48)
        name = pe.string(u64(d, at))
        base, size = struct.unpack_from('<II', d, at + 8)
        checks = u64(d, at + 16)
        nc = u32(d, at + 24)
        units = u64(d, at + 32)
        nu = u32(d, at + 40)
        req(size <= 33554432 and nc <= 2097152 and (nu <= 20000), 'module_bounds')
        pairs = [struct.unpack_from('<II', pe.get(checks + 8 * j, 8)) for j in range(nc)]
        rows = []
        for j in range(nu):
            raw = pe.get(units + 24 * j, 24)
            ub, us = struct.unpack_from('<II', raw)
            fn, entries = struct.unpack_from('<QQ', raw, 8)
            req(us % 4 == 0 and us <= 33554432, 'unit_bounds')
            pe.off(fn - pe.base)
            emap = pe.get(entries, us // 2)
            ent = [{'offset': ub + 4 * k, 'entry_id': v[0]} for k, v in enumerate(struct.iter_unpack('<H', emap)) if v[0]]
            rows.append({'base': ub, 'size': us, 'function_PE_VA': f'{fn:016X}', 'entries': ent})
        mods.append({'name': name, 'fixed_base': base, 'size': size, 'check_count': nc, 'checks': [{'offset': a, 'word': f'{b:08X}'} for a, b in pairs], 'unit_count': nu, 'units': rows, 'entry_count': sum((len(x['entries']) for x in rows))})
    return {'stamp': stamp, 'init_bytes': init, 'modules': mods}

def decode_vu(d):
    pe = PE(d)
    table, count, stamp, init = pe.table('vu')
    rows = []
    for i in range(count):
        key, fn = struct.unpack('<QQ', pe.get(table + 16 * i, 16))
        pe.off(fn - pe.base)
        rows.append({'key': f'{key:016X}', 'function_PE_VA': f'{fn:016X}'})
    return {'stamp': stamp, 'init_bytes': init, 'programs': rows}


def owned(path):
    return path.resolve().is_relative_to(Path(__file__).resolve().parents[2]/'build')


def copy_path(harvest, artifact):
    # Retained inventories contain absolute original paths; resolve their copies
    # relative to the explicitly supplied harvest for portable offline replay.
    parts = Path(artifact['copy']).parts
    positions = [i for i, value in enumerate(parts) if value == 'copies']
    req(len(positions) == 1, 'ambiguous_harvest_copy_path')
    relative = Path(*parts[positions[0]:])
    path = harvest/relative
    req(path.resolve().is_relative_to(harvest.resolve()) and not path.is_symlink(), 'harvest_path_escape')
    return path, relative


def summarize(ee, vu, errors):
    return {
        'EE_DLLs': len(ee), 'EE_module_records': sum(len(x['modules']) for x in ee),
        'EE_unique_module_names': len({m['name'] for x in ee for m in x['modules']}),
        'EE_unique_content_checks': len({(m['fixed_base'], m['size'], tuple((c['offset'], c['word']) for c in m['checks'])) for x in ee for m in x['modules']}),
        'EE_entry_records': sum(m['entry_count'] for x in ee for m in x['modules']),
        'EE_stamp_counts': dict(Counter(x['stamp'] for x in ee)),
        'EE_rank_counts': {str(k): v for k, v in Counter(x['rank'] for x in ee).items()},
        'VU_DLLs': len(vu), 'VU_program_records': sum(len(x['programs']) for x in vu),
        'VU_unique_keys': len({p['key'] for x in vu for p in x['programs']}),
        'VU_stamp_counts': dict(Counter(x['stamp'] for x in vu)), 'errors': len(errors),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--harvest', type=Path, required=True)
    parser.add_argument('--expected-inventory-sha', default=DEFAULT_INVENTORY_SHA)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference-metadata', type=Path)
    parser.add_argument('--expected-reference-sha')
    args = parser.parse_args()
    req(owned(args.harvest) and owned(args.output), 'explicit_owned_build_paths_required')
    req(not args.output.exists(), 'fresh_output_required')
    inventory_bytes = (args.harvest/'RESULT.json').read_bytes()
    req(sha(inventory_bytes) == args.expected_inventory_sha.lower(), 'harvest_inventory_pin_mismatch')
    original = json.loads(inventory_bytes)
    ee, vu, errors, package = [], [], [], []
    for artifact in original['inventory']:
        path, relative = copy_path(args.harvest, artifact)
        if path.suffix != '.dll':
            continue
        req(relative.parts[:3] in (('copies', 'game', 'ee-live'), ('copies', 'game', 'vu1-live')),
            'unexpected_DLL_tree')
        before = path.stat()
        raw = path.read_bytes()
        after = path.stat()
        req((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), 'DLL_changed_during_read')
        req(len(raw) == artifact['size'] and sha(raw) == artifact['sha256'], 'DLL_pin_mismatch')
        kind = 'ee' if path.parent.name == 'ee-live' else 'vu'
        try:
            decoded = decode_ee(raw) if kind == 'ee' else decode_vu(raw)
            decoded['artifact'] = artifact
            decoded['rank'] = 0 if 'q' in path.name[9:] else 1
            decoded['metadata_scope'] = 'Static PE parse, no DLL loading or execution. Tables joined to pinned generated.hpp/codegen.cpp ABI.'
            (ee if kind == 'ee' else vu).append(decoded)
            package.append({'domain': kind, 'file': str(relative), 'size': len(raw), 'sha256': artifact['sha256'],
                            'stamp': decoded['stamp'], 'rank': decoded['rank'],
                            'module_names': [m['name'] for m in decoded['modules']] if kind == 'ee' else [],
                            'record_count': len(decoded['modules'] if kind == 'ee' else decoded['programs'])})
        except Exception as exc:
            errors.append({'copy': str(path), 'error': str(exc)})
    summary = summarize(ee, vu, errors)
    comparison = None
    if args.reference_metadata:
        req(args.expected_reference_sha is not None, 'reference_SHA_required')
        reference_bytes = args.reference_metadata.read_bytes()
        req(sha(reference_bytes) == args.expected_reference_sha.lower(), 'reference_pin_mismatch')
        reference = json.loads(reference_bytes)
        comparison = {'path': str(args.reference_metadata.resolve()), 'sha256': sha(reference_bytes),
                      'EE_exact': ee == reference['EE'], 'VU_exact': vu == reference['VU'],
                      'summary_exact': summary == reference['summary']}
    status = 'STATIC_PE_CACHE_METADATA_COMPLETE' if not errors else 'INCOMPLETE'
    if comparison and not all(comparison[name] for name in ('EE_exact', 'VU_exact', 'summary_exact')):
        status = 'REFERENCE_COMPARISON_FAILED'
    args.output.mkdir(parents=True, exist_ok=False)
    report = {'status': status, 'tool': {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__).read_bytes())},
              'harvest_RESULT_sha256': sha(inventory_bytes), 'summary': summary, 'EE': ee, 'VU': vu,
              'errors': errors, 'reference_comparison': comparison,
              'scope': 'Read-only static PE decoding of pinned owned copies. No DLL, compiler, game, process or card execution/access.'}
    pins = {}
    for name, value in [('METADATA.json', report), ('PACKAGE.json', {'status': status, 'artifacts': package, 'summary': summary})]:
        target = args.output/name
        target.write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8', newline='\n')
        pins[name] = sha(target.read_bytes())
    receipt = {'status': status, 'source_sha256': report['tool']['sha256'], 'outputs': pins,
               'summary': summary, 'reference_comparison': comparison}
    target = args.output/'RESULT.json'
    target.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target.resolve()), 'sha256': sha(target.read_bytes()), **receipt}))
    return 0 if status == 'STATIC_PE_CACHE_METADATA_COMPLETE' else 1


if __name__ == '__main__':
    raise SystemExit(main())
