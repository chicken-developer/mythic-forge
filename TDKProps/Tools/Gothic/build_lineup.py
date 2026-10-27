"""Create the saved horizontal Gothic render-test level."""
from pathlib import Path
import json,math,traceback
import unreal as u
ROOT=Path('E:/Repos/mythic-forge')
REVIEW=ROOT/'TDKProps/Saved/GothicReview'
LEVEL='/Game/Gothic/Maps/L_Gothic_Lineup'
A='/Game/Gothic'
AT=u.AssetToolsHelpers.get_asset_tools()
EA=u.get_editor_subsystem(u.EditorActorSubsystem)
LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
M=u.MaterialEditingLibrary

def spawn(cls,label,location=(0,0,0),rotation=(0,0,0),folder='Studio'):
    a=EA.spawn_actor_from_class(cls,u.Vector(*location),u.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
    a.set_actor_label(label);a.set_folder_path(folder);a.tags=['GothicLineupGenerated'];return a

def flat(name,color,metal=0.,roughness=.8):
    path=A+'/Materials/'+name
    mat=u.load_asset(path) or AT.create_asset(name,A+'/Materials',u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(mat)
    c=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-300,0)
    c.set_editor_property('constant',u.LinearColor(*color,1.))
    M.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
    if name=='M_Gothic_StudioFloor':
        def expr(cls,x,y):return M.create_material_expression(mat,cls,x,y)
        world=expr(u.MaterialExpressionWorldPosition,-1200,-500)
        scale=expr(u.MaterialExpressionDivide,-1000,-500);scale.set_editor_property('const_b',200.)
        M.connect_material_expressions(world,'',scale,'A')
        frac=expr(u.MaterialExpressionFrac,-800,-500);M.connect_material_expressions(scale,'',frac,'')
        lines=[]
        for i in range(2):
            mask=expr(u.MaterialExpressionComponentMask,-600,-800+i*150)
            mask.set_editor_property('r',i==0);mask.set_editor_property('g',i==1)
            M.connect_material_expressions(frac,'',mask,'')
            sub=expr(u.MaterialExpressionSubtract,-400,-800+i*150);sub.set_editor_property('const_a',.015)
            M.connect_material_expressions(mask,'',sub,'B')
            mult=expr(u.MaterialExpressionMultiply,-200,-800+i*150);mult.set_editor_property('const_b',80.)
            M.connect_material_expressions(sub,'',mult,'A')
            clamp=expr(u.MaterialExpressionSaturate,0,-800+i*150);M.connect_material_expressions(mult,'',clamp,'');lines.append(clamp)
        maximum=expr(u.MaterialExpressionMax,200,-800)
        M.connect_material_expressions(lines[0],'',maximum,'A');M.connect_material_expressions(lines[1],'',maximum,'B')
        dark=expr(u.MaterialExpressionConstant3Vector,0,-300);dark.set_editor_property('constant',u.LinearColor(.068,.083,.089,1.))
        lerp=expr(u.MaterialExpressionLinearInterpolate,400,-300)
        M.connect_material_expressions(c,'',lerp,'A');M.connect_material_expressions(dark,'',lerp,'B');M.connect_material_expressions(maximum,'',lerp,'Alpha')
        M.connect_material_property(lerp,'',u.MaterialProperty.MP_BASE_COLOR)
    if name=='M_Gothic_Backdrop':
        mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
        M.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    for y,prop,val in [(150,u.MaterialProperty.MP_ROUGHNESS,roughness),(300,u.MaterialProperty.MP_METALLIC,metal)]:
        n=M.create_material_expression(mat,u.MaterialExpressionConstant,-300,y);n.set_editor_property('r',val);M.connect_material_property(n,'',prop)
    M.recompile_material(mat);u.EditorAssetLibrary.save_loaded_asset(mat);return mat

def cube(label,loc,dimensions,material,folder='Studio'):
    a=spawn(u.StaticMeshActor,label,loc,folder=folder)
    c=a.static_mesh_component;c.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    a.set_actor_scale3d(u.Vector(*(s/100. for s in dimensions)))
    c.set_material(0,material);return a

def camera(label,location,target,focal=35.,aspect=16/9):
    a=spawn(u.CineCameraActor,label,location,folder='Cameras')
    a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(*location),u.Vector(*target)),False)
    c=a.get_cine_camera_component()
    film=c.get_editor_property('filmback');film.sensor_width=36.;film.sensor_height=36./aspect;c.set_editor_property('filmback',film)
    lens=c.get_editor_property('lens_settings');lens.min_focal_length=10.;lens.max_focal_length=200.;c.set_editor_property('lens_settings',lens)
    c.set_editor_property('current_focal_length',focal)
    focus=c.get_editor_property('focus_settings');focus.focus_method=u.CameraFocusMethod.DISABLE;c.set_editor_property('focus_settings',focus)
    c.set_editor_property('constrain_aspect_ratio',True)
    return a

