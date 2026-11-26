"""Authenticate the original resource schema and execute its compiled emitters.

This offline proof does not expand the admitted semantic backend. Original
format classification, emitted transfer words, producer bounds and remaining
semantic obligations are separate fields. No emitted instruction is executed
by a decoder here and no executable/capture input is changed.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as original

BUILDER_RECEIPT = 'ffa154fa526bd0af42c3404f9454ee45a28a196b9936f82281fc61fa062a603c'
TABLES = [
    (0x5ca9d0, [(0x300, 1, 1), (0x303, 4, 1), (0x304, 8, 1),
                 (0x301, 2, 3), (0x302, 0x200, 4), (0x305, 0x10, 3),
                 (0x306, 0x20, 3), (0x307, 0x40, 4),
                 (0x308, 0x80, 4), (0x309, 0x100, 5)], 'mask'),
    (0x5caa60, [(0x100, 2, 1), (0x104, 0x100, 1), (0x101, 0x10, 2),
                 (0x102, 0x20, 3), (0x103, 0x40, 3), (0x165, 0x200, 1)], 'mask'),
    (0x5caab0, [(0x400, 0, 1), (0x402, 0, 3), (0x403, 0, 3)], 'class'),
]
RANGES = [(0x2eae10, 0x2eaf20), (0x2ed300, 0x2ee2e0),
          (0x2ef700, 0x2eff20), (0x2f0000, 0x2f0500),
          (0x2e7960, 0x2e79d0), (0x2e7c20, 0x2e7cd0)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['elf', 'listing', 'builder-proof', 'boundary',
                 'candidate-build', 'compiler', 'output']:
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    original.need(out.is_relative_to((original.W / 'build/oracle-telemetry').resolve())
                  and not out.exists(), 'fresh_owned_output_required')
    original.need(original.pin(a.elf)['sha256'] == original.ELF_SHA,
                  'retail_ELF_changed')
    original.need(original.pin(a.listing)['sha256'] == original.LISTING_SHA,
                  'original_listing_changed')
    receipt = a.builder_proof / 'RESULT.json'
    original.need(original.pin(receipt)['sha256'] == BUILDER_RECEIPT,
                  'original_builder_receipt_changed')
    prior = json.loads(receipt.read_text())
    fixture = a.builder_proof / 'original_builder.cpp'
    pinned = next(x for x in prior['source_pins'] if Path(x['path']).resolve() == fixture.resolve())
    original.need(original.pin(fixture) == pinned, 'original_compiled_builder_fixture_changed')
    prefix = fixture.read_text().split('static void read(', 1)[0]
    original.need('static void reference(' in prefix and 'L_002EFDA0:' in prefix,
                  'complete_original_helper_body_missing')
    elf = original.Original(a.elf.read_bytes())
    tables, tags = [], []
    for address, expected, sentinel in TABLES:
        rows = []
        for i, row in enumerate(expected):
            actual = [elf.word(address + 12*i + j*4) for j in range(3)]
            original.need(actual == list(row), 'resource_format_table_changed')
            rows.append(dict(address=f'{address + i*12:08X}', tag=f'{row[0]:08X}',
                             mask=f'{row[1]:08X}', resource_class=row[2]))
            tags.append((row[0], row[2]))
        end = address + 12*len(expected)
        # The first two scans stop on mask==0; the third stops on class==0.
        original.need(elf.word(end + (4 if sentinel == 'mask' else 8)) == 0,
                      'resource_format_sentinel_changed')
        tables.append(dict(address=f'{address:08X}', rows=rows,
                           sentinel_field=sentinel, sentinel_address=f'{end:08X}',
                           table_sha256=hashlib.sha256(elf.bytes(address, 12*len(expected))).hexdigest()))
    tags.append((0x201, 1))
    joins = []
    with a.listing.open(encoding='utf8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            pc = int(row['address'], 16)
            if not any(lo <= pc < hi for lo, hi in RANGES):
                continue
            word = int.from_bytes(bytes.fromhex(row['bytes']), 'little')
            original.need(elf.word(pc) == word, 'source_listing_ELF_word_mismatch')
            joins.append(dict(pc=f'{pc:08X}', word=f'{word:08X}', mnemonic=row['mnemonic']))
    original.need(joins, 'original_word_joins_missing')
    tag_rows = ','.join('{%du,%du}' % pair for pair in tags)
    test = TEST.replace('@TAGS@', tag_rows).replace('@BOUNDARY@', a.boundary.resolve().as_posix())
    out.mkdir(parents=True)
    cpp = out / 'original_schema_helpers.cpp'
    cpp.write_text(prefix + test, encoding='utf8', newline='\n')
    binary = out / 'original_schema_helpers.exe'
    inputs = [Path(__file__), Path(original.__file__), a.elf, a.listing, receipt,
              fixture, a.compiler, *(a.boundary / n for n in ['Cpu.bin', 'RAM.bin', 'SPR.bin']),
              *(a.candidate_build / 'src/runtime/include/ps2').glob('*.hpp')]
    pins = [original.pin(x) for x in inputs]
    argv = [str(a.compiler), '-std=c++20', '-mavx2', '-ffp-contract=off', '-O1',
            '-static-libgcc', '-static-libstdc++',
            '-I' + str((a.candidate_build / 'src/runtime/include').resolve()),
            str(cpp), '-o', str(binary)]
    compiled = subprocess.run(argv, capture_output=True)
    (out / 'compile.log').write_bytes(compiled.stdout + compiled.stderr)
    original.need(compiled.returncode == 0, 'original_schema_fixture_compile_failed')
    runs = []
    for name, flag, expected in [('positive', '', 0),
                                 ('wrong-format', '--planted-format', 1),
                                 ('wrong-transfer-word', '--planted-transfer-word', 1),
                                 ('wrong-unaligned-pair', '--planted-unaligned-pair', 1),
                                 ('wrong-offset-sign', '--planted-offset-sign', 1)]:
        command = [str(binary)] + ([flag] if flag else [])
        run = subprocess.run(command, capture_output=True)
        (out / (name + '.log')).write_bytes(run.stdout + run.stderr)
        runs.append(dict(name=name, argv=command, exit=run.returncode,
                         expected_exit=expected, stdout=run.stdout.decode(errors='replace'),
                         stderr=run.stderr.decode(errors='replace')))
    changed = [x['path'] for x in pins if original.pin(Path(x['path'])) != x]
    passed = not changed and all(x['exit'] == x['expected_exit'] for x in runs)
    result = dict(schema='r192_original_effect_resource_schema_v1',
        status='PASS_BOUNDED_ORIGINAL_SCHEMA_PROOF' if passed else 'FAIL',
        source_pins=pins, compile_argv=argv, artifacts=[original.pin(cpp), original.pin(binary)],
        input_changes=changed, runs=runs, resource_tables=tables,
        fallback=dict(tag='00000201', resource_class=1),
        resource_classes=[
            dict(classes=[1, 2], bank=0, value='four_32_bit_VF_lanes',
                 load='LQC2', store='SQC2', bits=128),
            dict(classes=[3], bank=1, value='FPR_bits', load='LWC1', store='SWC1', bits=32),
            dict(classes=[4], bank=2, value='GPR_low32_with_original_LW_extension',
                 load='LW', store='SW', bits=32),
            dict(classes=[5], bank=3, value='GPR_low64',
                 load='LD_or_LDL_then_LDR', store='SD_or_SDL_then_SDR', bits=64)],
        original_word_joins=joins,
        callback_discriminators=[
            dict(ids=[1], callback='002EE290', selection='destination node bank must match bank1 step for ADD helper; callback itself returns1 even when helper emits nothing'),
            dict(ids=[25, 2, 3], callback='002EE160', selection='source tag class1 vector or class3 scalar; other classes return0; destination helper must have compatible bank'),
            dict(ids=[26, 5, 6], callback='002EDFA0', selection='destination tag class1 vector or class3 scalar; other classes return0'),
            dict(ids=[17], callback='002EDC50', selection='destination tag exactly257, source node exists, only one remap context; post-filter block and source alias semantics still require composition proof'),
            dict(ids=[22, 21, 24], callback='002EE2D0', selection='return1, no emitted words; independent non-emitted route is not equivalent by inference')],
        filter=dict(id=2, callback='002ED300', record_stride=24,
            local_guards=['record+16 must be0', 'selected source node bank must be1'],
            unknown_id='late lookup sets result0 and stops filter construction; initial lookup does not itself reject the whole descriptor',
            cardinality='UNPROVED: append2E7C20 has no local capacity guard; no universal count inferred'),
        source_local_bounds=dict(effect_append='signed_count<13; negative count not locally refused',
            attribute_list='FFFFFFFF sentinel; no local list length or stream-index bound at2ED6B8',
            register_allocator='32 candidate registers per bank; GPR low32/64 share allocation map; node pool/capacity guards separate',
            emitted_memory_offsets='original helper emits low16 then EE instruction sign-extends; 64-bit unaligned pair uses low16(offset),low16(offset+7)',
            emitted_alignment='VF/GPR128 transfers align down to16 in original execution; 64-bit helper chooses pair from source offset&7',
            branch_fixups='signed displacement magnitude<32768 checked by2EE5F0; not proof all producer compositions fit',
            output='capacity and code/data block layout checked by original helpers, not an arbitrary sequence-length theorem'),
        unresolved=['All asset/producer invariants for attribute list, stream index and signed counts.',
            'All logical aliases and source/destination compatible-bank combinations including silent helper no-ops.',
            'All valid effect sequences and remap late-source semantics; finite ID table alone is insufficient.',
            'Multiple-filter producer bounds and failed/unknown-filter behavior across actual caller.',
            'Per-configuration register allocation, scratch/VU/FCR effects, budgets, yields and continuation projection.',
            'No widening of semantic core/adapter, candidate integration, ownership or native credit.'])
    target = out / 'RESULT.json'
    target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8', newline='\n')
    print(json.dumps(dict(status=result['status'], result=str(target), sha256=original.pin(target)['sha256'])))
    return 0 if passed else 1


TEST = r'''
static void load_boundary(Cpu&c){
 auto read=[](const char*n,void*p,unsigned size){std::ifstream f(std::string("@BOUNDARY@/")+n,std::ios::binary);f.read(static_cast<char*>(p),size);if(!f||f.peek()!=EOF)throw std::runtime_error("boundary size mismatch");};
 read("Cpu.bin",&c,sizeof c);read("RAM.bin",g_ram,kRamSize);read("SPR.bin",g_spr,kScratchpadSize);
}
int main(int argc,char**argv){try{
 const std::string mode=argc>1?argv[1]:"";Cpu initial{};load_boundary(initial);const std::vector<unsigned char> captured(g_ram,g_ram+kRamSize);
 auto invoke=[&](Cpu&c,unsigned entry){c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=1000000;reference(c,entry);if(c.pc!=0x100000||c.r[29].d[0]!=0x1fff000)throw std::runtime_error("helper continuation changed");};
 struct Tag{unsigned tag,format;};constexpr Tag tags[]={@TAGS@};unsigned classes=0,transfers=0;
 if(mode=="--planted-format")st32(0x5caa44,4); // private copy of tag309's class5 table word
 for(const auto&row:tags){Cpu c=initial;c.r[4].d[0]=row.tag;invoke(c,0x2eae10);const unsigned expected=row.format;if(c.r[2].w[0]!=expected){std::fprintf(stderr,"FORMAT_MISMATCH tag=%X actual=%u expected=%u\n",row.tag,c.r[2].w[0],expected);return 1;}++classes;}
 {Cpu c=initial;c.r[4].d[0]=0x7fffffff;invoke(c,0x2eae10);if(c.r[2].w[0])return 1;}
 constexpr unsigned node=0x1b00000,stream=0x1b00040,input=0x1b00080,output=0x1b000a0,block=0x1b00100,words=0x1b00200;
 const unsigned context=ld32(initial.r[28].w[0]-21920);if(context>kRamSize-3800)throw std::runtime_error("captured context bounds");
 for(unsigned bank=0;bank<4;++bank)for(unsigned offset:{0u,1u,7u,8u,15u,16u,0x7ff8u,0x8000u,0xffffu,0x10000u,0xffffffffu})for(unsigned reg:{0u,17u,31u})for(bool store:{false,true}){
  std::memset(g_ram+node,0,0x300);st32(context+3736,block);st32(block,32);st32(block+24,words);
  st32(node+4,reg);st32(node+8,bank);st32(node+12,stream);st32(node+16,offset);
  st32(stream+4,input);st32(stream+8,output);st32(input+4,4);st32(output+4,6);
  Cpu c=initial;c.r[4].d[0]=node;invoke(c,store?0x2ef700:0x2ef990);
  const unsigned base=store?6:4;std::vector<unsigned> expected;
  auto word=[&](unsigned op,unsigned off){return op|(base<<21)|(reg<<16)|(off&65535);};
  if(bank<3){constexpr unsigned loads[]={0xd8000000,0xc4000000,0x8c000000},stores[]={0xf8000000,0xe4000000,0xac000000};expected.push_back(word(store?stores[bank]:loads[bank],offset));}
  else if(!(offset&7))expected.push_back(word(store?0xfc000000:0xdc000000,offset));
  else{expected.push_back(word(store?0xb4000000:0x6c000000,offset));expected.push_back(word(store?0xb0000000:0x68000000,offset+7));}
  if(mode=="--planted-transfer-word"&&bank==2&&store)st32(words,ld32(words)^0x4000000);
  if(mode=="--planted-unaligned-pair"&&bank==3&&(offset&7))st32(words+4,ld32(words+4)^1);
  if(mode=="--planted-offset-sign"&&offset==0x8000)st32(words,ld32(words)&~0x8000u);
  if(ld32(block+8)!=expected.size()){std::fprintf(stderr,"TRANSFER_COUNT_MISMATCH bank=%u offset=%X\n",bank,offset);return 1;}
  for(unsigned i=0;i<expected.size();++i)if(ld32(words+i*4)!=expected[i]){std::fprintf(stderr,"TRANSFER_WORD_MISMATCH bank=%u store=%u offset=%X reg=%u actual=%08X expected=%08X\n",bank,store,offset,reg,ld32(words+i*4),expected[i]);return 1;}
  ++transfers;
 }
 unsigned whole_builders=0;
 for(const auto&row:tags)for(unsigned offset:{0u,1u}){
  std::memcpy(g_ram,captured.data(),kRamSize);Cpu c=initial;const unsigned object=initial.r[19].w[0];
  constexpr unsigned list=0x1c30000,code=0x1c00000,arena=0x1c10000,pool=0x1c20000;
  st32(object+192,0);st32(object+80,0);st32(list,row.tag);st32(list+4,0);st32(list+8,offset);st32(list+12,0xffffffff);
  c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=code;c.r[7].d[0]=2000;
  c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=pool;c.r[11].d[0]=400;invoke(c,0x2ed4b0);
  const unsigned n=c.r[2].w[0];if(!n||n>2000)throw std::runtime_error("single resource original descriptor refused");
  unsigned load_op=0,store_op=0;
  if(row.format<=2){load_op=0x36;store_op=0x3e;}
  else if(row.format==3){load_op=0x31;store_op=0x39;}
  else if(row.format==4){load_op=0x23;store_op=0x2b;}
  else{load_op=offset?0x1b:0x37;store_op=offset?0x2d:0x3f;}
  unsigned loads=0,stores=0;
  for(unsigned i=0;i<n;++i){const unsigned op=ld32(code+i*4)>>26;loads+=op==load_op;stores+=op==store_op;}
  if(!loads||!stores){std::fprintf(stderr,"CLASS_TO_BANK_TRANSFER_MISMATCH tag=%X class=%u offset=%u words=%u\n",row.tag,row.format,offset,n);return 1;}
  std::printf("original_resource_descriptor tag=%X class=%u offset=%u words=%u load_opcode=%X store_opcode=%X\n",row.tag,row.format,offset,n,load_op,store_op);++whole_builders;
 }
 // Deliberately bounded constructed filter records. Four records end at180,
 // before the effect-count field at192. This physical fact is NOT a proved
 // invariant of every original producer or a new backend count admission.
 unsigned filter_cases=0;
 for(unsigned filters:{0u,1u,2u,3u,4u})for(unsigned variant:{0u,1u,2u}){
  std::memcpy(g_ram,captured.data(),kRamSize);Cpu c=initial;const unsigned object=initial.r[19].w[0];
  constexpr unsigned list=0x1c30000,code=0x1c00000,arena=0x1c10000,pool=0x1c20000;
  const unsigned owner=ld32(object+72),count=ld32(owner+536),attributes=ld32(owner+540);
  if(count>32||attributes>kRamSize-count*12)throw std::runtime_error("captured attributes changed");
  std::memcpy(g_ram+list,g_ram+attributes,count*12);st32(list+count*12,0xffffffff);
  st32(object+192,0);st32(object+80,filters);
  for(unsigned i=0;i<filters;++i){const unsigned row=object+84+i*24;std::memset(g_ram+row,0,24);st16(row,2);st32(row+4,769);st32(row+20,0x3f800000+i*0x100000);}
  if(filters&&variant==1)st16(object+84,0x7fff);
  if(filters&&variant==2)st32(object+84+16,1);
  c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=code;c.r[7].d[0]=2000;
  c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=pool;c.r[11].d[0]=400;invoke(c,0x2ed4b0);
  const unsigned n=c.r[2].w[0];if(!n||n>2000)throw std::runtime_error("filter fixture whole builder refused");
  unsigned comparisons=0;for(unsigned i=0;i<n;++i)comparisons+=(ld32(code+i*4)&0xffe007ffu)==0x46000036u;
  const unsigned expected=variant==0?filters:0;
  if(comparisons!=expected){std::fprintf(stderr,"FILTER_STOP_SEMANTICS_MISMATCH count=%u variant=%u actual_comparisons=%u expected=%u\n",filters,variant,comparisons,expected);return 1;}
  std::printf("original_filter_descriptor count=%u variant=%u builder_words=%u emitted_scalar_comparisons=%u whole_builder_returned_nonzero=1\n",filters,variant,n,comparisons);++filter_cases;
 }
 std::printf("PASS original_tag_classifications=%u unknown_tag_zero_classifications=1 original_compiled_transfer_cases=%u original_whole_resource_descriptors=%u constructed_filter_descriptors=%u runtime_instruction_execution=0 backend_admission_changes=0\n",classes,transfers,whole_builders,filter_cases);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''


if __name__ == '__main__':
    raise SystemExit(main())
