"""Capture one model at a time and wait for its saved screenshot before cleanup."""
from pathlib import Path
import json
import subprocess
import sys
import time

P=Path(__file__).resolve().parents[2]
R=P/'Saved/SkyTownReview'
OUT=P/'RenderTests/SkyTown'
requests=[
    {'reference':'SM_House02','filename':'SM_House02_reference.png'},
    {'family':'Medieval','id':'03_village_inn_base','filename':'Medieval_after.png'},
    {'family':'Gothic','id':'variant_17','filename':'Gothic_after.png'},
    {'family':'Gothic','id':'variant_21','view':'balcony','filename':'gothic_wide_balcony.png'},
    {'family':'Medieval','id':'03_village_inn_base','view':'joint','filename':'medieval_joint.png'},
    {'family':'Gothic','id':'variant_17','view':'joint','filename':'gothic_joint.png'},
    {'family':'Medieval','id':'03_village_inn_base','view':'interior','filename':'medieval_interior.png'},
    {'family':'Gothic','id':'variant_17','view':'interior','filename':'gothic_interior.png'}]
for request in requests:
    started=time.time()
    (R/'request.json').write_text(json.dumps(dict(action='capture',**request)),encoding='utf-8')
    subprocess.run([sys.executable,str(Path(__file__).with_name('submit_editor_once.py')),'skytown_catalog.py'],check=True)
    while time.time()-started<120:
        try:
            status=json.loads((R/'status.json').read_text())
            file=OUT/request['filename']
            if (status.get('status')=='passed' and status.get('file')==str(file)
                    and file.exists() and file.stat().st_mtime>=started):
                print('CAPTURED',request['filename'],flush=True)
                break
            error=R/'error.txt'
            if error.exists() and error.stat().st_mtime>=started:
                raise RuntimeError(error.read_text(encoding='utf-8'))
        except (FileNotFoundError,json.JSONDecodeError):
            pass
        time.sleep(.25)
    else:
        raise RuntimeError('Screenshot did not complete: '+request['filename'])
(R/'request.json').write_text(json.dumps({'action':'restore_reference'}),encoding='utf-8')
subprocess.run([sys.executable,str(Path(__file__).with_name('submit_editor_once.py')),'skytown_catalog.py'],check=True)
