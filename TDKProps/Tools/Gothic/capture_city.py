"""Explicitly capture a saved camera from the city or its geometry catalog."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('E:/Repos/mythic-forge');R=ROOT/'TDKProps/Saved/GothicReview/City';OUT=ROOT/'TDKProps/RenderTests/Gothic/City';OUT.mkdir(parents=True,exist_ok=True)
request=json.loads((R/'capture_request.json').read_text());view=request['view']
allowed=['City_Overview','City_Plan','Civic_Square','Craft_Street','Merchant_Street','Noble_Terraces','Cathedral_Close','Catalog_Overview','Type_11','Type_22','Type_10']
assert view in allowed
level='/Game/Gothic/Maps/L_Gothic_Catalog' if view.startswith(('Catalog','Type_')) else '/Game/Gothic/Maps/L_Gothic_City'
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
performance=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
state=R/'capture_performance.json'
if not state.exists():state.write_text(json.dumps({'bThrottleCPUWhenNotForeground':performance.get_editor_property('bThrottleCPUWhenNotForeground')}))
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)
u.SystemLibrary.execute_console_command(world,'t.IdleWhenNotForeground 0')
u.SystemLibrary.execute_console_command(world,'Slate.bAllowThrottling 0')
if world.get_path_name().split('.')[0]!=level:assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(level)
cam=next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='CAM_'+view)
u.EditorLevelLibrary.set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation())
size=(2400,2400) if view=='City_Plan' else (3200,1800) if view in ('City_Overview','Catalog_Overview') else (2560,1440)
u.AutomationLibrary.take_high_res_screenshot(*size,str(OUT/(view.lower()+'.png')),camera=cam,delay=0.)
