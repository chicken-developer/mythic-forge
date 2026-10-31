from pathlib import Path
import json
import time
import sys
import hou
import build_hda as B
import native_network as N

ROOT=Path(__file__).parent
N.CORE_SOURCE=(ROOT/'build_hda.py').read_text(encoding='utf-8').split('\n# INSTALLER_BOUNDARY')[0]
N.PRODUCTION_SOURCE=(ROOT/'production_geometry.py').read_text(encoding='utf-8')
ident=sys.argv[1] if len(sys.argv)>1 else 'variant_11'
record=json.loads((ROOT/'datasets'/(ident+'.json')).read_text(encoding='utf-8'))
N.PRESETS={ident:record}
obj=hou.node('/obj').createNode('geo','production_probe',run_init_scripts=False)
node=obj.createNode('subnet','generator');node.setParmTemplateGroup(N.interface(B.normalize(record)))
for k,v in record['parameters'].items():node.parm(k).set(int(v) if isinstance(v,bool) else v)
for k,v in record['seeds'].items():node.parm('seed_'+k).set(v)
started=time.time();N.rebuild(node);node.cook(force=True)
errors=[(n.path(),list(n.errors())) for n in node.allSubChildren() if n.errors()]
print('ERRORS',errors,flush=True)
assert not errors
g=node.geometry();print('PRODUCTION_PROBE',ident,len(g.points()),len(g.prims()),g.boundingBox(),round(time.time()-started,2),flush=True)
print('MATERIALS',sorted(set(g.primStringAttribValues('material_id'))),flush=True)
print('NAMES',list(sorted(set(g.primStringAttribValues('name'))))[:12],flush=True)
out=ROOT/'houdini_output';out.mkdir(exist_ok=True)
hou.hipFile.save(str(out/('probe_'+ident+'.hip')))
