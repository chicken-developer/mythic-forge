"""Convert locally exported SkyTown meshes to a portable, UV-preserving HDA library."""
from pathlib import Path
import hashlib
import json
import shutil
import hou

P = Path(__file__).resolve().parents[2]
R = P / 'Saved/SkyTownReview'
library = {}
for name, row in json.loads((R / 'prop_exports.json').read_text()).items():
    root, warnings = hou.hipFile.importFBX(row['file'], suppress_save_prompt=True,
        convert_units=True, convert_into_y_up_coordinate_system=True,
        import_animation=False, import_materials=True)
    geometry = hou.Geometry()
    for node in root.allSubChildren():
        if node.type().name() == 'geo' and node.displayNode():
            part = hou.Geometry(node.displayNode().geometry())
            part.transform(node.worldTransform())
            geometry.merge(part)
    assert geometry.prims(), name
    size = geometry.boundingBox().sizevec()
    expected = [(row['max_cm'][i] - row['min_cm'][i]) / 100 for i in (0, 2, 1)]
    assert max(abs(size[i] - expected[i]) for i in range(3)) < .001, (name, size, expected)
    materials = {path.split('.')[-1]: path for path in row['materials'].values()}
    faces, normals, uvs, assigned = [], [], [], []
    for prim in geometry.prims():
        faces.append([v.point().number() for v in prim.vertices()])
        normals.append([list(v.attribValue('N')) for v in prim.vertices()])
        uvs.append([list(v.attribValue('uv')) for v in prim.vertices()])
        material = prim.attribValue('shop_materialpath').split('/')[-1]
        assert material in materials, (name, material, materials)
        assigned.append(materials[material])
    library[name] = {'points': [list(p.position()) for p in geometry.points()],
        'faces': faces, 'normals': normals, 'uvs': uvs, 'materials': assigned,
        'source_asset': '/Game/SkyTown/Meshes/' + name,
        'source_fbx_sha256': hashlib.sha256(Path(row['file']).read_bytes()).hexdigest()}
    print('SKYTOWN_LIBRARY', name, len(faces), flush=True)
    root.destroy()
payload = json.dumps(library, separators=(',', ':'))
for family in ('Medieval', 'Gothic'):
    folder = P / 'SourceAssets/Houdini' / family
    (folder / 'skytown_library.json').write_text(payload, encoding='utf-8')
    shutil.copy2(P / 'Tools/Props/skytown_geometry.py', folder / 'skytown_geometry.py')
(P / 'RenderTests/SkyTown/library.json').write_text(json.dumps({
    'status': 'passed', 'runtime': hou.applicationVersionString(),
    'assets': {name: {'triangles': len(row['faces']), 'source': row['source_asset'],
                     'materials': sorted(set(row['materials']))} for name, row in library.items()}
}, indent=2))
