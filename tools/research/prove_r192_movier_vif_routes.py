"""Read-only original MOVIER DMA callers and selected VIF packet provenance.

Consumes the frozen five-image decoder masks; does not run game instructions,
reconstruct missing indirect targets, or turn command data into a bus address.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import audit_r192_vu0_routes as audit

AUDIT_SOURCE = 'fc654acb23866b558e459b816d5776ad0b5f5ee2a140c90c355f06e87ea1bc6e'
WRAPPERS = {0x7fb3e8: 0x7fb42c, 0x7fb430: 0x7fb468}
EXPECTED_CALLERS = {0x7fb140: 8, 0x7fb15c: 9, 0x7fb1b8: 8,
                    0x7fb1d4: 9, 0x7fb1f8: 8, 0x7fb208: 9}
PACKET_WORDS = [0xdfa40120, 0x7ba300b0, 0x8f85a4ac, 0x70833b89,
                0x3c041100, 0x3c031400, 0x708334c8, 0x7ca70000,
                0x8fa4011c, 0x8fa30118, 0x70831cc8, 0x70c31cc8,
                0x7ca30010, 0x24a30020]


def require(ok, reason):
    audit.require(ok, reason)


def joined(image, start, expected):
    actual = [image.word(start + 4*i) for i in range(len(expected))]
    require(actual == expected, 'original_selected_instruction_mismatch')
    return [{'pc': f'{start+4*i:08X}', 'file_offset': image.offset(start+4*i),
             'word': f'{w:08X}'} for i, w in enumerate(actual)]


def argument(words, pc):
    state, context = audit.local_constants(words, pc)
    delay = words.get(pc + 4)
    require(delay is not None and not audit.control(delay), 'unsupported_or_missing_call_delay')
    audit.update(state, delay)
    return state[4], context


def verify_forwarding(first, second):
    require(first == [0x27bdfff0, 0xffb00000, 0xffbf0008, 0x0c04091e, 0],
            'first_wrapper_A0_forwarding_unproved')
    require(second == [0x27bdfff0, 0xffb00000, 0xffbf0008, 0x0c04091e, 0x00a0802d],
            'second_wrapper_A0_forwarding_unproved')


def other_overlay_local(name, words, target):
    return name != 'MOVIER' and target in words


def packet_lanes(words):
    """Only this joined straight-line fragment; None is an unbound lane.

    Reg128 upper words remain unknown across low64 writes. PPACW follows the
    separately pinned reference mmi.hpp expression, with temporaries for aliasing.
    This is source dataflow analysis, not EE execution/equivalence.
    """
    require(words == PACKET_WORDS, 'packet_original_instruction_mismatch')
    regs = [[None]*4 for _ in range(32)]; regs[0] = [0]*4; stores = []
    for w in words:
        op, rs, rt, rd = w >> 26, w >> 21 & 31, w >> 16 & 31, w >> 11 & 31
        if op == 15:
            v = (w & 65535) << 16
            regs[rt][:2] = [v, 0xffffffff if v & 0x80000000 else 0]
        elif op in (0x37, 0x23): regs[rt][:2] = [None, None]
        elif op == 0x1e: regs[rt] = [None]*4
        elif op == 28 and w & 0x7ff == 0x4c8:  # PPACW rd,rs,rt
            s, t = regs[rs][:], regs[rt][:]
            regs[rd] = [t[0], t[2], s[0], s[2]]
        elif op == 28 and w & 0x7ff == 0x389:  # PCPYLD rd,rs,rt
            s, t = regs[rs][:], regs[rt][:]
            regs[rd] = t[:2] + s[:2]
        elif op == 0x1f:
            stores.append({'base_register': rs, 'offset': w & 65535, 'words': regs[rt][:]})
        elif op == 9: regs[rt][:2] = [None, None]
        else: raise RuntimeError('unsupported_selected_packet_operation')
    return stores


def controls():
    checks = []
    for name, changed in [('changed_channel_delay', 0x24040000), ('unknown_channel_delay', 0x00a0202d)]:
        value, _ = argument({0x1000: 0x0c1fecfa, 0x1004: changed}, 0x1000)
        require(value == (0 if name == 'changed_channel_delay' else None), 'argument_control_failed')
        checks.append({'name': name, 'status': 'PASS', 'actual_A0': value})
    f = [0x27bdfff0, 0xffb00000, 0xffbf0008, 0x0c04091e, 0]
    s = [0x27bdfff0, 0xffb00000, 0xffbf0008, 0x0c04091e, 0x00a0802d]
    bad = f[:]; bad[-1] = 0x24040000
    try: verify_forwarding(bad, s)
    except ValueError as e:
        require(str(e) == 'first_wrapper_A0_forwarding_unproved', 'wrong_control_refusal')
        checks.append({'name': 'forged_forwarding_delay', 'status': 'PASS', 'refusal': str(e)})
    else: raise RuntimeError('forged_forwarding_admitted')
    wrong = PACKET_WORDS[:]; wrong[11] ^= 0x10000
    try: packet_lanes(wrong)
    except ValueError as e:
        require(str(e) == 'packet_original_instruction_mismatch', 'wrong_packet_control_refusal')
        checks.append({'name': 'wrong_packet_register_word', 'status': 'PASS', 'refusal': str(e)})
    else: raise RuntimeError('wrong_packet_word_admitted')
    expected = [None, None, 0x14000000, 0x11000000]
    require(packet_lanes(PACKET_WORDS)[1]['words'] == expected and expected != [0x14000000, 0x11000000, None, None], 'packet_lane_order_control_failed')
    checks.append({'name': 'packed_command_lane_order_not_low_pair', 'status': 'PASS'})
    require(other_overlay_local('NETR',{0x7fb418:0x27bdffa0},0x7fb418) and
            not other_overlay_local('MOVIER',{0x7fb418:0x0200202d},0x7fb418), 'overlay_namespace_control_failed')
    checks.append({'name':'same_numeric_VA_does_not_merge_overlay_owners','status':'PASS'})
    return checks


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--audit-report', type=Path, required=True)
    p.add_argument('--expected-audit-sha', required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args(); args.audit_report = args.audit_report.resolve(); args.output = args.output.resolve()
    require(audit.sha(Path(audit.__file__)) == AUDIT_SOURCE, 'frozen_audit_dependency_changed')
    require(audit.sha(args.audit_report) == args.expected_audit_sha, 'audit_receipt_pin_mismatch')
    require(args.output.is_relative_to(audit.W/'build/oracle-telemetry') and not args.output.exists(), 'fresh_owned_output_required')
    source = Path(__file__).resolve()
    pins = [{'path': str(source), 'sha256': audit.sha(source)}, {'path': str(args.audit_report), 'sha256': audit.sha(args.audit_report)}]
    proof = json.loads(args.audit_report.read_text())
    require(proof['status'] == 'BOUNDED_ORIGINAL_VU0_ROUTE_AUDIT_COMPLETE' and not proof['input_changes'], 'complete_audit_required')
    for pin in proof['source_pins']:
        require(audit.sha(Path(pin['path'])) == pin['sha256'], 'frozen_audit_input_changed:' + pin['path'])
    pins += proof['source_pins']
    generation = next(Path(x['path']) for x in pins if Path(x['path']).parent.name == 'r192-remaining-disc-images-native-01' and Path(x['path']).name == 'RESULT.json')
    gen = json.loads(generation.read_text())
    paths = {'SLUS': next(Path(x['path']) for x in pins if Path(x['path']).name == 'SLUS_214.10'),
             **{k.split('.')[0]: Path(v['copy']) for k,v in gen['original_copies'].items()}}
    calls, refs, retained, coincidences = [], [], {}, []
    for name, row in proof['images'].items():
        image = audit.Original(paths[name]); mask_path = args.audit_report.parent/(name+'.code-mask.bin')
        require(audit.sha(mask_path) == row['mask_sha256'], 'decoder_mask_changed')
        pins.append({'path': str(mask_path), 'sha256': audit.sha(mask_path)})
        base = min(x['base'] for x in row['generated_units']); mask = mask_path.read_bytes()
        words = {base+4*i: image.word(base+4*i) for i,b in enumerate(mask) if b}
        require(len(words) == row['reconstructed_code_words'], 'decoder_word_population_changed')
        for pc,w in words.items():
            if w >> 26 not in (2,3): continue
            target = ((pc+4)&0xf0000000) | ((w&0x3ffffff)<<2)
            owner = next((start for start,end in WRAPPERS.items() if start <= target < end), None)
            if owner is None: continue
            value, context = argument(words,pc)
            if other_overlay_local(name,words,target):
                other = audit.Original(paths['MOVIER'])
                coincidences.append({'image':name,'pc':f'{pc:08X}','word':f'{w:08X}','target':f'{target:08X}',
                    'own_image_word':f'{words[target]:08X}','MOVIER_word_same_numeric_VA':f'{other.word(target):08X}',
                    'finding':'Target belongs to this image code mask. Equal numeric VA is not a cross-overlay MOVIER call.'})
                continue
            calls.append({'image': name, 'pc': f'{pc:08X}', 'file_offset': image.offset(pc), 'word': f'{w:08X}',
                          'target': f'{target:08X}', 'wrapper': f'{owner:08X}', 'interior_entry': target != owner,
                          'kind': 'JAL' if w>>26==3 else 'J_tail', 'A0_low64': value,
                          'original_context': context, 'delay_word': f'{words[pc+4]:08X}'})
        for offset in range(0,len(image.raw)-3,4):
            value = struct.unpack_from('<I',image.raw,offset)[0]
            if value in WRAPPERS: refs.append({'image':name,'file_offset':offset,'addresses':image.addresses(offset),'value':f'{value:08X}'})
        if name in ('SLUS','MOVIER'): retained[name] = (image,words)
    actual = {int(x['pc'],16):x['A0_low64'] for x in calls if x['image']=='MOVIER' and x['target']=='007FB3E8'}
    require(actual == EXPECTED_CALLERS, 'direct_MOVIER_wrapper_call_domain_changed')
    movier, words = retained['MOVIER']; first = [movier.word(0x7fb3e8+4*i) for i in range(5)]; second = [movier.word(0x7fb430+4*i) for i in range(5)]
    verify_forwarding(first,second)
    forwarding = [joined(movier,0x7fb3e8,first),joined(movier,0x7fb430,second)]
    main_image, main_words = retained['SLUS']; require(all(0x2e1e10+4*i in main_words for i in range(len(PACKET_WORDS))), 'packet_span_not_in_original_decoder_paths')
    packet = joined(main_image,0x2e1e10,PACKET_WORDS); stores = packet_lanes(PACKET_WORDS)
    require(stores[1]['words'] == [None,None,0x14000000,0x11000000], 'selected_MSCAL_FLUSH_lanes_unproved')
    normal = audit.W/'build/r192-compiled-konquest/src'; mmi = normal/'runtime/include/ps2/mmi.hpp'; vif = normal/'runtime/src/vif.cpp'
    mmi_text = mmi.read_text(); vif_text = vif.read_text()
    require('PS2_LANES(ppacw, w, 4, i < 2 ? t.w[i * 2] : s.w[(i - 2) * 2])' in mmi_text and 'PS2_LANES(pcpyld, d, 2, i ? s.d[0] : t.d[0])' in mmi_text, 'packed_lane_source_changed')
    require('case 0x14: case 0x15: run_vu(unit, imm * 8u); break;' in vif_text, 'VIF_MSCAL_source_changed')
    pins += [{'path':str(x),'sha256':audit.sha(x)} for x in (mmi,vif)]
    fragments = []
    for name,filename,label,length in [('MOVIER','m_MOVIER_unit_000D.cpp','L_007FB3E8:',4400),('SLUS','unit_0078.cpp','L_002E1E10:',1500)]:
        unit = next(x for x in proof['images'][name]['generated_units'] if Path(x['path']).name == filename)
        text = Path(unit['path']).read_text(); at = text.index(label); fragment = text[at:at+length]
        fragments.append({'source':unit,'label':label,'bounded_excerpt':fragment,'excerpt_sha256':hashlib.sha256(fragment.encode()).hexdigest()})
    checks=controls(); changed=[x['path'] for x in pins if audit.sha(Path(x['path']))!=x['sha256']]
    report={'status':'BOUNDED_MOVIER_CALLER_AND_PACKET_PROOF_PASS' if not changed else 'INPUT_CHANGED','source_pins':pins,'input_changes':changed,
            'classification':'source-only capability evidence; zero native credit','native_credit':0,'direct_calls':calls,'aligned_raw_wrapper_function_values':refs,
            'other_overlay_numeric_address_coincidences':coincidences,
            'wrapper_forwarding_original_words':forwarding,'selected_packet_original_words':packet,'selected_packet_stores':stores,'generated_fragments':fragments,
            'findings':['All six represented direct calls into007FB3E8 use channel8/9; its A0 reaches original getter00102478 unchanged, including delay.',
                        '007FB430 also forwards A0 unchanged, but has no represented direct caller or aligned raw function value in the five-image domain. Computed/indirect/interior entries remain OPEN.',
                        '002E1E40 stores [unbound,unbound,14000000,11000000] at cursor+16: source-backed MSCAL0 and FLUSH command data. Cursor is loaded from GP-23380; VIF unit/destination bus and subsequent submission remain OPEN.'],
            'controls':checks,'limits':['No runtime/EE/VU execution, no compiler or protected writes.',
                'Decoder masks retain their bounded source-discovery scope; no whole-game absence or unique indirect-call resolution.',
                'Original initial data/command lanes do not prove runtime immutability, packet framing, consumer path or microprogram ownership.',
                'The unchanged audit04 reset/FIFO packet proof remains separate; the MSCAL packet is not shown to reach VIF0.',
                'Unknown stores/indirect routes/dynamic producers and MPG/MSCNT routes outside the selected span remain OPEN.']}
    audit.reserve(1<<20); args.output.mkdir(parents=True); (args.output/'source.py').write_bytes(source.read_bytes())
    target=args.output/'RESULT.json'; target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'path':str(target),'sha256':audit.sha(target),'source_sha256':audit.sha(source),'status':report['status'],'direct_call_count':len(calls)}))
    return 0 if not changed else 1


if __name__=='__main__': raise SystemExit(main())
