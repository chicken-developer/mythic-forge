"""Verify roof shell continuity and agreement with assembly envelopes."""
import json,hashlib
from collections import Counter
from pathlib import Path
import build_hda as B
R=Path(__file__).resolve().parent
rows=[]
for n in [8,12]:
 for w,h in [(3.,10.),(7.5,9.),(4.5,2.)]:
  m=B.Mesh(B.BASELINE['parameters'],42);B.enclosed_spire(m,0,0,0,w,h,'test',n)
  edges=Counter();skin=[]
  for f,name in zip(m.faces,m.names):
   if not name.endswith('_weatherproof_skin'):continue
   pts=[tuple(round(v,8) for v in m.vertices[i]) for i in f];skin.extend(pts)
   for a,b in zip(pts,pts[1:]+pts[:1]):edges[tuple(sorted((a,b)))]+=1
  boundary=[e for e,count in edges.items() if count==1]
  assert len(boundary)==n and all(abs(p[1])<1e-7 for e in boundary for p in e),'Roof must close everywhere except its bearing perimeter'
  assert all(c in [1,2] for c in edges.values())
  volumes=B.spire_envelopes(0,0,0,w,h,n,'test')
  assert all(any(all(B.plane_value(pl,p)>=-1e-6 for pl in v['planes']) for v in volumes) for p in skin),'Roof/envelope mismatch'
  assert not any('diagonal' in name for name in m.names)
  rows.append(dict(sides=n,width=w,height=h,closed_above_bearing=True,envelope_agrees=True))
(R/'results/spire_validation.json').write_text(json.dumps(dict(generator_sha256=hashlib.sha256((R/'build_hda.py').read_bytes()).hexdigest(),checks=rows),indent=2))
print('PASS: 6 spire shell and envelope cases')
