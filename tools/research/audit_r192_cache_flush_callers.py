"""Closed byte/path census of original EE cache-flush call candidates.

This source-only audit distinguishes original executable paths, unmapped/raw
matches, original function boundaries and generated source joins. Unknown
indirect calls and caller roles remain open; no cache operation is assumed
necessary or sufficient for every code generator.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict, deque
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
import audit_r192_vu0_routes as audit

W, need, sha = audit.W, audit.require, audit.sha
AUDIT_SHA = '923827f5054336f46e52e6eba2c309ee90bbd1a8037ce0322d7b4c95d700cb11'
AUDIT_SOURCE_SHA = 'fc654acb23866b558e459b816d5776ad0b5f5ee2a140c90c355f06e87ea1bc6e'
SDK_COMMIT = 'd41ea62946e53d9a0c8788bb9eb8c8faf488b559'
MASK64 = (1 << 64) - 1
FLUSH = {100: 'FlushCache', (-104) & MASK64: 'iFlushCache'}
KERNEL_COPY_GROUPS = [
    ('SLUS',(0x116340,0x116348),[(0x116338,0x80075000,0x5981e8,0x330)],'tlbfunc.c'),
    ('SLUS',(0x116e50,0x116e58),[(0x116e48,0x80074000,0x598858,0x7a8)],'libosd.c'),
    ('SLUS',(0x117230,0x117238),[(0x117210,0x80076000,0x599020,0x740),(0x117228,0x82000,0x599760,0x28)],'alarm.c'),
    ('NTGUI',(0x1d45d8,0x1d45e0),[(0x1d45d0,0x80074000,0x501068,0x7a8)],'libosd.c'),
    ('NTGUI',(0x1d49b8,0x1d49c0),[(0x1d4998,0x80076000,0x501830,0x740),(0x1d49b0,0x82000,0x501f70,0x28)],'alarm.c'),
    ('NTGUI',(0x1d67a8,0x1d67b0),[(0x1d67a0,0x80075000,0x502000,0x330)],'tlbfunc.c'),
]
# These classify the selected source effect, not every effect of an enclosing
# function. Unknown producers/aliases remain separately recorded. In particular
# a DMA payload is not presumed incapable of containing executable bytes.
BOUNDARY_ROLES = {
 'SLUS':[
  ((0x1001f0,), 'startup_data_synchronization', 'Original bootstrap clears/initializes memory, establishes the initial thread via syscalls60/61 and then writebacks before its main handoff.'),
  ((0x1097cc,), 'DMA_packet_submission', 'Original SD packet construction precedes flush; following SWs program DMA registers1000B430/1000B420/1000B400.'),
  ((0x10b550,), 'IPU_DMA_initialization', 'Original body programs10002010 and DMA registers1000B4xx/B0xx around the writeback; this is an IPU/DMA boundary, not a proved EE instruction producer.'),
  ((0x10e194,), 'RPC_control_structure_initialization', 'Writeback precedes fixed RPC control structures and thread creation; the selected writes are fields/headers, with RPC callback stored separately.'),
  ((0x10f8fc,), 'SIF_DMA_initialization', 'Original body initializes SIF/control records, synchronizes and programs DMA/SBUS registers; no EE instruction-packing operation is established here.'),
  ((0x11ca40,0x11d4dc,0x11d63c), 'RPC_packet_submission', 'Original body copies caller bytes into static command structures, writes lengths/command values, then calls001108B0 after writeback.'),
  ((0x1ad638,), 'graphics_command_buffer_submission', 'Original stores append1121/122 command words and cursor metadata, then writeback precedes buffer swap and0010D218 range synchronization. Callback1AD628 remains an independent unknown target.'),
  ((0x2948a4,), 'GIF_DMA_packet_submission', 'Original001015A8 builds the stack packet; writeback precedes001018D0 which writes1000A020/1000A010/1000A000 DMA registers.'),
  ((0x2d0d78,), 'RPC_packet_submission', 'Original stores A1 into request+16 in the flush delay, then invokes0010FCB0/0010FCF0 with command80000014 and polls its completion.'),
  ((0x316ac0,), 'RPC_packet_submission', 'Flush delay writes a terminating zero into the selected6KiB buffer; following001108B0 receives that packet and explicit lengths.'),
  ((0x329098,0x329124), 'SIF_DMA_packet_submission', 'Original builds a four-word stack source/destination/size/attribute descriptor; writeback precedes0010F708→sceSifSetDma.'),
  ((0x32a34c,), 'asynchronous_data_transfer_setup', 'The selected writeback is between0010D218 range synchronization and request/pointer setup followed by001108B0; downstream pointer payload ownership remains open.'),
  ((0x3bacb8,), 'streaming_RPC_packet_submission', 'Original queue removal/decode/copy calls precede writeback and002D0D38 request submission; dynamic payload semantics stay open.'),
 ],
 'NTGUI':[
  ((0x101c1c,), 'graphics_object_data_initialization', 'Original COP1 constants and stores initialize object fields640/644 and a GP reference before writeback; following display setup remains dataflow, not EE opcode packing.'),
  ((0x102594,), 'GIF_DMA_packet_submission', 'Original copies a fixed32-byte packet to stack and programs1000A020/1000A010/1000E010 before writeback, then starts DMA1000A000.'),
  ((0x10265c,0x102700), 'graphics_packet_submission', 'Original display/GS packet helper calls precede writeback and GIF packet submission. No generated EE callable is inferred from the payload.'),
  ((0x102e78,0x1032c4), 'RPC_DMA_control_packet', 'Original writes a four-word static control packet and flushes before the source sceSifSetDma wrapper001CAF60 or its command wrapper.'),
  ((0x105480,0x105504,0x10d6f0,0x10db20,0x10dca8,0x133ed0,0x16d66c,0x1a1dcc,0x1a1e3c,0x1a1f04,0x1a25ec),
    'graphics_data_or_packet_boundary', 'Original surrounding stores/calls prepare rendering or transfer data and writeback; packet consumers/indirect effects are not closed by this classification.'),
  ((0x105890,), 'startup_data_synchronization', 'Original bootstrap thread initialization uses syscalls60/61 before writeback and optional constructor calls.'),
  ((0x1559f0,), 'SIF_DMA_packet_submission', 'Original stack source/destination/length fields are finalized in the flush delay, followed by the SIF DMA submission helper.'),
  ((0x1a55f8,0x1cc31c), 'RPC_control_structure_initialization', 'Original writeback precedes source thread/control initialization; caller/data bytes remain separate from callable code.'),
  ((0x1c52b8,0x1c5d54,0x1c5eb8), 'RPC_packet_submission', 'Original command/string/argument buffers are copied and length fields set, then writeback precedes001CE8B0 RPC submission.'),
  ((0x1c9cf4,), 'RPC_client_initialization', 'Original001CE6D0 binding and server pointer wait precede zeroing500F28/500F2C and writeback; these are client/GP metadata, not emitted instructions.'),
  ((0x1cd8fc,), 'SIF_DMA_initialization', 'Original SIF/control-record pointer stores precede writeback, then SBUS/DMA register1000E010/1000C000 consumers follow.'),
 ]
}


def target(word, pc): return ((pc + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2)


def wrapper(words, entry):
    seq = [words.get(entry + 4*i) for i in range(4)]
    if any(w is None for w in seq): return None
    if seq[0] & 0xffff0000 != 0x24030000 or seq[1:] != [12, 0x03e00008, 0]: return None
    value = seq[0] & 65535
    if value & 32768: value -= 65536
    if value & MASK64 not in FLUSH: return None
    return {'entry': entry, 'syscall_pc': entry+4, 'syscall_number_signed': value,
            'name': FLUSH[value & MASK64], 'words': [f'{w:08X}' for w in seq]}


def arguments(words, pc, link=False):
    regs, used = audit.local_constants(words, pc, 256)
    before = list(regs)
    if link: regs[31] = pc+8
    delay = words.get(pc+4)
    if delay is None or audit.control(delay):
        return {'A0': None, 'status': 'OPEN_missing_or_control_delay', 'before_delay_A0': before[4], 'context': used}
    audit.update(regs, delay)
    return {'A0': regs[4], 'A0_A3': regs[4:8], 'before_delay_A0': before[4], 'status': 'PROVED_LOCAL_SUFFIX_CONSTANT' if regs[4] is not None else 'OPEN_input_or_predecessor',
            'context': used, 'delay_pc': pc+4, 'delay_word': f'{delay:08X}'}


def listing_info(listing, image, pc):
    found = listing.get(pc)
    if not found: return {'status': 'MISSING_GHIDRA_ROW'}
    need(bytes.fromhex(found['bytes']) == image.read(pc, 4), 'Ghidra_original_word_mismatch')
    return {'status': 'ORIGINAL_WORD_JOINED', **found}


def source_join(units, pc, call=False):
    rows = []
    label = f'L_{pc:08X}:'
    for path, text in units:
        at = text.find(label)
        method = 'exact_PC_label'
        if at < 0 and call:
            # The emitter may coalesce labels in straight-line spans. The
            # exact JAL return-PC is independent of any guessed function name.
            pattern = re.compile(r'c\.r\[31\]\.sd\[0\] = static_cast<std::int32_t>\(0x'+f'{pc+8:08X}'+r'u\);')
            m = pattern.search(text)
            if m: at = m.start(); method = 'actual_JAL_return_PC'
        if at < 0: continue
        end = text.find('\nL_', at+len(label)); end = min(end if end >= 0 else at+1800, at+2400)
        start = text.rfind('\n', 0, at)+1
        excerpt = text[start:end]
        rows.append({'path': str(path), 'sha256': sha(path), 'join': method, 'line': text.count('\n',0,start)+1,
                     'fragment': excerpt, 'fragment_sha256': hashlib.sha256(excerpt.encode()).hexdigest()})
    return {'status': 'JOINED' if len(rows)==1 else 'MISSING' if not rows else 'MULTIPLE', 'rows': rows}


def self_test():
    tests=[]
    good={0x1000:0x24030064,0x1004:12,0x1008:0x03e00008,0x100c:0}
    need(wrapper(good,0x1000)['name']=='FlushCache','wrapper_positive_control')
    tests.append('original_wrapper_shape_accepted')
    for name, at, value in [('wrong_syscall_number',0x1000,0x24030068),('non_syscall',0x1004,0),('missing_return',0x1008,0x00400008),('non_NOP_delay',0x100c,0x24040002)]:
        bad=dict(good);bad[at]=value;need(wrapper(bad,0x1000) is None,name+'_accepted');tests.append(name+'_rejected')
    negative=dict(good);negative[0x1000]=0x2403ff98
    need(wrapper(negative,0x1000)['name']=='iFlushCache','negative_iFlushCache_lost');tests.append('negative_interrupt_syscall_retained')
    words={0x2000:0x24040003,0x2004:0x0c000400,0x2008:0x24040002}
    need(arguments(words,0x2004,True)['A0']==2,'delay_argument_not_applied');tests.append('delay_overrides_pre_call_argument')
    unknown={0x2000:0x0080202d,0x2004:0x0c000400,0x2008:0}
    need(arguments(unknown,0x2004,True)['A0'] is None,'unknown_input_invented');tests.append('unknown_argument_not_zero')
    need(target(0x0c043298,0x1000)==0x10ca60,'JAL_target_decode');tests.append('JAL_target_word_join')
    return [{'name':x,'status':'PASS'} for x in tests]


def boundary_role(name,image,words,pc):
    for module,sites,copies,sdkfile in KERNEL_COPY_GROUPS:
        if module!=name or pc not in sites:continue
        proofs=[]
        for call,dest,source,size in copies:
            need(words[call]>>26==3,'kernel_copy_not_original_JAL');callee=target(words[call],call)
            need([image.word(callee+4*i) for i in range(4)]==[0x2403005a,12,0x03e00008,0],'kernel_Copy_syscall_unproved')
            args=arguments(words,call,True);expected=[dest,source,size]
            need([None if x is None else x&0xffffffff for x in args['A0_A3'][:3]]==expected,'kernel_copy_original_arguments_changed')
            proofs.append({'call_pc':call,'word':f'{words[call]:08X}','callee':callee,'Copy_syscall_number':90,'arguments':args,
                'destination':dest,'source':source,'bytes':size,'source_file_offset':image.offset(source),'source_sha256':hashlib.sha256(image.read(source,size)).hexdigest()})
        return {'classification':'other','role':'fixed_original_kernel_code_installation','proof':proofs,'official_SDK_source':'ee/kernel/src/'+sdkfile,
            'reason':'Original Copy syscall moves an already stored fixed payload into a fixed kernel address before FlushCache0/2 and syscall-table installation. This is code copying/installation, not fresh arithmetic opcode synthesis.',
            'scope':'Selected original fixed-copy/install chain. Kernel memory/ABI execution and runtime source immutability are not inferred.'}
    if name=='SLUS' and pc in (0x334374,0x33437c):
        for at,q in {0x33434c:0x0c0448ce,0x334350:0x0260282d,0x334370:0x8e700014,0x3343a8:0x0c0cd068,
                     0x3341d0:0x8e020000,0x3341d4:0x0040f809}.items():need(image.word(at)==q,'original_overlay_loader_chain_changed')
        return {'classification':'overlay_loader','role':'file_image_read_then_constructor_range',
            'reason':'Original open/seek/read/close sequence reads into S3; flush0/2 follows. Header+20 supplies zero-fill length; header+24/+28 supplies a range whose LW entry/JALR loop is003341A0. No fresh opcode packing is shown.',
            'proof_addresses':[0x334304,0x334320,0x334334,0x33434c,0x334360,0x334370,0x334374,0x33437c,0x334394,0x3343a8,0x3341d0,0x3341d4],
            'scope':'Original loaded-image/constructor consumer chain; file identity/format and arbitrary constructor targets remain independent.'}
    for sites,role,reason in BOUNDARY_ROLES.get(name,[]):
        if pc in sites:return {'classification':'other','role':role,'reason':reason,
            'scope':'Selected cache boundary/source effects, not a proof the complete enclosing function or every payload cannot generate/copy executable bytes.'}
    if name=='SLUS' and pc==0x22ae2c:
        return {'classification':'UNRESOLVED','role':'resource_object_pointer_fixup_then_writeback',
            'reason':'Original22AE18 loads object+4,22AE1C dereferences+4,22AE20 adds50hex and22AE24 replaces object+4 when source flag bit0 is set. Source001C15A0 and resource parsing precede this. This proves pointer/header fixup, but code-vs-data interpretation of that resource and execution consumers remains unproved.',
            'proof_addresses':[0x22adf0,0x22adf8,0x22ae00,0x22ae08,0x22ae10,0x22ae18,0x22ae1c,0x22ae20,0x22ae24,0x22ae2c],
            'scope':'No fresh opcode construction is established by these words; this is not a no-generator claim for the enclosing parser.'}
    if name=='SLUS' and pc==0x3da98c:
        return {'classification':'UNRESOLVED','role':'multi_allocation_global_initialization_then_writeback',
            'reason':'Original003DA7D0 initializes GP globals, invokes00100370 allocation-like helpers at3DA894/3DA8C8/3DA8E4, initializes007Bxxxx buffers/objects and flushes before00101198 and later003E4DF0. No local opcode packing is established, but transitive callees and buffer consumers are not closed.',
            'proof_addresses':[0x3da7e4,0x3da894,0x3da8c8,0x3da8e4,0x3da8f0,0x3da900,0x3da924,0x3da984,0x3da98c,0x3da99c,0x3da9cc],
            'scope':'Allocator/resource semantics and later executable/data consumers remain open; do not label this a proved non-generator.'}
    if name=='NETR' and pc in (0x7f0118,0x7f0120,0x829f88,0x829f90):
        return {'classification':'UNRESOLVED','role':'descriptor_dispatched_in_place_word_transform',
            'reason':'Original entry selects one of two directional descriptor-dispatch chains. A nibble chooses handlers; independently retained selected handlers LW existing words/XOR constants/SW back before0/2 flush. Exact descriptor domains, complete handler table bounds and whether transformed buffers execute are open.',
            'scope':'Word mutation is source-proved; generator vs relocated/copied/decoded code vs ordinary data is not resolved. See NETR_unresolved_word_transform_chains.'}
    if name=='NETR' and pc==0x82df90:
        return {'classification':'UNRESOLVED','role':'decoder_path_without_independent_instruction_owner',
            'reason':'Exact original0C043298 is present and emitted at82DF90, but precedingCD677781/CD4F6781 and following load are in an unverified encoded/data-like path with no supplied Ghidra instruction listing. Generated path/mask admission does not prove this is executed retail code, nor its A0 ABI.',
            'scope':'Retain as source candidate; require original code/data/control provenance before assigning any generator/loader/other behavior.'}
    return {'classification':'UNRESOLVED','role':'unproved_code_or_data_consumer',
        'reason':'Source effects retained; instruction construction, existing-code relocation/copy or ordinary data semantics cannot yet be distinguished without the target/consumer domain.',
        'scope':'Must not be counted as non-generator or generator coverage.'}


def original_span(image,start,end):
    return {'start':start,'end_exclusive':end,'file_offset':image.offset(start),
        'sha256':hashlib.sha256(image.read(start,end-start)).hexdigest(),
        'words':[{'pc':pc,'word':f'{image.word(pc):08X}','file_offset':image.offset(pc)} for pc in range(start,end,4)]}


def direct_cache_paths(image,listing,roots,targets):
    edges=defaultdict(list);indirect=defaultdict(list);unowned=[];joined=0
    for pc,row in listing.items():
        if len(row['bytes'])!=8:continue
        try:raw=image.read(pc,4)
        except ValueError:unowned.append(pc);continue
        need(raw==bytes.fromhex(row['bytes']),'whole_main_listing_original_mismatch');joined+=1
        if not row['function_entry']:continue
        owner=int(row['function_entry'],16);word=int.from_bytes(raw,'little');op=word>>26
        if op in (2,3):
            to=target(word,pc);callee=listing.get(to,{}).get('function_entry')
            if op==3 or (callee and int(callee,16)!=owner):edges[owner].append({'pc':pc,'word':f'{word:08X}','target':to,'target_owner':int(callee,16) if callee else None})
        if op==0 and word&63 in (8,9) and not(word&63==8 and word>>21&31==31):indirect[owner].append(pc)
    results=[]
    for root in roots:
        queue=deque([(root,[])]);seen=set();found=[];unknown=[];dynamic=[]
        while queue:
            owner,path=queue.popleft()
            if owner in seen:continue
            seen.add(owner)
            if owner in targets:found.append(path);continue
            dynamic.extend(indirect[owner])
            for edge in edges[owner]:
                if edge['target_owner'] is None:unknown.append(edge)
                elif edge['target_owner'] not in seen:queue.append((edge['target_owner'],path+[edge]))
        results.append({'producer':root,'potential_direct_cache_paths':found,'visited_original_functions':len(seen),
            'unowned_direct_targets':unknown,'unknown_indirect_sites':sorted(set(dynamic)),
            'scope':'Potential original direct-call graph paths only; branches are not proved feasible and callback targets remain open. No negative global flush/generator inference.'})
    return {'whole_file_backed_listing_words_joined':joined,'non_file_backed_listing_addresses':unowned,'roots':results}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('route-proof','parent-root','sdk-root','output'): p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    for k,v in vars(a).items(): setattr(a,k,v.resolve())
    need(a.output.is_relative_to(W/'build/oracle-telemetry') and not a.output.exists(),'fresh_owned_output_required')
    need(sha(a.route_proof)==AUDIT_SHA and sha(Path(audit.__file__))==AUDIT_SOURCE_SHA,'frozen_original_route_proof_changed')
    old=json.loads(a.route_proof.read_text());pins=[{'path':str(x),'sha256':sha(x)} for x in (Path(__file__),Path(audit.__file__),a.route_proof)]
    def pin(path,expected=None):
        value=sha(path);need(expected is None or value==expected,'input_pin_changed:'+str(path));pins.append({'path':str(path),'sha256':value});return value
    sdkrev=subprocess.check_output(['git','-C',str(a.sdk_root),'rev-parse','HEAD'],text=True).strip();need(sdkrev==SDK_COMMIT,'official_SDK_commit_changed')
    sdk={}
    for relative in ('ee/kernel/include/syscallnr.h','ee/kernel/include/kernel.h','ee/kernel/src/kernel.S','ee/kernel/src/glue.c',
                     'ee/kernel/src/tlbfunc.c','ee/kernel/src/libosd.c','ee/kernel/src/alarm.c','ee/kernel/src/setup.c','ee/kernel/src/sifrpc.c','ee/kernel/src/sifcmd.c'):
        path=a.sdk_root/relative;raw=subprocess.check_output(['git','-C',str(a.sdk_root),'show',SDK_COMMIT+':'+relative]);working=path.read_bytes()
        need(working.replace(b'\r\n',b'\n')==raw.replace(b'\r\n',b'\n'),'SDK_working_content_not_commit')
        sdk[relative]={'path':str(path),'sha256':pin(path),'commit_blob_sha256':hashlib.sha256(raw).hexdigest(),
            'working_vs_commit':'EXACT_BYTES' if working==raw else 'ONLY_CRLF_LF_DIFFERENCE'}
    h=(a.sdk_root/'ee/kernel/include/syscallnr.h').read_text();need(re.search(r'__NR_FlushCache\s+0x64',h) and re.search(r'__NR_iFlushCache\s+\(-0x68\)',h),'SDK_flush_numbers_unproved')
    export=a.parent_root/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160'
    listing={};bodies=defaultdict(list);functions={}
    for filename in ('instructions.tsv','functions.tsv','function-bodies.tsv'):pin(export/filename)
    need(sha(export/'instructions.tsv')=='4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148','pinned_full_main_listing_changed')
    with (export/'instructions.tsv').open(encoding='utf-8',newline='') as f:
        for r in csv.DictReader(f,delimiter='\t'): listing[int(r['address'],16)]=r
    with (export/'function-bodies.tsv').open(encoding='utf-8',newline='') as f:
        for r in csv.DictReader(f,delimiter='\t'):bodies[int(r['entry'],16)].append((int(r['start'],16),int(r['end'],16)+1))
    with (export/'functions.tsv').open(encoding='utf-8',newline='') as f:
        for r in csv.DictReader(f,delimiter='\t'):functions[int(r['entry'],16)]=r
    images={}; wrappers={}; data={}
    for name,row in old['images'].items():
        path=next(Path(x['path']) for x in old['source_pins'] if x['sha256']==row['input_sha256']);pin(path,row['input_sha256']);image=audit.Original(path)
        maskpath=Path(row['decoder_run'][-2]);metapath=Path(row['decoder_run'][-1]);pin(maskpath,row['mask_sha256']);pin(metapath)
        mask=maskpath.read_bytes();base=int(metapath.read_text().splitlines()[0].split('\t')[1]);words={base+4*i:image.word(base+4*i) for i,b in enumerate(mask) if b}
        units=[]
        for u in row['generated_units']:
            path=Path(u['path']);pin(path,u['sha256']);units.append((path,path.read_text()))
        own={pc:wrapper(words,pc) for pc in words if wrapper(words,pc)}
        for pc,r in own.items():r.update(file_offset=image.offset(pc),span_sha256=hashlib.sha256(image.read(pc,16)).hexdigest(),generated_source=source_join(units,pc),Ghidra=listing_info(listing,image,pc) if name=='SLUS' else {'status':'NOT_SUPPLIED_FOR_IMAGE'})
        wrappers[name]=own;images[name]=image;data[name]=(words,units,base,mask)
    need(set(wrappers['SLUS'])=={0x10ca60,0x10ca90} and set(wrappers['NTGUI'])=={0x1cae20,0x1cae50},'original_flush_wrapper_identity_changed')
    results={};selected=set()
    for name,(words,units,base,mask) in data.items():
        image=images[name];known=dict(wrappers[name])
        if name in ('MOVIER','GAMER','NETR'):known.update(wrappers['SLUS'])
        calls=[];syscalls=[];raw=[];references=[];pointer_calls=[];unknown_indirect=[];cache_ops=[];formed_refs=[];forwarded=[];interior=[]
        # Full original byte census, separately from decoder-admitted paths.
        for va,off,size in image.mappings:
            for rel in range(0,size-3,4):
                pc=va+rel;q=struct.unpack_from('<I',image.raw,off+rel)[0]
                if q in known:references.append({'file_offset':off+rel,'mapped_address':pc,'value':f'{q:08X}','status':'raw_exact_function_value_NOT_call_proof','on_decoder_path':pc in words})
                if q>>26 in (2,3) and target(q,pc) in known and pc not in words:
                    raw.append({'pc':pc,'file_offset':off+rel,'word':f'{q:08X}','target':target(q,pc),'status':'OPEN_code_or_data_outside_reconstructed_paths'})
        for pc,q in sorted(words.items()):
            op,rs,fn=q>>26,q>>21&31,q&63
            if op in (9,13):
                regs,context=audit.local_constants(words,pc,256);audit.update(regs,q);value=regs[q>>16&31]
                if value is not None and value&0xffffffff in known:
                    formed_refs.append({'pc':pc,'word':f'{q:08X}','file_offset':image.offset(pc),'formed_value':value,
                        'destination_register':q>>16&31,'context':context,'status':'local_literal_address_formation_NOT_indirect_call_proof'})
            if op in (2,3):
                to=target(q,pc)
                if to not in known and any(entry<to<entry+16 for entry in known):interior.append({'pc':pc,'word':f'{q:08X}','target':to,'status':'wrapper_interior_transfer_requires_separate_semantics'})
                delay=words.get(pc+4)
                if delay is not None and delay&0xffff0000==0x24030000:
                    number=delay&65535;number=(number-65536 if number&32768 else number)&MASK64
                    if number in FLUSH:forwarded.append({'pc':pc,'word':f'{q:08X}','target':to,'delay_word':f'{delay:08X}',
                        'name':FLUSH[number],'status':'literal_V1_forwarding_candidate; target/syscall path must be proved'})
            if op in (2,3) and target(q,pc) in known:
                row={'pc':pc,'word':f'{q:08X}','file_offset':image.offset(pc),'kind':'JAL' if op==3 else 'J_tail',
                     'target':target(q,pc),'flush':known[target(q,pc)]['name'],'arguments':arguments(words,pc,op==3),
                     **boundary_role(name,image,words,pc),
                     'Ghidra':listing_info(listing,image,pc) if name=='SLUS' else {'status':'NOT_SUPPLIED_FOR_IMAGE'},'generated_source':source_join(units,pc,op==3)}
                row['original_boundary_context']=[{'pc':qpc,'file_offset':image.offset(qpc),'word':f'{words[qpc]:08X}'}
                    for qpc in range(pc-256,pc+260,4) if qpc in words]
                if name=='SLUS' and row['Ghidra']['status']=='ORIGINAL_WORD_JOINED':
                    owner=row['Ghidra']['function_entry'];row['function_entry']=owner
                    if owner: selected.add(int(owner,16))
                calls.append(row)
            if op==0x2f:
                recognized=(name=='SLUS' and (0x10d030<=pc<0x10d0d4 or 0x10d170<=pc<0x10d214 or 0x10fe78<=pc<0x10ff24)) or (name=='NTGUI' and (0x1cb3f0<=pc<0x1cb494 or 0x1cb530<=pc<0x1cb5d4 or 0x1cde78<=pc<0x1cdf24))
                cache_ops.append({'pc':pc,'word':f'{q:08X}','file_offset':image.offset(pc),'cache_operation_field':q>>16&31,
                    'base_register':rs,'classification':'other_cache_maintenance' if recognized else 'UNRESOLVED_instruction_or_data',
                    'status':'source_range_cache_loop' if recognized else 'decoder_path_candidate_NOT_independently_admitted_CACHE',
                    'Ghidra':listing_info(listing,image,pc) if name=='SLUS' else {'status':'NOT_SUPPLIED_FOR_IMAGE'},
                    'reason':'Two-way indexed TagLo/range comparison loop or64-byte DHWBIN loop; not an instruction producer.' if recognized else 'No independent instruction/data owner proof. Some original patterns are repeated float-like/encoded data; do not classify by opcode alone.'})
            if q&0xfc00003f==12:
                regs,context=audit.local_constants(words,pc,256);number=regs[3]
                syscalls.append({'pc':pc,'word':f'{q:08X}','file_offset':image.offset(pc),'V1_low64':number,
                    'classification':FLUSH.get(number,'OTHER_KNOWN_SYSCALL' if number is not None else 'OPEN_DYNAMIC_SYSCALL'),
                    'context':context,'Ghidra':listing_info(listing,image,pc) if name=='SLUS' else {'status':'NOT_SUPPLIED_FOR_IMAGE'}})
            if op==0 and fn in (8,9) and not(fn==8 and rs==31):
                regs,context=audit.local_constants(words,pc,256);value=regs[rs]
                row={'pc':pc,'word':f'{q:08X}','source_register':rs,'captured_target':value,'context':context}
                if value in known:row.update(flush=known[value]['name'],arguments=arguments(words,pc,fn==9));pointer_calls.append(row)
                else:row['status']='OPEN_TARGET_NOT_LOCALLY_PROVED';unknown_indirect.append(row)
        results[name]={'input_path':str(image.path),'input_sha256':sha(image.path),'mappings':[{'VA':v,'file_offset':o,'file_bytes':s} for v,o,s in image.mappings],
            'decoder_path_words':len(words),'mask_path':old['images'][name]['decoder_run'][-2],'wrappers':list(wrappers[name].values()),'direct_calls':calls,
            'unclassified_raw_direct_matches':raw,'raw_function_value_references':references,'literal_function_address_formations':formed_refs,
            'literal_V1_forwarding_candidates':forwarded,'wrapper_interior_transfers':interior,
            'locally_proved_pointer_calls':pointer_calls,'all_syscalls':syscalls,'inline_CACHE_paths':cache_ops,
            'unknown_indirect_count':len(unknown_indirect),'unknown_indirect_inventory':old['images'][name]['indirect_inventory'],
            'unknown_indirect_reason':'The complete original-path JR/JALR inventory remains open except locally proved targets; ordinary JR RA is a return, not a flush target claim.',
            'summary':{'direct_flush_calls':len(calls),'JAL':sum(r['kind']=='JAL' for r in calls),'J_tail':sum(r['kind']=='J_tail' for r in calls),
                'raw_outside_path_matches':len(raw),'raw_function_refs':len(references),'resolved_pointer_calls':len(pointer_calls),
                'literal_function_address_formations':len(formed_refs),'literal_V1_forwarding_candidates':len(forwarded),'wrapper_interior_transfers':len(interior),
                'syscall_paths':len(syscalls),'flush_syscall_paths':sum(r['classification'] in FLUSH.values() for r in syscalls),'unknown_indirect':len(unknown_indirect),
                'inline_CACHE_paths':len(cache_ops),'independently_classified_cache_maintenance':sum(r['classification']=='other_cache_maintenance' for r in cache_ops),
                'caller_classification_counts':dict(Counter(r['classification'] for r in calls))}}
    # Whole original Ghidra body spans for all main callers plus the two known
    # producer boundaries. Every word carries the exact original mapping.
    selected.update((0x2ed3e0,0x2fa9d0));function_proofs=[];mainimage=images['SLUS']
    for entry in sorted(selected):
        ranges=bodies.get(entry,[]);rows=[]
        for lo,hi in ranges:
            for pc in range(lo,hi,4):
                q=mainimage.word(pc);r={'pc':pc,'word':f'{q:08X}','file_offset':mainimage.offset(pc),'Ghidra':listing_info(listing,mainimage,pc)}
                if q>>26 in (2,3):r['direct_target']=target(q,pc)
                rows.append(r)
        function_proofs.append({'entry':entry,'function':functions.get(entry),'ranges':ranges,'original_words':rows,'generated_entry':source_join(data['SLUS'][1],entry),
            'direct_targets':dict(sorted(Counter(f'{r["direct_target"]:08X}' for r in rows if 'direct_target' in r).items())),
            'status':'FULL_ORIGINAL_GHIDRA_RANGES_JOINED' if ranges else 'OPEN_NO_GHIDRA_FUNCTION_BODY'})
    cache_graph=direct_cache_paths(mainimage,listing,[0x2ed3e0,0x2fa9d0],{0x10ca60,0x10ca90,0x10d030,0x10d170,0x10fe78})
    producer_path=Path(old['known_original_producer']['proof']);producer=json.loads(producer_path.read_text());pin(producer_path)
    producer_record=Path(producer['record']['path']);pin(producer_record,producer['record']['sha256']);producer_rows=json.loads(producer_record.read_text());producer_flush=[]
    for f in producer_rows:
        lo,hi=map(lambda x:int(x,16),f['span']);need(hashlib.sha256(mainimage.read(lo,hi-lo)).hexdigest()==f['span_sha256'],'original_producer_span_changed')
        for c in f['direct_calls']:
            pc,to=int(c['pc'],16),int(c['target'],16);need(target(mainimage.word(pc),pc)==to,'original_producer_call_changed')
            if to in {0x10ca60,0x10ca90,0x10d030,0x10d0d8,0x10d170,0x10d218,0x10fe78}:producer_flush.append({'function':f['entry'],**c})
    netr_chains=[]
    for entry,end,dispatchers,table in [(0x7f0068,0x7f0140,[(0x7f02a0,0x7f0378),(0x7f09c8,0x7f0aa0)],0x849b18),
                                      (0x829ed8,0x829fb0,[(0x82a110,0x82a1e8),(0x82a838,0x82a910)],0x84cae0)]:
        image=images['NETR'];values=list(struct.unpack('<16I',image.read(table,64)))
        netr_chains.append({'entry':entry,'classification':'UNRESOLVED_code_or_data_transform','entry_span':original_span(image,entry,end),
            'dispatchers':[original_span(image,lo,hi) for lo,hi in dispatchers],'raw_table':{'VA':table,'file_offset':image.offset(table),'words':[f'{x:08X}' for x in values]},
            'selected_handler':original_span(image,0x7f0aa0 if entry==0x7f0068 else 0x82a910,0x7f0b40 if entry==0x7f0068 else 0x82a9b0),
            'finding':'Original descriptor supplies a nibble dispatch. Selected handler loads existing32-bit words, XORs09F8ED17 and stores them back, conditional on007F01F8/0082A0xx selection. This proves word transformation, not that the bytes are executable or that all16 table indices are valid.',
            'remaining':'Descriptor/register inputs, all selector domains, buffer execution consumers and table extent/type are open. Raw16-word table includes zero/non-code candidates; no callable-domain admission.'})
    miss_join=W/'build/oracle-telemetry/r192-owned-miss-join-02/RESULT.json';pin(miss_join,'4bd5e5ea33162a9a3d898b26557f2c7dabc10ad166147bbfb2a391766a5fc096');miss=json.loads(miss_join.read_text())
    miss_page=W/'build/oracle-telemetry/r192-owned-aot-probe-01/misses/008A4000_D8F8253E.bin';pin(miss_page,miss['dump_sha256'])
    for row in miss['original_caller_rows']:need(mainimage.word(int(row['pc'],16))==int(row['word'],16),'known_generated_caller_original_changed')
    dynamic={'observed_entry':'008A5140','page':str(miss_page),'page_sha256':sha(miss_page),'entry_word':f'{struct.unpack_from("<I",miss_page.read_bytes(),0x1140)[0]:08X}',
        'caller_join':str(miss_join),'original_call_chain':miss['original_caller_rows'],
        'finding':'Original002E612C loads targetT4 from object+4C,002E6178 JALR uses that target and002E617C computes T3=T4+4*object394 in its delay. Observed target is008A5140; RA002E6180 is its caller continuation.',
        'source_builder_frontier':'002FA9D0→002FAEAC→002ED3E0/002ED4B0 builds words; final copy/patch sites002FAF44/002FAFE8/002FB070/002FB150 are source-derived. No observed fresh allocation→producer→commit transaction authenticates THIS saved allocation.',
        'writer_attribution':'OPEN; grammar/constructed builder agreement does not alone prove this exact historical writer, patch or copy source. Saved code/descriptor membership is distinct from fresh history.'}
    controls=self_test()
    for name,pc,at,mutated in [('wrong_copy_callee',0x116340,0x116338,0x0c0458a8),('wrong_copy_destination',0x116340,0x11632c,0x3c048008)]:
        original=dict(data['SLUS'][0]);original[at]=mutated
        try:boundary_role('SLUS',mainimage,original,pc)
        except ValueError as e:controls.append({'name':name,'status':'REJECTED','reason':str(e)})
        else:raise ValueError('planted_copy_role_witness_accepted:'+name)
    changed=[x['path'] for x in pins if sha(Path(x['path']))!=x['sha256']]
    report={'status':'SOURCE_CACHE_FLUSH_CENSUS_COMPLETE_WITH_OPEN_ROLES' if not changed else 'INPUT_CHANGED','classification':'source-only, zero runtime/native/ownership credit',
        'native_credit':0,'source_pins':pins,'input_changes':changed,'official_SDK':{'commit':sdkrev,'files':sdk,'numbers':{'FlushCache':100,'iFlushCache':-104},
            'modes':{'0':'WRITEBACK_DCACHE','1':'INVALIDATE_DCACHE','2':'INVALIDATE_ICACHE','3':'INVALIDATE_CACHE'},
            'limit':'SDK names/ABI identify syscall wrappers, not the retail kernel implementation or a universal flush obligation for code generation.'},
        'images':results,'caller_functions_and_known_producers':function_proofs,'known_resource_producer_direct_graph':cache_graph,
        'bounded_original_producer_spans':{'proof':str(producer_path),'record':str(producer_record),'span_count':len(producer_rows),'direct_cache_calls':producer_flush,
            'limit':'Selected65 original spans only; no global no-flush conclusion across unknown callees/callbacks.'},
        'NETR_unresolved_word_transform_chains':netr_chains,'observed_008A5140_provenance':dynamic,'controls':controls,
        'coverage':'All file-backed words of each pinned original image are searched for raw direct target/value matches; every pinned original-analyser code word is checked for direct transfers, inline syscall and indirect transfers. Data matches remain distinct. Main Ghidra original function-body ranges are joined word by word, including split ranges. Overlay/NTGUI Ghidra exports were not supplied and remain explicit missing evidence.',
        'limits':['Caller roles not proved by original dataflow remain UNRESOLVED; mode0 writeback or mode2 invalidation alone does not classify a generator.',
            'Unknown indirect transfers, runtime-generated words, unrepresented original code and dynamic syscall numbers remain OPEN. No claim every generator flushes.',
            '008A5140 is an observed generated-code PC, not file-backed original SLUS code; original producer/call-path attribution requires its existing construction evidence.',
            'No runtime, compiler, card, GUI, original/private-input write, rule change or function-credit change.']}
    audit.reserve(4<<20);a.output.mkdir(parents=True);out=a.output/'RESULT.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'result':str(out),'sha256':sha(out),'status':report['status'],'summary':{n:r['summary'] for n,r in results.items()}}));return 0 if not changed else 1


if __name__=='__main__':raise SystemExit(main())
