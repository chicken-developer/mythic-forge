"""Fresh-process test; only stdlib + hou, and only a relocated HDA file."""
from pathlib import Path
import json
import hashlib
import shutil
import hou

ROOT=Path(__file__).resolve().parent
folder=ROOT/'results/portable';folder.mkdir(exist_ok=True)
asset=folder/'relocated_medieval_building.hda';shutil.copy2(ROOT/'asset/medieval_building_modular.hda',asset)
hou.hda.installFile(str(asset))
obj=hou.node('/obj').createNode('geo','PORTABLE_ONLY',run_init_scripts=False)
node=obj.createNode('mythic::medieval_house::4.0','BUILDING')
cfg=json.loads(node.type().definition().sections()['Baseline.json'].contents())
for key,value in dict(cfg['parameters'],seed_decoration=cfg['seeds']['decoration']).items():node.parm(key).set(int(value) if isinstance(value,bool) else value)
node.cook(force=True);g=node.geometry();first={'points':g.intrinsicValue('pointcount'),'bounds':list(g.boundingBox().sizevec()),'hash':hashlib.sha256(str(g.pointFloatAttribValues('P')).encode()).hexdigest()}
node.parm('width_m').set(cfg['parameters']['width_m']+.6);node.cook(force=True)
g=node.geometry();second={'points':g.intrinsicValue('pointcount'),'bounds':list(g.boundingBox().sizevec()),'hash':hashlib.sha256(str(g.pointFloatAttribValues('P')).encode()).hexdigest()}
errors=[{'path':n.path(),'errors':list(n.errors())} for n in node.allSubChildren() if n.errors()]
external_files=[]
for n in node.allSubChildren():
    if n.type().name()=='file':external_files.append(n.path())
definitions=[{'type':d.nodeTypeName(),'library':d.libraryFilePath()} for d in hou.hda.definitionsInFile(str(asset))]
result={'houdini':hou.applicationVersionString(),'hda_sha256':hashlib.sha256(asset.read_bytes()).hexdigest(),
        'relocated_asset':str(asset.relative_to(ROOT)).replace('\\','/'),'definitions':definitions,
        'baseline':first,'changed_width':second,'errors':errors,'external_geometry_files':external_files,
        'authoring_scripts_imported':False,'status':'pass' if first['points']>0 and first['hash']!=second['hash'] and not errors and not external_files else 'fail'}
(ROOT/'results/portability.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('PORTABILITY',result['status'],len(definitions),flush=True)
raise SystemExit(result['status']!='pass')
