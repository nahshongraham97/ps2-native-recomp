"""Bounded compiler-identification experiment; no ownership or native credit.

Freeze the sample before compiler execution. Drafts, originals and proprietary
compiler binaries belong only in ignored/private tooling outputs.
"""
from pathlib import Path
import argparse
import collections
import csv
import hashlib
import json
import shutil
import struct

ROOT = Path(__file__).resolve().parents[2]
SEED = 'MKAR:compiler-identification:fixed40:20261126'
KIT = '3ae9a231d887f187617e25756c7fa866703fa5a1'
CAMERAS = ('001a6410','001a64a0','001a6510','001a6540','001a65a0','001a66a0',
           '001a68f0','001a6a10','001a6a20','001a6b30','001a6be0','001a6c30',
           '001a6d70','001a6dd0','001a6f40','001a6fd0','001a70f0','001a7510')
MANDATORY = ('001c4210','00167cf0','0016ae10','002e1710',*CAMERAS,'001637d0')

def sha(data):
    return hashlib.sha256(data).hexdigest()

def select(parent, output, extended=False):
    from audit_ps2_rtti import segments, ELF_SHA
    if output.exists():
        raise ValueError('fixed sample already exists; never replace it')
    if shutil.disk_usage(ROOT).free < 12*(1<<30):
        raise ValueError('12GiB reserve')
    original = parent/'research/inputs/SLUS_214.10'
    image = original.read_bytes()
    if sha(image) != ELF_SHA:
        raise ValueError('retail ELF identity')
    loads = segments(image)
    def read(address,size):
        for segment in loads:
            delta=address-segment['vaddr']
            if 0 <= delta and delta+size <= segment['filesz']:
                return image[segment['offset']+delta:segment['offset']+delta+size]
        raise ValueError('file-backed original range')
    manifest=json.loads((ROOT/'research/runs/renderer-scope.json').read_bytes())
    listing=parent/manifest['bounded_inputs']['ghidra_reproduction']['private_listing']['path']
    ranges=collections.defaultdict(list)
    with (listing.parent/'function-bodies.tsv').open() as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            ranges[row['entry'].lower()].append((int(row['start'],16),int(row['end'],16)+1))
    counts=collections.Counter()
    same=set()
    with (ROOT/'research/runs/phase7_executed_base_entries.csv').open() as stream:
        for row in csv.DictReader(stream):
            key=row['module']+':'+row['address']
            counts[key]+=int(row['wrapper_entries'])
            if row['parity']=='SAME_TO_END':
                same.add(key)
    base={}
    for address,spans in ranges.items():
        raw=b''.join(read(low,high-low) for low,high in spans)
        key='SLUS_214.10:'+address
        base[address]=dict(module='SLUS_214.10',address=address,
            ranges=[[f'{low:08x}',f'{high:08x}'] for low,high in spans],
            instructions=len(raw)//4, original_body_sha256=sha(raw),
            observed_wrapper_entries=counts[key], observed_in_matched_replay=key in same,
            observation='observed' if counts[key] else 'not-observed; not a proof of never executed')
    rows=[dict(base[address],selection='mandatory TrackA/camera/input') for address in MANDATORY]
    overlay=parent/'research/inputs/modules/MOVIER.ovl'
    data=overlay.read_bytes()
    if data[:4]!=b'MWo3':
        raise ValueError('MWo3 identity')
    overlay_base=struct.unpack_from('<I',data,8)[0]
    overlay_pool=[]
    with (parent/'recomp/overlays/MOVIER-functions.csv').open() as stream:
        for row in csv.DictReader(stream):
            start,end=int(row['Start'],16),int(row['End'],16)
            size=int(row['Size'])
            if end-start!=size or start<overlay_base or end-overlay_base>len(data):
                raise ValueError('overlay catalogue range')
            if 25<=size//4<=120:
                overlay_pool.append(dict(module='MOVIER.ovl',address=f'{start:08x}',
                    ranges=[[f'{start:08x}',f'{end:08x}']],instructions=size//4,
                    original_body_sha256=sha(data[start-overlay_base:end-overlay_base]),
                    observed_wrapper_entries=None,observed_in_matched_replay=None,
                    observation='unmeasured: base counter corpus does not cover overlay DLL entries'))
    def rank(row):
        return sha((SEED+'\0'+row['module']+':'+row['address']).encode())
    rows.extend(dict(row,selection='seeded MOVIER25..120') for row in sorted(overlay_pool,key=rank)[:10])
    remaining=[row for address,row in base.items() if address not in MANDATORY and 25<=row['instructions']<=120]
    for observed,number in ((True,4),(False,3)):
        pool=[row for row in remaining if bool(row['observed_wrapper_entries'])==observed]
        rows.extend(dict(row,selection='seeded base25..120 '+('observed' if observed else 'not-observed'))
                    for row in sorted(pool,key=rank)[:number])
    assert len(rows)==40 and len({(r['module'],r['address']) for r in rows})==40
    if extended:
        selected={(row['module'],row['address']) for row in rows}
        for pool, number in ((overlay_pool,6),(remaining,5)):
            choices=[row for row in pool if (row['module'],row['address']) not in selected]
            rows.extend(dict(row,selection='pre-execution revised40nontrivial extension') for row in sorted(choices,key=rank)[:number])
        assert len(rows)==51 and sum(25<=row['instructions']<=120 for row in rows)==40
    threshold=sum(25<=row['instructions']<=120 for row in rows)
    result=dict(schema='mkar_fixed_compiler_feasibility_sample_v1',status='FROZEN_BEFORE_COMPILER_EXECUTION',
        toolkit_commit=KIT, seed=SEED, sample=rows, total=len(rows), threshold_denominator=threshold,
        selection_rule='23mandatory (4TrackA+18camera+001637d0),10SHA-rankedMOVIER25..120,7SHA-rankedbase25..120(4observed+3not-observed). No replacements after outcomes.',
        input_hashes={str(original.relative_to(parent)):sha(image),
                      str(overlay.relative_to(parent)):sha(data),
                      'function-bodies.tsv':sha((listing.parent/'function-bodies.tsv').read_bytes()),
                      'phase7_executed_base_entries.csv':sha((ROOT/'research/runs/phase7_executed_base_entries.csv').read_bytes())},
        thresholds=dict(go='>=50% of fixed25..120denominator under ONEcompiler/flagset',
            pilot='>=10% and <50%',no_go='<10% under tested configurations; not proof every matching technique impossible',
            near_match='at most4different decoded instructions, with byte identity/relocations reported separately'),
        limits=['Not-observed is not never-executed; internal edges/overlays/collisions limit the existing observer.',
                'Original PS2 code equivalence does not prove portable C defined behavior or64-bit native pointer/ABI correctness.',
                'No ownership, transformed or native implementation credit from this experiment.'])
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-root',type=Path,required=True)
    parser.add_argument('--freeze',type=Path)
    parser.add_argument('--pack',type=Path)
    parser.add_argument('--sample',type=Path)
    parser.add_argument('--extend',action='store_true')
    args=parser.parse_args()
    if args.pack:
        from audit_ps2_rtti import segments
        parent=args.parent_root.resolve()
        sample=json.loads(args.sample.read_bytes())
        source=parent/'research/inputs/SLUS_214.10'
        image=source.read_bytes()
        loads=segments(image)
        def read(va,size):
            for segment in loads:
                delta=va-segment['vaddr']
                if 0<=delta and delta+size<=segment['filesz']:
                    start=segment['offset']+delta
                    return image[start:start+size]
            raise ValueError('original file-backed range')
        overlay=(parent/'research/inputs/modules/MOVIER.ovl').read_bytes()
        base=struct.unpack_from('<I',overlay,8)[0]
        rows=[]
        for row in sample['sample']:
            spans=[(int(a,16),int(b,16)) for a,b in row['ranges']]
            raw=b''.join(read(a,b-a) if row['module']=='SLUS_214.10' else overlay[a-base:b-base] for a,b in spans)
            assert sha(raw)==row['original_body_sha256']
            rows.append(dict(row,raw_hex=raw.hex(),contiguous=len(spans)==1))
        args.pack.parent.mkdir(parents=True,exist_ok=True)
        assert not args.pack.exists()
        args.pack.write_text(json.dumps(dict(sample_sha256=sha(args.sample.read_bytes()),sample=rows,
            mapped_ranges=[[s['vaddr'],s['vaddr']+s['memsz']] for s in loads]+[[base,base+len(overlay)+struct.unpack_from('<I',overlay,20)[0]]],
            source_sha256=sha(image),main_gp=0x6540f0,
            main_gp_evidence='Original.reginfofileoffset54DFEC=f0406500;startup100198/1001AC/1001C0 independently agree. Overlay-specificGPunproved.',
            scope='Private original bytes; compiler experiment only'),indent=2)+'\n',encoding='utf8',newline='\n')
        print('PACKED_PRIVATE_ORIGINALS',len(rows))
        return
    assert args.freeze is not None
    result=select(args.parent_root.resolve(),args.freeze.resolve(),extended=args.extend)
    if args.extend:
        result['selection_rule'] += ' Latest owner revision before sample execution appends6MOVIER+5base25..120:51total,40nontrivial. Original40retained unchanged.'
        args.freeze.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(total=result['total'],threshold_denominator=result['threshold_denominator'],
        sample=[(row['module'],row['address'],row['instructions']) for row in result['sample']],
        sha256=sha(args.freeze.read_bytes()))))

if __name__=='__main__':
    main()
