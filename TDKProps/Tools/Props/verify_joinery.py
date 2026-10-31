"""Regression checks for fitted construction joints, not just watertight primitives."""
from pathlib import Path
from collections import Counter
import importlib.util
import json
import math
import sys
import hou

P=Path(__file__).resolve().parents[2];OUT=P/'RenderTests/BuildingJoints';OUT.mkdir(parents=True,exist_ok=True)
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
M=module('joinery_medieval',P/'SourceAssets/Houdini/Medieval/build_hda.py')
G=module('joinery_gothic',P/'SourceAssets/Houdini/Gothic/build_hda.py')
def topology(mesh):
    count=Counter()
    for face in mesh.faces:
        points=[tuple(round(x,7) for x in mesh.vertices[i]) for i in face]
        for a,b in zip(points,points[1:]+points[:1]):count[tuple(sorted((a,b)))]+=1
    return {'boundary_edges':sum(n==1 for n in count.values()),'nonmanifold_edges':sum(n>2 for n in count.values())}
report={'status':'running','runtime':hou.applicationVersionString(),'tests':[]}
for quality in ('low','normal','high'):
    cfg=M.normalize();cfg['parameters']['mesh_quality']=quality
    mesh=M.Mesh(cfg);outline=[(-.5,0),(-.5,1),(-.4,1.17),(0,1.3),(.4,1.17),(.5,1),(.5,0)]
    M.opening_frame(mesh,lambda x,y,z:(x,y,z),outline,(0,0,1),name='frame')
    stats=topology(mesh);assert not any(stats.values()),stats
    for coords in mesh.uvs:
        for i in range(1,len(coords)-1):
            a,b,c=coords[0],coords[i],coords[i+1]
            assert abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))>1e-10,'Collapsed frame UV causes tangent shading wedges'
    assert len(set(mesh.names))==1,'Opening surround split into abutting parts'
    geo=hou.Geometry();M.write_hou_geometry(mesh,geo)
    assert min(p.normal().dot(hou.Vector3(v.attribValue('N'))) for p in geo.prims() for v in p.vertices())>.8
    report['tests'].append(dict(kind='continuous_opening_frame',quality=quality,**stats))
mesh=M.Mesh(M.normalize());mesh.beam((0,0,0),(0,2,0),.2,name='post')
assert all(abs(mesh.vertices[i][1])<1e-8 or abs(mesh.vertices[i][1]-2)<1e-8 for f in mesh.faces for i in f),'Bearing ends rounded back from their contact plane'
assert not any(topology(mesh).values())
for face,uv in zip(mesh.faces,mesh.uvs):
    if max(mesh.vertices[v][1] for v in face)-min(mesh.vertices[v][1] for v in face)>1:
        for a,b in zip(range(len(face)),range(1,len(face))):
            assert abs((uv[b][0]-uv[a][0])-(mesh.vertices[face[b]][1]-mesh.vertices[face[a]][1]))<1e-7
report['tests'].append({'kind':'square_bearing_ends_and_longitudinal_grain','status':'passed'})
mesh=M.Mesh(M.normalize());M.fitted_brace(mesh,lambda x,y,z:(x,y,z),.1,1.9,.14,2.47,(0,0,1))
assert mesh.faces and not any(topology(mesh).values())
lo=[min(p[i] for p in mesh.vertices) for i in range(3)];hi=[max(p[i] for p in mesh.vertices) for i in range(3)]
assert abs(lo[0]-.085)<1e-6 and abs(lo[1]-.125)<1e-6 and abs(hi[0]-1.915)<1e-6 and abs(hi[1]-2.485)<1e-6,(lo,hi)
report['tests'].append({'kind':'brace_bearings_cut_to_frame','bounds':[lo,hi]})
model=G.Mesh(G.normalize()['parameters'],101);G.course_ring(model,0,2,0,6,8,.22,.17,'floor_course')
stats=topology(model);assert not any(stats.values()),stats
assert len(set(model.names))==1,'Course has separate corner caps'
report['tests'].append(dict(kind='gothic_moulding_continuous_corners',**stats))
sys.path.insert(0,str(P/'SourceAssets/Houdini/Gothic'))
import native_network as N
exec(compile((P/'SourceAssets/Houdini/Gothic/production_geometry.py').read_text(),'joint_production','exec'),N.__dict__)
obj=hou.node('/obj').createNode('geo','joint_native_tests',run_init_scripts=False)
for width,height in ((1.4,2.7),(2.8,3.1),(3.5,4.),(8.,9.)):
    parent=obj.createNode('subnet','panel')
    result=N.p_panel(parent,{'width':width,'height':height,'opening':min(1.65,width*.6),'thickness':.35,'door':False,'bottom':.05,'profile':'lancet','detail':.6})
    result.cook(force=True);assert not result.errors(),result.errors();geo=result.geometry()
    arch=[v.point().position() for p in geo.prims() if p.attribValue('name')=='continuous_archivolt' for v in p.vertices()]
    sill=[v.point().position() for p in geo.prims() if p.attribValue('name')=='sloping_sill' for v in p.vertices()]
    assert arch and sill
    gap=min(v[1] for v in arch)-max(v[1] for v in sill)
    clearance=height-max(v[1] for v in arch)
    assert gap<-.005,(width,height,'floating jamb',gap)
    assert clearance>.25,(width,height,'arch collides with course',clearance)
    report['tests'].append({'kind':'native_gothic_jamb_bedding_and_headroom','width':width,'height':height,'bedding_overlap_m':-gap,'headroom_m':clearance})
report['status']='passed';(OUT/'joint_regressions.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
