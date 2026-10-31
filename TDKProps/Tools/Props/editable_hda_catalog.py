"""Set up and inspect editable Houdini actors paired with the catalog references."""
from pathlib import Path
import json
import hashlib
import math
import os
import sys
import traceback
import unreal as u

P=Path('E:/Repos/mythic-forge/TDKProps');R=P/'Saved/EditableHDA';R.mkdir(parents=True,exist_ok=True)
request=json.loads((R/'request.json').read_text());mode=request.get('mode','probe')
sys.path.insert(0,str(P/'Tools/Props'));import update_catalogs as C
EA=u.get_editor_subsystem(u.EditorActorSubsystem);LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
WRAPPERS=[]

def rows_for(family):return json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants']
def reference_for(family,ident):return next(a for a in EA.get_all_level_actors() if family+'Catalog' in [str(t) for t in a.tags] and ident in [str(t) for t in a.tags])
def box_for(reference,row,offset=(0,0)):
    lo=row['min_m'];hi=row['max_m'];origin=row['source_origin_m']
    points=[C.world_position(reference,((x-origin[0])*100+offset[0],(z-origin[2])*100+offset[1],(y-origin[1])*100)).to_tuple() for x in (lo[0],hi[0]) for y in (lo[1],hi[1]) for z in (lo[2],hi[2])]
    return ([min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)])
def overlaps(a,b):return all(min(a[1][i],b[1][i])-max(a[0][i],b[0][i])>1 for i in (0,1))

def stage():
    report={'status':'running','families':[]}
    for family in ('Medieval','Gothic'):
        assert LE.load_level('/Game/'+family+'/Maps/L_'+family+'_Catalog')
        rows=rows_for(family);refs={r['id']:reference_for(family,r['id']) for r in rows}
        boxes={r['id']:box_for(refs[r['id']],r) for r in rows};occupied=list(boxes.values());entries=[]
        for row in sorted(rows,key=lambda r:-r['size_m'][0]*r['size_m'][2]):
            ident=row['id'];reference=refs[ident]
            actor=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='HDA_'+family+'_'+ident)
            w,d=row['size_m'][0]*100,row['size_m'][2]*100
            for offset in ((w+240,0),(-w-240,0),(0,d+240),(0,-d-240)):
                box=box_for(reference,row,offset)
                if not any(overlaps(box,b) for b in occupied):break
            else:raise AssertionError((family,ident,'no adjacent clear space'))
            origin=row['source_origin_m'];transform=reference.get_actor_transform()
            transform.translation=C.world_position(reference,(offset[0]-origin[0]*100,offset[1]-origin[2]*100,-origin[1]*100))
            actor.set_actor_transform(transform,False,False);occupied.append(box)
            lo,hi=box;center=[(a+b)*.5 for a,b in zip(lo,hi)]
            plinth_label='EDIT_Plinth_'+ident;label='EDIT_Label_'+ident
            for old in [a for a in EA.get_all_level_actors() if a.get_actor_label() in (plinth_label,label)]:EA.destroy_actor(old)
            original=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='Plinth_'+ident)
            material=original.get_component_by_class(u.StaticMeshComponent).get_material(0)
            base=C.C.cube(plinth_label,(center[0],center[1],0 if family=='Medieval' else 8),(w+120,d+120,30 if family=='Medieval' else 24),material,'Editable HDA/Plinths');base.tags=['EditableHDAStage',ident]
            text=C.C.spawn(u.TextRenderActor,label,(center[0],lo[1]-120,40),(0,-90,0),'Editable HDA/Labels');text.tags=['EditableHDAStage',ident]
            text.text_render.set_text('HDA / '+ident+'\n'+('Params > Apply/Rebuild > Recook > Bake' if family=='Gothic' else 'Params > Recook > Bake'))
            text.text_render.set_world_size(32. if family=='Medieval' else 42.);text.text_render.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
            text.text_render.set_text_render_color(u.Color(100,220,245,255))
            rb=boxes[ident];pair_lo=[min(lo[i],rb[0][i]) for i in range(3)];pair_hi=[max(hi[i],rb[1][i]) for i in range(3)]
            target=[(a+b)*.5 for a,b in zip(pair_lo,pair_hi)];radius=max(b-a for a,b in zip(pair_lo,pair_hi))*.85
            cam_label='CAM_Edit_'+family+'_'+ident
            for old in [a for a in EA.get_all_level_actors() if a.get_actor_label()==cam_label]:EA.destroy_actor(old)
            camera=C.C.camera('Edit_'+family+'_'+ident,(target[0]+radius*.65,target[1]-radius*1.8,target[2]+radius*.8),target,32.,1.6);camera.tags=['EditableHDAStage',ident]
            entries.append({'id':ident,'hda_actor':actor.get_path_name(),'args':'SourceAssets/Houdini/'+family+'/Args/'+ident+'.json','offset_cm':offset,'expected_bounds_cm':box,'reference_bounds_cm':rb,'camera':cam_label})
        ground=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='Catalog_Ground')
        left=min(b[0][0] for b in occupied)-800;right=max(b[1][0] for b in occupied)+800
        front=min(b[0][1] for b in occupied)-800;rear=max(b[1][1] for b in occupied)+800
        old_location=ground.get_actor_location();old_scale=ground.get_actor_scale3d()
        ground.set_actor_location(u.Vector((left+right)/2,(front+rear)/2,old_location.z),False,False)
        ground.set_actor_scale3d(u.Vector((right-left)/100,(rear-front)/100,old_scale.z))
        LE.save_current_level();report['families'].append({'family':family,'records':entries})
    report['status']='passed';(R/'stage.json').write_text(json.dumps(report,indent=2))