def build():
    assert (REVIEW/'materials_complete.txt').exists(),'Materials incomplete'
    records=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())['variants']
    # Validate actual engine bounds before placing anything.
    meshes=[]
    for r in records:
        mesh=u.load_asset(A+'/Buildings/SM_Gothic_'+r['id']);box=mesh.get_bounding_box()
        size=box.max-box.min
        expected=[r['size_m'][0]*100.,r['size_m'][2]*100.,r['size_m'][1]*100.]
        assert max(abs(a-b) for a,b in zip(size.to_tuple(),expected))<1.,(r['id'],size,expected)
        meshes.append((mesh,box,size))
    if u.EditorAssetLibrary.does_asset_exist(LEVEL):LE.load_level(LEVEL)
    else:assert LE.new_level(LEVEL)
    # Only replace actors in the dedicated generated test level.
    for old in EA.get_all_level_actors():
        if 'GothicLineupGenerated' in [str(t) for t in old.tags]:EA.destroy_actor(old)
    floor=flat('M_Gothic_StudioFloor',(.115,.145,.16),roughness=.88)
    base=flat('M_Gothic_Plinth',(.20,.245,.255),roughness=.8)
    edge=flat('M_Gothic_Accent',(.48,.34,.15),metal=.65,roughness=.48)
    total=sum(s.x+500. for _,_,s in meshes)
    cube('Studio_Floor',(0,-1500,-40),(total+30000,70000,80),floor)
    backdrop=flat('M_Gothic_Backdrop',(.15,.18,.20))
    cube('Studio_Backdrop',(0,-16000,10000),(total+30000,80,23000),backdrop)
    x=-total/2
    rows=[]
    for i,(r,(mesh,box,size)) in enumerate(zip(records,meshes)):
        x+=size.x/2+250.
        y=-box.max.y
        a=spawn(u.StaticMeshActor,r['id']+' | '+r['label'],(x,y,20),folder='Buildings')
        a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_mobility(u.ComponentMobility.STATIC)
        a.static_mesh_component.set_collision_profile_name('BlockAll')
        a.tags=['GothicLineupGenerated','GothicPreset',r['id']]
        cube('Base_'+r['id'],(x,y,8),(size.x+160,size.y+160,24),base,'Plinths')
        cube('Nameplate_'+r['id'],(x,170,12),(min(size.x+160,1500),120,24),edge,'Labels')
        text=spawn(u.TextRenderActor,'Label_'+r['id'],(x,238,43),(0,90,0),'Labels')
        tc=text.text_render;tc.set_text(f'{i+1:02d} / {r["label"].upper()}')
        tc.set_world_size(25.);tc.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
        tc.set_text_render_color(u.Color(224,225,205,255))
        rows.append({'id':r['id'],'label':r['label'],'actor':a.get_path_name(),'position_cm':list(a.get_actor_location().to_tuple()),'size_cm':list(size.to_tuple())})
        # Detail views use the same kind of perspective, independently framed.
        radius=math.sqrt(size.x**2+size.y**2+size.z**2)/2.
        camera('CAM_'+r['id'],(x+radius*2.2,y+radius*4.,size.z*.5+radius*1.8),(x,y,size.z*.5+20),40.)
        x+=size.x/2+250.
    sun=spawn(u.DirectionalLight,'Key_Warm',rotation=(-38,-118,0))
    sun.light_component.set_editor_property('intensity',4.0)
    sun.light_component.set_editor_property('light_color',u.Color(255,235,207,255))
    sun.light_component.set_editor_property('light_source_angle',3.)
    sun.light_component.set_editor_property('atmosphere_sun_light',True)
    fill=spawn(u.DirectionalLight,'Fill_Soft',rotation=(-28,-65,0))
    fill.light_component.set_editor_property('intensity',1.2)
    fill.light_component.set_editor_property('cast_shadows',False)
    sky=spawn(u.SkyLight,'Fill_Cool',(0,0,3000))
    sky.light_component.set_editor_property('real_time_capture',False)
    sky.light_component.set_editor_property('source_type',u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap',u.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    sky.light_component.set_editor_property('intensity',1.1)
    spawn(u.SkyAtmosphere,'SkyAtmosphere')
    fog=spawn(u.ExponentialHeightFog,'Studio_Haze')
    fog.component.set_editor_property('fog_density',.0008)
    fog.component.set_editor_property('fog_height_falloff',.12)
    pp=spawn(u.PostProcessVolume,'Exposure_Grade')
    pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':-.5,'override_bloom_intensity':True,'bloom_intensity':.12,'override_vignette_intensity':True,'vignette_intensity':.12}.items():settings.set_editor_property(k,v)
    pp.set_editor_property('settings',settings)
    hero=camera('CAM_Overview',(0,total*1.0,total*.42),(0,-900,1350),35.,2.5)
    groups=[('CAM_Group_A',0,3),('CAM_Group_B',3,7),('CAM_Group_C',7,10)]
    for name,start,end in groups:
        group=rows[start:end];center=sum(r['position_cm'][0] for r in group)/len(group)
        width=sum(r['size_cm'][0]+500 for r in group)
        height=max(r['size_cm'][2] for r in group)
        camera(name,(center+width*.10,width*1.35,height*1.12),(center,-750,height*.48),35.)
    start=spawn(u.PlayerStart,'PlayerStart_Lineup',(rows[0]['position_cm'][0],1800,220),(0,-90,0),'Gameplay')
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
    u.EditorLevelLibrary.set_level_viewport_camera_info(hero.get_actor_location(),hero.get_actor_rotation())
    u.SystemLibrary.execute_console_command(world,'r.ViewDistanceScale 2')
    u.SystemLibrary.execute_console_command(world,'r.Streaming.PoolSize 3000')
    u.SystemLibrary.execute_console_command(world,'r.ScreenPercentage 100')
    LE.save_current_level()
    (REVIEW/'lineup_scene.json').write_text(json.dumps({'map':LEVEL,'row_width_cm':total,'buildings':rows,'cameras':[a.get_actor_label() for a in EA.get_all_level_actors() if isinstance(a,u.CineCameraActor)]},indent=2))
    (REVIEW/'lineup_complete.txt').write_text('10 buildings in one horizontal row; saved render-test level')
try:build()
except Exception:
    (REVIEW/'lineup_error.txt').write_text(traceback.format_exc());raise

