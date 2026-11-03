"""One-shot import and gray-material review of the approved facade prototype."""
from pathlib import Path
import json
import traceback
import unreal as u

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Assets/_render/Saved/GothicReview/Facade'
OUT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'Assets/_render/SourceAssets/Gothic/FacadeReview'
ASSETS='/Game/Gothic/FacadeReview'
LEVEL=ASSETS+'/L_Gothic_Facade_Review'
AT=u.AssetToolsHelpers.get_asset_tools()
EA=u.get_editor_subsystem(u.EditorActorSubsystem)
LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
M=u.MaterialEditingLibrary

def spawn(cls,label,loc=(0,0,0),rot=(0,0,0),folder='Studio'):
    a=EA.spawn_actor_from_class(cls,u.Vector(*loc),u.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
    a.set_actor_label(label);a.set_folder_path(folder);a.tags=['GothicFacadeReview']
    return a

def flat(name,color,roughness=.52,unlit=False):
    mat=u.load_asset(ASSETS+'/Materials/'+name) or AT.create_asset(name,ASSETS+'/Materials',u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(mat)
    c=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-300,0)
    c.set_editor_property('constant',u.LinearColor(*color,1.))
    M.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
    if unlit:
        mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
        M.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    n=M.create_material_expression(mat,u.MaterialExpressionConstant,-300,150)
    n.set_editor_property('r',roughness);M.connect_material_property(n,'',u.MaterialProperty.MP_ROUGHNESS)
    M.recompile_material(mat);u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

def cube(label,loc,size,mat):
    a=spawn(u.StaticMeshActor,label,loc)
    a.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    a.set_actor_scale3d(u.Vector(*(s/100 for s in size)))
    a.static_mesh_component.set_material(0,mat)
    return a

def camera(label,loc,target,focal=50.,aspect=16/9):
    a=spawn(u.CineCameraActor,'CAM_'+label,loc,folder='Cameras')
    a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(*loc),u.Vector(*target)),False)
    c=a.get_cine_camera_component()
    film=c.filmback;film.sensor_width=36.;film.sensor_height=36./aspect;c.filmback=film
    lens=c.lens_settings;lens.min_focal_length=10.;lens.max_focal_length=200.;c.lens_settings=lens
    c.current_focal_length=focal
    focus=c.focus_settings;focus.focus_method=u.CameraFocusMethod.DISABLE;c.focus_settings=focus
    c.constrain_aspect_ratio=True
    return a

