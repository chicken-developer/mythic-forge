"""Run a finite list of explicit captures, waiting for each PNG to finish."""
from pathlib import Path
import json
import subprocess
import sys
import time
from PIL import Image

ROOT=Path(__file__).resolve().parents[3];TOOLS=ROOT/'TDKProps/Tools/Gothic'
SAVED=ROOT/'TDKProps/Saved/GothicReview/City';OUT=ROOT/'TDKProps/RenderTests/Gothic/City'
allowed={'City_Overview','City_Plan','Civic_Square','Craft_Street','Merchant_Street','Noble_Terraces','Cathedral_Close','Catalog_Overview','Type_11','Type_22','Type_10'}
views=sys.argv[1:];assert views and set(views)<=allowed
for view in views:
    target=OUT/(view.lower()+'.png');before=target.stat().st_mtime_ns if target.exists() else 0
    (SAVED/'capture_request.json').write_text(json.dumps({'view':view}),encoding='utf-8')
    subprocess.run([sys.executable,str(TOOLS/'run_app_script.py'),'unreal',str(TOOLS/'capture_city.py')],check=True,cwd=str(ROOT))
    last_size=-1;stable=0
    for tick in range(110):
        time.sleep(.5)
        if target.exists() and target.stat().st_mtime_ns>before:
            current=target.stat().st_size
            stable=stable+1 if current==last_size and current>1024 else 0;last_size=current
            if stable>=3:
                with Image.open(target) as im:im.verify()
                break
    else:raise RuntimeError('No completed Unreal render for '+view)
    print('CAPTURED',view,target.stat().st_size,flush=True)
