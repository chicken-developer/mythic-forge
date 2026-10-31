"""One-shot import of selected detail meshes and reusable building/light Blueprints."""
from pathlib import Path
import importlib
import json
import sys
import traceback
import unreal as u

P=Path('E:/Repos/mythic-forge/TDKProps');R=P/'Saved/PropsDetail';R.mkdir(parents=True,exist_ok=True)
AT=u.AssetToolsHelpers.get_asset_tools();SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
SD=u.get_engine_subsystem(u.SubobjectDataSubsystem);LIB=u.SubobjectDataBlueprintFunctionLibrary

def light_sections(family,mesh):
    # Opaque preview glass must not shadow the point light enclosed by the lantern.
    for section in range(mesh.get_num_sections(0)):
        index=SM.get_lod_material_slot(mesh,0,section)
        slot=str(mesh.static_materials[index].material_slot_name).removeprefix(family+'_')
        if slot==('light' if family=='Medieval' else 'glass') and SM.is_section_cast_shadow_enabled(mesh,0,section):
            SM.enable_section_cast_shadow(mesh,False,0,section)

def blueprint(family,row,mesh):
    name='BP_'+family+'_'+row['id'];folder='/Game/'+family+'/Blueprints'
    bp=u.load_asset(folder+'/'+name)
    if not bp:
        factory=u.BlueprintFactory();factory.set_editor_property('parent_class',u.Actor)
        bp=AT.create_asset(name,folder,u.Blueprint,factory)
    handles=SD.k2_gather_subobject_data_for_blueprint(bp)
    components={}
    for handle in handles:
        data=LIB.get_data(handle);obj=LIB.get_object_for_blueprint(data,bp)
        if isinstance(obj,u.ActorComponent):components[str(obj.get_name()).removesuffix('_GEN_VARIABLE')]=(handle,obj)
        if isinstance(obj,u.SceneComponent) and not isinstance(obj,u.LightComponent):obj.set_mobility(u.ComponentMobility.STATIC)
    def component(name,cls):
        if name in components:return components[name]
        params=u.AddNewSubobjectParams(parent_handle=handles[0],new_class=cls,blueprint_context=bp)
        handle,reason=SD.add_new_subobject(params)
        assert LIB.is_handle_valid(handle),str(reason)
        assert SD.rename_subobject(handle,name)
        obj=LIB.get_object_for_blueprint(LIB.get_data(handle),bp);components[name]=(handle,obj)
        return handle,obj
    building,sc=component('Building',u.StaticMeshComponent)
    sc.set_static_mesh(mesh);sc.set_mobility(u.ComponentMobility.STATIC)
    sc.set_collision_profile_name('BlockAll')
    for i,entry in enumerate(row['interior_lights']):
        handle,light=component('InteriorLight_'+str(i),u.PointLightComponent)
        SD.attach_subobject(building,handle)
        x,y,z=entry['position_m'];light.set_editor_property('relative_location',u.Vector(x*100,z*100,y*100))
        light.set_mobility(u.ComponentMobility.MOVABLE)
        light.set_editor_property('intensity_units',u.LightUnits.LUMENS)
        light.set_editor_property('intensity',entry['lumens'])
        light.set_editor_property('attenuation_radius',entry['radius_m']*100)
        light.set_editor_property('use_temperature',True);light.set_editor_property('temperature',entry['temperature'])
        light.set_editor_property('cast_shadows',True);light.set_editor_property('source_radius',8.)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp)
    return bp.get_path_name()

