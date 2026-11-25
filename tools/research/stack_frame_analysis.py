"""Bounded original-byte stack provenance analysis; never an EE executor.

The abstract domain tracks only SP-derived addresses and input-value identity.
It cannot provide concrete guest addresses or authorize native memory access.
Unknown aliases/callee effects remain explicit native-emission blockers.
"""
from __future__ import annotations
from collections import defaultdict, deque
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


class FrameRefusal(ValueError):
    pass


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def signed16(word): return (word & 65535) - (65536 if word & 32768 else 0)
def hx(value): return f"{value:08x}"


def elf_reader(image):
    if image[:6] != b"\x7fELF\x01\x01": raise FrameRefusal("frame_original_ELF32_LE_required")
    start = struct.unpack_from("<I", image, 28)[0]
    stride, count = struct.unpack_from("<HH", image, 42)
    segments = [struct.unpack_from("<8I", image, start+i*stride) for i in range(count)]
    def original(va, size):
        found = [image[off+va-base:off+va-base+size] for kind,off,base,_,length,_,_,_ in segments
                 if kind == 1 and base <= va and va+size <= base+length]
        if len(found) != 1 or len(found[0]) != size: raise FrameRefusal("frame_original_address_not_unique:"+hx(va))
        return found[0]
    return original


# Width and direction only; these are not C++ pointer/type assertions.
MEMORY = {0x20:(8,"load"),0x24:(8,"load"),0x21:(16,"load"),0x25:(16,"load"),
          0x23:(32,"load"),0x27:(32,"load"),0x31:(32,"load"),0x37:(64,"load"),0x1e:(128,"load"),
          0x28:(8,"store"),0x29:(16,"store"),0x2b:(32,"store"),0x39:(32,"store"),0x3f:(64,"store"),0x1f:(128,"store")}
PARTIAL_MEMORY = {0x22,0x26,0x1a,0x1b,0x2a,0x2e,0x2c,0x2d}
CALLEE_SAVED = set(range(16,24)) | {30,31}


