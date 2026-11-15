"""Bootstrap for scripts that must keep the full editor alive (screenshots).

-ExecutePythonScript closes the editor as soon as the script returns, so tick-driven
scripts are started from here instead. Usage:
  set UE_PYTHONPATH=<project>/Tools/DreamDemo/ue/autorun
  set DREAMDEMO_AUTORUN=<script.py>
  UnrealEditor.exe TDKProps.uproject
Without DREAMDEMO_AUTORUN this file does nothing.
"""
import os
import runpy

_script = os.environ.get("DREAMDEMO_AUTORUN")
if _script:
    import unreal
    unreal.log("DREAMDEMO autorun %s" % _script)
    runpy.run_path(_script, run_name="__main__")
