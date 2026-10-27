"""Open the final city and start its actual Third Person play session."""
from pathlib import Path
import unreal as u
LE=u.get_editor_subsystem(u.LevelEditorSubsystem)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith('/Game/Gothic/Maps/L_Gothic_City.'):
    assert LE.load_level('/Game/Gothic/Maps/L_Gothic_City')
LE.editor_request_begin_play()
