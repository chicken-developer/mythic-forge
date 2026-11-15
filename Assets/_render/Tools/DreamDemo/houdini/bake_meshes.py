"""SM_StarFish.fbx, plus SM_DustMote.fbx (icosahedron) and SM_Bubble.fbx (UV sphere).

The last two are additions to the prompt list: the instanced dust and bubbles move by
world position offset, so they need meshes of ours that can carry a bounds extension.

SM_StarFish contract:
  - about 60 cm long, long axis X, head toward +X, Houdini Y is up (Unreal Z after import)
  - UV0.x runs 0 at the nose to 1 at the tail tip (bend mask for the swim wave)
  - vertex colour R = 1 on fin and tail edges (glow), 0 on the body
"""
import math
import os

import hou

import _bake_common as bc

# ---- parameters -------------------------------------------------------------
LENGTH = 0.60            # metres
BODY_HEIGHT = 0.15
BODY_WIDTH = 0.075
RING_SIDES = 12
# station along the body (0 = nose, 1 = tail root), relative radius
PROFILE = ((0.0, 0.0), (0.06, 0.42), (0.16, 0.78), (0.30, 1.0), (0.46, 0.92),
           (0.62, 0.66), (0.76, 0.38), (0.86, 0.2))
TAIL_ROOT = 0.86         # fraction of LENGTH where the tail fin starts
TAIL_SPAN = 0.21         # tail fin height, metres
DUST_RADIUS = 0.05
BUBBLE_RADIUS = 0.5      # 1 m bubble; instances scale it to 0.5-2 m
# -----------------------------------------------------------------------------


class Builder:
    def __init__(self):
        self.geo = hou.Geometry()
        self.geo.addAttrib(hou.attribType.Point, "Cd", (0.0, 0.0, 0.0))
        self.geo.addAttrib(hou.attribType.Point, "uv", (0.0, 0.0, 0.0))

    def point(self, pos, uv, glow):
        pt = self.geo.createPoint()
        pt.setPosition(hou.Vector3(pos))
        pt.setAttribValue("uv", (uv[0], uv[1], 0.0))
        pt.setAttribValue("Cd", (glow, 0.0, 0.0))
        return pt

    def poly(self, *pts):
        prim = self.geo.createPolygon()
        for pt in pts:
            prim.addVertex(pt)


def station_x(t):
    """t along the fish (0 nose .. 1 tail tip) -> x, with the nose at +LENGTH/2."""
    return LENGTH * (0.5 - t)


