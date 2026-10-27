"""Explicit one-shot import of the 28 native Gothic building datasets."""
from pathlib import Path
import json
import traceback
import unreal as u

ROOT=Path('E:/Repos/mythic-forge');R=ROOT/'TDKProps/Saved/GothicReview/City';R.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'TDKProps/SourceAssets/Gothic'
AT=u.AssetToolsHelpers.get_asset_tools();SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);M=u.MaterialEditingLibrary
A='/Game/Gothic'

def expr(mat,cls,x,y,**props):
    n=M.create_material_expression(mat,cls,x,y)
    for k,v in props.items():n.set_editor_property(k,v)
    return n

def link(a,port,b,pin):
    assert M.connect_material_expressions(a,port,b,pin)

def materials():
    parent=u.load_asset(A+'/Materials/City/M_Gothic_City') or AT.create_asset('M_Gothic_City',A+'/Materials/City',u.Material,u.MaterialFactoryNew())
    if u.EditorAssetLibrary.get_metadata_tag(parent,'CityMaterialVersion')!='3.0':
        M.delete_all_material_expressions(parent)
        def texture(param,channel,y):
            tex=u.load_asset(A+'/Textures/T_Gothic_Stone_Limestone_'+channel)
            return expr(parent,u.MaterialExpressionTextureSampleParameter2D,-700,y,parameter_name=param,texture=tex,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='Normal' else u.MaterialSamplerType.SAMPLERTYPE_MASKS if channel=='ORM' else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        bc=texture('BaseColor','BaseColor',-350);normal=texture('Normal','Normal',0);orm=texture('ORM','ORM',350)
        tint=expr(parent,u.MaterialExpressionVectorParameter,-700,-600,parameter_name='Tint',default_value=u.LinearColor(1,1,1,1))
        mul=expr(parent,u.MaterialExpressionMultiply,-350,-350);link(bc,'RGB',mul,'A');link(tint,'RGB',mul,'B')
        M.connect_material_property(mul,'',u.MaterialProperty.MP_BASE_COLOR)
        strength=expr(parent,u.MaterialExpressionConstant3Vector,-700,180,constant=u.LinearColor(.16,.16,1.,1.))
        scaled=expr(parent,u.MaterialExpressionMultiply,-350,0);link(normal,'RGB',scaled,'A');link(strength,'',scaled,'B')
        normalized=expr(parent,u.MaterialExpressionNormalize,-100,0);link(scaled,'',normalized,'')
        M.connect_material_property(normalized,'',u.MaterialProperty.MP_NORMAL)
        M.connect_material_property(orm,'R',u.MaterialProperty.MP_AMBIENT_OCCLUSION)
        M.connect_material_property(orm,'G',u.MaterialProperty.MP_ROUGHNESS)
        M.connect_material_property(orm,'B',u.MaterialProperty.MP_METALLIC)
        glow=expr(parent,u.MaterialExpressionScalarParameter,-350,-650,parameter_name='Glow',default_value=0.)
        em=expr(parent,u.MaterialExpressionMultiply,-100,-350);link(mul,'',em,'A');link(glow,'',em,'B')
        M.connect_material_property(em,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        M.recompile_material(parent);u.EditorAssetLibrary.set_metadata_tag(parent,'CityMaterialVersion','3.0');u.EditorAssetLibrary.save_loaded_asset(parent)
    specs={'Limestone':('Stone_Limestone',(1,1,1)),'Plaster':('Wall_Plaster',(1,1,1)),
        'Ivory':('Trim_Ivory',(1,1,1)),'Slate':('Roof_Slate',(1,1,1)),
        'Copper':('Roof_Copper',(1,1,1)),'Oxblood':('Roof_Oxblood',(1,1,1)),
        'Brass':('Metal_Brass',(1,1,1)),'Iron':('Metal_Iron',(1,1,1)),
        'Walnut':('Wood_Walnut',(1,1,1)),'AmberGlass':('Glass_Amber',(1,1,1)),
        'RosePlaster':('Wall_Plaster',(1,.78,.72)),'SagePlaster':('Wall_Plaster',(.72,.88,.77)),
        'BluePlaster':('Wall_Plaster',(.72,.85,.95)),'DarkStone':('Stone_Limestone',(.46,.55,.60)),
        'WarmStone':('Stone_Limestone',(1,.94,.80)),'PaleStone':('Stone_Limestone',(1.08,1.09,1.06))}
    instances={}
    for name,(spec,tint) in specs.items():
        mi=u.load_asset(A+'/Materials/City/MI_City_'+name) or AT.create_asset('MI_City_'+name,A+'/Materials/City',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
        M.set_material_instance_parent(mi,parent)
        for channel in ['BaseColor','Normal','ORM']:
            tex=u.load_asset(A+'/Textures/T_Gothic_'+spec+'_'+channel);assert tex is not None
            M.set_material_instance_texture_parameter_value(mi,channel,tex)
        M.set_material_instance_vector_parameter_value(mi,'Tint',u.LinearColor(*tint,1.))
        M.set_material_instance_scalar_parameter_value(mi,'Glow',.08 if name=='AmberGlass' else 0.)
        M.update_material_instance(mi);u.EditorAssetLibrary.save_loaded_asset(mi);instances[name]=mi
    return instances

def run():
    request=json.loads((R/'import_request.json').read_text()) if (R/'import_request.json').exists() else {}
    selected=request.get('ids')
    if selected:
        rows=[json.loads((ROOT/'HoudiniWorkspace/gothic_building/houdini_output/production/records'/(i+'_game.json')).read_text()) for i in selected]
    else:rows=json.loads((SOURCE/'mesh_manifest.json').read_text())['variants']
    instances=materials();u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    previous=json.loads((R/'asset_import.json').read_text()) if (R/'asset_import.json').exists() else {'meshes':[]}
    by_id={r['id']:r for r in previous['meshes']};report={'status':'running','run_id':request.get('run_id','preview'),'meshes':list(by_id.values())}
    (R/'asset_import.json').write_text(json.dumps(report,indent=2))
    old_walls=['Limestone','RosePlaster','SagePlaster','PaleStone','Plaster','DarkStone','BluePlaster','BluePlaster','WarmStone','PaleStone']
    for row in rows:
        task=u.AssetImportTask()
        for k,v in {'filename':str(ROOT/row['file']),'destination_path':A+'/Buildings','destination_name':'SM_Gothic_'+row['id'],'automated':True,'replace_existing':True,'replace_existing_settings':True,'save':True}.items():task.set_editor_property(k,v)
        options=u.FbxImportUI()
        for k,v in {'automated_import_should_detect_type':False,'mesh_type_to_import':u.FBXImportType.FBXIT_STATIC_MESH,'import_as_skeletal':False,'import_materials':False,'import_textures':False}.items():options.set_editor_property(k,v)
        data=options.static_mesh_import_data
        for k,v in {'combine_meshes':True,'convert_scene':True,'convert_scene_unit':True,'import_uniform_scale':1.,'generate_lightmap_u_vs':False,'auto_generate_collision':False,'build_nanite':True,'remove_degenerates':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():data.set_editor_property(k,v)
        task.options=options;task.factory=u.FbxFactory();AT.import_asset_tasks([task])
        mesh=next(o for o in task.get_objects() if isinstance(o,u.StaticMesh))
        number=int(row['id'][-2:]);district=row['district']
        wall=old_walls[number-1] if number<=10 else ['WarmStone','RosePlaster','Plaster','SagePlaster'][number%4] if district=='working' else ['Limestone','Plaster','SagePlaster'][number%3] if district=='middle' else 'PaleStone' if district=='upper' else 'Limestone'
        roof={'slate':'Slate','copper':'Copper','oxblood':'Oxblood'}[row['parameters']['roof_material']]
        choices={'stone':wall,'trim':'Ivory','roof':roof,'glass':'AmberGlass','metal':'Iron','gold':'Brass','wood':'Walnut','floor':'Limestone','interior':'Plaster'}
        for i,slot in enumerate(mesh.static_materials):
            family=str(slot.material_slot_name).replace('Gothic_','');assert family in choices,family
            mesh.set_material(i,instances[choices[family]])
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        nanite=SM.get_nanite_settings(mesh);nanite.enabled=True;nanite.fallback_relative_error=.1
        SM.set_nanite_settings(mesh,nanite,True)
        for k,v in {'SourceHIP':'HoudiniWorkspace/gothic_building/gothic_building.hip','GeneratorVersion':'3.0','District':district,'Program':row['use'],'Dataset':row['id']}.items():u.EditorAssetLibrary.set_metadata_tag(mesh,k,v)
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        box=mesh.get_bounding_box();size=list((box.max-box.min).to_tuple())
        expected=[row['size_m'][0]*100,row['size_m'][2]*100,row['size_m'][1]*100]
        assert max(abs(a-b) for a,b in zip(size,expected))<.2,(row['id'],size,expected)
        actual=mesh.get_num_nanite_triangles()
        assert actual==row['triangles'],(row['id'],actual,row['triangles'])
        by_id[row['id']]={'id':row['id'],'label':row['label'],'asset':mesh.get_path_name(),'size_cm':size,'triangles':actual,'materials':[s.material_interface.get_path_name() for s in mesh.static_materials],'nanite':True,'collision':'complex_as_simple'}
        report['meshes']=list(by_id.values());(R/'asset_import.json').write_text(json.dumps(report,indent=2))
        u.log('CITY_IMPORTED '+row['id'])
    report['status']='passed';(R/'asset_import.json').write_text(json.dumps(report,indent=2))
    (R/'import_error.txt').unlink(missing_ok=True)
try:run()
except Exception:
    (R/'import_error.txt').write_text(traceback.format_exc());raise
