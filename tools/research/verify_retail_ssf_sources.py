"""Join private Ghidra listing ranges and explicit gap decodes to pinned ELF bytes."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def gap_instruction(w, pc):
    op, rs, rt, rd, sa, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
    immediate = w & 65535
    signed = immediate if immediate < 32768 else immediate - 65536
    if w == 0:
        return "nop"
    if op == 0 and fn in (0x21, 0x2d):
        return f"{'addu' if fn == 0x21 else 'daddu'} r{rd},r{rs},r{rt}"
    if op == 0 and fn == 2 and rs == 0:
        return f"srl r{rd},r{rt},{sa}"
    if op in (9, 12, 0x23, 0x2b, 0x3f):
        return f"{dict([(9,'addiu'),(12,'andi'),(0x23,'lw'),(0x2b,'sw'),(0x3f,'sd')])[op]} r{rt},r{rs},{immediate if op == 12 else signed}"
    if op == 4:
        return f"beq r{rs},r{rt},0x{(pc+4+signed*4)&0xffffffff:08x} ; one ordinary delay slot"
    raise ValueError(f"Unproved omitted instruction at0x{pc:08x}:0x{w:08x}")


def verify(elf_path, expected_sha, listings):
    data = elf_path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_sha:
        raise ValueError("Pinned original ELF identity mismatch")
    if data[:6] != b'\x7fELF\x01\x01':
        raise ValueError("Expected original little-endian ELF32")
    phoff = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    if stride < 32 or phoff > len(data) or count > (len(data)-phoff)//stride:
        raise ValueError("Invalid ELF program-header table")
    segments = [struct.unpack_from('<8I', data, phoff+i*stride) for i in range(count)]
    rows = []
    for listing in sorted(listings):
        document = json.loads(listing.read_text(encoding='utf-8-sig'))
        info, items = document['info'], document['items']
        start, end = int(info['body_start'], 16), int(info['body_end'], 16)+1
        if end <= start or (end-start) % 4 or len(items)*4 != info['body_size']:
            raise ValueError("Invalid listing/body byte count")
        owners = [s for s in segments if s[0] == 1 and s[2] <= start and end-s[2] <= s[4]]
        if len(owners) != 1:
            raise ValueError("Function span lacks one file-backed PT_LOAD")
        segment = owners[0]
        offset = segment[1]+start-segment[2]
        if offset > len(data) or end-start > len(data)-offset:
            raise ValueError("Original span outside file")
        raw = data[offset:offset+end-start]
        listed = {}
        ranges = []
        for item in items:
            at = int(item['address'], 16)
            if at in listed or not start <= at < end or (at-start) % 4:
                raise ValueError("Duplicate/unmapped instruction address")
            word = bytes.fromhex(item['bytes'])
            if len(word) != 4 or word != raw[at-start:at-start+4]:
                raise ValueError("Ghidra word differs from original PT_LOAD bytes")
            listed[at] = item
            if ranges and ranges[-1]['end_exclusive'] == at:
                ranges[-1]['end_exclusive'] += 4
            else:
                ranges.append({'start': at, 'end_exclusive': at+4})
        gaps = []
        for at in range(start, end, 4):
            if at in listed:
                continue
            w = struct.unpack_from('<I', raw, at-start)[0]
            previous = struct.unpack_from('<I', raw, at-start-4)[0] if at > start else 0
            preceding_op = previous >> 26
            delay = preceding_op in (1,2,3,4,5,6,7,20,21,22,23) or (preceding_op == 0 and previous & 63 in (8,9))
            gaps.append({'address': f'{at:08x}', 'word': f'{w:08x}', 'instruction': gap_instruction(w, at),
                         'immediately_follows_control_transfer': delay,
                         'role': 'Explicit original code; never discarded as padding. Branch-likely delay slots may be annulled.'})
        rows.append({'address': info['address'], 'listed_body_bytes': info['body_size'],
                     'contiguous_span_bytes': end-start, 'elf_offset': offset,
                     'span_sha256': hashlib.sha256(raw).hexdigest(), 'listing_words': len(items),
                     'listing_sha256': hashlib.sha256(listing.read_bytes()).hexdigest(),
                     'listed_ranges': ranges, 'explicit_gap_words': gaps,
                     'status': 'ALL_SPAN_WORDS_JOINED_WITH_EXPLICIT_OMITTED_CODE'})
    return {'schema': 'mkar.ssf.full-helper-word-join.v1', 'elf_sha256': expected_sha,
            'function_count': len(rows), 'listed_words': sum(r['listing_words'] for r in rows),
            'decoded_gap_words': sum(len(r['explicit_gap_words']) for r in rows), 'functions': rows,
            'scope': 'Original bytes and decoded code ranges only; no behavioral, ownership, native ABI or credit admission.'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--elf', type=Path, required=True)
    ap.add_argument('--elf-sha256', required=True)
    ap.add_argument('--listings', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    if a.output.exists():
        raise ValueError("Preserve existing receipts; choose a new output")
    report = verify(a.elf, a.elf_sha256, a.listings.glob('*.full-listing.json'))
    if not report['function_count']:
        raise ValueError("No listing inputs")
    a.output.write_bytes((json.dumps(report, indent=2)+'\n').encode())
    print('PASS original SSF helper spans', report['function_count'], 'listed_words', report['listed_words'], 'explicit_gap_words', report['decoded_gap_words'])


if __name__ == '__main__':
    main()
