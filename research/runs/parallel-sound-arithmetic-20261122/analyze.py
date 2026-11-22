# Mathematical alternatives only; not EE instruction semantics or runtime code.
from fractions import Fraction as F
from pathlib import Path
import json, hashlib
out=Path(__file__).parent
def value(b):
 e=(b>>23)&255;m=(b&0x7fffff)|0x800000
 assert 0<e<255 and b<0x80000000
 return F(m)*F(2)**(e-150)
def quantize(x,nearest):
 if not x:return 0
 e=x.numerator.bit_length()-x.denominator.bit_length()
 if x<F(2)**e:e-=1
 scaled=x/F(2)**(e-23);q,rem=divmod(scaled.numerator,scaled.denominator)
 if nearest and (2*rem>scaled.denominator or (2*rem==scaled.denominator and q&1)):q+=1
 if q==1<<24:e+=1;q>>=1
 assert -126<=e<128
 return ((e+127)<<23)|(q-0x800000)
def run(nearest):
 a=quantize(F(100,100),nearest)
 b=quantize(value(0x39034835)*value(a),nearest)
 c=quantize(value(b)*value(0x3f266666),nearest)
 d=quantize(value(c)*value(0x3f400000),nearest)
 return {'stages':[hex(x) for x in [a,b,c,d]],'q14':int(value(d)*16384), 'last_exact_value':str(value(d))}
r={'input_volume_bits':'0x39034835','sound_index':0,'packed_word':'0x06fc8000','category_scalar_bits':'0x3f266666','live_gain_bits':'0x3f400000','rne':run(True),'rtz':run(False),'limit':'Exact rational comparison of two IEEE mathematical choices only; neither establishes EE hardware result.'}
assert r['rne']['q14']==1 and r['rtz']['q14']==0
# The last stage is either exactly2^-14 or the preceding binary32.
assert r['rne']['stages'][-1]=='0x38800000'
assert r['rtz']['stages'][-1]=='0x387fffff'
(out/'RATIONAL_RESULTS.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r,indent=2))
