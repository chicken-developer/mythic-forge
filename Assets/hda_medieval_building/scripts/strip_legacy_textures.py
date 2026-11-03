"""Material-only HIP cleanup with measured geometry equivalence for all configs."""
from pathlib import Path
import array
import hashlib
import json
import shutil
import sys
import hou
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
import build_hda as B

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def config_sha(record):return hashlib.sha256(json.dumps(record,sort_keys=True).encode()).hexdigest()

def geometry_sha(geo):
    h=hashlib.sha256()
    for values in (geo.pointFloatAttribValues('P'),geo.vertexFloatAttribValues('N'),geo.vertexFloatAttribValues('uv'),geo.primFloatAttribValues('Cd')):
        h.update(array.array('f',values).tobytes())
    h.update(array.array('i',[v.point().number() for p in geo.prims() for v in p.vertices()]).tobytes())
    h.update(array.array('i',[len(p.vertices()) for p in geo.prims()]).tobytes())
    for attr in ('name','material_family','shop_materialpath'):h.update(json.dumps(geo.primStringAttribValues(attr)).encode())
    return h.hexdigest()

def snapshot(b):
    return {n.path():{'type':n.type().name(),'inputs':[i.path() if i else None for i in n.inputs()],
                     'parms':{p.name():p.rawValue() for p in n.parms()}} for n in (b,*b.allSubChildren())}

def main():
    path=ROOT/'medieval_building.hip';before_sha=sha(path)
    backup=ROOT/'backup/before_basecolor_20261102';backup.mkdir(parents=True,exist_ok=True)
    if not (backup/path.name).exists():shutil.copy2(path,backup/path.name)
    hou.hipFile.load(str(path),suppress_save_prompt=True,ignore_load_warnings=True)
    b=hou.node('/obj/medieval_building');ctrl=b.node('CTRL_BUILDING')
    original={p.name():p.eval() for p in ctrl.parms()}
    manifest=json.loads((ROOT/'manifest.json').read_text())
    records=[json.loads((ROOT/p).read_text()) for p in [manifest['baseline']]+manifest['datasets']]
    hashes={}
    for rec in records:
        B.apply_config(b,rec);hashes[rec['variant_id']]={'config_sha256':config_sha(rec),'before':geometry_sha(b.node('OUT_BUILDING').geometry())}
    ctrl.setParms(original);before=snapshot(b);allowed=set();changes=[]
    for mat in b.node('materials').children():
        for p in mat.parms():
            t=p.parmTemplate();key=p.name().lower();replacement=None
            if isinstance(t,hou.ToggleParmTemplate) and ('usetexture' in key or key=='basebumpandnormal_enable'):replacement=0
            elif isinstance(t,hou.StringParmTemplate) and (key.endswith('_texture') or t.stringType()==hou.stringParmType.FileReference):replacement=''
            if replacement is not None:
                allowed.add((mat.path(),p.name()));old=p.rawValue();p.set(replacement,follow_parm_reference=False)
                if old!=p.rawValue():changes.append({'node':mat.path(),'parameter':p.name(),'before':old,'after':p.rawValue()})
    for rec in records:
        B.apply_config(b,rec);row=hashes[rec['variant_id']];row['after']=geometry_sha(b.node('OUT_BUILDING').geometry())
        if row['before']!=row['after']:raise ValueError(('Geometry changed',rec['variant_id']))
    ctrl.setParms(original);after=snapshot(b)
    if set(before)!=set(after):raise ValueError('Node graph changed')
    for key,a in before.items():
        z=after[key]
        if a['type']!=z['type'] or a['inputs']!=z['inputs'] or set(a['parms'])!=set(z['parms']):raise ValueError(('Graph changed',key))
        for p,value in a['parms'].items():
            if value!=z['parms'][p] and (key,p) not in allowed:raise ValueError(('Unexpected parameter change',key,p))
    hou.hipFile.save(str(path))
    report={'status':'pass','change_kind':'material input parameters only','before_hip_sha256':before_sha,
            'after_hip_sha256':sha(path),'parameter_changes':changes,'geometry_equivalence':hashes,
            'geometry_properties_compared':['P','vertex N','vertex uv','primitive Cd','topology','name','material_family','shop_materialpath'],
            'export_provenance':'Existing exports retain their actual original HIP hash. No export or bake is claimed for this material-only save.'}
    (ROOT/'results/basecolor_scene_equivalence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('MATERIAL_ONLY_SAVE',len(changes),'parameters;',len(hashes),'identical geometries',flush=True)

if __name__=='__main__':main()
