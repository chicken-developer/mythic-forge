from pathlib import Path
import hashlib
import json
import time
import collections
import numpy as np
import hou

ROOT = Path(__file__).resolve().parents[1]

def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def digest(values):
    return hashlib.sha256(json.dumps(values, separators=(',', ':'), sort_keys=True).encode()).hexdigest()

def geometry_metrics(g):
    xyz = np.asarray(g.pointFloatAttribValues('P'), dtype=np.float64).reshape(-1, 3)
    # Quantized P/topology excludes timestamps, node paths and volatile data IDs.
    positions = hashlib.sha256(np.round(xyz, 5).astype('<f8').tobytes()).hexdigest()
    topology = hashlib.sha256()
    degenerate = 0
    duplicate = 0
    seen = set()
    valid_seen = set()
    for p in g.prims():
        verts = p.vertices()
        ids = [v.point().number() for v in verts]
        topology.update(np.asarray(ids, dtype='<i4').tobytes())
        topology.update(b'|')
        if p.type() == hou.primType.Polygon:
            pts = xyz[ids]
            area = np.linalg.norm(np.cross(pts, np.roll(pts, -1, axis=0)).sum(axis=0)) * .5
            degenerate += int(area < 1e-10)
            key = tuple(sorted(map(tuple, np.round(pts, 5))))
            if area>=1e-10:valid_seen.add(key)
            duplicate += int(key in seen)
            seen.add(key)
    names = list(g.primStringAttribValues('name')) if g.findPrimAttrib('name') else []
    families = list(g.primStringAttribValues('material_family')) if g.findPrimAttrib('material_family') else []
    uv = None
    if g.findVertexAttrib('uv'):
        uv = np.asarray(g.vertexFloatAttribValues('uv'), dtype=np.float64)
    elif g.findPointAttrib('uv'):
        uv = np.asarray(g.pointFloatAttribValues('uv'), dtype=np.float64)
    normals = g.findVertexAttrib('N') or g.findPointAttrib('N') or g.findPrimAttrib('N')
    normal_values=None
    if g.findVertexAttrib('N'):normal_values=g.vertexFloatAttribValues('N')
    elif g.findPointAttrib('N'):normal_values=g.pointFloatAttribValues('N')
    elif g.findPrimAttrib('N'):normal_values=g.primFloatAttribValues('N')
    uvhash = hashlib.sha256(np.round(uv, 5).astype('<f8').tobytes()).hexdigest() if uv is not None else None
    result = {'points': len(xyz), 'primitives': len(g.prims()), 'vertices': g.intrinsicValue('vertexcount'),
              'bounds_min': list(g.boundingBox().minvec()), 'bounds_max': list(g.boundingBox().maxvec()),
              'dimensions': list(g.boundingBox().sizevec()), 'finite_positions': bool(np.isfinite(xyz).all()),
              'position_hash': positions, 'topology_hash': topology.hexdigest(), 'name_hash': digest(names),
              'uv_hash': uvhash, 'material_hash': digest(families), 'names': sorted(set(names)),
              'material_families': dict(collections.Counter(families)),
              'uv_present': uv is not None, 'uv_finite': bool(np.isfinite(uv).all()) if uv is not None else False,
              'normals_present': normals is not None, 'degenerate_polygons': degenerate,
              'normals_finite': bool(np.isfinite(normal_values).all()) if normal_values else False,
              'valid_unique_surface_hash': digest(sorted(valid_seen)),
              'exact_duplicate_polygons': duplicate,
              'primitive_attributes': [a.name() for a in g.primAttribs()],
              'point_attributes': [a.name() for a in g.pointAttribs()]}
    result['normalized_geometry_hash'] = digest({k: result[k] for k in ('position_hash','topology_hash','name_hash','uv_hash','material_hash')})
    return result

def node_messages(root):
    return {'errors': [{'path': root.relativePathTo(n), 'messages': list(n.errors())} for n in root.allSubChildren() if n.errors()],
            'warnings': [{'path': root.relativePathTo(n), 'messages': list(n.warnings())} for n in root.allSubChildren() if n.warnings()]}

def cook_record(root, output, record, geometry_path):
    start = time.perf_counter()
    output.cook(force=True)
    g = output.geometry()
    seconds = time.perf_counter() - start
    metrics = geometry_metrics(g)
    messages = node_messages(root)
    target = ROOT / geometry_path
    target.parent.mkdir(parents=True, exist_ok=True)
    g.saveToFile(str(target))
    result = {'variant_id': record['variant_id'], 'effective_config': record,
              'houdini': hou.applicationVersionString(), 'license': str(hou.licenseCategory()),
              'metrics': metrics, 'cook_seconds': seconds, **messages,
              'geometry': geometry_path, 'geometry_sha256': sha(target)}
    result['cook_status'] = 'pass' if metrics['points'] and metrics['finite_positions'] and not messages['errors'] else 'fail'
    return result
