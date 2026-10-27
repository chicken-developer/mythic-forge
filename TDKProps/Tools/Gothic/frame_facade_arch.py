"""Apply the final wider arch framing without reimporting geometry."""
import unreal as u
actors={a.get_actor_label():a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()}
for name,x in [('Legacy',290),('Game',-290)]:
    loc=u.Vector(x+170,-1250,458);target=u.Vector(x,0,365)
    a=actors['CAM_'+name+'_Arch'];a.set_actor_location(loc,False,False)
    a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(loc,target),False)
u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