def control(pc, word):
    op, rs, rt, fn = word>>26, word>>21&31, word>>16&31, word&63
    if op == 3: return {"kind":"call", "target":((pc+4)&0xf0000000)|((word&0x3ffffff)<<2), "successors":[pc+8]}
    if op == 2: return {"kind":"jump", "target":((pc+4)&0xf0000000)|((word&0x3ffffff)<<2), "successors":[((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)]}
    if op == 0 and fn == 9: return {"kind":"indirect_call", "target_register":rs, "successors":[pc+8]}
    if op == 0 and fn == 8: return {"kind":"return" if rs==31 else "indirect_jump", "target_register":rs, "successors":[]}
    if op in {1,4,5,6,7,0x14,0x15,0x16,0x17} or (op in {0x10,0x11,0x12} and rs==8):
        return {"kind":"branch", "successors":[pc+4+(signed16(word)<<2),pc+8],
                "likely":op in {0x14,0x15,0x16,0x17} or (op==1 and rt in {2,3,18,19}) or (op in {0x10,0x11,0x12} and bool(rt&2)),
                "unsupported_link":op==1 and rt in {16,17,18,19}}
    return None


def recover_stack_frame(original, entry, spans):
    """Recover conservative frame facts from a pinned VA reader and body spans.

    A missing exported extent permits reference-only prefix geometry, never a
    guessed complete function. Every elision remains gated independently.
    """
    entry = int(entry,16) if isinstance(entry,str) else entry
    words = {}
    for span in spans:
        start,end=(int(span[k],16) if isinstance(span[k],str) else span[k] for k in ("start","end"))
        if start&3 or (end-start+1)%4: raise FrameRefusal("frame_body_alignment")
        if span.get('sha256') and hashlib.sha256(original(start,end-start+1)).hexdigest()!=span['sha256']:
            raise FrameRefusal('frame_original_body_pin_changed')
        for at in range(start,end+1,4): words[at]=int.from_bytes(original(at,4),"little")
    extent_known = bool(words)
    if not words:
        for at in range(entry,entry+256,4):
            words[at]=int.from_bytes(original(at,4),"little")
            if at>entry and control(at,words[at]):
                words[at+4]=int.from_bytes(original(at+4,4),"little");break
    if entry not in words: raise FrameRefusal("frame_entry_outside_body")
    first=words[entry]
    if first>>26 not in {9,0x19} or first>>21&31!=29 or first>>16&31!=29 or signed16(first)>=0:
        raise FrameRefusal("frame_no_fixed_SP_prologue")
    size=-signed16(first)
    if not size or size%16 or size>65536: raise FrameRefusal("frame_size_or_alignment_unproved")
    facts={"schema":"original_stack_frame_v1","source_entry":hx(entry),"frame_bytes":size,
           "entry_alignment":16,"extent_known":extent_known,"body_spans":spans,
           "original_words":[{"pc":hx(p),"word":f"{w:08x}"} for p,w in sorted(words.items())],
           "SP_adjustments":[],"stack_accesses":[],"aliases":[],"escapes":[],"calls":[],"returns":[],"blockers":[]}
    if not extent_known:facts["blockers"].append("frame_original_extent_unresolved")
    blockers=set(facts["blockers"]);records=defaultdict(dict)
    # None denotes no proved frame provenance; '?' denotes a mixed/unknown
    # SP-derived value. Offsets are symbolic bytes from entry SP, not addresses.
    initial={"aliases":tuple(frozenset({0}) if i==29 else frozenset({None}) for i in range(32)),
             "origins":tuple(frozenset({"input:"+str(i)}) for i in range(32)),"memory":{}}
    states={entry:initial};queue=deque([entry]);visits=defaultdict(int)
    def event(kind, pc, data):records[kind][(pc,json.dumps(data,sort_keys=True))]={"pc":hx(pc),**data}
    def cloned(state):return {"aliases":list(state["aliases"]),"origins":list(state["origins"]),"memory":dict(state["memory"])}
    def freeze(state):return {**state,"aliases":tuple(state["aliases"]),"origins":tuple(state["origins"])}
    def merge(old,new):
        if old is None:return new,True
        a=tuple(x|y for x,y in zip(old["aliases"],new["aliases"]));o=tuple(x|y for x,y in zip(old["origins"],new["origins"]))
        memory={}
        for key in old["memory"].keys()|new["memory"].keys():
            x=old["memory"].get(key,(frozenset({None}),frozenset({"uninitialized"})))
            y=new["memory"].get(key,(frozenset({None}),frozenset({"uninitialized"})))
            memory[key]=(x[0]|y[0],x[1]|y[1])
        merged={"aliases":a,"origins":o,"memory":memory}
        return merged,merged!=old
    def execute_abstract(pc,state):
        w=words[pc];op,rs,rt,rd,fn=w>>26,w>>21&31,w>>16&31,w>>11&31,w&63
        immediate=signed16(w);aliases=state["aliases"]
        def assign(reg,value,origin=None):
            if reg:
                state["aliases"][reg]=frozenset(value)
                state["origins"][reg]=origin if origin is not None else frozenset({"definition:"+hx(pc)})
        if op in {9,0x19}:
            values={v+immediate if isinstance(v,int) else v for v in aliases[rs]}
            if any(isinstance(v,int) and abs(v)>size*4 for v in values):
                values={"?"};blockers.add("frame_dynamic_or_unbounded_SP_alias")
            assign(rt,values)
            if rt==29:
                event("SP_adjustments",pc,{"delta":immediate,"offsets_after":sorted(str(v) for v in values)})
                if len(values)!=1 or not all(isinstance(v,int) for v in values):blockers.add("frame_SP_join_ambiguous")
            elif any(v is not None for v in values):event("aliases",pc,{"register":rt,"offsets":sorted(str(v) for v in values)})
        elif op==0 and fn in {0x21,0x2d,0x25} and (rs==0 or rt==0):
            src=rt if rs==0 else rs;assign(rd,aliases[src],state["origins"][src])
            if any(v is not None for v in aliases[src]):event("aliases",pc,{"register":rd,"offsets":sorted(str(v) for v in aliases[src])})
        elif op in MEMORY or op in PARTIAL_MEMORY:
            width,direction=MEMORY.get(op,(0,"partial"))
            base=aliases[rs];frame_values=[v for v in base if v is not None]
            bank='FPR' if op in {0x31,0x39} else 'GPR'
            source_alias=aliases[rt] if bank=='GPR' else frozenset({None})
            source_origins=state['origins'][rt] if bank=='GPR' else frozenset({'unbound_FPR:'+str(rt)})
            key=None
            if frame_values:
                if len(base)!=1 or not isinstance(next(iter(base)),int) or not width:
                    blockers.add("frame_alias_address_or_partial_width_unresolved")
                else:
                    entry_offset=next(iter(base))+immediate;offset=entry_offset+size
                    key=(entry_offset,width)
                    data={"offset_from_allocated_sp":offset,"offset_from_entry_sp":entry_offset,"width_bits":width,
                          "direction":direction,"register":rt,"register_bank":bank,"base_register":rs,"word":f"{w:08x}",
                          "source_value_origins":sorted(source_origins)}
                    event("stack_accesses",pc,data)
                    if offset<0 or offset+width//8>size:blockers.add("frame_outside_owned_extent_or_outgoing_argument")
                    if offset%(width//8):blockers.add("frame_slot_alignment_unproved")
                    if direction=="store":
                        for existing in list(state["memory"]):
                            if existing!=key and max(existing[0],entry_offset)<min(existing[0]+existing[1]//8,entry_offset+width//8):
                                state["memory"][existing]=(frozenset({"?"}),frozenset({"clobbered"}));blockers.add("frame_overlapping_slot_clobber")
                        state["memory"][key]=(source_alias,source_origins)
                        if any(v is not None for v in source_alias):
                            event("escapes",pc,{"kind":"frame_address_stored_in_local_slot","slot_offset":offset,"source_register":rt});blockers.add("frame_local_address_storage_requires_typed_alias_proof")
                    elif direction=="load":
                        value,origin=state["memory"].get(key,(frozenset({None}),frozenset({"uninitialized"})))
                        if "uninitialized" in origin:blockers.add("frame_slot_read_before_definite_store")
                        if width<128:
                            origin=frozenset('input_low'+str(width)+':'+v.split(':',1)[1] if v.startswith('input:') else v for v in origin)
                        if op not in {0x31}:assign(rt,value,origin)
            if direction=="store" and not frame_values and any(v is not None for v in source_alias):
                event("escapes",pc,{"kind":"frame_address_stored_outside_frame","source_register":rt,"base_register":rs});blockers.add("frame_address_escape_store")
            if direction=="load" and key is None and op!=0x31:assign(rt,{None})
        elif op==0x0f:assign(rt,{None})
        elif op==0 and fn not in {8,9,0x0c,0x0d,0x11,0x13,0x18,0x19,0x1a,0x1b}:
            derived=any(v is not None for reg in (rs,rt) for v in aliases[reg])
            assign(rd,{"?"} if derived else {None})
            if derived:blockers.add("frame_alias_arithmetic_unproved")
        elif op in {8,0xa,0xb,0xc,0xd,0xe,0x18}:
            derived=any(v is not None for v in aliases[rs]);assign(rt,{"?"} if derived else {None})
            if derived:blockers.add("frame_alias_scalar_conversion_unproved")
        elif op in {0x10,0x11,0x12} and rs in {0,1,2}:assign(rt,{None})
        return state
    while queue:
        pc=queue.popleft();visits[pc]+=1
        if visits[pc]>64:blockers.add("frame_dataflow_iteration_limit");continue
        if pc not in words:blockers.add("frame_CFG_leaves_known_body");continue
        state=cloned(states[pc]);transfer=control(pc,words[pc]);execute_abstract(pc,state)
        variants=[]
        if transfer:
            if transfer.get('unsupported_link'):blockers.add('frame_REGIMM_link_RA_semantics_unproved')
            if pc+4 not in words:blockers.add("frame_missing_executed_delay_word");continue
            target_identity=state['origins'][31] in (frozenset({'input:31'}),frozenset({'input_low64:31'}))
            if transfer["kind"] in {"call","indirect_call"}:
                destination=31 if transfer["kind"]=="call" else words[pc]>>11&31
                if destination:
                    state["aliases"][destination]=frozenset({None})
                    state["origins"][destination]=frozenset({"call_return_address:"+hx(pc)})
            delayed=execute_abstract(pc+4,cloned(freeze(state)))
            if transfer["kind"] in {"call","indirect_call"}:
                event("calls",pc,{k:v for k,v in transfer.items() if k!="successors"})
                blockers.add("frame_callee_stack_effects_unproved")
                for reg in range(2,29):
                    if any(v is not None for v in delayed["aliases"][reg]):
                        event("escapes",pc,{"kind":"SP_derived_register_at_unbound_call","register":reg,"offsets":sorted(str(v) for v in delayed["aliases"][reg])});blockers.add("frame_address_escape_or_callee_alias_unproved")
                for reg in list(range(2,16))+[24,25,31]:
                    delayed["aliases"][reg]=frozenset({None});delayed["origins"][reg]=frozenset({"call_result:"+hx(pc)})
                variants=[(pc+8,delayed)]
            elif transfer["kind"]=="return":
                balanced=delayed["aliases"][29]==frozenset({0})
                restored=delayed["origins"][31] in (frozenset({'input:31'}),frozenset({'input_low64:31'}))
                event("returns",pc,{"delay_pc":hx(pc+4),"SP_balanced":balanced,"RA_input_identity_locally_restored":restored,
                                    "JR_target_input_identity_before_delay":target_identity,
                                    "input_registers_locally_restored":[reg for reg in sorted(CALLEE_SAVED) if delayed["origins"][reg]==frozenset({"input:"+str(reg)})]})
                if not balanced:blockers.add("frame_unbalanced_return_SP")
                if not restored:blockers.add("frame_return_RA_clobber_unproved")
                if not target_identity:blockers.add('frame_JR_target_before_delay_unproved')
                if any(v is not None for reg in (2,3) for v in delayed["aliases"][reg]):blockers.add("frame_address_return_escape")
            elif transfer["kind"]=="indirect_jump":blockers.add("frame_indirect_exit_unproved")
            elif transfer["kind"]=="branch":
                variants=[(transfer["successors"][0],delayed),(transfer["successors"][1],state if transfer.get("likely") else delayed)]
            else:variants=[(transfer["successors"][0],delayed)]
        else:variants=[(pc+4,state)]
        for target,new in variants:
            if target not in words:blockers.add("frame_CFG_exit_or_tail_call_unproved");continue
            merged,changed=merge(states.get(target),freeze(new))
            if changed:states[target]=merged;queue.append(target)
    for kind in records:facts[kind]=sorted(records[kind].values(),key=lambda x:(x["pc"],json.dumps(x,sort_keys=True)))
    facts["reachable_original_pcs"]=[hx(p) for p in sorted(states)]
    slots=[]
    grouped=defaultdict(list)
    for access in facts["stack_accesses"]:grouped[(access["offset_from_allocated_sp"],access["width_bits"])].append(access)
    for (offset,width),accesses in sorted(grouped.items()):
        stores=[a for a in accesses if a["direction"]=="store"];loads=[a for a in accesses if a["direction"]=="load"]
        regs={a["register"] for a in accesses};reg=next(iter(regs)) if len(regs)==1 else None
        banks={a['register_bank'] for a in accesses};bank=next(iter(banks)) if len(banks)==1 else 'MIXED'
        category="RA_transport_candidate" if reg==31 and width==64 else "callee_save_transport_candidate" if reg in CALLEE_SAVED and stores and loads else "typed_local_candidate"
        if bank!='GPR':
            category='unbound_FPR_or_mixed_bank_local';blockers.add('frame_FPR_or_mixed_local_type_unproved')
        if offset<0 or offset+width//8>size:category="outgoing_or_external_slot_unresolved"
        elidable=(not blockers and category in {"RA_transport_candidate","callee_save_transport_candidate"}
                  and bool(stores) and bool(loads) and bool(facts['returns'])
                  and all(a['source_value_origins']==['input:'+str(reg)] for a in stores)
                  and all(ret['JR_target_input_identity_before_delay'] for ret in facts['returns'])
                  and all((ret['RA_input_identity_locally_restored'] if reg==31 and width==64 else reg in ret['input_registers_locally_restored'] and width==128) for ret in facts['returns']))
        slots.append({"offset_from_allocated_sp":offset,"width_bits":width,"source_register":reg,"register_bank":bank,
                      "store_pcs":[a["pc"] for a in stores],"load_pcs":[a["pc"] for a in loads],"classification":category,
                      "transport_elision_proved":elidable,
                      "elision_status":"PROVED_ORIGINAL_TRANSPORT_REQUIRES_AST_JOIN" if elidable else "REFERENCE_ONLY_UNPROVED",
                      "native_type":{8:"u8",16:"u16",32:"u32",64:"u64",128:"bits128"}[width] if bank=='GPR' else None})
    facts["slots"]=slots
    if not facts["returns"]:blockers.add("frame_no_proved_return")
    facts["blockers"]=sorted(blockers)
    facts["native_admission"]={"status":"REFUSED" if blockers else "REQUIRES_GENERATED_AST_AND_TYPED_ABI_JOIN","blockers":sorted(blockers)}
    return facts


def reference_slices(frame, source, unit_path, unit_sha, entry_case):
    """Select whole generated control bundles; duplicated delay labels stay explicit."""
    entry=int(frame["source_entry"],16);words={int(x["pc"],16):int(x["word"],16) for x in frame["original_words"]}
    labels={int(m[1],16):m.start() for m in re.finditer(r"(?m)^L_([0-9A-F]{8}):",source)}
    cases={int(pc,16):int(case) for case,pc in re.findall(r"case (\d+)u: goto L_([0-9A-F]+);",source)}
    included=[];delays=[];stops=[]
    for pc in range(entry,max(words)+1,4):
        if pc not in words:break
        transfer=control(pc,words[pc])
        if transfer:
            if transfer["kind"]=="branch":
                included += [pc,pc+4];delays.append({"owner_pc":hx(pc),"delay_pc":hx(pc+4)});stops=transfer["successors"]
            else:stops=[pc]
            break
        included.append(pc)
    epilogues=[]
    accesses={int(a["pc"],16):a for a in frame["stack_accesses"]}
    for ret in frame["returns"]:
        pc=int(ret["pc"],16);start=pc
        prior=[p+8 for p,w in words.items() if p<pc and control(p,w)]
        floor=max(prior) if prior else entry
        loads=[p for p,a in accesses.items() if floor<=p<pc and a["direction"]=="load"]
        if loads:start=min(loads)
        epilogues.append({"start_pc":hx(start),"entry_case":cases.get(start),"JR_pc":hx(pc),"delay_pc":ret["delay_pc"],
                          "original_instruction_pcs":[hx(p) for p in range(start,pc+8,4)]})
    used=set(included)|set(stops)|{int(x["start_pc"],16) for x in epilogues}
    return {"source_entry":frame["source_entry"],"source_unit":unit_path,"source_unit_sha256":unit_sha,"entry_case":entry_case,
            "frame_bytes":frame["frame_bytes"],"entry_alignment":16,"body_spans":frame["body_spans"],"slots":frame["slots"],
            "SP_adjustments":frame["SP_adjustments"],"stack_accesses":frame["stack_accesses"],
            "entry_slice":{"start_pc":hx(entry),"stop_before_pcs":[hx(p) for p in stops],"original_instruction_pcs":[hx(p) for p in included],
                           "inline_delay_pcs":delays,"source_label_offsets":{hx(p):labels.get(p) for p in sorted(used)},
                           "includes_unjoined_semantic_prefix":True},"epilogue_slices":epilogues,"native_admission":frame["native_admission"]}


def validate_frame_plan(original, plan, *, require_native=False):
    """Re-derive every proof-bearing field; serialized classifications are not authority."""
    actual=recover_stack_frame(original,plan["source_entry"],plan["body_spans"])
    for key in ("frame_bytes","extent_known","original_words","SP_adjustments","stack_accesses","aliases","escapes","calls","returns","slots","blockers","native_admission"):
        if actual[key]!=plan[key]:raise FrameRefusal("frame_plan_not_original_derived:"+key)
    if require_native and actual["blockers"]:
        raise FrameRefusal("frame_native_elision_unproved:"+",".join(actual["blockers"]))
    return actual


def self_test(original, plan):
    """Pure static controls; reference S3 separately mutates actual RAM/frame expectations."""
    import copy
    validate_frame_plan(original,plan)
    cases=[]
    for label,key,mutate in (
        ("forged frame size","frame_bytes",lambda p:p.update(frame_bytes=p["frame_bytes"]+16)),
        ("escaping/unproved slot mislabeled elidable","slots",lambda p:p["slots"][0].update(elision_status="PROVED_ELIDABLE")),
        ("forged escape removal","escapes",lambda p:p["escapes"].append({"pc":"00000000","kind":"invented"})),
    ):
        if key=="slots" and not plan["slots"]:continue
        planted=copy.deepcopy(plan);mutate(planted)
        try:validate_frame_plan(original,planted)
        except FrameRefusal as error:
            if str(error)!="frame_plan_not_original_derived:"+key:raise
            cases.append({"plant":label,"rejection":str(error)})
        else:raise AssertionError("Forged frame proof admitted: "+label)
    return {"status":"PASS","controls":cases}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root",type=Path,required=True);parser.add_argument("--cohort",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    parser.add_argument('--frame-span-receipt',type=Path)
    args=parser.parse_args();parent=args.parent_root.resolve();fork=parent/"external/forks/ps2-native-recomp";output=args.output.resolve()
    if output.exists() or not output.is_relative_to((fork/"build/oracle-telemetry").resolve()):raise FrameRefusal("frame_output_requires_new_ignored_directory")
    cohort=json.loads(args.cohort.read_bytes());image=(parent/"research/inputs/SLUS_214.10").read_bytes();original=elf_reader(image)
    pin=hashlib.sha256(image).hexdigest()
    if pin!=cohort["inputs"]["original_elf_sha256"]:raise FrameRefusal("frame_original_pin_changed")
    targets=[];slices=[]
    recovery=None
    if args.frame_span_receipt:
        recovery=json.loads(args.frame_span_receipt.read_bytes())
        if recovery['original_elf_sha256']!=pin or recovery['provenance']!='bounded_frame_CFG_not_catalogue_function':
            raise FrameRefusal('frame_recovered_span_provenance')
        span=recovery['span'];start,end=int(span['start'],16),int(span['end'],16)
        rows=recovery['rows']
        if len(rows)*4!=end-start+1 or [int(r['address'],16) for r in rows]!=list(range(start,end+1,4)):
            raise FrameRefusal('frame_recovered_span_listing_extent')
        for row in rows:
            if original(int(row['address'],16),4)!=bytes.fromhex(row['bytes']):raise FrameRefusal('frame_recovered_span_word_join')
    for row in cohort["samples"]:
        if row["reason"]!="unbound_value:register_29":continue
        spans=row.get("original_function_context",{}).get("body_spans",[])
        recovered=False
        if not spans and recovery and row['address']==recovery['source_entry']:
            spans=[recovery['span']];recovered=True
        frame=recover_stack_frame(original,row["address"],spans)
        if recovered:
            if any('CFG_' in b or 'missing_executed_delay' in b or 'no_proved_return' in b or 'unbalanced_return' in b for b in frame['blockers']):
                raise FrameRefusal('frame_recovered_CFG_not_closed')
            frame['extent_provenance']='bounded_frame_CFG_not_catalogue_function'
            frame['frame_extent_receipt_sha256']=sha(args.frame_span_receipt)
            frame['original_function_ownership']='UNRESOLVED; no catalogue or ownership mutation'
        path=fork/"build/generated-aot/phase4-all"/row["generated_unit"]
        if sha(path)!=row["source_unit_sha256"]:raise FrameRefusal("frame_generated_unit_pin_changed")
        frame.update(source_unit=row["generated_unit"],source_unit_sha256=sha(path),entry_case=row["entry_case"])
        targets.append(frame);slices.append(reference_slices(frame,path.read_text(encoding="utf-8"),path.relative_to(fork).as_posix(),sha(path),row["entry_case"]))
    if len(targets)!=28:raise FrameRefusal("frame_requires_exact28_stack_first_targets")
    output.mkdir()
    common={"original_elf_sha256":pin,"cohort_sha256":sha(args.cohort),"analyzer_sha256":sha(Path(__file__))}
    (output/"ANALYSIS.json").write_text(json.dumps({"schema":"generated_stack_frame_analysis_v1",**common,"targets":targets},indent=2)+"\n",encoding="utf-8",newline="\n")
    (output/"SLICES.json").write_text(json.dumps({"schema":"generated_stack_frame_slices_v1",**common,"targets":slices},indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"targets":len(targets),"ANALYSIS_sha256":sha(output/"ANALYSIS.json"),"SLICES_sha256":sha(output/"SLICES.json")}))


if __name__=="__main__":main()
