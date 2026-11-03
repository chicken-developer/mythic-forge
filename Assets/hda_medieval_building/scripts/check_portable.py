"""Fresh-process HDA validation without importing the authoring builder."""
from pathlib import Path
import collections
import hashlib
import json
import shutil
import hou
ROOT=Path(__file__).resolve().parents[1]
dest=ROOT/'review_20261102/portable';dest.mkdir(exist_ok=True)
path=dest/'medieval_house_layout.hda';shutil.copy2(ROOT/'asset/medieval_house_layout.hda',path)
hou.hda.installFile(str(path))
obj=hou.node('/obj').createNode('geo','portable_test',run_init_scripts=False)
asset=obj.createNode('mythic::medieval_house::3.0','portable_house')
cfg=json.loads(asset.type().definition().sections()['Baseline.json'].contents())
for key,val in dict(cfg['parameters'],seed_decoration=cfg['seeds']['decoration']).items():asset.parm(key).set(int(val) if isinstance(val,bool) else val)
g=asset.geometry();first=len(g.points())
asset.parm('width_m').set(cfg['parameters']['width_m']+.6);asset.cook(force=True)
second=asset.geometry();errors=[(n.path(),n.errors()) for n in asset.allSubChildren() if n.errors()]
report={'houdini':hou.applicationVersionString(),'definition':str(path),'hda_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'first_points':first,'changed_points':len(second.points()),
        'output_bounds':list(second.boundingBox().sizevec()),'errors':errors,
        'python_sops':[n.path() for n in asset.allSubChildren() if n.type().name()=='python'],
        'node_types':dict(collections.Counter(n.type().name() for n in asset.allSubChildren()))}
report['status']='pass' if not errors and first>0 and not report['python_sops'] else 'fail'
(ROOT/'results/portable_hda.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report),flush=True)
if report['status']!='pass':raise SystemExit(1)
