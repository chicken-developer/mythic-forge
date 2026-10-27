"""Shared explicit Unreal scene construction helpers for the Gothic city."""
from pathlib import Path
import json
import math
import datetime
import unreal as u

ROOT=Path('E:/Repos/mythic-forge');R=ROOT/'TDKProps/Saved/GothicReview/City';R.mkdir(parents=True,exist_ok=True)
A='/Game/Gothic';EA=u.get_editor_subsystem(u.EditorActorSubsystem);LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
AT=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
TAG='GothicCityGenerated'

def spawn(cls,label,loc=(0,0,0),rot=(0,0,0),folder='City'):
    a=EA.spawn_actor_from_class(cls,u.Vector(*loc),u.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
    a.set_actor_label(label);a.set_folder_path(folder);a.tags=[TAG];return a

def flat(name,color,roughness=.8,metallic=0.,glow=0.,paving=False):
    folder=A+'/Materials/City';mat=u.load_asset(folder+'/'+name) or AT.create_asset(name,folder,u.Material,u.MaterialFactoryNew())
    if u.EditorAssetLibrary.get_metadata_tag(mat,'CityFlatVersion')=='1':return mat
    M.delete_all_material_expressions(mat)
    c=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-600,0);c.constant=u.LinearColor(*color,1.)
    result=c
    if paving:
        world=M.create_material_expression(mat,u.MaterialExpressionWorldPosition,-1600,-600)
        scale=M.create_material_expression(mat,u.MaterialExpressionDivide,-1400,-600);scale.set_editor_property('const_b',75.)
        M.connect_material_expressions(world,'',scale,'A')
        frac=M.create_material_expression(mat,u.MaterialExpressionFrac,-1200,-600);M.connect_material_expressions(scale,'',frac,'')
        edges=[]
        for i in range(2):
            mask=M.create_material_expression(mat,u.MaterialExpressionComponentMask,-1000,-600+i*130);mask.set_editor_property('r',i==0);mask.set_editor_property('g',i==1)
            M.connect_material_expressions(frac,'',mask,'')
            sub=M.create_material_expression(mat,u.MaterialExpressionSubtract,-800,-600+i*130);sub.set_editor_property('const_a',.035)
            M.connect_material_expressions(mask,'',sub,'B')
            amp=M.create_material_expression(mat,u.MaterialExpressionMultiply,-600,-600+i*130);amp.set_editor_property('const_b',30.)
            M.connect_material_expressions(sub,'',amp,'A');edges.append(amp)
        maximum=M.create_material_expression(mat,u.MaterialExpressionMax,-400,-600)
        M.connect_material_expressions(edges[0],'',maximum,'A');M.connect_material_expressions(edges[1],'',maximum,'B')
        clamp=M.create_material_expression(mat,u.MaterialExpressionSaturate,-200,-600);M.connect_material_expressions(maximum,'',clamp,'')
        grout=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-300,-200);grout.constant=u.LinearColor(color[0]*.38,color[1]*.38,color[2]*.38,1.)
        mix=M.create_material_expression(mat,u.MaterialExpressionLinearInterpolate,0,0)
        M.connect_material_expressions(c,'',mix,'A');M.connect_material_expressions(grout,'',mix,'B');M.connect_material_expressions(clamp,'',mix,'Alpha');result=mix
    M.connect_material_property(result,'',u.MaterialProperty.MP_BASE_COLOR)
    for prop,val,y in [(u.MaterialProperty.MP_ROUGHNESS,roughness,160),(u.MaterialProperty.MP_METALLIC,metallic,280)]:
        n=M.create_material_expression(mat,u.MaterialExpressionConstant,-300,y);n.r=val;M.connect_material_property(n,'',prop)
    if glow:
        amp=M.create_material_expression(mat,u.MaterialExpressionMultiply,0,-200);amp.set_editor_property('const_b',glow);M.connect_material_expressions(c,'',amp,'A');M.connect_material_property(amp,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    M.recompile_material(mat);u.EditorAssetLibrary.set_metadata_tag(mat,'CityFlatVersion','1');u.EditorAssetLibrary.save_loaded_asset(mat);return mat

def mesh(label,asset,loc=(0,0,0),yaw=0.,folder='Buildings',scale=(1,1,1)):
    a=spawn(u.StaticMeshActor,label,loc,(0,yaw,0),folder)
    a.static_mesh_component.set_static_mesh(asset);a.static_mesh_component.set_collision_profile_name('BlockAll')
    a.set_actor_scale3d(u.Vector(*scale));return a

def cube(label,loc,size,material,folder='Streets'):
    a=mesh(label,u.load_asset('/Engine/BasicShapes/Cube'),loc,folder=folder,scale=tuple(x/100 for x in size))
    a.static_mesh_component.set_material(0,material);return a

def camera(label,location,target,focal=35.,aspect=16/9):
    a=spawn(u.CineCameraActor,'CAM_'+label,location,folder='Cameras')
    a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(*location),u.Vector(*target)),False)
    c=a.get_cine_camera_component();film=c.filmback;film.sensor_width=36.;film.sensor_height=36/aspect;c.filmback=film
    lens=c.lens_settings;lens.min_focal_length=10.;lens.max_focal_length=200.;c.lens_settings=lens;c.current_focal_length=focal
    focus=c.focus_settings;focus.focus_method=u.CameraFocusMethod.DISABLE;c.focus_settings=focus;c.constrain_aspect_ratio=True
    return a

