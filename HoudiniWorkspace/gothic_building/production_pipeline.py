"""Build/export/publish the approved native generator and 28 Gothic city presets."""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import math
import os
import shutil
import sys
import time
import hou
import numpy as np
import build_hda as B
import native_network as N

ROOT=Path(__file__).parent
REPO=next(p for p in (ROOT.parents[1],ROOT.parents[1]/'mythic-forge') if (p/'TDKProps/TDKProps.uproject').is_file())
EXPORT=REPO/'TDKProps/SourceAssets/Gothic'
OUT=ROOT/'houdini_output/production'
TYPE='quynh::gothic_building::3.0'
EXPORT_FOLDER=None

def configure():
    N.CORE_SOURCE=(ROOT/'build_hda.py').read_text(encoding='utf-8').split('\n# INSTALLER_BOUNDARY')[0]
    N.PRODUCTION_SOURCE=(ROOT/'production_geometry.py').read_text(encoding='utf-8')
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    records=[json.loads((ROOT/p).read_text(encoding='utf-8')) for p in [manifest['baseline']]+manifest['datasets']]
    N.PRESETS={r['variant_id']:r for r in records}
    return records

def apply(node,record,quality='game'):
    for k,v in B.normalize(record)['parameters'].items():node.parm(k).set(int(v) if isinstance(v,bool) else v)
    for k,v in record['seeds'].items():node.parm('seed_'+k).set(v)
    node.parm('building_preset').set(record['variant_id']);node.parm('mesh_quality').set(quality)

def create(record,quality='game'):
    obj=hou.node('/obj').createNode('geo','gothic_building_v2_1',run_init_scripts=False)
    node=obj.createNode('subnet','generator');node.setParmTemplateGroup(N.interface(B.normalize(record)))
    apply(node,record,quality);N.rebuild(node)
    node.setDisplayFlag(True);node.setRenderFlag(True)
    return obj,node

def validate(node):
    node.cook(force=True);nodes=[node]+list(node.allSubChildren(recurse_in_locked_nodes=False))
    errors=[{'node':n.path(),'errors':list(n.errors())} for n in nodes if n.errors()]
    assert not errors,errors[:6]
    assert not any(n.type().name()=='python' for n in nodes),'Python SOP in final graph'
    geo=node.geometry();assert geo.prims(),'Empty building'
    positions=np.array(geo.pointFloatAttribValues('P'));normals=np.array(geo.vertexFloatAttribValues('N')).reshape(-1,3)
    assert np.isfinite(positions).all() and np.isfinite(normals).all(),'Non-finite attributes'
    bad_normals=int(np.count_nonzero(np.abs(np.linalg.norm(normals,axis=1)-1)>1e-3))
    assert not bad_normals,('Invalid normals',bad_normals)
    areas=np.array([p.intrinsicValue('measuredarea') for p in geo.prims()])
    bad=np.nonzero(areas<1e-12)[0]
    assert not len(bad),('Degenerate triangles',len(bad),[geo.prim(int(i)).attribValue('name') for i in bad[:8]])
    materials=collections.Counter(geo.primStringAttribValues('material_id'))
    assert 'stone' in materials and ('trim' in materials or 'gold' in materials) and 'roof' in materials,materials
    names=geo.primStringAttribValues('name')
    assert not any('crocket' in name for name in names),'Unapproved protruding ornament'
    box=geo.boundingBox();lo=list(box.minvec());hi=list(box.maxvec())
    return {'points':len(geo.points()),'triangles':len(geo.prims()),'size_m':[hi[i]-lo[i] for i in range(3)],
        'min_m':lo,'max_m':hi,'invalid_normals':bad_normals,'degenerate_triangles':0,'python_sops':0,
        'native_nodes':len(nodes),'sweeps':sum(n.type().name().startswith('sweep') for n in nodes),
        'bevels':sum(n.type().name().startswith('polybevel') for n in nodes),'materials':dict(materials)}

