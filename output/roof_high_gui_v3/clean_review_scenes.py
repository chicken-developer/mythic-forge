"""Remove evaluation-only frozen geometry from the saved review scenes."""
from pathlib import Path
import hou,json
ROOT=Path(__file__).resolve().parent
records=[]
for scene in sorted((ROOT/'scenes').glob('*.hip')):
    if scene.name.startswith(('source_','debug_')):continue
    hou.hipFile.load(str(scene),suppress_save_prompt=True,ignore_load_warnings=True)
    qa=hou.node('/obj/QA_ONLY')
    if qa:qa.destroy()
    b=hou.node('/obj/medieval_roof_high_v3/BUILDING')
    if b is None:raise RuntimeError(str(scene))
    b.parm('preview_quality').set('high')
    for obj in hou.node('/obj').children():
        if obj.type().name()=='geo':obj.setDisplayFlag(obj==b.parent())
    hou.hipFile.save(str(scene));records.append(scene.name)
(ROOT/'results/clean_scenes.json').write_text(json.dumps({'status':'pass','scenes':records,'removed':'QA_ONLY evaluation stash'},indent=2),encoding='utf-8')
print('CLEANED',len(records),flush=True)
