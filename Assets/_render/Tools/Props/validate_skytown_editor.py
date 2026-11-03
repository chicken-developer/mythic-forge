"""Read-only audit in a separate editor, leaving the user's open map untouched."""
import os
import runpy
from pathlib import Path

os.environ['SKYTOWN_ACTION']='validate'
try:
    runpy.run_path(str(Path(__file__).with_name('skytown_catalog.py')),run_name='__main__')
finally:
    import unreal
    if '-SkyTownValidation' in unreal.SystemLibrary.get_command_line():
        unreal.SystemLibrary.quit_editor()
