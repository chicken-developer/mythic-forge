"""Leave the saved render-test level visible through its overview camera."""
import unreal as u
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
cam=next(a for a in actors if a.get_actor_label()=='CAM_Overview')
le=u.get_editor_subsystem(u.LevelEditorSubsystem)
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation())
le.pilot_level_actor(cam)
le.save_current_level()
