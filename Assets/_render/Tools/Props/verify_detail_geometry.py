"""Regression checks for the specific circular-profile failures."""
from pathlib import Path
from collections import Counter
import importlib.util
import json
import math
import sys
import hou

P=Path(__file__).resolve().parents[2];R=P/'RenderTests/PropsDetail';R.mkdir(parents=True,exist_ok=True)
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
M=module('medieval_detail',P/'SourceAssets/Houdini/Medieval/build_hda.py')
def edges(geo):
    counts=Counter()
    for prim in geo.prims():
        vertices=[tuple(round(x,7) for x in v.point().position()) for v in prim.vertices()]
        for a,b in zip(vertices,vertices[1:]+vertices[:1]):counts[tuple(sorted((a,b)))]+=1
    return {'boundary_edges':sum(n==1 for n in counts.values()),'nonmanifold_edges':sum(n>2 for n in counts.values())}
report={'runtime':hou.applicationVersionString(),'status':'running','tests':[]}
for quality in ('low','normal','high'):
    cfg=M.normalize();cfg['parameters']['mesh_quality']=quality
    mesh=M.Mesh(cfg);mesh.tube((0,0,0),2.,.3,'stone',16,inner=1.7,name='ring')
    geo=hou.Geometry();M.write_hou_geometry(mesh,geo);stats=edges(geo)
    assert not stats['boundary_edges'] and not stats['nonmanifold_edges'],stats
    assert min(p.normal().dot(hou.Vector3(v.attribValue('N'))) for p in geo.prims() for v in p.vertices())>.95
    report['tests'].append(dict(kind='medieval_annulus',quality=quality,faces=len(geo.prims()),**stats))
mesh=M.Mesh(M.normalize());mesh.path_beam([(math.cos(i*math.pi/32),math.sin(i*math.pi/32),0.) for i in range(33)],.18,(0,0,1))
geo=hou.Geometry();M.write_hou_geometry(mesh,geo);stats=edges(geo);assert not any(stats.values()),stats
report['tests'].append(dict(kind='medieval_continuous_arch',**stats))
sys.path.insert(0,str(P/'SourceAssets/Houdini/Gothic'))
import native_network as N
G=module('gothic_detail',P/'SourceAssets/Houdini/Gothic/build_hda.py')
model=G.Mesh(G.normalize()['parameters'],101);G.ring(model,(0,0,0),(1,0,0),(0,1,0),2.,.09,'rose_rim')
assert len(set(model.names))==1 and len(model.faces)==64*8
caps=0
for face in model.faces:
    points=[model.vertices[i] for i in face];midpoint=tuple(sum(p[j] for p in points)/len(points) for j in range(3))
    tangent=G.unit((-midpoint[1],midpoint[0],0.));normal=G.unit(G.cross(G.sub(points[1],points[0]),G.sub(points[2],points[0])))
    caps+=abs(sum(a*b for a,b in zip(tangent,normal)))>.05
assert not caps,caps
report['tests'].append({'kind':'gothic_ring_survives_raw_face_transform','internal_segment_caps':caps,'faces':len(model.faces)})
exec(compile((P/'SourceAssets/Houdini/Gothic/production_geometry.py').read_text(),'production_geometry','exec'),N.__dict__)
obj=hou.node('/obj').createNode('geo','ring_regression',run_init_scripts=False)
for axis in ('xy','xz','yz'):
    N.P_LIBRARY=N.p_group(obj,'library_'+axis)
    def pos(a):
        return (2*math.cos(a),2*math.sin(a),0.) if axis=='xy' else (2*math.cos(a),0.,2*math.sin(a)) if axis=='xz' else (0.,2*math.cos(a),2*math.sin(a))
    ops=[dict(radius=.09,material='trim',name='rose_rim'+str(i),a=pos(i*math.tau/64),b=pos((i+1)*math.tau/64)) for i in range(64)]
    parent=N.p_group(obj,'ring_'+axis);outputs=N.p_compile_beams(parent,ops,'test/')
    result=N.merge_nodes(parent,'result',outputs);result.cook(force=True);assert not result.errors(),result.errors()
    geo=result.geometry();stats=edges(geo);assert not any(stats.values()),stats
    assert min(p.intrinsicValue('measuredarea') for p in geo.prims())>1e-10
    thickness=[geo.boundingBox().sizevec()[i] for i in range(3)]
    normal_axis={'xy':2,'xz':1,'yz':0}[axis]
    assert .179<thickness[normal_axis]<.181,(axis,thickness)
    report['tests'].append(dict(kind='gothic_closed_profile',plane=axis,faces=len(geo.prims()),thickness=thickness,**stats))
report['status']='passed';(R/'circular_geometry.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