def ensure_level(family):
    path='/Game/'+family+'/Maps/L_'+family+'_Catalog'
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split('.')[0]!=path:
        assert LE.save_current_level();assert LE.load_level(path)

def selected_wrapper(api,family,ident):
    ensure_level(family)
    actor=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='HDA_'+family+'_'+ident)
    wrapper=u.HoudiniPublicAPIAssetWrapper.create_wrapper(api,actor);WRAPPERS.append(wrapper)
    return actor,wrapper

def record_cook(actor,wrapper,context):
    family=context['family'];ident=context['id']
    objects=wrapper.get_output_objects(u.StaticMesh);assert objects
    if context.get('parameters'):check_parameters(wrapper,context['parameters'])
    records=[]
    for output in objects:
        mesh=output.output_object;box=mesh.get_bounding_box()
        materials=[s.material_interface.get_path_name() for s in mesh.static_materials]
        assert all(m.startswith('/Game/'+family+'/Materials/') for m in materials),materials
        assert u.EditorAssetLibrary.save_loaded_asset(mesh)
        records.append({'mesh':mesh.get_path_name(),'triangles':mesh.get_num_triangles(0),'size_cm':list((box.max-box.min).to_tuple()),'materials':materials})
    actor.tags=['EditableHDA',family+'EditableHDA',ident,'Args=SourceAssets/Houdini/'+family+'/Args/'+ident+'.json']
    assert LE.save_current_level()
    out=P/'RenderTests/EditableHDA';out.mkdir(parents=True,exist_ok=True)
    report={'status':'passed','family':family,'id':ident,'parameters_changed':context.get('parameters',{}),'outputs':records,'auto_bake':wrapper.is_auto_bake_enabled()}
    name=context.get('run',family.lower()+'_'+ident)
    (out/('cook_'+name+'.json')).write_text(json.dumps(report,indent=2));(R/'error.txt').unlink(missing_ok=True)

