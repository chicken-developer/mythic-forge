"""Read-only capability and source audit for the catalog detail pass."""
from pathlib import Path
import json
import sys
import hou

ROOT = Path('E:/Repos/HoudiniWorkspace')
OUT = Path('E:/Repos/mythic-forge/TDKProps/Saved/PropsDetail')
OUT.mkdir(parents=True, exist_ok=True)
print('RUNTIME', sys.executable, hou.applicationVersionString(), hou.licenseCategory(), flush=True)
obj = hou.node('/obj').createNode('geo', '__capability_probe', run_init_scripts=False)
for typename in ('sweep::2.0', 'normal', 'fuse::2.0', 'polybevel::3.0'):
    node = obj.createNode(typename)
    rows = []
    for p in node.parms():
        if not any(k in p.name() for k in ('upvector','orient','endcap','cusp','attrib','snap','tol','group','divisions')): continue
        row = {'name':p.name(), 'value':p.eval()}
        try: row.update(items=list(p.menuItems()), labels=list(p.menuLabels()))
        except hou.OperationFailed: pass
        rows.append(row)
    print(typename, json.dumps(rows), flush=True)
    node.destroy()
sys.path.insert(0, str(ROOT/'medieval_building'))
import build_hda as M
records = []
for path in sorted((ROOT/'medieval_building/assets/datasets').glob('*.json')):
    cfg = M.normalize(json.loads(path.read_text()))
    mesh = M.generate(cfg)
    records.append({'id':path.stem, 'quality':cfg['parameters']['detail_level'],
                    'interior':cfg['parameters']['interior_enable'], 'faces':len(mesh.faces),
                    'interior_faces':sum('int_' in n for n in mesh.names), 'features':mesh.features})
(OUT/'baseline_medieval.json').write_text(json.dumps(records, indent=2))
print('MEDIEVAL_BASELINE', json.dumps(records), flush=True)
