import unreal as u
from pathlib import Path
import json,math
ea=u.get_editor_subsystem(u.EditorActorSubsystem)
actors={a.get_actor_label():a for a in ea.get_all_level_actors()}
rows=json.loads((Path(__file__).resolve().parents[4] / 'Assets/_render/Saved/GothicReview/lineup_scene.json').read_text())['buildings']
for a in ea.get_all_level_actors():
    if a.get_actor_label()=='Exposure_Grade':
        settings=a.get_editor_property('settings')
        settings.set_editor_property('auto_exposure_bias',-.5)
        a.set_editor_property('settings',settings)
def move(camera,location,target):
    camera.set_actor_location(u.Vector(*location),False,False)
    camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(*location),u.Vector(*target)),False)
for label,start,end in [('CAM_Group_A',0,3),('CAM_Group_B',3,7),('CAM_Group_C',7,10)]:
    group=rows[start:end];center=sum(r['position_cm'][0] for r in group)/len(group)
    width=sum(r['size_cm'][0]+500 for r in group);height=max(r['size_cm'][2] for r in group)
    move(actors[label],(center+width*.10,width*1.35,height*1.12),(center,-750,height*.48))
for r in rows:
    x,y,z=r['position_cm'];sx,sy,sz=r['size_cm'];radius=math.sqrt(sx*sx+sy*sy+sz*sz)/2.
    move(actors['CAM_'+r['id']],(x+radius*2.2,y+radius*4.,sz*.5+radius*1.8),(x,y,sz*.5+20))
hero=next(a for a in ea.get_all_level_actors() if a.get_actor_label()=='CAM_Overview')
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(hero.get_actor_location(),hero.get_actor_rotation())
u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
