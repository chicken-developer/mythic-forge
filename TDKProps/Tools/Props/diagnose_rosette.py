from pathlib import Path
from collections import Counter,defaultdict
import json
import hou
P=Path(__file__).resolve().parents[2]
hou.hda.installFile(str(P/'SourceAssets/Houdini/Gothic/gothic_building.hda'))
obj=hou.node('/obj').createNode('geo','diagnostic',run_init_scripts=False)
asset=obj.createNode('quynh::gothic_building::3.0','generator');asset.cook(force=True)
geo=asset.geometry();groups=defaultdict(list)
for prim in geo.prims():
    name=prim.attribValue('name')
    if 'rose_rim' in name or 'rose_centre' in name:groups[name].append(prim)
out={}
for name,prims in groups.items():
    points=[v.point().position() for p in prims for v in p.vertices()]
    lo=[min(v[i] for v in points) for i in range(3)];hi=[max(v[i] for v in points) for i in range(3)]
    flat=[]
    for prim in prims:
        positions=[v.point().position() for v in prim.vertices()]
        if max(p[2] for p in positions)-min(p[2] for p in positions)<1e-7:
            flat.append({'z':round(positions[0][2],8),'winding':list(prim.normal()),'normals':[list(v.attribValue('N')) for v in prim.vertices()]})
    cx=(lo[0]+hi[0])*.5;cy=(lo[1]+hi[1])*.5;caps=0
    for prim in prims:
        positions=[v.point().position() for v in prim.vertices()];middle=sum(positions,hou.Vector3())/len(positions)
        tangent=hou.Vector3(-(middle[1]-cy),middle[0]-cx,0).normalized()
        caps+=abs(prim.normal().dot(tangent))>.08
    out[name]={'triangles':len(prims),'lo':lo,'hi':hi,'internal_segment_caps':caps,'flat_z':dict(Counter(str(p['z']) for p in flat)),
               'flat_bevel_normal_samples':[p for p in flat if min(abs(v[2]) for v in p['normals'])<.99][:10]}
Path('E:/Repos/mythic-forge/TDKProps/Saved/PropsDetail/rosette_diagnostic.json').write_text(json.dumps(out,indent=2))
assert all(row['internal_segment_caps']==0 for row in out.values()),out
Path('E:/Repos/mythic-forge/TDKProps/RenderTests/PropsDetail/rosette_native.json').write_text(json.dumps({'status':'passed','rings':out},indent=2))
print(json.dumps(out,indent=2))
