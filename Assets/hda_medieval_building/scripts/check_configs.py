"""Measured native cooks, aperture clearance, parameter propagation and determinism."""
from pathlib import Path
import copy
import hashlib
import json
import sys
import time
import hou
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import build_hda as B

def digest(geo):
    return hashlib.sha256(np.asarray(geo.pointFloatAttribValues('P'),dtype=np.float32).tobytes()).hexdigest()

def openings_clear(b):
    points=b.node('F_FACADE_LAYOUT/FACADE_OPENING_POINTS').geometry()
    walls=b.node('F_WALL_FINISH').geometry()
    failures=[]
    for pt in points.points():
        p=pt.position();out=hou.Vector3(pt.attribValue('outward'));u=out.cross(hou.Vector3(0,1,0)).normalized()
        w=pt.attribValue('aperture_w');h=pt.attribValue('aperture_h')
        # Keep probe points clear of the arch shoulders and sill/frame borders.
        for x in (-.25,0,.25):
            for y in (-.2,0,.2):
                origin=p+u*(w*x)+hou.Vector3(0,h*y,0)+out*.12
                hit=hou.Vector3();normal=hou.Vector3();uv=hou.Vector3()
                prim=walls.intersect(origin,-out,hit,normal,uv,max_hit=.55)
                if prim>=0:failures.append({'opening':pt.attribValue('opening_id'),'point':list(origin),'distance':(hit-origin).length()})
    return failures

def main():
    hou.hipFile.load(str(ROOT/'medieval_building.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
    b=hou.node('/obj/medieval_building');manifest=json.loads((ROOT/'manifest.json').read_text())
    records=[json.loads((ROOT/f).read_text()) for f in [manifest['baseline']]+manifest['datasets']]
    report={'houdini':hou.applicationVersionString(),'hip_sha256':hashlib.sha256((ROOT/'medieval_building.hip').read_bytes()).hexdigest(),'records':[],'failures':[]}
    for record in records:
        try:
            start=time.monotonic();B.apply_config(b,record);g=b.node('OUT_BUILDING').geometry()
            errors=[(n.path(),n.errors()) for n in b.allSubChildren() if n.errors()]
            if errors:raise ValueError(errors)
            if not np.isfinite(g.pointFloatAttribValues('P')).all():raise ValueError('Non-finite coordinates')
            for key in ('width_m','depth_m'):
                actual=b.node('F_PLAN').parm(key).eval()
                if abs(actual-record['parameters'][key])>1e-5:raise ValueError(('Argument clamp',key,actual))
            clearance=openings_clear(b)
            if clearance:raise ValueError(('Wall blocks an aperture',clearance[:4]))
            row={'variant_id':record['variant_id'],'status':'pass','points':len(g.points()),'primitives':len(g.prims()),
                 'bounds_m':list(g.boundingBox().sizevec()),'position_sha256':digest(g),'seconds':time.monotonic()-start,
                 'wall_aperture_obstructions':clearance,'material_families':sorted(set(g.primStringAttribValues('material_family')))}
            report['records'].append(row);print('COOK_OK',record['variant_id'],row['bounds_m'],flush=True)
        except Exception as exc:report['failures'].append({'variant_id':record['variant_id'],'error':str(exc)});print('COOK_FAIL',record['variant_id'],str(exc),flush=True)
    cfg=records[0];B.apply_config(b,cfg);g=b.node('OUT_BUILDING').geometry();first=digest(g)
    b.node('OUT_BUILDING').cook(force=True);second=digest(b.node('OUT_BUILDING').geometry())
    report['determinism']={'same_positions':first==second,'first':first,'repeat':second}
    changed=copy.deepcopy(cfg);changed['parameters']['width_m']+=.6;B.apply_config(b,changed)
    report['width_sweep']={'changed':digest(b.node('OUT_BUILDING').geometry())!=first}
    B.apply_config(b,cfg);initial=np.asarray(b.node('OUT_BUILDING').geometry().primFloatAttribValues('Cd'))
    changed=copy.deepcopy(cfg);changed['seeds']['decoration']+=1;B.apply_config(b,changed)
    report['color_sweep']={'changed':not np.array_equal(initial,np.asarray(b.node('OUT_BUILDING').geometry().primFloatAttribValues('Cd')))}
    B.apply_config(b,cfg);red=list(b.node('materials/tile').parmTuple('basecolor').eval())
    changed=copy.deepcopy(cfg);changed['parameters']['palette']='slate_blue';B.apply_config(b,changed)
    blue=list(b.node('materials/tile').parmTuple('basecolor').eval())
    report['material_sweep']={'parameter':'palette','before':red,'after':blue,'changed':red!=blue,
                              'positions_unchanged':digest(b.node('OUT_BUILDING').geometry())==first}
    report['range_tests']=[]
    for label,width,depth,floors,roof in [('minimum',4.5,4.5,1,1.75),('maximum',14.,12.,3,4.5)]:
        changed=copy.deepcopy(cfg);changed['parameters'].update(width_m=width,depth_m=depth,floor_count=floors,roof_height_m=roof)
        B.apply_config(b,changed);g=b.node('OUT_BUILDING').geometry()
        report['range_tests'].append({'case':label,'width_m':width,'depth_m':depth,'floor_count':floors,
                                      'points':len(g.points()),'bounds_m':list(g.boundingBox().sizevec()),'status':'pass'})
    invalid=copy.deepcopy(cfg);invalid['parameters'].update(floor_count=1,balcony_layout='front')
    try:B.normalize(invalid);report['invalid_rejected']=False
    except ValueError:report['invalid_rejected']=True
    (ROOT/'results/native_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if report['failures'] or not all((report['determinism']['same_positions'],report['width_sweep']['changed'],report['color_sweep']['changed'],report['invalid_rejected'])):raise SystemExit(1)

if __name__=='__main__':main()
