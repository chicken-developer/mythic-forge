"""One-shot import of verified Medieval FBX plus the existing texture set."""
from pathlib import Path
import json
import traceback
import unreal as u
ROOT=Path(__file__).resolve().parents[4];P=ROOT/'Assets/_render';R=P/'Saved/MedievalReview';R.mkdir(parents=True,exist_ok=True)
S=P/'SourceAssets/Medieval';A='/Game/Medieval';AT=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary;SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
def expr(mat,cls,x,y,**props):
    n=M.create_material_expression(mat,cls,x,y)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def link(a,port,b,pin):assert M.connect_material_expressions(a,port,b,pin)
def materials():
    rows=json.loads((S/'material_manifest.json').read_text())['materials'];textures={}
    for row in rows:
        family=row['family'];textures[family]={}
        for key,source in row['files'].items():
            name='T_MD_'+family+'_'+key;path=A+'/Textures/'+name;tex=u.load_asset(path)
            if not tex:
                task=u.AssetImportTask()
                for k,v in dict(filename=source,destination_path=A+'/Textures',destination_name=name,automated=True,save=True,replace_existing=False).items():task.set_editor_property(k,v)
                AT.import_asset_tasks([task]);tex=u.load_asset(path);assert tex
            if key!='basecolor':tex.set_editor_property('srgb',False)
            if key=='normal_opengl':
                tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property('flip_green_channel',True)
            elif key=='roughness':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
            u.EditorAssetLibrary.save_loaded_asset(tex);textures[family][key]=tex
    parent=u.load_asset(A+'/Materials/M_Medieval') or AT.create_asset('M_Medieval',A+'/Materials',u.Material,u.MaterialFactoryNew())
    if u.EditorAssetLibrary.get_metadata_tag(parent,'MedievalVersion')!='5.1.1':
        M.delete_all_material_expressions(parent)
        def sample(param,key,y):
            return expr(parent,u.MaterialExpressionTextureSampleParameter2D,-900,y,parameter_name=param,texture=textures['plaster'][key],sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if key=='normal_opengl' else u.MaterialSamplerType.SAMPLERTYPE_MASKS if key=='roughness' else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        bc=sample('BaseColor','basecolor',-400);normal=sample('Normal','normal_opengl',0);rough=sample('RoughnessMap','roughness',400)
        tint=expr(parent,u.MaterialExpressionVectorParameter,-900,-650,parameter_name='Tint',default_value=u.LinearColor(1,1,1,1))
        color=expr(parent,u.MaterialExpressionMultiply,-550,-400);link(bc,'RGB',color,'A');link(tint,'RGB',color,'B');M.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
        strength=expr(parent,u.MaterialExpressionVectorParameter,-900,200,parameter_name='NormalScale',default_value=u.LinearColor(.25,.25,1,1))
        scaled=expr(parent,u.MaterialExpressionMultiply,-550,0);link(normal,'RGB',scaled,'A');link(strength,'RGB',scaled,'B')
        norm=expr(parent,u.MaterialExpressionNormalize,-300,0);link(scaled,'',norm,'');M.connect_material_property(norm,'',u.MaterialProperty.MP_NORMAL)
        scale=expr(parent,u.MaterialExpressionScalarParameter,-900,650,parameter_name='RoughnessScale',default_value=1.)
        rm=expr(parent,u.MaterialExpressionMultiply,-550,400);link(rough,'R',rm,'A');link(scale,'',rm,'B');M.connect_material_property(rm,'',u.MaterialProperty.MP_ROUGHNESS)
        metal=expr(parent,u.MaterialExpressionScalarParameter,-550,650,parameter_name='Metallic',default_value=0.)
        M.connect_material_property(metal,'',u.MaterialProperty.MP_METALLIC)
        emission=expr(parent,u.MaterialExpressionScalarParameter,-550,-650,parameter_name='Emission',default_value=0.)
        glow=expr(parent,u.MaterialExpressionMultiply,-300,-650);link(color,'',glow,'A');link(emission,'',glow,'B');M.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        M.recompile_material(parent);u.EditorAssetLibrary.set_metadata_tag(parent,'MedievalVersion','5.1.1');u.EditorAssetLibrary.save_loaded_asset(parent)
    instances={}
    for row in rows:
        family=row['family'];mi=u.load_asset(A+'/Materials/MI_MD_'+family) or AT.create_asset('MI_MD_'+family,A+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
        M.set_material_instance_parent(mi,parent)
        for param,key in [('BaseColor','basecolor'),('Normal','normal_opengl'),('RoughnessMap','roughness')]:M.set_material_instance_texture_parameter_value(mi,param,textures[family][key])
        M.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(*row['texture_tint'],1))
        M.set_material_instance_vector_parameter_value(mi,'NormalScale',u.LinearColor(row['normal_strength'],row['normal_strength'],1,1))
        for key,value in [('Metallic',row['metallic']),('Emission',row['emissive']),('RoughnessScale',row['roughness_override']/.82 if row['roughness_override'] is not None else 1.)]:M.set_material_instance_scalar_parameter_value(mi,key,value)
        M.update_material_instance(mi);u.EditorAssetLibrary.save_loaded_asset(mi);instances[family]=mi
    return instances
def run():
    report={'status':'running','meshes':[]};(R/'asset_import.json').write_text(json.dumps(report))
    instances=materials();u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    rows=json.loads((S/'mesh_manifest.json').read_text())['variants']
    request=R/'import_request.json'
    if request.exists():
        ids=json.loads(request.read_text()).get('ids')
        if ids:rows=[row for row in rows if row['id'] in ids]
    for row in rows:
        task=u.AssetImportTask()
        for k,v in dict(filename=str(ROOT/row['file']),destination_path=A+'/Buildings',destination_name='SM_Medieval_'+row['id'],automated=True,replace_existing=True,replace_existing_settings=True,save=True).items():task.set_editor_property(k,v)
        options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
        for k,v in dict(combine_meshes=True,convert_scene=True,convert_scene_unit=True,import_uniform_scale=1.,generate_lightmap_u_vs=False,auto_generate_collision=False,build_nanite=True,remove_degenerates=True,normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
        task.options=options;task.factory=u.FbxFactory();AT.import_asset_tasks([task]);mesh=next(o for o in task.get_objects() if isinstance(o,u.StaticMesh))
        for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,instances[str(slot.material_slot_name).removeprefix('Medieval_')])
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        nanite=SM.get_nanite_settings(mesh);nanite.enabled=True;nanite.fallback_relative_error=0.;SM.set_nanite_settings(mesh,nanite,True)
        for k,v in {'SourceHIP':'../hda_medieval_building/medieval_building.hip','GeneratorVersion':'5.1-native','Dataset':row['id']}.items():u.EditorAssetLibrary.set_metadata_tag(mesh,k,v)
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        size=list((mesh.get_bounding_box().max-mesh.get_bounding_box().min).to_tuple());expected=[row['size_m'][i]*100 for i in (0,2,1)]
        assert max(abs(a-b) for a,b in zip(size,expected))<.3,(row['id'],size,expected)
        assert mesh.get_num_nanite_triangles()==row['triangles'],(row['id'],mesh.get_num_nanite_triangles(),row['triangles'])
        report['meshes'].append({'id':row['id'],'triangles':mesh.get_num_nanite_triangles(),'size_cm':size,'materials':len(mesh.static_materials),'nanite':True,'collision':'complex_as_simple'})
        (R/'asset_import.json').write_text(json.dumps(report,indent=2));u.log('MEDIEVAL_IMPORTED '+row['id'])
    report['status']='passed';(R/'asset_import.json').write_text(json.dumps(report,indent=2));(R/'import_error.txt').unlink(missing_ok=True)
if __name__=='__main__':
    try:run()
    except Exception:(R/'import_error.txt').write_text(traceback.format_exc());raise
