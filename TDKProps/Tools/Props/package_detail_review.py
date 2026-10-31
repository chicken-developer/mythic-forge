"""Package only successful source/Unreal evidence and synchronize verified HDAs."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

P=Path(__file__).resolve().parents[2];R=P/'Saved/PropsDetail';OUT=P/'RenderTests/PropsDetail'
ap=argparse.ArgumentParser();ap.add_argument('--sync-sources',action='store_true');args=ap.parse_args()
if args.sync_sources:
    for family in ('Medieval','Gothic'):
        file=R/(family.lower()+'_source_publish.json');report=json.loads(file.read_text())
        source=Path(report['repository_hda']);target=Path(report['hda'])
        proof=json.loads((OUT/(family.lower()+'_portable_hda.json')).read_text())
        assert proof['status']=='passed'
        digest=hashlib.sha256(source.read_bytes()).hexdigest()
        assert proof['hda_sha256']==digest
        if hashlib.sha256(target.read_bytes()).hexdigest()!=digest:shutil.copy2(source,target)
        assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
        report['hda_sha256']=digest;file.write_text(json.dumps(report,indent=2))
        qualities=('normal','low','high') if family=='Medieval' else ('normal',)
        for quality in qualities:
            filename='mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json'
            manifest=P/'SourceAssets'/family/filename;data=json.loads(manifest.read_text())
            data.pop('native_run',None)
            data.update(source_hip=str(Path(report['hip'])),source_hda=str(source),
                source_sha256=hashlib.sha256(Path(report['hip']).read_bytes()).hexdigest(),
                source_hda_sha256=digest,
                source_code_sha256=hashlib.sha256((source.parent/'build_hda.py').read_bytes()).hexdigest())
            for row in data['variants']:
                for key in ('geometry_sha256','minimum_winding_normal_dot','source_polygons'):row.pop(key,None)
                row['polygons']=row['triangles']
            manifest.write_text(json.dumps(data,indent=2))
    proof=json.loads((OUT/'gothic_portable_hda.json').read_text())
    rows=json.loads((P/'SourceAssets/Gothic/mesh_manifest.json').read_text())['variants']
    by_id={row['id']:row for row in rows}
    assert set(by_id)=={row['id'] for row in proof['records']}
    lights=[]
    for record in proof['records']:
        row=by_id[record['id']]
        assert record['connected_light_sources_checked'] and record['maximum_light_position_error_m']<1e-5
        assert record['lights']==len(row['interior_lights'])
        lights.append({'id':record['id'],'actual_lights':record['lights'],'maximum_light_position_error_m':record['maximum_light_position_error_m'],'triangles':row['triangles'],'status':'passed'})
    (OUT/'gothic_light_layout.json').write_text(json.dumps({'status':'passed','method':'connected primitive components in independent HDA recooks; position comparison against every exported source','hda_sha256':proof['hda_sha256'],'presets':len(lights),'lights':sum(row['actual_lights'] for row in lights),'records':lights},indent=2))
    print('VERIFIED_HDA_BINARIES_SYNCHRONIZED')
else:
    names=['circular_geometry','rosette_native','exports','medieval_portable_hda','gothic_portable_hda','medieval_source_scene','gothic_source_scene','gothic_light_layout','catalogs','unreal_validation']
    reports={name:json.loads((OUT/(name+'.json')).read_text()) for name in names}
    assert all(value['status']=='passed' for value in reports.values())
    joints=P/'RenderTests/BuildingJoints'
    joint_proof=json.loads((joints/'joint_regressions.json').read_text())
    assert joint_proof['status']=='passed'
    meshes=reports['unreal_validation']['meshes'];assert len(meshes)==100
    expected={}
    for family,qualities in [('Medieval',('normal','low','high')),('Gothic',('normal',))]:
        for quality in qualities:
            name='mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json'
            manifest=json.loads((P/'SourceAssets'/family/name).read_text())
            for row in manifest['variants']:expected[(family,quality,row['id'])]=row['triangles']
    assert {(row['family'],row['quality'],row['id']) for row in meshes}==set(expected)
    assert all(row['source_triangles']==expected[(row['family'],row['quality'],row['id'])] for row in meshes),'Unreal proof belongs to an older source export'
    catalogs=reports['catalogs']['catalogs'];assert sum(c['building_count'] for c in catalogs)==52
    images=['medieval_catalog_overview.png','medieval_01_townhouse_base_interior.png','medieval_02_round_workshop_base_interior.png','gothic_variant_01_interior.png','gothic_variant_01_round.png']
    assert all((OUT/name).is_file() for name in images)
    sources=[]
    for family in ('Medieval','Gothic'):
        source=json.loads((R/(family.lower()+'_source_publish.json')).read_text())
        assert source['hda_sha256']==hashlib.sha256(Path(source['hda']).read_bytes()).hexdigest()
        assert source['hda_sha256']==reports[family.lower()+'_portable_hda']['hda_sha256']
        sources.append(source)
    joint_images=['medieval_01_townhouse_base_window','medieval_01_townhouse_base_attic','medieval_03_village_inn_base_roof_join','gothic_variant_12_corner']
    assert all((joints/(name+'_'+stage+'.png')).is_file() for name in joint_images for stage in ('before','after'))
    joint_review={'status':'passed','date':'2026-10-31','building_presets':52,'static_meshes':100,'versions':{s['family']:s['version'] for s in sources},'changes':['Continuous Medieval opening surrounds and roof edges','Square bearing ends, fitted braces and timber pegs','Roof junction flashing at actual mass intersections','Continuous Gothic corner mouldings and jamb/sill bedding','Timber grain aligned to the member'], 'geometry_evidence':'joint_regressions.json','native_unreal_validation':'../PropsDetail/unreal_validation.json','images':[name+'_'+stage+'.png' for name in joint_images for stage in ('before','after')]}
    (joints/'final_review.json').write_text(json.dumps(joint_review,indent=2))
    report={'status':'passed','date':'2026-10-31','scope':'props catalogs, source HDA/HIP, interiors, lighting, curved detail, architectural joints',
        'maps':reports['unreal_validation']['remaining_maps'],'building_presets':52,'static_meshes':100,
        'interior_lights':sum(c['point_lights'] for c in catalogs),'sources':sources,'evidence':[name+'.json' for name in names],
        'images':images,'architectural_joints':'../BuildingJoints/final_review.json','limitations':['Basic furnished interiors; exterior doors remain closed meshes; no door interaction logic.',
            'No packaged-game or target-hardware performance validation.',
            'Nanite may discard a few tiny triangles; source and engine counts are recorded separately.']}
    (OUT/'final_handoff.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
