# Test sensitivity of two mathematical alternatives, not EE emulation.
import analyze as a
from pathlib import Path
import re,json
s=Path('runtime/src/native/retail_sound_metadata_data.inc').read_text()
words=[int(x,16) for x in re.findall(r'0x([0-9a-f]+)u',s.split('struct Category')[0])]
cats=[(int(x,16),int(y)) for x,y in re.findall(r'\{0x([0-9a-f]+)u,(\d+)\}',s)]
different=[]
for i,w in enumerate(words):
 percent=(w>>13)&127;cb,gi=cats[w>>25];results=[]
 for near in [True,False]:
  v=a.F(1)
  pct=a.quantize(a.F(percent,100),near) if percent else 0
  for rhs in [a.value(pct) if pct else a.F(0),a.value(cb),a.F(3,4)]:
   b=a.quantize(v*rhs,near);v=a.value(b) if b else a.F(0)
  results.append(int(v*16384))
 if results[0]!=results[1]:different.append({'index':i,'q14':results})
assert len(words)==9656 and not different
result={'rows':len(words),'different':different,'volume':1,'gain':'3/4','limit':'Only agreement between IEEE RNE and RTZ alternatives, not an EE result or exhaustive live-gain/caller domain.'}
(Path(__file__).parent/'DEFAULT_CORPUS_ALTERNATIVES.json').write_text(json.dumps(result,indent=2)+'\n')
print(result)
