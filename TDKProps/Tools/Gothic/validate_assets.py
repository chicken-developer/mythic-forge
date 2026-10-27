"""Inspect saved engine assets and verify dimensions against source HIP export."""
from pathlib import Path
import json,traceback
import unreal as u
ROOT=Path('E:/Repos/mythic-forge')
R=ROOT/'TDKProps/Saved/GothicReview'
sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
source=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())
report={'engine':u.SystemLibrary.get_engine_version(),'checks':[],'errors':[]}
for row in source['variants']:
    try:
        mesh=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_'+row['id'])
        box=mesh.get_bounding_box();size=(box.max-box.min).to_tuple()
        expected=[row['size_m'][0]*100,row['size_m'][2]*100,row['size_m'][1]*100]
        err=max(abs(a-b) for a,b in zip(size,expected))
        mats=mesh.get_editor_property('static_materials')
        assert err<1.,('dimensions',size,expected)
        assert all(s.material_interface is not None for s in mats),'Unbound material'
        assert all(s.material_interface.get_path_name().startswith('/Game/Gothic/Materials/') for s in mats),'Unexpected placeholder material'
        assert sm.get_nanite_settings(mesh).enabled,'Nanite disabled'
        assert sm.get_collision_complexity(mesh)==u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,'Collision disabled'
        assert sm.get_num_uv_channels(mesh,0)>=1,'Missing UV'
        assert mesh.get_bounds().box_extent.z>100.,'Unexpected height'
        report['checks'].append({'id':row['id'],'dimensions_cm':size,'source_error_cm':err,'material_count':len(mats),'materials':[s.material_interface.get_path_name() for s in mats],'nanite':True,'nanite_triangles':mesh.get_num_nanite_triangles(),'fallback_triangles':mesh.get_num_triangles(0),'collision':'complex_as_simple','uv_channels':sm.get_num_uv_channels(mesh,0)})
    except Exception:report['errors'].append({'id':row['id'],'error':traceback.format_exc()})
report['status']='passed' if len(report['checks'])==len(source['variants']) and not report['errors'] else 'failed'
(R/'asset_validation.json').write_text(json.dumps(report,indent=2))
assert report['status']=='passed',report['errors']