def export_one(record,quality='game'):
    started=time.time();obj,node=create(record,quality);stats=validate(node)
    geo=hou.Geometry(node.geometry());lo=stats['min_m'];hi=stats['max_m']
    center=hou.Vector3(((lo[0]+hi[0])/2,lo[1],(lo[2]+hi[2])/2))
    geo.transform(hou.hmath.buildTranslate(-center)*hou.hmath.buildScale((100.,100.,100.)))
    matroot=hou.node('/mat')
    for mat in stats['materials']:
        if matroot.node('Gothic_'+mat) is None:matroot.createNode('principledshader::2.0','Gothic_'+mat)
    if geo.findPrimAttrib('shop_materialpath') is None:geo.addAttrib(hou.attribType.Prim,'shop_materialpath','')
    families=geo.primStringAttribValues('material_id')
    uvs=np.array(geo.vertexFloatAttribValues('uv')).reshape(-1,3)
    tiles=np.repeat(np.array([2. if family in ('roof','wood','glass') else 4. for family in families]),3)
    uvs[:,:2]/=tiles[:,None];geo.setVertexFloatAttribValues('uv',uvs.reshape(-1).tolist())
    for prim in geo.prims():
        family=prim.attribValue('material_id');prim.setAttribValue('shop_materialpath','/mat/Gothic_'+family)
    baked=hou.node('/obj').createNode('geo','export_only',run_init_scripts=False)
    stash=baked.createNode('stash','baked_native_output');stash.parm('stash').set(geo);stash.setDisplayFlag(True);stash.setRenderFlag(True)
    folder=EXPORT/(EXPORT_FOLDER or ('Meshes' if quality=='game' else 'High'));folder.mkdir(parents=True,exist_ok=True)
    target=folder/('SM_Gothic_'+record['variant_id']+'.fbx')
    rop=hou.node('/out').createNode('filmboxfbx','production_export');rop.parm('startnode').set(baked.path());rop.parm('trange').set(0)
    rop.parm('sopoutput').set(str(target));rop.render()
    row={**stats,'id':record['variant_id'],'label':record['label'],'district':record.get('district','landmark'),
        'use':record.get('use','civic'),'file':target.relative_to(REPO).as_posix(),'quality':quality,
        'parameters':B.normalize(record)['parameters'],'seeds':record['seeds'],
        'fbx_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'seconds':round(time.time()-started,2)}
    (OUT/'records').mkdir(parents=True,exist_ok=True)
    (OUT/'records'/(record['variant_id']+'_'+quality+'.json')).write_text(json.dumps(row,indent=2),encoding='utf-8')
    print('EXPORTED',record['variant_id'],quality,row['triangles'],row['seconds'],flush=True)
    obj.destroy();baked.destroy();rop.destroy()
    return row

def embedded():
    return (ROOT/'native_network.py').read_text(encoding='utf-8')+'\nCORE_SOURCE='+repr(N.CORE_SOURCE)+'\nPRODUCTION_SOURCE='+repr(N.PRODUCTION_SOURCE)+'\nPRESETS=json.loads('+repr(json.dumps(N.PRESETS))+')\n'

def publish(records):
    # Preserve the v2 source/HDA before changing the canonical generator.
    backups=ROOT/'houdini_output/backups/v3_rollout';backups.mkdir(parents=True,exist_ok=True)
    for path in [ROOT/'gothic_building.hip',ROOT/'asset/gothic_building.hda']:
        if path.exists() and not (backups/path.name).exists():shutil.copy2(path,backups/path.name)
    obj,node=create(records[0]);stats=validate(node)
    stage=OUT/'gothic_building_v3.hda';stage.parent.mkdir(parents=True,exist_ok=True)
    if stage.exists():stage.unlink()
    templates=node.parmTemplateGroup()
    asset=node.createDigitalAsset(name=TYPE,hda_file_name=str(stage),description='Gothic City - Approved Native Modules',min_num_inputs=0,max_num_inputs=0)
    definition=asset.type().definition();definition.setParmTemplateGroup(templates)
    definition.addSection('PythonModule',embedded());definition.addSection('EditableNodes','construction MANUAL_EDIT final_boundary_alignment')
    definition.addSection('GothicGraphVersion','3.0')
    target=ROOT/'asset/gothic_building.hda';definition.copyToHDAFile(str(target))
    definition.copyToHDAFile('Embedded')
    embed=next(d for d in hou.hda.definitionsInFile('Embedded') if d.nodeTypeName()==TYPE);embed.setIsPreferred(True)
    asset.setCurrent(True,clear_all_selected=True)
    hou.hscript('otconfig -s 1');hou.hipFile.save(str(ROOT/'gothic_building.hip'))
    report={'status':'passed','version':'3.0','presets':len(records)-1,'type_name':TYPE,'source':'gothic_building.hip','hda':'asset/gothic_building.hda','backup':'houdini_output/backups/v3_rollout','validation':stats}
    (ROOT/'reports/production_deployment.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PUBLISHED_V3',stats['triangles'],flush=True)

def collect(records):
    rows=[json.loads((OUT/'records'/(r['variant_id']+'_game.json')).read_text()) for r in records[1:]]
    assert len(rows)==28 and len(set(r['id'] for r in rows))==28
    report={'version':'3.0','status':'passed','source_hip':os.path.relpath(ROOT/'gothic_building.hip',REPO).replace(chr(92),'/'),
        'source_sha256':hashlib.sha256((ROOT/'gothic_building.hip').read_bytes()).hexdigest(),
        'approved_reference':'facade_review/gothic_facade_review.hip','fbx_units':'centimetres','houdini':hou.applicationVersionString(),'variants':rows}
    (EXPORT/'mesh_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (ROOT/'reports/production_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('COLLECTED',len(rows),sum(r['triangles'] for r in rows),flush=True)

def main():
    global EXPORT_FOLDER
    parser=argparse.ArgumentParser();parser.add_argument('--ids',nargs='*');parser.add_argument('--quality',default='game',choices=['game','high']);parser.add_argument('--publish',action='store_true');parser.add_argument('--collect',action='store_true')
    parser.add_argument('--folder')
    args=parser.parse_args();EXPORT_FOLDER=args.folder;records=configure()
    if args.publish:publish(records)
    elif args.collect:collect(records)
    else:
        for r in records[1:]:
            if not args.ids or r['variant_id'] in args.ids:export_one(r,args.quality)

if __name__=='__main__':main()
