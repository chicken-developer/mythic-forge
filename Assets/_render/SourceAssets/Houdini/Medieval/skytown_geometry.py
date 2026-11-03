"""Portable SkyTown geometry assembly; embedded with the local library in each HDA."""
import json
import math
from pathlib import Path

_SKYTOWN_CACHE = None


def skytown_library(node=None):
    global _SKYTOWN_CACHE
    if _SKYTOWN_CACHE is None:
        import hou
        current = node or hou.pwd()
        payload = None
        while current:
            definition = current.type().definition()
            if definition and 'SkyTownLibrary.json' in definition.sections():
                payload = definition.sections()['SkyTownLibrary.json'].contents()
                break
            current = current.parent()
        if payload is None:
            payload = Path(__file__).with_name('skytown_library.json').read_text(encoding='utf-8')
        _SKYTOWN_CACHE = json.loads(payload)
    return _SKYTOWN_CACHE


def skytown_geometry(asset, center, size, yaw, family, name, node=None):
    import hou
    row = skytown_library(node)[asset]
    geo = hou.Geometry()
    for owner, attr, default in [(hou.attribType.Prim, 'name', ''),
            (hou.attribType.Prim, 'material_family', ''), (hou.attribType.Prim, 'material_id', ''),
            (hou.attribType.Prim, 'skytown_source', ''), (hou.attribType.Prim, 'unreal_material', ''),
            (hou.attribType.Prim, 'Cd', (1., 1., 1.)),
            (hou.attribType.Vertex, 'N', (0., 1., 0.)), (hou.attribType.Vertex, 'uv', (0., 0., 0.))]:
        geo.addAttrib(owner, attr, default, transform_as_normal=attr == 'N')
    points = geo.createPoints(row['points'])
    for indices, normals, uvs, material in zip(row['faces'], row['normals'], row['uvs'], row['materials']):
        prim = geo.createPolygon()
        for index, normal, uv in zip(indices, normals, uvs):
            vertex = prim.addVertex(points[index])
            vertex.setAttribValue('N', normal)
            vertex.setAttribValue('uv', uv)
        mat = 'skytown_' + material.split('.')[-1]
        prim.setAttribValue('name', name + '/SkyTown/' + asset)
        prim.setAttribValue('material_family', mat)
        prim.setAttribValue('material_id', mat)
        prim.setAttribValue('skytown_source', row['source_asset'])
        prim.setAttribValue('unreal_material', material)
    geo.transform(hou.hmath.buildRotate((0, yaw, 0)))
    box = geo.boundingBox()
    scale = [size[i] / max(box.sizevec()[i], 1e-6) for i in range(3)]
    geo.transform(hou.hmath.buildTranslate(-box.center()) * hou.hmath.buildScale(scale)
                  * hou.hmath.buildTranslate(center))
    return geo


def replace_skytown_props(geo, family):
    groups = {}
    for prim in geo.prims():
        name = prim.attribValue('name')
        if 'SKYTOWN|' in name:
            groups.setdefault(name, []).append(prim)
    replacements = []
    for name, prims in groups.items():
        _, asset, yaw, _ = name[name.index('SKYTOWN|'):].split('|', 3)
        points = [v.point().position() for p in prims for v in p.vertices()]
        low = [min(p[i] for p in points) for i in range(3)]
        high = [max(p[i] for p in points) for i in range(3)]
        replacements.append(skytown_geometry(asset, [(a+b)/2 for a,b in zip(low, high)],
            [b-a for a,b in zip(low, high)], float(yaw), family, name))
    geo.deletePrims([p for prims in groups.values() for p in prims], True)
    for replacement in replacements:
        geo.merge(replacement)
    if replacements:
        geo.addAttrib(__import__('hou').attribType.Global, 'skytown_prop_count', 0)
        geo.setGlobalAttribValue('skytown_prop_count', len(replacements))
