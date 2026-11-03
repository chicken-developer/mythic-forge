"""Bake the ten presets from the canonical native-SOP HIP to Unreal FBX."""
import collections
import hashlib
import json
from pathlib import Path
import sys
import time
import hou

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'Assets/hda_gothic_building'
OUT = ROOT / 'Assets/_render/SourceAssets/Gothic/Meshes'
OUT.mkdir(parents=True, exist_ok=True)
hou.hipFile.load(str(SOURCE / 'gothic_building.hip'), suppress_save_prompt=True, ignore_load_warnings=True)
asset = hou.node('/obj/gothic_building_v2_1/generator')
assert asset is not None
manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
records = [json.loads((SOURCE / p).read_text(encoding='utf-8')) for p in manifest['datasets']]
geoobj = hou.node('/obj').createNode('geo', '__unreal_export', run_init_scripts=False)
stash = geoobj.createNode('stash', 'baked_preset')
mat = hou.node('/mat')
for name in ('stone','trim','roof','glass','wood','metal','gold','floor','interior'):
    if mat.node('Gothic_'+name) is None:
        mat.createNode('principledshader::2.0', 'Gothic_'+name)
rop = hou.node('/out').createNode('filmboxfbx', 'export_gothic')
rop.parm('startnode').set(geoobj.path())
rop.parm('trange').set(0)
if rop.parm('exportkind'): rop.parm('exportkind').set(0)
report = {'source_hip':str(SOURCE/'gothic_building.hip'), 'source_sha256':hashlib.sha256((SOURCE/'gothic_building.hip').read_bytes()).hexdigest(), 'houdini':hou.applicationVersionString(), 'fbx_units':'centimetres', 'variants':[]}
for record in records:
    started = time.time()
    variant = record['variant_id']
    asset.parm('building_preset').set(variant)
    asset.hdaModule().apply_preset({'node':asset})
    asset.cook(force=True)
    assert not asset.errors(), asset.errors()
    geo = hou.Geometry()
    geo.merge(asset.geometry())
    bounds = geo.boundingBox()
    low, high = list(bounds.minvec()), list(bounds.maxvec())
    # This FBX ROP declares centimetres. Bake metre-to-centimetre conversion
    # into vertex positions; UVs retain their original metric texel density.
    center = hou.Vector3((.5*(low[0]+high[0]), low[1], .5*(low[2]+high[2])))
    for pt in geo.points(): pt.setPosition((pt.position()-center)*100.)
    if geo.findPrimAttrib('shop_materialpath') is None: geo.addAttrib(hou.attribType.Prim,'shop_materialpath','')
    counts = collections.Counter()
    for prim in geo.prims():
        family = prim.attribValue('material_id')
        counts[family] += 1
        prim.setAttribValue('shop_materialpath','/mat/Gothic_'+family)
        tile = 2.0 if family in ('roof','wood','glass') else 4.0
        for vertex in prim.vertices():
            uv = vertex.attribValue('uv')
            vertex.setAttribValue('uv',(uv[0]/tile,uv[1]/tile,0.))
    stash.parm('stash').set(geo)
    stash.setDisplayFlag(True); stash.setRenderFlag(True)
    target = OUT / ('SM_Gothic_'+variant+'.fbx')
    rop.parm('sopoutput').set(str(target))
    rop.render()
    assert target.exists() and target.stat().st_size > 1024
    row = {'id':variant,'label':record.get('label',variant),'file':str(target.relative_to(ROOT)).replace('\\','/'), 'size_m':[high[i]-low[i] for i in range(3)],'primitives':len(geo.prims()),'points':len(geo.points()),'triangles':sum(max(0,len(p.vertices())-2) for p in geo.prims()),'materials':dict(counts),'parameters':record['parameters'],'seeds':record['seeds'],'seconds':round(time.time()-started,2),'fbx_sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
    report['variants'].append(row)
    print(json.dumps({k:row[k] for k in ('id','triangles','materials','seconds')}),flush=True)
temporary=OUT.parent/'mesh_manifest.json.tmp'
temporary.write_text(json.dumps(report,indent=2),encoding='utf-8')
temporary.replace(OUT.parent/'mesh_manifest.json')
print('EXPORT_COMPLETE',flush=True)
