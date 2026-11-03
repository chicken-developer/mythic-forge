"""Fresh-process portability with only the relocated HDA, stdlib and hou."""
from pathlib import Path
import json,hashlib,shutil
import hou
ROOT=Path(__file__).resolve().parent
folder=ROOT/'results/portable';folder.mkdir(exist_ok=True)
asset=folder/'relocated_medieval_building.hda';shutil.copy2(ROOT/'asset/medieval_building_modular.hda',asset)
hou.hda.installFile(str(asset))
obj=hou.node('/obj').createNode('geo','PORTABLE_ONLY',run_init_scripts=False)
n=obj.createNode('mythic::medieval_house::4.1','BUILDING')
cfg=json.loads(n.type().definition().sections()['Baseline.json'].contents())
for key,value in dict(cfg['parameters'],seed_decoration=cfg['seeds']['decoration']).items():n.parm(key).set(int(value) if isinstance(value,bool) else value)
def state():
    n.cook(force=True);g=n.geometry()
    return {'primitives':g.intrinsicValue('primitivecount'),'dimensions':list(g.boundingBox().sizevec()),'hash':hashlib.sha256(str(g.pointFloatAttribValues('P')).encode()).hexdigest()}
n.parm('preview_quality').set('low');low=state();n.parm('preview_quality').set('high');high=state()
n.parm('width_m').set(cfg['parameters']['width_m']+.6);wide=state()
errors=[{'path':c.path(),'errors':list(c.errors())} for c in n.allSubChildren() if c.errors()]
files=[c.path() for c in n.allSubChildren() if c.type().name()=='file']
result={'houdini':hou.applicationVersionString(),'relocated_hda_sha256':hashlib.sha256(asset.read_bytes()).hexdigest(),
        'authoring_scripts_imported':False,'external_geometry_files':files,'low':low,'high':high,'changed_width':wide,'errors':errors,
        'output_count':n.type().maxNumOutputs(),'quality_parameter_visible':not n.parm('preview_quality').isHidden(),
        'status':'pass' if high['primitives']>low['primitives']*2 and high['hash']!=wide['hash'] and not errors and not files else 'fail'}
(ROOT/'results/portability.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(result['status'],flush=True)
raise SystemExit(result['status']!='pass')
