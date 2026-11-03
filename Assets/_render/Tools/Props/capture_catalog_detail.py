"""Capture the actual Unreal interior or round detail, with the prop's own lights."""
from pathlib import Path
import json
import math
import sys
import unreal as u
P=(Path(__file__).resolve().parents[4] / 'Assets/_render');R=P/'Saved/PropsDetail';OUT=P/'RenderTests/PropsDetail';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(P/'Tools/Props'));import update_catalogs as C
request=json.loads((R/'capture_request.json').read_text());family=request['family'];ident=request['id'];view=request.get('view','interior')
row=json.loads((R/family/'normal'/(ident+'.json')).read_text())
level='/Game/'+family+'/Maps/L_'+family+'_Catalog';assert C.LE.load_level(level)
actor=next(a for a in C.EA.get_all_level_actors() if ident in [str(t) for t in a.tags] and family+'Catalog' in [str(t) for t in a.tags])
if isinstance(actor,u.StaticMeshActor):
    C.LE.save_current_level();C.backup_map(level)
    transform=actor.get_actor_transform();tags=list(actor.tags);label=actor.get_actor_label();folder=actor.get_folder_path()
    C.EA.destroy_actor(actor)
    cls=u.EditorAssetLibrary.load_blueprint_class('/Game/'+family+'/Blueprints/BP_'+family+'_'+ident)
    actor=C.EA.spawn_actor_from_class(cls,transform.translation);actor.set_actor_transform(transform,False,False)
    actor.tags=tags;actor.set_actor_label(label);actor.set_folder_path(folder)
if view=='interior':
    name=C.camera_for(actor,family,row);camera=next(a for a in C.EA.get_all_level_actors() if a.get_actor_label()==name)
elif view=='round':
    p=row['parameters'];origin=row['source_origin_m']
    peak=p['wall_height']+p['nave_width']/2*math.tan(math.radians(p['roof_pitch']))
    cy=p['wall_height']+min(1.3,(peak-p['wall_height'])*.45)
    def location(x,y,z):
        return C.world_position(actor,((x-origin[0])*100,(z-origin[2])*100,(y-origin[1])*100)).to_tuple()
    camera=C.C.camera('Round_Detail_'+ident,location(0,cy,-2.5),location(0,cy,-.08),38.,1.6)
    camera.tags=['PropsDetailGenerated','RoundReview',ident]
elif view=='overview':
    camera=next(a for a in C.EA.get_all_level_actors() if a.get_actor_label()=='CAM_Catalog_Overview')
else:
    camera=next(a for a in C.EA.get_all_level_actors() if a.get_actor_label()==('CAM_Type_'+ident[-2:] if family=='Gothic' else 'CAM_Family_'+ident[:2]))
u.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
performance=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
u.SystemLibrary.execute_console_command(None,'Slate.bAllowThrottling 0')
u.SystemLibrary.execute_console_command(None,'r.ScreenPercentage 100')
C.LE.save_current_level()
path=OUT/(family.lower()+'_catalog_overview.png' if view=='overview' else family.lower()+'_'+ident+'_'+view+'.png')
size=(1600,1000) if view=='overview' else (960,600)
u.AutomationLibrary.take_high_res_screenshot(*size,str(path),camera=camera,delay=1.)
(R/'capture_status.json').write_text(json.dumps({'status':'requested','file':str(path),'camera':camera.get_actor_label()}))
