"""Freeze connected0406 source authority and independent gamma1/extent expectations before native edits."""
from pathlib import Path
from fractions import Fraction
import gzip,hashlib,json,struct
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
OLD=R.parent/"native-xbox-renderer-0406-20261122"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):
 with p.open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
prior=json.loads((OLD/"RECOVERY_AUTHENTICATION.json").read_text())
device=json.loads((R/"device-descriptor.data.json").read_text())["data"][0]
table=json.loads((R/"device-copy-switch.data.json").read_text())["data"][0]
assert struct.unpack("<I",bytes.fromhex(device["bytes_hex"])[:4])[0]==0x3f800000
assert struct.unpack("<23I",bytes.fromhex(table["bytes_hex"]))[4]==0x2c74ca
manual=PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/.agents/skills/ps2-recomp/resources/pdfs/EE_Core_Instruction_Set_Manual.pdf"
assert sha(manual)=="104205afc6f5a5071168d8bf693a6c8b4db9a02e1aa66eb51a828e8951f5c069"
text=PARENT/"research/doc-audit-20261118/pdf-text/104205afc6f5a5071168d8bf693a6c8b4db9a02e1aa66eb51a828e8951f5c069.txt"
assert "value with rounding toward 0" in text.read_text()
def fp(bits):
 e=bits>>23;m=bits&0x7fffff
 return Fraction((1<<23)+m if e else m)*Fraction(2)**(e-150 if e else -149)
def bound(q,up):
 lo,hi=0,0x48000000
 while lo<hi:
  mid=(lo+hi+1)//2
  if fp(mid)<=q:lo=mid
  else:hi=mid-1
 return fp(lo if not up or fp(lo)==q else lo+1)
cases=[]
for i in range(1,256):
 # Exact rational interval bounds cover either neighboring representable
 # single-precision result after each operation,no host FP mode assumption.
 low,high=bound(Fraction(i,255),False),bound(Fraction(i,255),True)
 low,high=bound(low*255,False),bound(high*255,True)
 low,high=bound(low+Fraction(1,2),False),bound(high+Fraction(1,2),True)
 assert i<=low<=high<i+1
 cases.append(dict(index=i,lower=str(low),upper=str(high),expected=i))
gold=dict(status="SOURCE_DERIVED_GAMMA_ONE_EXPECTATIONS",selected_source="PS2 1A8380 and11EA20 positive exponent1 early return; Xbox descriptor gamma1 handoff",
 tables=[list(range(256)),list(range(256))],quantization="EE CVT.W.S toward zero; original manual p356; positive normal intermediates",
 intervals=cases,non_one_gamma="OPEN_NOT_IMPLEMENTED_OR_ASSUMED_EQUIVALENT")
save(R/"gamma-one-expected.json",gold)
selected={}
for row in prior["functions"].values():
 if row["scope"]=="SOURCE_DERIVED_SELECTED_CONTRACT_WITH_RECORDED_LIMITS":
  assert sha(ROOT/row["listing"])==row["listing_sha256"];selected[row["listing"]]=row["listing_sha256"]
for folder in sorted(R.glob("source-init-*")):
 if folder.is_dir():
  for p in folder.glob("*.asm"):selected[p.relative_to(ROOT).as_posix()]=sha(p)
for entry in ["002c2b90","002c2bf0"]:
 p=ROOT/"research/runs/native-xbox-renderer-0403-20261122/source-0403-04"/(entry+".asm")
 selected[p.relative_to(ROOT).as_posix()]=sha(p)
before={}
for n in ["runtime/include/mka/retail_renderer_core.hpp","cmake/native_game.cmake"]:
 before[n]=sha(ROOT/n)
 with (R/(Path(n).name+".before-images.gz")).open("xb") as f:f.write(gzip.compress((ROOT/n).read_bytes(),mtime=0))
save(R/"SOURCE_IMPLEMENTATION.json",dict(status="SOURCE_DERIVED_0406_IMPLEMENTATION_SELECTION",
 input_sha256=prior["image_sha256"],prior_recovery_authentication_sha256=sha(OLD/"RECOVERY_AUTHENTICATION.json"),
 listings=selected,device_descriptor_sha256=sha(R/"device-descriptor.data.json"),
 device_copy_switch_sha256=sha(R/"device-copy-switch.data.json"),gamma_expected_sha256=sha(R/"gamma-one-expected.json"),
 manual_sha256=sha(manual),manual_text_sha256=sha(text),manual_reference="EE Core Instruction Set Manual v6 p356 CVT.W.S;p357 DIV.S; not generic TX79 rounding rules",
 native_before=before,selected_gamma_domain="exact positive1.0 from actual Xbox descriptor/current callers; non1 fails by name",
 callbacks="typed host callouts; forward init/reverse rollback; image birth preserves ignored registry refusal; reverse destruction; real content exists/read predicate",
 limitations=["non1 gamma numerical/oracle parity","concrete new format codecs and image plugins must supply actual implementations","raster/GPU/game acceptance"],
 implementation="NOT_YET_EDITED",acceptance="OPEN"))
print("PASS0406 source frozen;256 gamma1 entries independently bounded; actual descriptor/system copy and callback order pinned",flush=True)