def cook_one(api):
    context=dict(request);family=context['family'];ident=context['id'];actor,wrapper=selected_wrapper(api,family,ident)
    if mode=='collect_cook':record_cook(actor,wrapper,context);return
    if context.get('parameters'):set_parameters(wrapper,context['parameters'])
    # For a newly loaded Gothic actor, initialize once before sending a button.
    if family=='Gothic' and mode!='initialize':
        assert wrapper.trigger_button_parameter('apply_editor_parameters')
    # Use the native completion event: a loaded HDA may first need instantiation,
    # which the SDK's blocking helper can report before output translation ends.
    completed=[False]
    def finish(asset_wrapper):
        if completed[0]:return
        completed[0]=True
        try:record_cook(actor,asset_wrapper,context)
        except Exception:(R/'error.txt').write_text(traceback.format_exc())
        finally:asset_wrapper.on_post_processing_delegate.remove_callable(finish)
    wrapper.on_post_processing_delegate.add_callable(finish)
    (R/'cook_phase.json').write_text(json.dumps({'family':family,'id':ident,'phase':'cook'}))
    assert wrapper.recook(u.HoudiniPublicAPICookMode.NON_BLOCKING)

def verify(api):
    out=P/'RenderTests/EditableHDA';out.mkdir(parents=True,exist_ok=True)
    report={'status':'running','families':[]};staging=json.loads((R/'stage.json').read_text())
    for family in ('Medieval','Gothic'):
        assert LE.load_level('/Game/'+family+'/Maps/L_'+family+'_Catalog')
        actors=EA.get_all_level_actors();rows=rows_for(family);records=[]
        live=[a for a in actors if isinstance(a,u.HoudiniAssetActor) and a.get_actor_label().startswith('HDA_'+family+'_')]
        assert len(live)==len(rows)
        for row in rows:
            ident=row['id'];args_path=P/'SourceAssets/Houdini'/family/'Args'/(ident+'.json');args=json.loads(args_path.read_text())
            file=(args_path.parent/args['hda']).resolve();assert hashlib.sha256(file.read_bytes()).hexdigest()==args['hda_sha256']
            actor=next(a for a in live if a.get_actor_label()=='HDA_'+family+'_'+ident)
            wrapper=u.HoudiniPublicAPIAssetWrapper.create_wrapper(api,actor);WRAPPERS.append(wrapper)
            check_parameters(wrapper,args['houdini_parameters'])
            assert wrapper.get_houdini_asset_info().houdini_asset.get_path_name().split('.')[0]==args['houdini_asset']
            assert not wrapper.is_auto_bake_enabled() and not wrapper.is_auto_cooking_enabled()
            assert wrapper.get_bake_folder().path==args['bake_folder']
            ref=reference_for(family,ident);assert len(ref.get_components_by_class(u.PointLightComponent))==len(row['interior_lights'])
            labels=[a for a in actors if a.get_actor_label()=='EDIT_Label_'+ident];assert len(labels)==1
            records.append({'id':ident,'parameters_checked':len(args['houdini_parameters']),'args':args_path.relative_to(P).as_posix(),'hda_actor':actor.get_path_name(),'auto_cook':False,'auto_bake':False,'bake_folder':args['bake_folder'],'preview_ready':bool(wrapper.get_output_objects(u.StaticMesh))})
        stage_rows=next(f['records'] for f in staging['families'] if f['family']==family)
        occupied=[r['reference_bounds_cm'] for r in stage_rows]
        for record in stage_rows:
            box=record['expected_bounds_cm'];assert not any(overlaps(box,b) for b in occupied);occupied.append(box)
        report['families'].append({'family':family,'editable_hdas':len(records),'reference_buildings':len(records),'reference_point_lights':sum(len(r['interior_lights']) for r in rows),'overlapping_pairs':0,'records':records})
    report['status']='passed';report['editable_hdas']=sum(f['editable_hdas'] for f in report['families'])
    (out/'validation.json').write_text(json.dumps(report,indent=2));(R/'error.txt').unlink(missing_ok=True)

def capture():
    family=request['family'];ident=request['id'];ensure_level(family)
    camera=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='CAM_Edit_'+family+'_'+ident)
    actor=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='HDA_'+family+'_'+ident)
    EA.set_selected_level_actors([actor]);u.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
    LE.save_current_level()
    file=P/'RenderTests/EditableHDA'/(family.lower()+'_'+ident+'_pair.png')
    u.AutomationLibrary.take_high_res_screenshot(960,600,str(file),camera=camera,delay=1.)

