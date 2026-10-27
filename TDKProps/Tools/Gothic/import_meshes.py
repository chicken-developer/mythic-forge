"""Import baked Gothic presets as standalone static meshes in TDKProps."""
from pathlib import Path
import json,traceback
import unreal as u
ROOT=Path('E:/Repos/mythic-forge')
REVIEW=ROOT/'TDKProps/Saved/GothicReview'
source=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())
tools=u.AssetToolsHelpers.get_asset_tools()
sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
report={'meshes':[]}
try:
    for row in source['variants']:
        task=u.AssetImportTask()
        task.set_editor_property('filename',str(ROOT/row['file']))
        task.set_editor_property('destination_path','/Game/Gothic/Buildings')
        task.set_editor_property('destination_name','SM_Gothic_'+row['id'])
        task.set_editor_property('automated',True)
        task.set_editor_property('replace_existing',True)
        task.set_editor_property('replace_existing_settings',True)
        task.set_editor_property('save',True)
        opts=u.FbxImportUI()
        opts.set_editor_property('automated_import_should_detect_type',False)
        opts.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH)
        opts.set_editor_property('import_as_skeletal',False)
        opts.set_editor_property('import_materials',False)
        opts.set_editor_property('import_textures',False)
        data=opts.get_editor_property('static_mesh_import_data')
        for k,v in {'combine_meshes':True,'convert_scene':True,'convert_scene_unit':True,'import_uniform_scale':1.,'generate_lightmap_u_vs':True,'auto_generate_collision':False,'build_nanite':True,'remove_degenerates':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():data.set_editor_property(k,v)
        task.set_editor_property('options',opts)
        task.set_editor_property('factory',u.FbxFactory())
        tools.import_asset_tasks([task])
        mesh=next(x for x in task.get_objects() if isinstance(x,u.StaticMesh))
        mesh.set_editor_property('light_map_resolution',256)
        body=mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        settings=sm.get_nanite_settings(mesh)
        settings.set_editor_property('enabled',True)
        sm.set_nanite_settings(mesh,settings,True)
        u.EditorAssetLibrary.set_metadata_tag(mesh,'SourceHIP','HoudiniWorkspace/gothic_building/gothic_building.hip')
        u.EditorAssetLibrary.set_metadata_tag(mesh,'Preset',row['id'])
        u.EditorAssetLibrary.set_metadata_tag(mesh,'Usage','Static architectural exterior; Nanite desktop; complex collision')
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        box=mesh.get_bounding_box()
        item={'id':row['id'],'path':mesh.get_path_name(),'min':list(box.min.to_tuple()),'max':list(box.max.to_tuple()),'slots':[str(x.material_slot_name) for x in mesh.get_editor_property('static_materials')],'lods':sm.get_lod_count(mesh),'uv_channels':sm.get_num_uv_channels(mesh,0),'nanite':sm.get_nanite_settings(mesh).enabled,'collision':str(sm.get_collision_complexity(mesh))}
        report['meshes'].append(item)
        (REVIEW/'mesh_import.json').write_text(json.dumps(report,indent=2))
        u.log('GOTHIC_IMPORTED '+row['id'])
    (REVIEW/'mesh_import_complete.txt').write_text('ok')
except Exception:
    (REVIEW/'mesh_import_error.txt').write_text(traceback.format_exc())
    raise
