"""Read-only original-word/listing/caller proof for four camera operations.

Private output only. No game process, inferred ownership admission or credit.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import audit_ps2_rtti as rtti
import bounded_ownership as bounded
from verify_phase7_capture import parse_capture

PINS={
    0x1a6410:(0x1a649c,"4d308fe0e012e274fc02ef2d44caa0f7a7577bd0c0e7e6fa67136cef154ef313"),
    0x1a6540:(0x1a65a0,"d770626b6f286f6d86649bf9c55dec0e74cccaca6f572a4d5e61d96dd03daa6f"),
    0x1a65a0:(0x1a6698,"bd26f5d5cefcf7da7d100775a99096b77ccb7ed2d044e3045e6d8e0fe96cde24"),
    0x1a68f0:(0x1a6a04,"1806875d03b532f009ebb5bda0f6f8bb64c5f31134ca0929416b0bf0f19a5f2d")}
# Explicit source ranges include orphan-listed and switch-case instructions;
# catalogue function-entry membership is never mistaken for complete coverage.
SUPPORT=[(0x19f460,0x19f4cc),(0x19f510,0x19f548),(0x1a3480,0x1a351c),
         (0x1a3520,0x1a3584),(0x1aa120,0x1aa14c),(0x1a4f00,0x1a4fb4),
         (0x1a3120,0x1a3128),(0x1a3130,0x1a3138),(0x1cdfd0,0x1ce02c),
         (0x1ce080,0x1ce0f4),(0x1c9300,0x1c931c),(0x1c9320,0x1c9328),
         (0x1b9cf0,0x1b9d2c),(0x1b9ea4,0x1b9eb4),(0x1ba0f4,0x1ba108),(0x2209a0,0x2209ac)]

def sha(data):return hashlib.sha256(data).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("--output",type=Path,required=True);ap.add_argument("--capture-root",type=Path);args=ap.parse_args()
    fork=Path(__file__).resolve().parents[2];parent=fork.parents[2];out=args.output.resolve()
    if out.exists() or not out.is_relative_to((fork/"build/oracle-telemetry").resolve()):raise ValueError("new private telemetry output required")
    scope=json.loads((fork/"research/runs/renderer-scope.json").read_bytes())
    raw=(parent/"research/inputs/SLUS_214.10").read_bytes()
    if sha(raw)!=rtti.ELF_SHA:raise ValueError("ELF identity differs")
    loads=rtti.segments(raw)
    def read(va,size):
        for s in loads:
            d=va-s["vaddr"]
            if 0<=d and d+size<=s["filesz"]:return raw[s["offset"]+d:s["offset"]+d+size]
        raise ValueError("requested range is not file-backed")
    listing=parent/scope["bounded_inputs"]["ghidra_reproduction"]["private_listing"]["path"]
    if sha(listing.read_bytes())!=scope["bounded_inputs"]["ghidra_reproduction"]["private_listing"]["sha256"]:raise ValueError("EE listing identity differs")
    all_ranges=list((a,b)for a,(b,_)in PINS.items())+SUPPORT;joined={}
    for row in bounded.csv_rows(listing):
        pc=int(row["address"],16)
        if any(a<=pc<b for a,b in all_ranges):
            if read(pc,len(bytes.fromhex(row["bytes"])))!=bytes.fromhex(row["bytes"]):raise ValueError("original listing word differs")
            joined[pc]=row
    ranges=[]
    for a,b in all_ranges:
        if any(pc not in joined for pc in range(a,b,4)):raise ValueError(f"incomplete original listing coverage {a:08x}")
        data=read(a,b-a)
        if a in PINS and sha(data)!=PINS[a][1]:raise ValueError("camera body pin differs")
        ranges.append({"begin":f"{a:08x}","end_exclusive":f"{b:08x}","bytes":b-a,"sha256":sha(data),"listing":[joined[pc]for pc in range(a,b,4)]})
    table=struct.unpack("<6I",read(0x5fce90,24))
    if table!=(0x1a696c,0x1a6978,0x1a69a8,0x1a69b4,0x1a6964,0x1a6964):raise ValueError("device fallback table differs")
    if struct.unpack("<I",read(0x5fd160+10*4,4))[0]!=0x1b9ea4:raise ValueError("actual delegate request10 table differs")
    rows=list(csv.DictReader((fork/"research/ledger/functions.csv").read_text("utf-8-sig").splitlines()))
    starts={int(r["address"],16)for r in rows if r["module"]=="SLUS_214.10"}
    graph=bounded.direct_calls(listing.parent,read,starts)
    callers=[{"callee":f"{callee:08x}","caller":f"{caller:08x}","site":f"{site:08x}","jal_delay_bytes_le":read(site,8).hex()}
             for (caller,callee),sites in graph.items()if callee in PINS for site in sites]
    mutations=[]
    for a,(b,pin)in PINS.items():
        for offset in (0,b-a-4):
            altered=bytearray(read(a,b-a));altered[offset]^=1
            if sha(altered)==pin:raise AssertionError("mutated body accepted")
            mutations.append({"target":f"{a:08x}","offset":offset,"rejected":True})
    captures=[]
    if args.capture_root:
        for path in sorted(args.capture_root.resolve().rglob("cap_001a*.bin")):
            data=path.read_bytes();pc=struct.unpack_from("<I",data,8)[0]
            if pc not in PINS:continue
            producer_path=path.parent/"RESULT.json"
            producer=json.loads(producer_path.read_bytes())
            if not isinstance(producer.get("captures"),list) or producer.get("exit_code")!=0 or producer.get("completed_recording_length") is not True or producer.get("parity")!="SAME_TO_END" or producer.get("observed_end_frame")!=producer.get("end"):
                raise ValueError("producer replay receipt is not same-to-end")
            receipt=next((row for row in producer["captures"] if row.get("file")==path.name),None)
            if not receipt or receipt.get("sha256")!=sha(data) or receipt.get("bytes")!=len(data):raise ValueError("capture identity differs from producer receipt")
            parsed=parse_capture(data);cpu=data[36:1492];pos=36+1456+(32<<20)+(16<<10);after=data[pos:pos+1456];ram=data[1492:1492+(32<<20)]
            u=lambda address:struct.unpack_from("<I",ram,address)[0]
            ra=struct.unpack_from("<I",cpu,31*16)[0];site=(ra-8)&0xffffffff;word=int.from_bytes(read(site,4),"little")
            if (struct.unpack_from("<I",after,548)[0]!=ra or cpu[29*16:30*16]!=after[29*16:30*16] or word>>26!=3 or ((word&0x3ffffff)<<2)!=pc):raise ValueError("partial/unproved capture boundary")
            entry={"path":str(path.relative_to(fork)),"sha256":sha(data),"pc":parsed["pc"],"frame":parsed["frame"],"source_jal_delay_le":read(site,8).hex(),"ordinary_return_full_sp":True,
                   "producer_receipt_sha256":sha(producer_path.read_bytes()),"producer_replay":{"begin":producer["begin"],"end":producer["end"],"observed_end_frame":producer["observed_end_frame"],"parity":producer["parity"]},
                   "args64":[f"{struct.unpack_from('<Q',cpu,i*16)[0]:016x}"for i in range(4,9)],"actual_callee_bindings":{f"{a:08x}":f"{u(a):08x}"for a in (0x69f3d4,0x69f3d8,0x69f388,0x69f3c8)}}
            if pc==0x1a68f0:entry["device_callback"]=f"{u(struct.unpack_from('<I',cpu,4*16)[0]+4):08x}"
            plugin_nodes=[];node=u(0x5a3640);seen=set()
            while node:
                if node in seen or len(seen)>256:raise ValueError("cyclic/unbounded plugin list")
                seen.add(node);plugin_nodes.append({"node":f"{node:08x}","offset":u(node),"size":u(node+4),"construct":f"{u(node+32):08x}","destroy":f"{u(node+36):08x}"});node=u(node+48)
            entry["plugin_initialization_list"]=plugin_nodes;captures.append(entry)
    result={"schema":"mkar.camera-operations-original-proof.v1","input_pins":{"elf":sha(raw),"listing":sha(listing.read_bytes()),"tool":sha(Path(__file__).read_bytes())},"load_segments":loads,"ranges":ranges,"direct_callers":callers,
        "fallback_table_bytes_le":read(0x5fce90,24).hex(),"device_request10_table_word_le":read(0x5fd160+40,4).hex(),"negative_body_mutations":mutations,"captures":captures,
        "scope":"Original source/ABI and ordinary-return capture identities. Constructed boundary composition and actual typed-field checks are separate from full CPU/RAM/function equivalence. No ownership promotion or native credit.",
        "open_boundaries":["arbitrary device request callback cases","populated plugin callback execution beyond selected source-bound cohort","full source heap accounting/diagnostics versus native lifetime normalization","complete numeric equivalence beyond explicit tested inputs","production owner integration"]}
    out.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps({"output":str(out),"sha256":sha(out.read_bytes()),"target_functions":4,"joined_ranges":len(ranges),"captures":len(captures),"negative_body_mutations":len(mutations)}))
if __name__=="__main__":main()
