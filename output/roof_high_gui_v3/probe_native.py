from pathlib import Path
import json
import hou
ROOT=Path(__file__).resolve().parent
obj=hou.node('/obj').createNode('geo','CAPABILITY_PROBE',run_init_scripts=False)
types=hou.sopNodeTypeCategory().nodeTypes()
report={}
for kind in ('polyextrude::2.0','boolean::2.0','polybevel::3.0','polywire','fuse::2.0','normal','switch'):
    if kind not in types:
        report[kind]={'available':False};continue
    n=obj.createNode(kind)
    report[kind]={'available':True,'parms':{p.name():{'value':p.rawValue(),'menu':list(p.menuItems()) if p.parmTemplate().type()==hou.parmTemplateType.Menu else []} for p in n.parms()}}
(ROOT/'results/native_capabilities.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('CAPABILITIES',list(report),flush=True)
