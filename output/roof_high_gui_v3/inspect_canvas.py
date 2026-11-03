import hou,numpy as np,json
from pathlib import Path
root=Path(__file__).resolve().parent;g=hou.Geometry();g.loadFromFile(str(root/'geometry/10_normal_house_c_canopy_low.bgeo.sc'))
out={}
for family in ('canvas','wood'):
    faces=[p for p in g.prims() if p.attribValue('material_family')==family and 'awning' in p.attribValue('name')]
    pts=np.array([list(pt.position()) for p in faces for pt in p.points()])
    out[family]={'names':sorted({p.attribValue('name') for p in faces}),'count':len(faces),'min':pts.min(axis=0).tolist() if pts.size else [],'max':pts.max(axis=0).tolist() if pts.size else []}
sample=next(p for p in g.prims() if p.attribValue('material_family')=='canvas' and 'awning' in p.attribValue('name'))
out['sample']={a.name():sample.attribValue(a) for a in g.primAttribs()}
out['point']={a.name():sample.points()[0].attribValue(a) for a in g.pointAttribs() if a.name()!='P'}
out['vertex']={a.name():sample.vertices()[0].attribValue(a) for a in g.vertexAttribs()}
print(json.dumps(out,default=str),flush=True)
