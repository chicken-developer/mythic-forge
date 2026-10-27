"""Apply review framing refinements to the already saved sample maps."""
import unreal as u
LE=u.get_editor_subsystem(u.LevelEditorSubsystem);EA=u.get_editor_subsystem(u.EditorActorSubsystem)
assert LE.load_level('/Game/Gothic/Maps/L_Gothic_Catalog')
for a in EA.get_all_level_actors():
    if a.get_actor_label().startswith('CAM_Type_'):a.get_cine_camera_component().current_focal_length=30.
LE.save_current_level()
assert LE.load_level('/Game/Gothic/Maps/L_Gothic_City')
actors={a.get_actor_label():a for a in EA.get_all_level_actors()}
actors['City_Terrain'].set_actor_scale3d(u.Vector(6000,6000,.6))
for name,location,target in [('City_Overview',(-48380,-55460,45804),(0,0,1200)),('Civic_Square',(-1000,-2550,440),(1200,1800,750))]:
    a=actors['CAM_'+name];loc=u.Vector(*location);a.set_actor_location(loc,False,False)
    a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(loc,u.Vector(*target)),False)
LE.save_current_level()
