"""Capture one requested saved camera; invoked explicitly, with no watcher."""
from pathlib import Path
import json
import unreal as u

ROOT=Path(__file__).resolve().parents[4]
SAVED=ROOT/'Assets/_render/Saved/GothicReview/Facade'
OUT=ROOT/'Assets/_render/RenderTests/Gothic/FacadeReview'
OUT.mkdir(parents=True,exist_ok=True)
view=json.loads((SAVED/'capture_request.json').read_text())['view']
assert view in ['Comparison','Legacy_Arch','Game_Arch','Legacy_Pinnacle','Game_Pinnacle','Legacy_Cornice','Game_Cornice','Game_ThreeQuarter','High_Pinnacle','High_Arch']
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
for a in actors:
    if a.get_actor_label()=='HighSource_Review':a.static_mesh_component.set_cast_shadow(view.startswith('High_'))
cam=next(a for a in actors if a.get_actor_label()=='CAM_'+view)
u.EditorLevelLibrary.set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation())
u.AutomationLibrary.take_high_res_screenshot(2560,1440,str(OUT/(view.lower()+'.png')),camera=cam,delay=2.)