def main():
    request=json.loads((R/'import_request.json').read_text())
    report={'status':'running','meshes':[]};(R/'asset_import.json').write_text(json.dumps(report))
    u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    for family in request.get('families',['Medieval','Gothic']):
        sys.path.insert(0,str(P/'Tools'/family))
        helper=importlib.import_module('import_assets' if family=='Medieval' else 'import_city_assets')
        if request.get('reuse_materials'):
            if family=='Medieval':
                materials={r['family']:u.load_asset('/Game/Medieval/Materials/MI_MD_'+r['family']) for r in json.loads((P/'SourceAssets/Medieval/material_manifest.json').read_text())['materials']}
            else:
                materials={p.rsplit('/',1)[-1].split('.')[0].removeprefix('MI_City_'):u.load_asset(p) for p in u.EditorAssetLibrary.list_assets('/Game/Gothic/Materials/City',recursive=False) if '/MI_City_' in p}
            assert materials and all(materials.values()),family
        else:materials=helper.materials()
        for quality in request.get('qualities',['normal']):
            filename='mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json'
            source=P/'SourceAssets'/family/filename
            rows=json.loads(source.read_text())['variants'] if source.exists() else []
            if request.get('ids'):
                selected=set(request['ids']);rows=[json.loads((R/family/quality/(ident+'.json')).read_text()) for ident in selected if (R/family/quality/(ident+'.json')).exists()]
            for row in rows:
                folder='/Game/'+family+'/Buildings'+('' if quality=='normal' else '/'+quality.title())
                if request.get('blueprints_only') or (quality=='normal' and family in request.get('reuse_normal_families',[])):
                    if quality!='normal':continue
                    mesh=u.load_asset(folder+'/SM_'+family+'_'+row['id']);assert mesh
                    light_sections(family,mesh);u.EditorAssetLibrary.save_loaded_asset(mesh)
                    bp=blueprint(family,row,mesh)
                    report['meshes'].append({'family':family,'id':row['id'],'quality':quality,'blueprint':bp,'lights':len(row['interior_lights'])})
                    (R/'asset_import.json').write_text(json.dumps(report,indent=2))
                    continue
                task=u.AssetImportTask()
                existing=u.load_asset(folder+'/SM_'+family+'_'+row['id'])
                if existing:
                    before=SM.get_nanite_settings(existing);before.enabled=False
                    SM.set_nanite_settings(existing,before,False)
                    # FBX reimport restores stored import settings. Preserve valid small
                    # bevel triangles when building Nanite after the material pass.
                    existing.get_editor_property('asset_import_data').set_editor_property('remove_degenerates',quality=='low')
                for k,v in dict(filename=str(P.parent/row['file']),destination_path=folder,destination_name='SM_'+family+'_'+row['id'],automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
                options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
                # Build Nanite once after material and collision configuration.
                for k,v in dict(combine_meshes=True,convert_scene=True,convert_scene_unit=True,import_uniform_scale=1.,generate_lightmap_u_vs=False,auto_generate_collision=False,build_nanite=False,remove_degenerates=quality=='low',normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
                task.options=options;task.factory=u.FbxFactory();AT.import_asset_tasks([task])
                mesh=next(o for o in task.get_objects() if isinstance(o,u.StaticMesh))
                if family=='Medieval':choices=materials
                else:
                    number=int(row['id'][-2:]);district=row.get('district','landmark')
                    old=['Limestone','RosePlaster','SagePlaster','PaleStone','Plaster','DarkStone','BluePlaster','BluePlaster','WarmStone','PaleStone']
                    wall=old[number-1] if number<=10 else ['WarmStone','RosePlaster','Plaster','SagePlaster'][number%4] if district=='working' else ['Limestone','Plaster','SagePlaster'][number%3] if district=='middle' else 'PaleStone' if district=='upper' else 'Limestone'
                    roof={'slate':'Slate','copper':'Copper','oxblood':'Oxblood'}[row['parameters']['roof_material']]
                    names={'stone':wall,'trim':'Ivory','roof':roof,'glass':'AmberGlass','metal':'Iron','gold':'Brass','wood':'Walnut','floor':'Walnut','interior':'Plaster'}
                    choices={key:materials[name] for key,name in names.items()}
                slots=list(mesh.static_materials)
                for slot in slots:slot.set_editor_property('material_interface',choices[str(slot.material_slot_name).removeprefix(family+'_')])
                mesh.set_editor_property('static_materials',slots)
                light_sections(family,mesh)
                mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                settings=SM.get_nanite_settings(mesh);settings.enabled=quality!='low';settings.fallback_relative_error=0.;SM.set_nanite_settings(mesh,settings,True)
                for k,v in {'GeneratorVersion':row['generator_version'],'MeshQuality':quality,'Dataset':row['id'],'InteriorLights':str(len(row['interior_lights']))}.items():u.EditorAssetLibrary.set_metadata_tag(mesh,k,v)
                u.EditorAssetLibrary.save_loaded_asset(mesh)
                size=list((mesh.get_bounding_box().max-mesh.get_bounding_box().min).to_tuple());expected=[row['size_m'][i]*100 for i in (0,2,1)]
                assert max(abs(a-b) for a,b in zip(size,expected))<.5,(row['id'],size,expected)
                actual=mesh.get_num_nanite_triangles() if quality!='low' else mesh.get_num_triangles(0)
                imported=mesh.get_static_mesh_description(0).get_triangle_count()
                # Nanite can discard a few subpixel triangles at clipped roof joins.
                collapsed=row['triangles']-actual
                assert 0<=collapsed<=max(4,round(row['triangles']*.00001)),(row['id'],actual,row['triangles'])
                bp=blueprint(family,row,mesh) if quality=='normal' else None
                report['meshes'].append({'family':family,'id':row['id'],'quality':quality,'asset':mesh.get_path_name(),'blueprint':bp,'source_triangles':row['triangles'],'imported_triangles':imported,'engine_triangles':actual,'engine_collapsed_triangles':collapsed,'nanite_position_precision':settings.position_precision,'lights':len(row['interior_lights'])})
                (R/'asset_import.json').write_text(json.dumps(report,indent=2));u.log('DETAIL_IMPORTED '+family+' '+quality+' '+row['id'])
    report['status']='passed';(R/'asset_import.json').write_text(json.dumps(report,indent=2));(R/'import_error.txt').unlink(missing_ok=True)
    (R/('asset_import_'+request.get('run','latest')+'.json')).write_text(json.dumps(report,indent=2))

try:main()
except Exception:(R/'import_error.txt').write_text(traceback.format_exc());raise
