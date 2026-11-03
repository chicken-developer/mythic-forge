"""Cook, inspect and export catalog assets from the versioned Houdini sources."""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import sys
import time
import hou
import numpy as np
from light_layout import sources as light_sources
from skytown_checks import audit as skytown_audit

P=Path(__file__).resolve().parents[2]
SOURCE=P.parent
R=P/'Saved/PropsDetail'
EXPORT_FOLDER=None

def load(family):
    code=P/'SourceAssets/Houdini'/family
    sys.path.insert(0,str(code))
    import build_hda as B
    canonical=SOURCE/('hda_'+family.lower()+'_building')
    if family=='Medieval':
        records={p.stem:B.normalize(json.loads(p.read_text())) for p in sorted((canonical/'assets/datasets').glob('*.json'))}
        for cfg in records.values():cfg['parameters']['layout_input_enable']=False
        return B,None,canonical,records
    import native_network as N
    N.CORE_SOURCE=(code/'build_hda.py').read_text(encoding='utf-8').split('\n# INSTALLER_BOUNDARY')[0]
    N.PRODUCTION_SOURCE=(code/'production_geometry.py').read_text(encoding='utf-8')
    manifest=json.loads((canonical/'manifest.json').read_text())
    rows=[json.loads((canonical/p).read_text()) for p in [manifest['baseline']]+manifest['datasets']]
    N.PRESETS={row['variant_id']:row for row in rows}
    return B,N,canonical,{row['variant_id']:row for row in rows if row['variant_id']!='baseline'}

def cook(B,N,family,ident,cfg,quality):
    if family=='Medieval':
        cfg=B.normalize(cfg);cfg['parameters']['mesh_quality']=quality
        if quality=='low':cfg['parameters']['detail_level']='low'
        mesh=B.generate(cfg)
        geo=hou.Geometry();B.write_hou_geometry(mesh,geo)
        return geo,None,dict(features=mesh.features,parameters=cfg['parameters'])
    obj=hou.node('/obj').createNode('geo','gothic_detail',run_init_scripts=False)
    node=obj.createNode('subnet','generator');normalized=B.normalize(cfg)
    node.setParmTemplateGroup(N.interface(normalized))
    node.setParms({k:int(v) if isinstance(v,bool) else v for k,v in normalized['parameters'].items()})
    node.setParms({'seed_'+k:v for k,v in cfg['seeds'].items()})
    node.parm('building_preset').set(ident);node.parm('mesh_quality').set('high' if quality=='high' else 'game')
    try:N.rebuild(node);node.cook(force=True)
    except hou.OperationFailed:
        errors=[(n.path(),n.errors()) for n in (node,*node.allSubChildren()) if n.errors()]
        raise RuntimeError(errors[:8])
    errors=[(n.path(),n.errors()) for n in (node,*node.allSubChildren()) if n.errors()]
    assert not errors,errors[:5]
    assert not any(n.type().name()=='python' for n in node.allSubChildren())
    return hou.Geometry(node.geometry()),obj,dict(parameters=normalized['parameters'])