def set_parameters(wrapper,parameters):
    available={str(k):v for k,v in wrapper.get_parameter_tuples().items()}
    missing=set(parameters)-set(available);assert not missing,missing
    values={}
    for name,value in parameters.items():
        before=available[name]
        if before.string_values:entry=u.HoudiniParameterTuple(string_values=[str(value)])
        elif before.bool_values:entry=u.HoudiniParameterTuple(bool_values=[bool(value)])
        elif before.int32_values:entry=u.HoudiniParameterTuple(int32_values=[int(value)])
        else:entry=u.HoudiniParameterTuple(float_values=[float(value)])
        values[name]=entry
    assert wrapper.set_parameter_tuples(values)

def check_parameters(wrapper,parameters):
    actual={str(k):v for k,v in wrapper.get_parameter_tuples().items()}
    for name,expected in parameters.items():
        entry=actual[name]
        value=list(entry.string_values or entry.bool_values or entry.int32_values or entry.float_values)[0]
        if isinstance(expected,float):assert math.isclose(value,expected,abs_tol=1e-4,rel_tol=1e-5),(name,value,expected)
        else:assert value==expected,(name,value,expected)

def import_hda(family):
    source=P/'SourceAssets/Houdini'/family/'Editor'/(family.lower()+'_building_editable.hda')
    path='/Game/'+family+'/Houdini/'+source.stem
    asset=u.load_asset(path)
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    if not asset or u.EditorAssetLibrary.get_metadata_tag(asset,'EditableSourceSHA')!=digest:
        task=u.AssetImportTask();task.filename=str(source);task.destination_path='/Game/'+family+'/Houdini';task.destination_name=source.stem;task.automated=True;task.save=True
        task.replace_existing=True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset=u.load_asset(path)
    assert isinstance(asset,u.HoudiniAsset),path
    u.EditorAssetLibrary.set_metadata_tag(asset,'EditableSourceSHA',digest);u.EditorAssetLibrary.save_loaded_asset(asset)
    return asset

def build(api):
    assert api.is_session_valid()
    report={'status':'running','records':[]}
    for family in request.get('families',['Medieval','Gothic']):
        level='/Game/'+family+'/Maps/L_'+family+'_Catalog';assert LE.load_level(level)
        LE.save_current_level();C.backup_map(level)
        asset=import_hda(family)
        rows=json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants']
        for row in rows:
            if request.get('ids') and row['id'] not in request['ids']:continue
            args=json.loads((P/'SourceAssets/Houdini'/family/'Args'/(row['id']+'.json')).read_text())
            reference=next(a for a in EA.get_all_level_actors() if family+'Catalog' in [str(t) for t in a.tags] and row['id'] in [str(t) for t in a.tags])
            label='HDA_'+family+'_'+row['id']
            existing=next((a for a in EA.get_all_level_actors() if a.get_actor_label()==label),None)
            if existing and request.get('reset'):
                assert isinstance(existing,u.HoudiniAssetActor) and existing.get_actor_label()==label
                EA.destroy_actor(existing);existing=None
            if existing:
                wrapper=u.HoudiniPublicAPIAssetWrapper.create_wrapper(api,existing)
                if wrapper.get_houdini_asset_info().houdini_asset!=asset:
                    assert isinstance(existing,u.HoudiniAssetActor) and existing.get_actor_label()==label
                    EA.destroy_actor(existing);existing=None
            if not existing:
                origin=row['source_origin_m'];offset=row['size_m'][0]*100+240.
                transform=reference.get_actor_transform()
                transform.translation=C.world_position(reference,(offset-origin[0]*100,-origin[2]*100,-origin[1]*100))
                settings=u.HoudiniPublicAPISettings(transform=transform,enable_auto_cook=False,enable_auto_bake=False,bake_directory_path=args['bake_folder'],replace_previous_bake=False,remove_output_after_bake=False)
                wrapper=api.instantiate(asset,u.HoudiniPublicAPIInstantiationType.HOUDINI_ASSET_COMPONENT,settings)
                assert wrapper,(family,row['id'],'instantiate')
            WRAPPERS.append(wrapper)
            actor=wrapper.get_houdini_asset_actor();actor.set_actor_label(label);actor.set_folder_path('Editable HDA/'+row['id'])
            actor.tags=['EditableHDA',family+'EditableHDA',row['id'],'Args=SourceAssets/Houdini/'+family+'/Args/'+row['id']+'.json']
            wrapper.set_auto_bake_enabled(False);wrapper.set_auto_cooking_enabled(False)
            wrapper.set_proxy_mesh_enabled(False)
            proxy=wrapper.cached_houdini_cookable.get_editor_property('proxy_data')
            proxy.set_editor_property('override_global_proxy_static_mesh_settings',True)
            proxy.set_editor_property('enable_proxy_static_mesh_override',False)
            wrapper.set_temporary_cook_folder(u.DirectoryPath('/Game/'+family+'/Houdini/Working/'+row['id']))
            set_parameters(wrapper,args['houdini_parameters'])
            check_parameters(wrapper,args['houdini_parameters'])
            if request.get('cook',False):
                if family=='Gothic':
                    assert wrapper.trigger_button_parameter('apply_editor_parameters')
                assert wrapper.recook(u.HoudiniPublicAPICookMode.BLOCKING),(family,row['id'],'cook')
                objects=wrapper.get_output_objects(u.StaticMesh)
                assert objects,(family,row['id'],'no output')
            check_parameters(wrapper,args['houdini_parameters'])
            assert not wrapper.is_auto_bake_enabled()
            actor.tags=['EditableHDA',family+'EditableHDA',row['id'],'Args=SourceAssets/Houdini/'+family+'/Args/'+row['id']+'.json']
            LE.save_current_level()
            report['records'].append({'family':family,'id':row['id'],'actor':actor.get_path_name(),'parameters':len(args['houdini_parameters']),'cook':request.get('cook',False),'auto_bake':False})
            (R/'build.json').write_text(json.dumps(report,indent=2))
            u.log('EDITABLE_HDA '+family+' '+row['id'])
    u.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    report['status']='passed';(R/'build.json').write_text(json.dumps(report,indent=2));(R/'error.txt').unlink(missing_ok=True)

