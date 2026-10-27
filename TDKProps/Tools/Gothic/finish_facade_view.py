"""Leave the saved editor at the comparison camera after the PIE check."""
from pathlib import Path
import json
import unreal as u
EA=u.get_editor_subsystem(u.EditorActorSubsystem)
hero=next(a for a in EA.get_all_level_actors() if a.get_actor_label()=='CAM_Comparison')
u.EditorLevelLibrary.set_level_viewport_camera_info(hero.get_actor_location(),hero.get_actor_rotation())
EA.clear_actor_selection_set()
u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
dirty=u.EditorLoadingAndSavingUtils.get_dirty_map_packages()+u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
Path('E:/Repos/mythic-forge/TDKProps/Saved/GothicReview/Facade/editor_after.json').write_text(json.dumps({'dirty_packages':[p.get_name() for p in dirty]},indent=2))