def export(B,N,family,ident,cfg,quality,old):
    started=time.monotonic();geo,obj,extra=cook(B,N,family,ident,cfg,quality)
    opening_clearance=skytown_audit(geo)
    source_polygons=len(geo.prims())
    temporary=hou.node('/obj').createNode('geo','catalog_export',run_init_scripts=False)
    stash=temporary.createNode('stash','source');stash.parm('stash').set(geo)
    divide=temporary.createNode('divide','triangulate');divide.setInput(0,stash)
    divide.parm('usemaxsides').set(1);divide.parm('numsides').set(3);divide.cook(force=True)
    geo=hou.Geometry(divide.geometry())
    bad=[p for p in geo.prims() if p.intrinsicValue('measuredarea')<1e-12]
    if bad:geo.deletePrims(bad,True)
    points=np.asarray(geo.pointFloatAttribValues('P')).reshape(-1,3)
    normals=np.asarray(geo.vertexFloatAttribValues('N')).reshape(-1,3)
    uvs=np.asarray(geo.vertexFloatAttribValues('uv')).reshape(-1,3)
    assert np.isfinite(points).all() and np.isfinite(normals).all() and np.isfinite(uvs).all()
    invalid=int(np.count_nonzero(np.abs(np.linalg.norm(normals,axis=1)-1)>1e-3));assert not invalid,invalid
    box=geo.boundingBox();lo=list(box.minvec());hi=list(box.maxvec())
    family_attr='material_family' if family=='Medieval' else 'material_id'
    names=geo.primStringAttribValues('name');families=geo.primStringAttribValues(family_attr)
    center=hou.Vector3(0,0,0) if family=='Medieval' else hou.Vector3((.5*(lo[0]+hi[0]),lo[1],.5*(lo[2]+hi[2])))
    lights=[]
    for source in light_sources(geo):
        p=hou.Vector3(source['position_m'])-center
        lights.append({'name':source['name'],'position_m':list(p),'lumens':650.,'temperature':2700.,'radius_m':4.5})
    assert lights,(ident,'No interior light-source geometry')
    interior=sum('int_' in n or 'interior' in n or 'spiral_tread' in n or 'pew' in n for n in names)
    assert interior>0,(ident,'Missing interior geometry')
    for point in geo.points():point.setPosition((point.position()-center)*100.)
    if not geo.findPrimAttrib('shop_materialpath'):geo.addAttrib(hou.attribType.Prim,'shop_materialpath','')
    tiles={'plaster':7.,'wood':1.6,'tile':.8,'stone':1.1,'rock':3.}
    scales=[1. if f.startswith('skytown_') else tiles.get(f,1.) if family=='Medieval' else 2. if f in ('roof','wood','glass') else 4. for f in families]
    uvs[:,:2]/=np.repeat(scales,3)[:,None];geo.setVertexFloatAttribValues('uv',uvs.ravel().tolist())
    for mat in set(families):
        name=family+'_'+mat
        if not hou.node('/mat').node(name):hou.node('/mat').createNode('principledshader::2.0',name)
    for prim,mat in zip(geo.prims(),families):prim.setAttribValue('shop_materialpath','/mat/'+family+'_'+mat)
    ready=temporary.createNode('stash','FBX_READY');ready.parm('stash').set(geo);ready.setDisplayFlag(True);ready.setRenderFlag(True)
    folder=P/'SourceAssets'/family/(EXPORT_FOLDER or {'normal':'Meshes','low':'Low','high':'High'}[quality]);folder.mkdir(parents=True,exist_ok=True)
    path=folder/('SM_'+family+'_'+ident+'.fbx')
    rop=hou.node('/out').createNode('filmboxfbx','detail_fbx');rop.parm('startnode').set(temporary.path());rop.parm('trange').set(0);rop.parm('sopoutput').set(str(path));rop.render()
    row=dict(old,**extra)
    for key in ('geometry_sha256','minimum_winding_normal_dot'):row.pop(key,None)
    row.update(id=ident,file=path.relative_to(P.parent.parent).as_posix(),quality=quality,points=len(geo.points()),triangles=len(geo.prims()),
        polygons=len(geo.prims()),source_polygons=source_polygons,
        size_m=[b-a for a,b in zip(lo,hi)],min_m=lo,max_m=hi,materials=dict(collections.Counter(families)),
        interior_triangles=interior,interior_lights=lights,invalid_normals=0,nonfinite_attributes=0,
        degenerate_triangles_removed=len(bad),fbx_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        generator_version=B.VERSION,seconds=round(time.monotonic()-started,3),source_origin_m=list(center),opening_clearance=opening_clearance,
        skytown_assets=sorted(set(geo.primStringAttribValues('skytown_source'))-{''}) if geo.findPrimAttrib('skytown_source') else [])
    records=R/family/quality;records.mkdir(parents=True,exist_ok=True)
    (records/(ident+'.json')).write_text(json.dumps(row,indent=2),encoding='utf-8')
    rop.destroy();temporary.destroy()
    if obj:obj.destroy()
    print('EXPORTED',family,quality,ident,row['triangles'],'lights',len(lights),row['seconds'],flush=True)
    return row

def main():
    global EXPORT_FOLDER
    ap=argparse.ArgumentParser();ap.add_argument('family',choices=['Medieval','Gothic']);ap.add_argument('--quality',default='normal',choices=['low','normal','high']);ap.add_argument('--ids',nargs='*');ap.add_argument('--collect',action='store_true');ap.add_argument('--folder')
    args=ap.parse_args();EXPORT_FOLDER=args.folder;B,N,canonical,records=load(args.family)
    manifest=P/'SourceAssets'/args.family/'mesh_manifest.json';old=json.loads(manifest.read_text());byid={r['id']:r for r in old['variants']}
    if not args.collect:
        for ident,cfg in records.items():
            if not args.ids or ident in args.ids:export(B,N,args.family,ident,cfg,args.quality,byid[ident])
    if args.collect or not args.ids:
        rows=[json.loads((R/args.family/args.quality/(ident+'.json')).read_text()) for ident in records]
        old.update(status='passed',version=B.VERSION,source=str(canonical),houdini=hou.applicationVersionString(),quality=args.quality,variants=rows)
        target=manifest if args.quality=='normal' else manifest.with_name('mesh_manifest_'+args.quality+'.json')
        target.write_text(json.dumps(old,indent=2),encoding='utf-8')
        print('COLLECTED',args.family,args.quality,len(rows),flush=True)

if __name__=='__main__':main()