def build_fish():
    b = Builder()
    rings = []
    nose = None
    for t, rad in PROFILE:
        x = station_x(t * TAIL_ROOT / PROFILE[-1][0])
        u = t * TAIL_ROOT / PROFILE[-1][0]
        if rad == 0.0:
            nose = b.point((x, 0.0, 0.0), (u, 0.5), 0.0)
            continue
        ring = []
        for j in range(RING_SIDES):
            a = 2.0 * math.pi * j / RING_SIDES
            y = math.cos(a) * BODY_HEIGHT * 0.5 * rad
            z = math.sin(a) * BODY_WIDTH * 0.5 * rad
            ring.append(b.point((x, y, z), (u, j / float(RING_SIDES)), 0.0))
        rings.append(ring)
    first = rings[0]
    for j in range(RING_SIDES):
        b.poly(nose, first[(j + 1) % RING_SIDES], first[j])
    for a, c in zip(rings[:-1], rings[1:]):
        for j in range(RING_SIDES):
            k = (j + 1) % RING_SIDES
            b.poly(a[j], a[k], c[k], c[j])
    last = rings[-1]
    top = last[0]
    bottom = last[RING_SIDES // 2]

    # forked tail fin in the XY plane (thin, rendered two-sided)
    xr = station_x(TAIL_ROOT)
    xt = station_x(1.0)
    xm = station_x(0.94)
    half = TAIL_SPAN * 0.5
    t_top = b.point((xt, half, 0.0), (1.0, 0.0), 1.0)
    t_bot = b.point((xt, -half, 0.0), (1.0, 1.0), 1.0)
    t_mid = b.point((xm, 0.0, 0.0), (0.94, 0.5), 1.0)
    t_top_mid = b.point(((xr + xt) * 0.5, half * 0.62, 0.0), (0.93, 0.2), 0.6)
    t_bot_mid = b.point(((xr + xt) * 0.5, -half * 0.62, 0.0), (0.93, 0.8), 0.6)
    b.poly(top, t_top_mid, t_mid)
    b.poly(top, t_mid, bottom)
    b.poly(bottom, t_mid, t_bot_mid)
    b.poly(t_top_mid, t_top, t_mid)
    b.poly(t_bot_mid, t_mid, t_bot)

    # dorsal fin: swept back triangle strip on the back
    def back_point(t):
        return rings[min(range(len(rings)), key=lambda i: abs(PROFILE[i + 1][0] - t))][0]

    d0 = back_point(0.30)
    d1 = back_point(0.46)
    d2 = back_point(0.62)
    dy = BODY_HEIGHT * 0.5
    d_tip = b.point((station_x(0.50), dy + 0.085, 0.0), (0.50, 0.0), 1.0)
    d_tip2 = b.point((station_x(0.64), dy * 0.75 + 0.05, 0.0), (0.64, 0.0), 1.0)
    b.poly(d0, d_tip, d1)
    b.poly(d1, d_tip, d_tip2)
    b.poly(d1, d_tip2, d2)

    # pectoral fins, one per side, swept back and slightly down
    for side in (1.0, -1.0):
        side_index = RING_SIDES // 4 if side > 0 else 3 * RING_SIDES // 4
        ring_a = rings[2]
        ring_b = rings[3]
        root_a = ring_a[side_index]
        root_b = ring_b[side_index]
        tip = b.point((station_x(0.50), -0.035, side * (BODY_WIDTH * 0.5 + 0.10)), (0.5, 0.5), 1.0)
        mid = b.point((station_x(0.40), -0.01, side * (BODY_WIDTH * 0.5 + 0.075)), (0.4, 0.5), 0.8)
        b.poly(root_a, mid, root_b)
        b.poly(root_b, mid, tip)

    # ventral fin
    v0 = rings[3][RING_SIDES // 2]
    v1 = rings[4][RING_SIDES // 2]
    v_tip = b.point((station_x(0.58), -dy - 0.05, 0.0), (0.58, 1.0), 1.0)
    b.poly(v0, v1, v_tip)
    return b.geo


def build_icosahedron(radius):
    b = Builder()
    phi = (1.0 + math.sqrt(5.0)) / 2.0
    raw = [(-1, phi, 0), (1, phi, 0), (-1, -phi, 0), (1, -phi, 0),
           (0, -1, phi), (0, 1, phi), (0, -1, -phi), (0, 1, -phi),
           (phi, 0, -1), (phi, 0, 1), (-phi, 0, -1), (-phi, 0, 1)]
    scale = radius / math.sqrt(1.0 + phi * phi)
    pts = [b.point((x * scale, y * scale, z * scale), (0.5, 0.5), 1.0) for x, y, z in raw]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
             (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
             (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
             (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for i, j, k in faces:
        b.poly(pts[i], pts[j], pts[k])
    return b.geo


def build_uv_sphere(radius, columns, rows):
    b = Builder()
    grid = []
    for i in range(rows + 1):
        lat = math.pi * i / rows            # 0 at the top
        ring = []
        for j in range(columns + 1):
            lon = 2.0 * math.pi * j / columns
            pos = (radius * math.sin(lat) * math.cos(lon), radius * math.cos(lat),
                   radius * math.sin(lat) * math.sin(lon))
            ring.append(b.point(pos, (j / float(columns), i / float(rows)), 0.0))
        grid.append(ring)
    for i in range(rows):
        for j in range(columns):
            a, c = grid[i][j], grid[i][j + 1]
            d, e = grid[i + 1][j], grid[i + 1][j + 1]
            if i == 0:
                b.poly(a, e, d)
            elif i == rows - 1:
                b.poly(a, c, d)
            else:
                b.poly(a, c, e, d)
    return b.geo


def export_fbx(geo, name, timer, cusp=60.0):
    bgeo = os.path.join(bc.CHECK_DIR, name + ".bgeo.sc")
    geo.saveToFile(bgeo)
    obj = hou.node("/obj").createNode("geo", name)
    src = obj.createNode("file")
    src.parm("file").set(bgeo.replace("\\", "/"))
    tri = obj.createNode("divide")
    tri.setInput(0, src)
    promote = obj.createNode("attribpromote")
    promote.setInput(0, tri)
    promote.parm("inname").set("uv")
    promote.parm("inclass").set("point")
    promote.parm("outclass").set("vertex")
    normal = obj.createNode("normal")
    normal.setInput(0, promote)
    normal.parm("cuspangle").set(cusp)
    out = obj.createNode("null", "OUT")
    out.setInput(0, normal)
    out.setDisplayFlag(True)
    out.setRenderFlag(True)
    result = out.geometry()
    tris = len(result.prims())
    bbox = result.boundingBox()

    path = os.path.join(bc.RAW_DIR, name + ".fbx")
    rop = hou.node("/out").createNode("filmboxfbx", "export_" + name)
    rop.parm("startnode").set(obj.path())
    rop.parm("sopoutput").set(path.replace("\\", "/"))
    rop.parm("createsubnetroot").set(0)
    rop.parm("convertunits").set(1)      # metres -> centimetres, so Unreal imports at 1:1
    rop.render()
    timer.output(path)
    timer.note(name, {
        "triangles": tris,
        "size_m": [round(v, 4) for v in bbox.sizevec()],
        "min": [round(v, 4) for v in bbox.minvec()],
        "max": [round(v, 4) for v in bbox.maxvec()],
    })
    return tris


def main():
    timer = bc.BakeTimer("SM_Meshes")
    tris = export_fbx(build_fish(), "SM_StarFish", timer)
    if not 150 <= tris <= 400:
        timer.note("warning", "SM_StarFish has %d triangles, outside 150-400" % tris)
    export_fbx(build_icosahedron(DUST_RADIUS), "SM_DustMote", timer, cusp=180.0)
    export_fbx(build_uv_sphere(BUBBLE_RADIUS, 24, 14), "SM_Bubble", timer, cusp=180.0)
    timer.finish()


main()
