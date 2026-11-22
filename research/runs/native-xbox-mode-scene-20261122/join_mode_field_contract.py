"""Join the resolved field, presets and complete startup words to their original images."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
prior=ROOT/"research/runs/parallel-saved-profile-child-20261122"
elf=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
xbe=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
elfid=json.loads((ROOT/"research/runs/native-mode-frame-startup-20261122/source-01.original-heads.json").read_text())
xid=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
assert hashlib.sha256(elf).hexdigest()==elfid["elf_sha256"]
assert hashlib.sha256(xbe).hexdigest()==xid["xbe_sha256"]
def mapped(image,sections,at,n,xbox=False):
 keys=("virtual_address","file_size","file_offset") if xbox else ("p_vaddr","p_filesz","p_offset")
 v,z,o=keys;matches=[s for s in sections if s[v]<=at and at+n<=s[v]+s[z]];assert len(matches)==1
 s=matches[0];offset=s[o]+at-s[v]
 return image[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,section=s.get("name",s.get("index")))
ps2={}
previous=json.loads((prior/"SOURCE_JOIN.json").read_text())["functions"]
for name in ["source-08/002460b0.asm","source-07/00245cb0.asm",
             "source-09/00241210.asm","source-08/0034db70.asm","source-08/0034dc40.asm"]:
 path=prior/name;rows=[r.split("\t",2) for r in path.read_text().splitlines()]
 old=previous[name];at=int(old["begin"],16);end=int(old["end_exclusive"],16)
 raw,m=mapped(elf,elfid["pt_load"],at,end-at)
 listed={int(r[0],16):r[1] for r in rows};assert len(listed)==len(rows)==old["listed_words"]
 assert set(range(at,end,4))-set(listed)=={int(v,16) for v in old["omitted"]},name
 for pc,text in listed.items():assert raw[pc-at:pc-at+4]==bytes.fromhex(text),(name,hex(pc))
 for pc,value in old["omitted"].items():
  actual=raw[int(pc,16)-at:int(pc,16)-at+4]
  assert actual.hex()==value["bytes"]=="00000000" and "NOP" in value["decode"],(name,pc)
 assert hashlib.sha256(raw).hexdigest()==old["span_sha256"]
 ps2[name]=dict(mapping=m,listed_words=len(rows),full_words=len(raw)//4,bytes_hex=raw.hex(),
   reused_omitted_words=old["omitted"],listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
   prior_measured_listing_sha256=old["listing_sha256"])
def word(at):return struct.unpack("<I",mapped(elf,elfid["pt_load"],at,4)[0])[0]
def imm(w):return (w&65535)-65536 if w&32768 else w&65535
def address(lui,addiu):return ((word(lui)&65535)<<16)+imm(word(addiu))
child0=address(0x2460b4,0x2460bc);child1=address(0x2461bc,0x2461c4)
names=[address(0x2462cc,0x2462d4),address(0x2462dc,0x2462e4)]
motor=[address(0x2462e8,0x2462f0),address(0x2462f4,0x2462fc)]
presets=[address(0x246300,0x246308),address(0x24630c,0x246314)]
assert [names[i]-c for i,c in enumerate([child0,child1])]==[8,8]
assert [motor[i]-c for i,c in enumerate([child0,child1])]==[0x8f8,0x8f8]
assert [presets[i]-c for i,c in enumerate([child0,child1])]==[0x238,0x238]
assert 0x238+6*0x120==0x8f8
data=[]
for image,sections,at,xbox in [(elf,elfid["pt_load"],0x5be100,False),(xbe,xid["sections"],0x54cca0,True)]:
 raw,m=mapped(image,sections,at,16*12,xbox)
 values=[struct.unpack_from("<I",raw,i*12)[0] for i in range(16)]
 assert values==[1<<i for i in range(16)]
 data.append(dict(mapping=m,bytes_hex=raw.hex(),action_masks=values))
tables=[]
for at,expected in [(0x104b68,[0x104afa,0x104b14,0x104b2c,0x104b46]),
                    (0x104c08,[0x104b9a,0x104bb4,0x104bcc,0x104be6])]:
 raw,m=mapped(xbe,xid["sections"],at,16,True);values=list(struct.unpack("<4I",raw))
 # Targets must match saved branch blocks, not a decompiler switch variable.
 assert values==expected,(hex(at),values)
 tables.append(dict(mapping=m,bytes_hex=raw.hex(),targets=[hex(v) for v in values]))
with (RECORD/"MODE_FIELD_CONTRACT.json").open("x",newline="\n") as f:
 json.dump(dict(elf_sha256=elfid["elf_sha256"],xbe_sha256=xid["xbe_sha256"],ps2_spans=ps2,
   child_addresses=[hex(child0),hex(child1)],native_projection=dict(name="known_values.empty_name",
     motor_counters="statistics.mode_pairs[2]",motor_results="statistics.timed_results[0..4]",
     presets="named_presets[0..5]"),source_aliases=dict(names=list(map(hex,names)),
     motor=list(map(hex,motor)),presets=list(map(hex,presets))),action_mask_tables=data,
   xbox_counter_switch_tables=tables,scope="Source-derived field/default/init contract; best-time arithmetic and whole-race execution not adopted",
   numerical_gate="OPEN",hd_gate="OPEN"),f,indent=2);f.write("\n")
print("Joined full PS2 initialization/field/preset spans, signed alias addresses, both mask tables and Xbox counter switches",flush=True)
