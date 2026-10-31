"""Exterior surface and attachment validation (stdlib only)."""
import json,math,time
from pathlib import Path
import build_hda as B

def polygon_area(poly):
    if len(poly)<3:return 0.
    a=poly[0];area=0.
    for i in range(1,len(poly)-1):area+=math.sqrt(sum(x*x for x in B.cross(B.sub(poly[i],a),B.sub(poly[i+1],a))))*.5
    return area

def intersection_area(poly,vol):
    lo,hi=vol['bounds']
    if any(max(p[j] for p in poly)<=lo[j]+1e-7 or min(p[j] for p in poly)>=hi[j]-1e-7 for j in range(3)):return 0.
    for plane in vol['planes']:
        if max(B.plane_value(plane,p) for p in poly)<=1e-7:return 0.
        poly=B.clip3(poly,plane)
        if not poly:return 0.
    return polygon_area(poly)

def check(m,c):
    scale=c['parameters']['global_scale'];groups=getattr(m,'assembly_volumes',{});bad=[];tested=0
    for idx,f in enumerate(m.faces):
        name=m.names[idx];own=name.split('/')[0]
        if own=='interior':continue # intentionally occupied space, not exterior envelope
        if c['parameters']['building_type']=='townhouse':
            if not name.startswith('town_roof'):continue
            cuts=groups.get('dormer_cutters',[])
        else:cuts=[v for label,vs in groups.items() if label!=own for v in vs]
        poly=[tuple(x/scale for x in m.vertices[j]) for j in f]
        for v in cuts:
            tested+=1;area=intersection_area(poly,v)
            if area>1e-6:bad.append(dict(part=name,volume=v['label'],area=area))
    return dict(surface_volume_tests=tested,residual_intrusions=len(bad),examples=bad[:8],attachment_checks=getattr(m,'attachment_checks',{}),trim_stats=getattr(m,'assembly_checks',[]))
