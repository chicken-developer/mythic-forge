"""Inspect, apply and verify the SkyTown adaptation in the open project editor."""
from pathlib import Path
import json
import hashlib
import os
import traceback
import unreal as u

P = Path(__file__).resolve().parents[2]
R = P / 'Saved/SkyTownReview'
OUT = P / 'RenderTests/SkyTown'
R.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
EA = u.get_editor_subsystem(u.EditorActorSubsystem)
LE = u.get_editor_subsystem(u.LevelEditorSubsystem)
SM = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
MEL = u.MaterialEditingLibrary


def inspect():
    report = {'world': u.EditorLevelLibrary.get_editor_world().get_path_name(),
              'meshes': {}, 'materials': {}, 'capabilities': {}}
    names = ['SM_House01', 'SM_House02', 'SM_House03', 'SM_House04',
             'SM_Furniture01_Table01', 'SM_Furniture01_Bench01',
             'SM_Furniture01_Chair01', 'SM_Furniture01_Shelf01',
             'SM_Furniture01_Rack01', 'SM_Bed01', 'SM_Barrel01', 'SM_Box01',
             'SM_FruitCrate01', 'SM_Basket01', 'SM_Lamp01']
    for name in names:
        mesh = u.load_asset('/Game/SkyTown/Meshes/' + name)
        assert mesh, name
        box = mesh.get_bounding_box()
        report['meshes'][name] = {'min_cm': list(box.min.to_tuple()),
            'max_cm': list(box.max.to_tuple()), 'triangles': mesh.get_num_triangles(0),
            'materials': [s.material_interface.get_path_name() if s.material_interface else None
                          for s in mesh.static_materials]}
    wanted = ['MI_House01WoodenStuff01', 'MI_HouseFloor_Tileable01',
              'MI_HouseWall01', 'MI_HouseWall02', 'MI_HouseWall03',
              'MI_StoneRandomTileable01', 'MI_RoofSlate02', 'MI_GlassWindow01',
              'MI_WindowsAndDoors01', 'MI_Tools01']
    for name in wanted:
        mat = u.load_asset('/Game/SkyTown/Materials/' + name)
        assert mat, name
        textures = {}
        for param in MEL.get_texture_parameter_names(mat):
            tex = MEL.get_material_instance_texture_parameter_value(mat, param)
            textures[str(param)] = tex.get_path_name() if tex else None
        report['materials'][name] = {'parent': mat.parent.get_path_name(),
            'textures': textures,
            'scalars': {str(n): MEL.get_material_instance_scalar_parameter_value(mat, n)
                        for n in MEL.get_scalar_parameter_names(mat)}}
    for cls, methods in [(u.SubobjectDataSubsystem, ['delete_subobjects', 'delete_subobject']),
                         (u.StaticMeshDescription, ['get_vertex_positions', 'get_vertex_position']),
                         (u.MaterialEditingLibrary, ['get_used_textures', 'get_vector_parameter_names'])]:
        for method in methods:
            report['capabilities'][cls.__name__ + '.' + method] = str(getattr(getattr(cls, method, None), '__doc__', None))
    (R / 'inspection.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    u.log('SKYTOWN_INSPECTION_COMPLETE')


def export_props():
    names = ['SM_Furniture01_Table01', 'SM_Furniture01_Bench01',
             'SM_Furniture01_Chair01', 'SM_Furniture01_Shelf01',
             'SM_Furniture01_Rack01', 'SM_Bed01', 'SM_Barrel01', 'SM_Box01',
             'SM_FruitCrate01', 'SM_Basket01', 'SM_Lamp01',
             'SM_Ceramics01_Item01', 'SM_IvyFlowers01', 'SM_Tapestry01',
             'SM_Bathtub01', 'SM_Books01_Item01']
    folder = R / 'FBX'
    folder.mkdir(exist_ok=True)
    report = {}
    for name in names:
        mesh = u.load_asset('/Game/SkyTown/Meshes/' + name)
        if not mesh:
            if name in names[-2:]:
                continue
            raise ValueError(name)
        task = u.AssetExportTask()
        task.object = mesh
        task.filename = str(folder / (name + '.fbx'))
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = u.StaticMeshExporterFBX()
        options = u.FbxExportOption()
        options.ascii = False
        options.level_of_detail = False
        options.collision = False
        task.options = options
        assert u.Exporter.run_asset_export_task(task), name
        box = mesh.get_bounding_box()
        report[name] = {'file': task.filename, 'min_cm': list(box.min.to_tuple()),
            'max_cm': list(box.max.to_tuple()),
            'materials': {str(s.material_slot_name): s.material_interface.get_path_name()
                          for s in mesh.static_materials}}
    (R / 'prop_exports.json').write_text(json.dumps(report, indent=2))


def materials():
    """Use SkyTown tileable maps on procedural surfaces; keep vendor prop materials."""
    AT = u.AssetToolsHelpers.get_asset_tools()
    folder = '/Game/CatalogShared/Materials'
    master = u.load_asset(folder + '/M_SkyTownSurface')
    if not master:
        master = AT.create_asset('M_SkyTownSurface', folder, u.Material, u.MaterialFactoryNew())
    MEL.delete_all_material_expressions(master)
    def expression(cls, x, y, **properties):
        node = MEL.create_material_expression(master, cls, x, y)
        for key, value in properties.items():
            node.set_editor_property(key, value)
        return node
    def connect(a, output, b, pin):
        assert MEL.connect_material_expressions(a, output, b, pin)
    samples = {}
    for i, (param, suffix, sampler) in enumerate([
        ('BaseColor', '_BC', u.MaterialSamplerType.SAMPLERTYPE_COLOR),
        ('Normal', '_Norm', u.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        ('HRM', '_HRM', u.MaterialSamplerType.SAMPLERTYPE_MASKS)]):
        samples[param] = expression(u.MaterialExpressionTextureSampleParameter2D, -800, i*220,
            parameter_name=param, texture=u.load_asset('/Game/SkyTown/Textures/T_HouseFloor_Tileable01'+suffix), sampler_type=sampler)
    tint = expression(u.MaterialExpressionVectorParameter,-800,-240,parameter_name='Tint',default_value=u.LinearColor(1,1,1,1))
    flat = expression(u.MaterialExpressionVectorParameter,-800,-80,parameter_name='FlatColor',default_value=u.LinearColor(1,1,1,1))
    weight = expression(u.MaterialExpressionScalarParameter,-800,-160,parameter_name='BaseColorTextureWeight',default_value=1.)
    blend = expression(u.MaterialExpressionLinearInterpolate,-550,-80)
    connect(flat,'RGB',blend,'A');connect(samples['BaseColor'],'RGB',blend,'B');connect(weight,'',blend,'Alpha')
    color = expression(u.MaterialExpressionMultiply,-400,0)
    connect(blend,'',color,'A');connect(tint,'RGB',color,'B')
    MEL.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
    strength = expression(u.MaterialExpressionVectorParameter,-800,650,parameter_name='NormalScale',default_value=u.LinearColor(.65,.65,1,1))
    normal = expression(u.MaterialExpressionMultiply,-400,220)
    connect(samples['Normal'],'RGB',normal,'A');connect(strength,'RGB',normal,'B')
    unit = expression(u.MaterialExpressionNormalize,-180,220);connect(normal,'',unit,'')
    MEL.connect_material_property(unit,'',u.MaterialProperty.MP_NORMAL)
    roughness_scale=expression(u.MaterialExpressionScalarParameter,-800,850,parameter_name='RoughnessScale',default_value=1.)
    roughness=expression(u.MaterialExpressionMultiply,-400,440)
    connect(samples['HRM'],'G',roughness,'A');connect(roughness_scale,'',roughness,'B')
    MEL.connect_material_property(roughness,'',u.MaterialProperty.MP_ROUGHNESS)
    metal = expression(u.MaterialExpressionScalarParameter,-400,650,parameter_name='Metallic',default_value=0.)
    MEL.connect_material_property(metal,'',u.MaterialProperty.MP_METALLIC)
    emission = expression(u.MaterialExpressionScalarParameter,-400,-240,parameter_name='Emission',default_value=0.)
    glow = expression(u.MaterialExpressionMultiply,-180,-240)
    connect(color,'',glow,'A');connect(emission,'',glow,'B')
    MEL.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(master)
    u.EditorAssetLibrary.save_loaded_asset(master)
    presets = {
        'wood': ('T_HouseFloor_Tileable01',(0.42,.32,.24)),
        'plaster': ('T_HouseWallPlaster_Tileable01',(1.,.94,.82)),
        'tile': ('T_RoofSlate02',(1.,1.,1.)),
        'stone': ('T_StoneRandomTileable01',(.95,.92,.86)),
        'teal': ('T_HouseFloor_Tileable01',(.20,.55,.48)),
        'glass': ('T_GlassStained01',(.10,.18,.17)),
        'metal': ('T_HouseFloor_Tileable01',(.14,.16,.17)),
        'canvas': ('T_Tents01',(.55,.64,.53)),
        'sail': ('T_Tents01',(.92,.85,.68)),
        'ember': ('T_HouseWallPlaster_Tileable01',(1.,.25,.06)),
        'water': ('T_HouseWallPlaster_Tileable01',(.08,.25,.30)),
        'leaf': ('T_HouseWallPlaster_Tileable01',(.15,.35,.10)),
        'flower': ('T_HouseWallPlaster_Tileable01',(.85,.30,.15)),
        'light': ('T_GlassStained01',(1.,.73,.32)),
        'rock': ('T_StoneRandomTileable01',(.75,.77,.70))}
    gothic = {'Limestone':'stone','PaleStone':'stone','WarmStone':'stone','DarkStone':'stone',
        'Ivory':'stone','Plaster':'plaster','RosePlaster':'plaster','SagePlaster':'plaster',
        'BluePlaster':'plaster','Slate':'tile','Copper':'tile','Oxblood':'tile',
        'AmberGlass':'glass','Iron':'metal','Brass':'metal','Walnut':'wood'}
    report = []
    for family in ('Medieval','Gothic'):
        entries = {key:key for key in presets} if family=='Medieval' else gothic
        for name, key in entries.items():
            path = '/Game/Medieval/Materials/MI_MD_'+name if family=='Medieval' else '/Game/Gothic/Materials/City/MI_City_'+name
            instance = u.load_asset(path)
            assert instance,path
            source, tint_value = presets[key]
            if family=='Gothic':
                tint_value = {'RosePlaster':(1.,.70,.57),'SagePlaster':(.71,.84,.66),'BluePlaster':(.63,.76,.90),
                    'DarkStone':(.38,.40,.42),'PaleStone':(1.04,1.01,.94),'Ivory':(1.08,1.04,.94),
                    'Oxblood':(.95,.51,.38),'Copper':(.48,.79,.68)}.get(name,tint_value)
            MEL.set_material_instance_parent(instance,master)
            used = {}
            for parameter,suffix in [('BaseColor','_BC'),('Normal','_Norm'),('HRM','_HRM')]:
                texture_name='T_GlassWaves_Norm' if key=='glass' and parameter=='Normal' else source+suffix
                tex = u.load_asset('/Game/SkyTown/Textures/'+texture_name)
                assert tex,(source,suffix)
                MEL.set_material_instance_texture_parameter_value(instance,parameter,tex)
                used[parameter] = tex.get_path_name()
            MEL.set_material_instance_vector_parameter_value(instance,'Tint',u.LinearColor(*tint_value,1))
            strength=.25 if key=='glass' else 0. if key=='light' else .65
            MEL.set_material_instance_vector_parameter_value(instance,'NormalScale',u.LinearColor(strength,strength,1,1))
            MEL.set_material_instance_scalar_parameter_value(instance,'BaseColorTextureWeight',0. if key in ('glass','light') else 1.)
            MEL.set_material_instance_scalar_parameter_value(instance,'RoughnessScale',.2 if key=='glass' else 1.)
            MEL.set_material_instance_scalar_parameter_value(instance,'Metallic',.72 if key=='metal' else 0.)
            MEL.set_material_instance_scalar_parameter_value(instance,'Emission',2.2 if key=='light' else .12 if key=='glass' else 0.)
            MEL.update_material_instance(instance)
            u.EditorAssetLibrary.set_metadata_tag(instance,'TextureSource','SkyTown')
            u.EditorAssetLibrary.save_loaded_asset(instance)
            report.append({'asset':path,'textures':used,'basecolor_texture_weight':0. if key in ('glass','light') else 1.})
    (OUT / 'materials.json').write_text(json.dumps({'status':'passed','materials':report},indent=2))


def refresh_hdas():
    rows = []
    for family in ('Medieval','Gothic'):
        source = P/'SourceAssets/Houdini'/family/'Editor'/(family.lower()+'_building_editable.hda')
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = '/Game/'+family+'/Houdini'
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.save = True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = u.load_asset(task.destination_path+'/'+source.stem)
        assert isinstance(asset,u.HoudiniAsset),family
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        u.EditorAssetLibrary.set_metadata_tag(asset,'EditableSourceSHA',digest)
        u.EditorAssetLibrary.save_loaded_asset(asset)
        rows.append({'asset':asset.get_path_name(),'sha256':digest})
    (OUT/'unreal_hdas.json').write_text(json.dumps({'status':'passed','assets':rows},indent=2))


def validate():
    report = {'status':'running','meshes':[],'catalogs':[]}
    for family,qualities in [('Medieval',('low','normal','high')),('Gothic',('normal',))]:
        hda_source=P/'SourceAssets/Houdini'/family/'Editor'/(family.lower()+'_building_editable.hda')
        hda_asset=u.load_asset('/Game/'+family+'/Houdini/'+hda_source.stem)
        assert hda_asset,family
        assert u.EditorAssetLibrary.get_metadata_tag(hda_asset,'EditableSourceSHA')==hashlib.sha256(hda_source.read_bytes()).hexdigest(),(family,'HDA source differs')
        for quality in qualities:
            manifest = P/'SourceAssets'/family/('mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json')
            rows = json.loads(manifest.read_text())['variants']
            for row in rows:
                path = '/Game/'+family+'/Buildings'+('' if quality=='normal' else '/'+quality.title())+'/SM_'+family+'_'+row['id']
                mesh = u.load_asset(path)
                assert mesh,path
                assert u.EditorAssetLibrary.get_metadata_tag(mesh,'GeneratorVersion')==row['generator_version'],path
                assert not SM.get_lod_build_settings(mesh,0).recompute_normals,path
                imported = mesh.get_static_mesh_description(0).get_triangle_count()
                assert imported==row['triangles'],(path,imported,row['triangles'])
                assert SM.get_nanite_settings(mesh).enabled==(quality!='low'),path
                assert row['opening_clearance']['blocked_openings']==0,path
                assert row['skytown_assets'],(path,'no SkyTown props')
                sky_slots = []
                for slot in mesh.static_materials:
                    material = slot.material_interface
                    assert material,(path,slot.material_slot_name)
                    if material.get_path_name().startswith('/Game/SkyTown/'):
                        sky_slots.append(material.get_path_name())
                    else:
                        assert u.EditorAssetLibrary.get_metadata_tag(material,'TextureSource')=='SkyTown',(path,material)
                        normal = MEL.get_material_instance_texture_parameter_value(material,'Normal')
                        assert normal and normal.get_path_name().startswith('/Game/SkyTown/Textures/'),(path,material)
                        assert not normal.srgb and normal.compression_settings==u.TextureCompressionSettings.TC_NORMALMAP,normal
                assert sky_slots,(path,'missing prop material slots')
                report['meshes'].append({'family':family,'id':row['id'],'quality':quality,
                    'asset':path,'triangles':imported,'generator_version':row['generator_version'],
                    'source_fbx_sha256':row['fbx_sha256'],
                    'skytown_assets':row['skytown_assets'],'skytown_material_slots':sky_slots,
                    'opening_clearance':row['opening_clearance']})
                (OUT/'unreal_validation.json').write_text(json.dumps(report,indent=2))
        level='/Game/'+family+'/Maps/L_'+family+'_Catalog'
        assert LE.load_level(level),level
        actors = EA.get_all_level_actors()
        rows = json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants']
        counts=[]
        for row in rows:
            matches=[a for a in actors if family+'Catalog' in [str(t) for t in a.tags] and row['id'] in [str(t) for t in a.tags]]
            assert len(matches)==1,(family,row['id'],len(matches))
            actor=matches[0]
            meshes=actor.get_components_by_class(u.StaticMeshComponent)
            assert len(meshes)==1 and meshes[0].static_mesh,(family,row['id'])
            lights=actor.get_components_by_class(u.PointLightComponent)
            assert len(lights)==len(row['interior_lights']),(family,row['id'],'lights')
            for i,source in enumerate(row['interior_lights']):
                light=next(l for l in lights if str(l.get_name())=='InteriorLight_'+str(i))
                x,y,z=source['position_m']
                assert (light.relative_location-u.Vector(x*100,z*100,y*100)).length()<.05,(family,row['id'],i)
            counts.append({'id':row['id'],'lights':len(lights)})
        report['catalogs'].append({'map':level,'buildings':len(counts),'records':counts})
        del actors,matches,actor,meshes,lights
    report['status']='passed'
    (OUT/'unreal_validation.json').write_text(json.dumps(report,indent=2))


def capture():
    request = json.loads((R / 'request.json').read_text(encoding='utf-8-sig'))
    previous = u.EditorLevelLibrary.get_level_viewport_camera_info()
    previous_pilot=LE.get_pilot_level_actor()
    previous_game_view=LE.editor_get_game_view()
    selected = list(EA.get_selected_level_actors())
    created = []
    reference_mesh = u.load_asset('/Game/SkyTown/Meshes/SM_House02')
    reference_box = reference_mesh.get_bounding_box()
    reference_center = (reference_box.min + reference_box.max) * .5 + u.Vector(0, 0, 100000)
    reference_extent = (reference_box.max - reference_box.min) * .5
    reference_radius = max(reference_extent.x, reference_extent.y, reference_extent.z)
    failed_camera_location = reference_center + u.Vector(-reference_radius*1.45,-reference_radius*2.75,reference_radius*.40)
    for item in EA.get_all_level_actors():
        failed_reference = (isinstance(item,u.StaticMeshActor)
            and item.static_mesh_component.static_mesh == reference_mesh
            and (item.get_actor_location()-u.Vector(0,0,100000)).length()<.01)
        failed_camera = isinstance(item,u.CineCameraActor) and (item.get_actor_location()-failed_camera_location).length()<.5
        if 'SkyTownReviewTemporary' in [str(t) for t in item.tags] or failed_reference or failed_camera:
            EA.destroy_actor(item)
    if request.get('reference'):
        path = '/Game/SkyTown/Meshes/' + request['reference']
        actor = EA.spawn_actor_from_class(u.StaticMeshActor, u.Vector(0, 0, 100000))
        actor.static_mesh_component.set_static_mesh(u.load_asset(path))
    else:
        path = '/Game/{f}/Blueprints/BP_{f}_{id}'.format(f=request['family'], id=request['id'])
        actor = EA.spawn_actor_from_class(u.EditorAssetLibrary.load_blueprint_class(path), u.Vector(0, 0, 100000))
    created.append(actor)
    actor.tags = ['SkyTownReviewTemporary']
    center, extent = actor.get_actor_bounds(False)
    radius = max(extent.x, extent.y, extent.z)
    eye = center + u.Vector(-radius * 2.3, -radius * 5.4, radius * .8)
    target = center
    focal = 36.
    view = request.get('view','overview')
    if not request.get('reference') and view in ('interior','joint','balcony'):
        family=request['family']
        row=next(r for r in json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants'] if r['id']==request['id'])
        parameters=row['parameters'];origin=row['source_origin_m']
        def world(point):
            return u.MathLibrary.transform_location(actor.get_actor_transform(),
                u.Vector((point[0]-origin[0])*100,(point[2]-origin[2])*100,(point[1]-origin[1])*100))
        if family=='Medieval':
            w=parameters['width_m'];d=parameters['depth_m'];base=parameters['foundation_height_m']
            if view=='interior':
                eye=world((-w*.28,base+1.65,-d*.23));target=world((-w*.28,base+1.3,d*.32));focal=24.
            elif view=='balcony':
                fh=parameters['floor_height_m']
                eye=world((0,base+fh-.55,-d/2-8.));target=world((0,base+fh-.25,-d/2));focal=24.
            else:
                pattern=parameters['front_bays'].split('/')[0]
                bay=pattern.index('W');x=-w/2+(bay+.5)*w/len(pattern)
                eye=world((x+.10,base+1.95,-d/2-3.2));target=world((x,base+1.80,-d/2));focal=24.
        else:
            w=parameters['nave_width'];d=parameters['urban_depth'];fh=parameters['floor_height']
            if view=='interior':
                eye=world((-w*.28,1.75,d*.20));target=world((-w*.22,1.35,d*.80));focal=24.
            elif view=='balcony':
                eye=world((0,fh-.55,-8.));target=world((0,fh-.25,0));focal=24.
            else:
                x=-(parameters['front_bays']-1)/2*w/parameters['front_bays']
                eye=world((x-.10,fh*.72,-3.6));target=world((x,fh*.70,0));focal=24.
    camera = EA.spawn_actor_from_class(u.CineCameraActor, eye)
    created.append(camera)
    camera.tags = ['SkyTownReviewTemporary']
    camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye, target), False)
    camera.get_cine_camera_component().set_editor_property('current_focal_length', focal)
    camera.get_cine_camera_component().focus_settings = u.CameraFocusSettings(focus_method=u.CameraFocusMethod.DISABLE)
    u.EditorLevelLibrary.set_level_viewport_camera_info(eye, camera.get_actor_rotation())
    LE.pilot_level_actor(camera)
    LE.editor_set_game_view(True)
    LE.editor_set_viewport_realtime(True)
    EA.clear_actor_selection_set()
    u.SystemLibrary.execute_console_command(None, 'Slate.bAllowThrottling 0')
    u.SystemLibrary.execute_console_command(None, 'r.ScreenPercentage 100')
    u.get_default_object(u.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground', False)
    filename = request.get('filename', request.get('reference', request.get('family', 'capture')) + '_' + request.get('stage', 'after') + '.png')
    (OUT/Path(filename).with_suffix('.json')).write_text(json.dumps({'asset':path,'view':view,
        'renderer':'Unreal editor HighResShot','eye_cm':list(eye.to_tuple()),
        'target_cm':list(target.to_tuple()),'focal_length_mm':focal},indent=2))
    import time
    started = time.monotonic()
    started_wall = time.time()
    capture_file = OUT/filename
    status_file = R/'status.json'
    requested=False
    def cleanup(delta):
        nonlocal requested
        if not requested and time.monotonic()-started>=2.:
            requested=True
            world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
            u.SystemLibrary.execute_console_command(world,'HighResShot 960x600 filename="'+capture_file.as_posix()+'"')
        LE.editor_invalidate_viewports()
        complete = capture_file.exists() and capture_file.stat().st_mtime >= started_wall-.25
        if not complete and time.monotonic()-started<120.:
            return
        u.unregister_slate_post_tick_callback(handle)
        LE.eject_pilot_level_actor()
        for item in reversed(created):
            if item in EA.get_all_level_actors():
                EA.destroy_actor(item)
        u.EditorLevelLibrary.set_level_viewport_camera_info(*previous)
        if previous_pilot and previous_pilot in EA.get_all_level_actors():LE.pilot_level_actor(previous_pilot)
        LE.editor_set_game_view(previous_game_view)
        EA.set_selected_level_actors(selected)
        status_file.write_text(json.dumps({'status':'passed' if complete else 'failed',
            'action':'capture','file':str(capture_file)}))
    handle = u.register_slate_post_tick_callback(cleanup)


def restore_reference():
    reference=next((a for a in EA.get_all_level_actors() if a.get_actor_label()=='_TEST_SM_House03_Var24'),None)
    if reference:
        center,extent=reference.get_actor_bounds(False)
        radius=max(extent.x,extent.y,extent.z)
        eye=center-reference.get_actor_right_vector()*radius*3.4-reference.get_actor_forward_vector()*radius*.4+u.Vector(0,0,radius*.4)
        u.EditorLevelLibrary.set_level_viewport_camera_info(eye,u.MathLibrary.find_look_at_rotation(eye,center))
        EA.set_selected_level_actors([reference])
    else:
        camera=next((a for a in EA.get_all_level_actors() if a.get_actor_label()=='CAM_Catalog_Overview'),None)
        if camera:
            LE.eject_pilot_level_actor()
            u.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
            EA.clear_actor_selection_set()


def main():
    request_path = R / 'request.json'
    request = ({'action':os.environ['SKYTOWN_ACTION']} if os.environ.get('SKYTOWN_ACTION') else
        json.loads(request_path.read_text(encoding='utf-8-sig')) if request_path.exists() else {'action': 'inspect'})
    status_path=R/('validation_status.json' if request['action']=='validate' else 'status.json')
    error_path=R/('validation_error.txt' if request['action']=='validate' else 'error.txt')
    status_path.write_text(json.dumps({'status':'running','action':request['action']}))
    actions = {'inspect': inspect, 'export_props': export_props, 'capture': capture, 'materials': materials,
               'refresh_hdas':refresh_hdas,'validate':validate,'restore_reference':restore_reference}
    actions[request['action']]()
    error_path.unlink(missing_ok=True)
    if request['action']!='capture':
        status_path.write_text(json.dumps({'status': 'passed', 'action': request['action']}))


if not globals().get('_SKYTOWN_IMPORT_ONLY',False):
    try:
        main()
    except Exception:
        error_path=R/('validation_error.txt' if os.environ.get('SKYTOWN_ACTION')=='validate' else 'error.txt')
        error_path.write_text(traceback.format_exc(), encoding='utf-8')
        raise
