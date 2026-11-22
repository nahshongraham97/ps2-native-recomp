"""Freeze0405 source and independently rounded binary32 depth expectations."""
from pathlib import Path
from fractions import Fraction
import gzip,hashlib,json,struct
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def mapped(at,n):
 ss=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(ss)==1;s=ss[0];o=s["file_offset"]+at-s["virtual_address"]
 return image[o:o+n],dict(vma=hex(at),length=n,file_offset=o,section=s["name"])
selected=[("../native-xbox-scene-frame-20261122/source-module-callbacks-09","002bb510"),
 ("../native-xbox-scene-frame-20261122/source-module-callbacks-09","002bb4b0"),
 ("source-0405-02","002bb960"),("source-0405-02","002bb920"),
 ("source-0405-03","002c04d0"),("source-0405-04","002bb870")]
functions={}
for folder,entry in selected:
 d=(R/folder).resolve();j=Path(str(d)+".join.json");row=json.loads(j.read_text())["functions"][entry]
 p=d/(entry+".asm");assert sha(p)==row["listing_sha256"]
 bounds={r.split("\t")[0]:r.split("\t")[1] for r in (d/"functions.tsv").read_text().splitlines()};cursor=int(entry,16)
 for span in row["original_spans"]:
  at=int(span["vma"],16);assert at==cursor;raw,m=mapped(at,span["length"])
  assert raw.hex()==span["bytes_hex"] and m["file_offset"]==span["file_offset"];cursor+=len(raw)
 assert cursor==int(bounds[entry],16)+1
 functions[entry]=dict(listing=p.relative_to(ROOT).as_posix(),listing_sha256=sha(p),join_sha256=sha(j),
  inclusive_range=[entry,bounds[entry]],instructions=row["instructions"],unlisted_gaps=[])
data={}
for at,n in [(0x5a6718,32),(0x4d7b54,4),(0x4d7cb8,8),(0x4d7c64,4),(0x4dd748,4),(0x50b14c,8),(0x4d7c30,4)]:
 raw,m=mapped(at,n);data[hex(at)]=dict(**m,bytes_hex=raw.hex())
def word(at):return struct.unpack("<I",mapped(at,4)[0])[0]
def exact(bits):
 sign=-1 if bits>>31 else 1;e=(bits>>23)&255;m=bits&0x7fffff;assert e!=255
 return sign*Fraction(m+(1<<23) if e else m)*(Fraction(2)**(e-127-23 if e else -149))
def round_even(q):
 n,r=divmod(q.numerator,q.denominator);return n+(r*2>q.denominator or (r*2==q.denominator and n&1))
def binary32(q,negative_zero=False):
 if not q:return 0x80000000 if negative_zero else 0
 sign=0x80000000 if q<0 else 0;q=abs(q);e=q.numerator.bit_length()-q.denominator.bit_length()
 if q<Fraction(2)**e:e-=1
 if e<-126:return sign|round_even(q*(Fraction(2)**149))
 m=round_even(q*(Fraction(2)**(23-e)))
 if m==(1<<24):m>>=1;e+=1
 assert e<=127
 return sign|((e+127)<<23)|(m-(1<<23))
def add(a,b):return binary32(exact(a)+exact(b),bool(a&b&0x80000000))
def sub(a,b):return add(a,b^0x80000000)
def mul(a,b):return binary32(exact(a)*exact(b),bool((a^b)&0x80000000))
gold=[]
for near,far in [(0,0x3f800000),(0x3f800000,0),(0xbf800000,0x3f800000),(0x3e000000,0x3f600000),
                 (0,0),(0x3f000000,0x3f000000),(0x3f000001,0x3f000002)]:
 # Source2BBA22..2BBA5D: exact rationals rounded nearest-even after EACH
 # original SSE operation, independent of the new native C++ implementation.
 margin=mul(sub(far,near),word(0x4dd748));hi=sub(far,margin);lo=add(margin,near)
 scale=mul(sub(hi,lo),word(0x50b150));product=mul(scale,word(0x50b14c))
 shift=mul(sub(add(hi,lo),product),word(0x4d7c30))
 gold.append(dict(device_near_bits=near,device_far_bits=far,scale_bits=scale,shift_bits=shift))
before={}
for n in ["runtime/include/mka/retail_renderer_core.hpp","cmake/native_game.cmake"]:
 raw=(ROOT/n).read_bytes();before[n]=sha(ROOT/n)
 with (R/(Path(n).name+".before-0405.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
receipt=dict(status="SOURCE_DERIVED_0405_CAMERA_RESOURCE_BASE_SCOPE",xbe_sha256=identity["xbe_sha256"],
 functions=functions,data=data,native_before=before,depth_cases=gold,expected_rounding="binary32 nearest-even per source SSE operation",
 selected_scope="real0405 owner; explicitly supplied depth inputs; source base-camera defaults/window update/destroy; initial empty camera registry",
 production_depth_provider="OPEN_REQUIRED_EXPLICIT_INPUT_NOT_ASSUMED_FROM_BSS_OR_UNRELATED_INDEXED_FIELDS",
 unadopted=["nonempty camera plugins","frame/raster/world attachment","sync/view/frustum derivation","begin/end device/current-camera effects","clone/clip/projection setters","renderer/game acceptance"],
 verification_scope="source-derived before native edit",owner_action="none",acceptance="OPEN")
with (R/"SOURCE_0405.json").open("x",newline="\n") as f:json.dump(receipt,f,indent=2);f.write("\n")
with (R/"depth-expected.tsv").open("x",newline="\n") as f:
 f.write("device_near_bits device_far_bits scale_bits shift_bits\n")
 for row in gold:f.write(" ".join(str(v) for v in row.values())+"\n")
print("PASS0405 full selected words/data and",len(gold),"independent exact-rational SSE expectations",flush=True)
