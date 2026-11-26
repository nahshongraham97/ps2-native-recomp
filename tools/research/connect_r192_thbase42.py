"""Add only the proved thbase1.2/42 service to a NEW disposable v8 copy.

Delegates the published compiler/dispatcher builder without editing its source.
An in-process source-preparation hook adds one explicit typed registration to
the private kernel copy. Original N, cards, launchers and the frozen20 stay intact.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys

import analyze_r192_thbase42 as analysis
import build_r192_iop_v8_candidate as builder
import generate_r192_iop_aot as original

W = original.W
require = original.require
sha = original.file_sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('evidence', 'verification', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-evidence-sha', required=True)
    parser.add_argument('--expected-verification-sha', required=True)
    args, remaining = parser.parse_known_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.evidence) == args.expected_evidence_sha.lower() and
            sha(args.verification) == args.expected_verification_sha.lower(), 'source_or_behavior_evidence_pin_mismatch')
    role = json.loads(args.evidence.read_text()); tested = json.loads(args.verification.read_text())
    require(role['producer']['sha256'] == sha(Path(analysis.__file__)) and role['SDK']['commit'] == analysis.SDK_COMMIT and
            role['contract']['ordinal'] == 42 and role['contract']['version'] == '0102', 'source_role_contract_mismatch')
    for item in role['SDK']['files']:
        require(sha(Path(item['copy'])) == item['sha256'] == analysis.SDK_FILES[item['path']], 'official_SDK_pin_mismatch')
    for item in role['original_definitions'] + role['original_retail_imports']:
        require(sha(Path(item['original']['path'])) == item['original']['sha256'], 'original_IRX_changed')
    require(tested['status'] == 'THBASE42_ORIGINAL_METHOD_PASS' and all(c['status'] == 'REJECTED' for c in tested['controls']),
            'original_method_and_semantic_controls_required')
    header = W / 'runtime/src/iop/r192_thbase42.hpp'
    tested_header = next(p for p in tested['source_pins'] if Path(p['path']).resolve() == header.resolve())
    require(sha(header) == tested_header['sha256'], 'typed_service_changed_after_differential')
    frozen = W / 'build/oracle-telemetry/r192-iop-source-freeze-01/RESULT.json'
    require(sha(frozen) == 'd8c758d8bb43801adb3ef7904803b4627ce722a8d3a03ab9c86d93464c9ad13d', 'frozen_phase_receipt_changed')
    for source in json.loads(frozen.read_text())['source_files']:
        require(sha(Path(source['path'])) == source['sha256'], 'published_compiler_dispatcher_source_changed')
    original_prepare = builder.prepare_kernel
    def connected_prepare(data, include):
        text = original_prepare(data, include)
        before = '#include "iop/kernel.hpp"'
        require(text.count(before) == 1 and text.count('    install_sif();') == 1, 'single_service_connection_anchor_changed')
        text = text.replace(before, before + '\n#include ' + json.dumps(header.as_posix()))
        text = text.replace('    install_sif();', '    install_sif();\n    ps2::iop::r192_thbase42::install();')
        return text
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in
            [Path(__file__), Path(analysis.__file__), Path(builder.__file__), header, args.evidence, args.verification]]
    argv = [str(Path(builder.__file__)), *remaining, '--output', str(args.output)]
    require('--output' not in remaining, 'duplicate_output_argument_refused')
    old_argv = sys.argv
    try:
        builder.prepare_kernel = connected_prepare
        sys.argv = argv
        builder.main()
    finally:
        builder.prepare_kernel = original_prepare
        sys.argv = old_argv
    result = json.loads((args.output / 'BUILD.json').read_text())
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'connection_source_changed_during_build')
    receipt = {'status': 'PROVED_THBASE42_DISPOSABLE_CONNECTION_BUILT', 'classification': 'source-supported reference-route correction, capability only',
        'native_credit': 0, 'source_pins': pins, 'delegated_builder_argv': argv,
        'builder_receipt_sha256': sha(args.output / 'BUILD.json'), 'candidate': result['candidate'], 'candidate_sha256': result['candidate_sha256'],
        'scope': 'Exactly one SDK1.2 ordinal42 native registration in a fresh private kernel copy. Published compiler/dispatcher sources and original N untouched.',
        'limits': ['Not equivalent to the old unbound synthetic zero; original source-supported service semantics are authoritative.',
                   'No-thread/out-of-signed16 state refuses; interrupt context returns proved -100. Game replay must remain strict and may diverge.']}
    target = args.output / 'CONNECTION.json'; target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'candidate_sha256': result['candidate_sha256']}))


if __name__ == '__main__': main()
