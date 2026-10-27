"""Capture the actual UE viewport through a saved cinematic camera."""
from pathlib import Path
import unreal as u
REVIEW=Path('E:/Repos/mythic-forge/TDKProps/Saved/GothicReview')
view=globals().get('VIEW','Overview')
name='CAM_'+view
cam=next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()==name)
u.EditorLevelLibrary.set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation())
size=(5000,2000) if view=='Overview' else (2560,1440)
filename='gothic_lineup.png' if view=='Overview' else 'gothic_'+view.lower()+'.png'
u.AutomationLibrary.take_high_res_screenshot(*size,str(REVIEW/filename),camera=cam,delay=2.)