def main():
    if mode=='save_quit':
        assert u.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
        (R/'shutdown.json').write_text(json.dumps({'status':'saved'}))
        u.SystemLibrary.quit_editor()
        return
    api=u.HoudiniPublicAPIBlueprintLib.get_api()
    os.environ['HOUDINI_MAXTHREADS']='8'
    if mode=='restart_session':
        api.restart_session()
        (R/'session.json').write_text(json.dumps({'session_valid':api.is_session_valid(),'maxthreads':8}))
        return
    if mode=='stage':stage();return
    if mode in ('cook','collect_cook','initialize'):cook_one(api);return
    if mode=='verify':verify(api);return
    if mode=='capture':capture();return
    if mode=='probe':
        lines=[]
        for cls in (u.HoudiniPublicAPI,u.HoudiniPublicAPISettings,u.HoudiniPublicAPIAssetWrapper,u.HoudiniParameterTuple,u.HoudiniPublicAPICookMode):
            lines.append(cls.__name__+'\n'+str(cls.__doc__))
        for method in ('instantiate','instantiate_asset','create_session'):
            lines.append(method+'\n'+str(getattr(api,method).__doc__))
        for name in ('recook','set_parameter_tuples','get_parameter_tuples','get_output_objects','get_houdini_asset_actor','trigger_button_parameter','create_wrapper'):
            method=getattr(u.HoudiniPublicAPIAssetWrapper,name,None)
            lines.append(name+'\n'+str(getattr(method,'__doc__','missing')))
        (R/'api.txt').write_text('\n'.join(lines),encoding='utf-8')
        if not api.is_session_valid():api.create_session()
        (R/'probe.json').write_text(json.dumps({'session_valid':api.is_session_valid()}))
        return
    if mode=='build':
        if not api.is_session_valid():api.create_session()
        build(api)
        return
    raise ValueError(mode)

try:main()
except Exception:(R/'error.txt').write_text(traceback.format_exc());raise
