"""Read the existing authoring scene without modifying it."""
from pathlib import Path
import json
import hou

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'review_20261102'
OUT.mkdir(exist_ok=True)
hou.hipFile.load(str(ROOT / 'medieval_building.hip'), suppress_save_prompt=True, ignore_load_warnings=True)
rows = []
for node in hou.node('/').allSubChildren():
    row = {'path': node.path(), 'type': node.type().name(),
           'inputs': [n.path() if n else None for n in node.inputs()],
           'parameters': {}, 'user_data': node.userDataDict()}
    for parm in node.parms():
        try:
            value = parm.rawValue()
        except hou.Error:
            continue
        if value != '' or parm.name() in ('snippet', 'python'):
            row['parameters'][parm.name()] = value
    rows.append(row)
    definition = node.type().definition()
    if definition:
        row['definition'] = definition.libraryFilePath()
        for name, section in definition.sections().items():
            if name in ('PythonModule', 'DefaultState') or name.endswith('.py'):
                path = OUT / ('embedded_' + node.name() + '_' + name.replace('/', '_'))
                path.write_text(section.contents(), encoding='utf-8')
(OUT / 'scene_before.json').write_text(json.dumps({'houdini': hou.applicationVersionString(), 'license': str(hou.licenseCategory()), 'nodes': rows}, indent=2), encoding='utf-8')
print('SCENE', len(rows), flush=True)
for row in rows:
    if row['path'].count('/') < 4 or row['type'] in ('python', 'attribwrangle'):
        print(row['path'], row['type'], row.get('definition', ''), flush=True)
