"""Verify actual HDA recooks from an isolated directory, without source imports."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import sys
import time
import hou
import numpy as np
from light_layout import sources as light_sources

P=Path(__file__).resolve().parents[2];R=P/'RenderTests/PropsDetail';R.mkdir(parents=True,exist_ok=True)
ap=argparse.ArgumentParser();ap.add_argument('family',choices=['Medieval','Gothic']);ap.add_argument('--ids',nargs='*');args=ap.parse_args()
family=args.family
isolated=P/'Saved/PropsDetail/Portable'/family;isolated.mkdir(parents=True,exist_ok=True)
source=P/'SourceAssets/Houdini'/family/(family.lower()+'_building.hda')
target=isolated/source.name;shutil.copy2(source,target);os.chdir(isolated)
sys.path=[p for p in sys.path if 'HoudiniWorkspace' not in p and 'SourceAssets/Houdini' not in p]
hou.hda.installFile(str(target));definition=hou.hda.definitionsInFile(str(target))[0];definition.setIsPreferred(True)
obj=hou.node('/obj').createNode('geo','portable_verify',run_init_scripts=False)
asset=obj.createNode(definition.nodeTypeName(),'generator')
records=json.loads(definition.sections()['Presets.json'].contents()) if family=='Medieval' else asset.hdaModule().PRESETS
report={'family':family,'runtime':hou.applicationVersionString(),'source_imports':False,'status':'running','hda_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'records':[]}
for ident,cfg in records.items():
    if ident=='baseline' or (args.ids and ident not in args.ids):continue
    start=time.monotonic();asset.parm('building_preset').set(ident)
    if family=='Medieval':
        for k,v in cfg['parameters'].items():asset.parm(k).set(int(v) if isinstance(v,bool) else v)
        asset.setParms(dict(layout_seed=cfg['seeds']['layout'],decoration_seed=cfg['seeds']['decoration']))
        asset.parm('mesh_quality').set('normal')
    else:
        asset.parm('mesh_quality').set('game');asset.hdaModule().apply_preset({'node':asset})
    asset.cook(force=True);errors=[(n.path(),n.errors()) for n in (asset,*asset.allSubChildren()) if n.errors()];assert not errors,errors[:3]
    geo=asset.geometry();normals=np.asarray(geo.vertexFloatAttribValues('N')).reshape(-1,3)
    assert np.isfinite(normals).all() and np.max(abs(np.linalg.norm(normals,axis=1)-1))<1e-3
    names=geo.primStringAttribValues('name');actual_sources=light_sources(geo);lights=len(actual_sources);assert lights>0,(ident,'lights missing')
    expected=json.loads((P/'Saved/PropsDetail'/family/'normal'/(ident+'.json')).read_text())
    # HDA output is polygonal for Medieval and triangulated native SOPs for Gothic.
    triangle_count=sum(len(prim.vertices())-2 for prim in geo.prims())
    if family=='Gothic':assert triangle_count==expected['triangles'],(ident,triangle_count,expected['triangles'])
    assert lights==len(expected['interior_lights']),(ident,lights,len(expected['interior_lights']))
    max_error=max(abs(source['position_m'][i]-expected['source_origin_m'][i]-target['position_m'][i]) for source,target in zip(actual_sources,expected['interior_lights']) for i in range(3))
    assert max_error<1e-5,(ident,'light layout changed',max_error)
    if family=='Gothic':assert not any(n.type().name()=='python' for n in asset.allSubChildren())
    report['records'].append({'id':ident,'points':len(geo.points()),'polygons':len(geo.prims()),'lights':lights,'maximum_light_position_error_m':max_error,'connected_light_sources_checked':True,'invalid_normals':0,'seconds':round(time.monotonic()-start,3)})
    (R/(family.lower()+'_portable_hda.json')).write_text(json.dumps(report,indent=2))
    print('PORTABLE',family,ident,lights,flush=True)
report['status']='passed';(R/(family.lower()+'_portable_hda.json')).write_text(json.dumps(report,indent=2))
