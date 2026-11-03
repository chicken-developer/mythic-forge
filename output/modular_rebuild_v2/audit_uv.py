"""Measure actual vertex UV edge density on cached Houdini cooks."""
import json
import collections
import numpy as np
import hou
from hda_builder.evidence import ROOT,write_json

def measure(path):
    g=hou.Geometry();g.loadFromFile(str(path))
    xyz=np.asarray(g.pointFloatAttribValues('P')).reshape(-1,3)
    uv=np.asarray(g.vertexFloatAttribValues('uv')).reshape(-1,3)
    samples=collections.defaultdict(list)
    for p in g.prims():
        vertices=p.vertices()
        if len(vertices)<3:continue
        points=xyz[[v.point().number() for v in vertices]];tex=uv[[v.linearNumber() for v in vertices],:2]
        lengths=np.linalg.norm(np.roll(points,-1,axis=0)-points,axis=1)
        tex_lengths=np.linalg.norm(np.roll(tex,-1,axis=0)-tex,axis=1)
        ratios=tex_lengths[lengths>1e-5]/lengths[lengths>1e-5]
        samples[p.attribValue('material_family')].extend(ratios.tolist())
    return {family:{'edge_samples':len(values),'min':float(np.min(values)),
                    'p05':float(np.percentile(values,5)),'median':float(np.median(values)),
                    'p95':float(np.percentile(values,95)),'max':float(np.max(values))} for family,values in samples.items() if values}

manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
records=[json.loads((ROOT/p).read_text(encoding='utf-8')) for p in ['datasets/baseline.json']+manifest['datasets']]
rows=[]
for record in records:
    ident=record['variant_id'];row={'variant_id':ident,'after':measure(ROOT/f'geometry/after/{ident}.bgeo.sc')}
    if ident=='baseline':row['before']=measure(ROOT/f'geometry/before/{ident}.bgeo.sc')
    rows.append(row);print('UV_DENSITY',ident,flush=True)
write_json(ROOT/'results/uv_density.json',{'unit':'UV units per metre; tile scale 1.0','records':rows,
           'scope':'Local edge-length sampling; irregular seams/trim orientation still need visual checker review.'})