def begin(level):
    dirty=u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if dirty and world.get_path_name().split('.')[0]!=level:
        # Preserve intervening user scene edits in a concrete backup before switching.
        name='BeforeCity_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        assert u.EditorLoadingAndSavingUtils.save_map(world,A+'/Backups/'+name)
    if u.EditorAssetLibrary.does_asset_exist(level):assert LE.load_level(level)
    else:assert LE.new_level(level)
    for a in EA.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:EA.destroy_actor(a)

def lighting():
    sun=spawn(u.DirectionalLight,'Sun_Afternoon',rot=(-38,35,0),folder='Lighting')
    sun.light_component.set_editor_property('intensity',4.5);sun.light_component.set_editor_property('light_color',u.Color(255,237,211,255))
    sun.light_component.set_editor_property('light_source_angle',2.);sun.light_component.set_editor_property('atmosphere_sun_light',True)
    sky=spawn(u.SkyLight,'Sky_Ambient',folder='Lighting')
    sky.light_component.set_editor_property('source_type',u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap',u.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    sky.light_component.set_editor_property('intensity',1.0)
    spawn(u.SkyAtmosphere,'SkyAtmosphere',folder='Lighting')
    fog=spawn(u.ExponentialHeightFog,'Distance_Haze',folder='Lighting')
    fog.component.set_editor_property('fog_density',.0010);fog.component.set_editor_property('fog_height_falloff',.13)
    pp=spawn(u.PostProcessVolume,'City_Exposure',folder='Lighting');pp.unbound=True;s=pp.settings
    for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':-.25,'override_bloom_intensity':True,'bloom_intensity':.15,'override_vignette_intensity':True,'vignette_intensity':.08}.items():s.set_editor_property(k,v)
    pp.settings=s

def finish(hero,level,report):
    EA.clear_actor_selection_set();u.EditorLevelLibrary.set_level_viewport_camera_info(hero.get_actor_location(),hero.get_actor_rotation())
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    game=u.EditorAssetLibrary.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode')
    world.get_world_settings().set_editor_property('default_game_mode',game or u.GameModeBase)
    for command in ['r.ScreenPercentage 100','r.Streaming.PoolSize 3000','r.ViewDistanceScale 2']:
        u.SystemLibrary.execute_console_command(world,command)
    LE.save_current_level()
    report['map']=level;report['cameras']=[a.get_actor_label() for a in EA.get_all_level_actors() if isinstance(a,u.CineCameraActor)]
    return report
