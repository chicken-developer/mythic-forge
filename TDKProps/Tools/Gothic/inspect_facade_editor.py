"""Record the current editor state before preparing the isolated clay study."""
from pathlib import Path
import json
import unreal as u
R=Path('E:/Repos/mythic-forge/TDKProps/Saved/GothicReview/Facade')
R.mkdir(parents=True,exist_ok=True)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
report={'world':world.get_path_name(),'dirty_maps':[p.get_path_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
        'dirty_content':[p.get_path_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
        'save_map_signature':u.EditorLoadingAndSavingUtils.save_map.__doc__}
(R/'editor_before.json').write_text(json.dumps(report,indent=2))
