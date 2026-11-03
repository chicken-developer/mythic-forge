"""Read-only Houdini discovery; all evidence is written under this package."""
from pathlib import Path
import json
import collections
import hou

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
SOURCE = REPO / 'Assets/hda_medieval_building'
(ROOT / 'results').mkdir(exist_ok=True)
hou.hipFile.load(str(SOURCE / 'medieval_building.hip'), suppress_save_prompt=True, ignore_load_warnings=True)
b = hou.node('/obj/medieval_building')
rows = []
for n in b.allSubChildren():
    row = {'path': n.relativePathTo(b), 'relative_path': b.relativePathTo(n),
           'type': n.type().name(), 'inputs': [], 'parameters': {},
           'user_data': n.userDataDict()}
    for c in n.inputConnections():
        row['inputs'].append({'input': c.inputIndex(), 'source': b.relativePathTo(c.inputNode()), 'output': c.outputIndex()})
    for p in n.parms():
        row['parameters'][p.name()] = p.rawValue()
    rows.append(row)
report = {'houdini': hou.applicationVersionString(), 'license': str(hou.licenseCategory()),
          'root_parameters': {p.name(): p.rawValue() for p in b.parms()}, 'nodes': rows,
          'types': dict(collections.Counter(r['type'] for r in rows))}
(ROOT / 'results/source_inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
for n in b.children():
    print(n.name(), n.type().name(), [(c.inputIndex(), c.inputNode().name(), c.outputIndex()) for c in n.inputConnections()], flush=True)
print('COLLAPSE_DOC', hou.Node.collapseIntoSubnet.__doc__ if hasattr(hou.Node, 'collapseIntoSubnet') else hou.OpNode.collapseIntoSubnet.__doc__, flush=True)
print('MOVE_DOC', hou.moveNodesTo.__doc__, flush=True)
print('EXTRACT_DOC', hou.OpNode.extractAndDelete.__doc__, flush=True)
