import hou
import numpy as np
import sys
from hda_builder.evidence import ROOT,write_json

def faces(path):
    g=hou.Geometry();g.loadFromFile(str(path));xyz=np.asarray(g.pointFloatAttribValues('P')).reshape(-1,3)
    result={}
    for p in g.prims():
        if p.type()!=hou.primType.Polygon:continue
        ids=[v.point().number() for v in p.vertices()];pts=xyz[ids]
        if np.linalg.norm(np.cross(pts,np.roll(pts,-1,axis=0)).sum(axis=0))*.5<1e-10:continue
        result[tuple(sorted(map(tuple,np.round(pts,5))))]={'name':p.attribValue('name'),'positions':pts.tolist()}
    return g,result

ident=sys.argv[1] if len(sys.argv)>1 else 'baseline'
a,before=faces(ROOT/f'geometry/before/{ident}.bgeo.sc');b,after=faces(ROOT/f'geometry/after/{ident}.bgeo.sc')
removed=[before[k] for k in before.keys()-after.keys()];added=[after[k] for k in after.keys()-before.keys()]
write_json(ROOT/f'results/surface_parity_{ident}.json',{'removed':removed,'added':added,'cleanup_counts':{a.name():b.attribValue(a) for a in b.globalAttribs()}})
print('REMOVED',len(removed),'ADDED',len(added),flush=True)
print(removed[:4],added[:4],flush=True)