def run():
    dirty=u.EditorLoadingAndSavingUtils.get_dirty_map_packages()+u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    # Preserve any intervening user edits, rather than discard or save unrelated packages.
    unrelated=[p.get_name() for p in dirty if not p.get_name().startswith(ASSETS+'/')]
    assert not unrelated, 'Unsaved packages outside the review folder: '+str(unrelated)
    manifest=json.loads((SOURCE/'facade_manifest.json').read_text())
    clay=flat('M_Facade_Clay',(.24,.24,.24))
    floor=flat('M_Facade_Floor',(.07,.075,.08),.8)
    backdrop=flat('M_Facade_Backdrop',(.085,.095,.11),.8,True)
    u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    imported={};report={'map':LEVEL,'meshes':[],'material_normal_texture':False,'concept_revision':manifest.get('concept_revision')}
    for row in manifest['variants']:
        name='SM_FacadeReview_'+row['name']
        task=u.AssetImportTask()
        for k,v in {'filename':str(SOURCE/(name+'.fbx')),'destination_path':ASSETS+'/Meshes','destination_name':name,'automated':True,'replace_existing':True,'replace_existing_settings':True,'save':True}.items():task.set_editor_property(k,v)
        opts=u.FbxImportUI()
        for k,v in {'automated_import_should_detect_type':False,'mesh_type_to_import':u.FBXImportType.FBXIT_STATIC_MESH,'import_as_skeletal':False,'import_materials':False,'import_textures':False}.items():opts.set_editor_property(k,v)
        data=opts.static_mesh_import_data
        for k,v in {'combine_meshes':True,'convert_scene':True,'convert_scene_unit':True,'import_uniform_scale':1.,'generate_lightmap_u_vs':False,'auto_generate_collision':False,'build_nanite':row['name']!='Legacy','remove_degenerates':True,'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():data.set_editor_property(k,v)
        task.options=opts;task.factory=u.FbxFactory();AT.import_asset_tasks([task])
        mesh=next(o for o in task.get_objects() if isinstance(o,u.StaticMesh))
        for i in range(len(mesh.static_materials)):mesh.set_material(i,clay)
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        if row['name']!='Legacy':
            settings=SM.get_nanite_settings(mesh);settings.enabled=True
            settings.set_editor_property('fallback_relative_error',0.)
            SM.set_nanite_settings(mesh,settings,True)
        u.EditorAssetLibrary.set_metadata_tag(mesh,'SourceHIP','Assets/hda_gothic_building/facade_review/gothic_facade_review.hip')
        u.EditorAssetLibrary.set_metadata_tag(mesh,'Status','Facade prototype for gray-material review; not propagated to the ten buildings')
        u.EditorAssetLibrary.save_loaded_asset(mesh)
        size=mesh.get_bounding_box().max-mesh.get_bounding_box().min
        assert abs(size.x-442)<.1 and abs(size.z-878)<.1,(name,size)
        actual=mesh.get_num_nanite_triangles() if row['name']!='Legacy' else mesh.get_num_triangles(0)
        assert actual==row['triangles'],(name,actual,row['triangles'])
        imported[row['name']]=mesh
        report['meshes'].append({'name':row['name'],'asset':mesh.get_path_name(),'triangles':actual,'size_cm':list(size.to_tuple()),'nanite':bool(SM.get_nanite_settings(mesh).enabled),'collision':str(SM.get_collision_complexity(mesh)),'uv_channels':SM.get_num_uv_channels(mesh,0)})
        (OUT/'import_progress.json').write_text(json.dumps(report,indent=2))
    if u.EditorAssetLibrary.does_asset_exist(LEVEL):assert LE.load_level(LEVEL)
    else:assert LE.new_level(LEVEL)
    for a in EA.get_all_level_actors():
        if 'GothicFacadeReview' in [str(t) for t in a.tags]:EA.destroy_actor(a)
    cube('Studio_Floor',(0,0,-13),(18000,18000,20),floor)
    cube('Studio_Backdrop',(0,1300,1900),(8000,50,4400),backdrop)
    for name,x in [('Legacy',290),('Game',-290)]:
        a=spawn(u.StaticMeshActor,'Facade_'+name,(x,0,0),folder='Comparison')
        a.static_mesh_component.set_static_mesh(imported[name])
        a.static_mesh_component.set_collision_profile_name('BlockAll')
        a.static_mesh_component.set_mobility(u.ComponentMobility.STATIC)
        cube('Plinth_'+name,(x,0,-3),(480,180,6),floor)
        text=spawn(u.TextRenderActor,'Label_'+name,(x,-112,13),(0,-90,0),'Labels')
        tc=text.text_render
        tc.set_text('01 / ORIGINAL ASSEMBLY' if name=='Legacy' else '02 / CONCEPT GEOMETRY STUDY')
        tc.set_world_size(14.);tc.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
        tc.set_text_render_color(u.Color(224,224,224,255))
    sun=spawn(u.DirectionalLight,'Key_Raking',rot=(-32,38,0))
    sun.light_component.set_editor_property('intensity',4.)
    sun.light_component.set_editor_property('light_source_angle',2.)
    fill=spawn(u.DirectionalLight,'Fill_Soft',rot=(-28,140,0))
    fill.light_component.set_editor_property('intensity',.8)
    fill.light_component.set_editor_property('cast_shadows',False)
    sky=spawn(u.SkyLight,'Ambient')
    sky.light_component.set_editor_property('source_type',u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap',u.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    sky.light_component.set_editor_property('intensity',.6)
    pp=spawn(u.PostProcessVolume,'Manual_Exposure')
    pp.unbound=True;settings=pp.settings
    for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':0.,'override_bloom_intensity':True,'bloom_intensity':0.,'override_vignette_intensity':True,'vignette_intensity':0.}.items():settings.set_editor_property(k,v)
    pp.settings=settings
    hero=camera('Comparison',(160,-2250,1050),(0,0,425),40.)
    for name,x in [('Legacy',290),('Game',-290)]:
        camera(name+'_Arch',(x+170,-1250,458),(x,0,365),50.)
        camera(name+'_Pinnacle',(x+350,-600,845),(x+172,0,767),50.)
        camera(name+'_Cornice',(x+125,-380,650),(x+80,0,625),65.)
    camera('Game_ThreeQuarter',(360,-1500,990),(-290,0,425),52.)
    # Dedicated high-source evidence, outside the two-facade comparison framing.
    high=spawn(u.StaticMeshActor,'HighSource_Review',(-2000,0,0),folder='High Source')
    high.static_mesh_component.set_static_mesh(imported['High'])
    high.static_mesh_component.set_cast_shadow(False)
    camera('High_Pinnacle',(-1650,-600,845),(-1828,0,767),50.)
    camera('High_Arch',(-1830,-1250,458),(-2000,0,365),50.)
    spawn(u.PlayerStart,'PlayerStart_Review',(-290,-700,190),(0,90,0),'Gameplay')
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
    u.EditorLevelLibrary.set_level_viewport_camera_info(hero.get_actor_location(),hero.get_actor_rotation())
    for command in ['r.ScreenPercentage 100','r.ViewDistanceScale 2','r.Streaming.PoolSize 3000']:
        u.SystemLibrary.execute_console_command(world,command)
    LE.save_current_level()
    report['cameras']=[a.get_actor_label() for a in EA.get_all_level_actors() if isinstance(a,u.CineCameraActor)]
    (OUT/'unreal_review.json').write_text(json.dumps(report,indent=2))
    (OUT/'build_complete.txt').write_text('Saved gray-material comparison; original level and ten building assets preserved.')
    (OUT/'build_error.txt').unlink(missing_ok=True)

try:run()
except Exception:
    (OUT/'build_error.txt').write_text(traceback.format_exc());raise
