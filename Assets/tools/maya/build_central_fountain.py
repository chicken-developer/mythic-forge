# -*- coding: utf-8 -*-
"""
Central Fountain (concept 03) -- Maya build script.

Builds the MAIN fountain object only (no water, flowers, banners, VFX, textures):
    CentralFountain_low   game mesh, hard/soft edges set, UVs unwrapped + packed in 0-1
    CentralFountain_high  bake source: stone blocks split, slight variation, bevelled edges
and writes them to SM_CentralFountain.ma next to the concept art.

Run inside Maya (Script Editor, Python tab):
    import runpy; runpy.run_path(r"E:/Repos/mythic-forge/Assets/tools/maya/build_central_fountain.py", run_name="__main__")

Conventions: units cm, Y up, front (stairs) = +Z, pivot = bottom centre at origin.
Sizes from the concept sheet: footprint ~14 m, height 7.2 m, main pool Ø10 m, upper pool Ø5 m.

Layout of this file
    1. Settings & dimensions      - edit these to tweak the asset
    2. MeshData + primitives      - pure python (revolve / prism), UVs generated per shell
    3. Parts                      - one function per fountain part, low/high variants
    4. Maya                       - create meshes, bevel high, layout UVs, save .ma
"""
from __future__ import print_function

import math
import random

# =============================================================================
# 1. SETTINGS & DIMENSIONS (cm)
# =============================================================================
OUT_PATH = r"E:/Repos/mythic-forge/Assets/3d_central_fountain/SM_CentralFountain.ma"
ROOT = "SM_CentralFountain"
LOW_NAME = "CentralFountain_low"
HIGH_NAME = "CentralFountain_high"
MATERIAL = "M_CentralFountain"
STONE_RGB = (0.62, 0.55, 0.47)

LODS = {
    # segs: segments for a full 360 revolve. blocks: split stone into blocks (high only)
    "low": dict(segs=32, blocks=False),
    "high": dict(segs=128, blocks=True),
}
HIGH_BEVEL_CM = 1.6       # edge bevel on the high mesh
HIGH_BEVEL_SEGS = 2
BLOCK_GAP_CM = 1.2        # mortar gap between stone blocks (high)
SEED = 3                  # variation seed (high)

FLOOR_Y = 35.0            # plinth top = main pool floor

PLINTH = dict(r=610.0, h=FLOOR_Y)
POOL_WALL = dict(r_in=500.0, r_out=560.0, y0=FLOOR_Y - 1.0, y1=118.0)   # main pool Ø10 m
WALL_CAP = dict(r_in=490.0, r_out=575.0, y0=117.0, y1=135.0)

BUTTRESS = dict(angles=(90, 180, 270), radius=585.0, w=150.0, d=170.0, h=150.0,
                cap_w=170.0, cap_d=190.0, cap_h=15.0)
PLANTER = dict(angles=(45, 135, 225, 315), radius=592.0, size=180.0, h=150.0,
               rim_t=22.0, rim_h=22.0)
STAIRS = dict(steps=5, rise=26.8, tread=30.0, width=242.0, r_start=545.0,
              cheek_w=40.0)   # top step 1 cm under the wall cap top (no z-fighting)

# Central pedestal + upper basin, revolve profile (r, y), counter-clockwise.
PEDESTAL_PROFILE = [
    (0, FLOOR_Y - 1), (170, FLOOR_Y - 1), (170, 100), (140, 100), (140, 170),
    (105, 170), (105, 250), (255, 315), (255, 365), (265, 365), (265, 385),
    (222, 385), (222, 345), (125, 345), (125, 400), (92, 400), (92, 422), (0, 422),
]

ARCH = dict(center_y=530.0, r_in=135.0, r_out=190.0, depth=56.0,
            gap_top_deg=14.0, end_deg=150.0, blocks=5, segs_low=10)
GEM = dict(center_y=530.0, outer_w=78.0, outer_h=115.0, inner_w=52.0,
           inner_h=82.0, frame_depth=44.0, gem_depth=30.0)
GEM_PLINTH = dict(w=60.0, h=26.0, d=50.0, y0=410.0)


# =============================================================================
# 2. MESH DATA + PRIMITIVES (pure python, no Maya)
# =============================================================================
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _norm(a):
    l = math.sqrt(_dot(a, a)) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def newell_normal(pts):
    n = [0.0, 0.0, 0.0]
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        n[0] += (p[1] - q[1]) * (p[2] + q[2])
        n[1] += (p[2] - q[2]) * (p[0] + q[0])
        n[2] += (p[0] - q[0]) * (p[1] + q[1])
    return _norm(n)


def planar_basis(n):
    """Tangent/bitangent with t x b = n, so CCW faces get non-flipped UVs.
    Vertical faces: u horizontal, v up. Horizontal faces: u = x."""
    ref = (0.0, 1.0, 0.0) if abs(n[1]) < 0.9 else (0.0, 0.0, 1.0)
    t = _norm(_cross(ref, n)) if abs(n[1]) < 0.9 else (1.0, 0.0, 0.0)
    b = _cross(n, t)
    return t, b


def signed_area_2d(pts):
    return 0.5 * sum(p[0] * q[1] - q[0] * p[1] for p, q in zip(pts, pts[1:] + pts[:1]))


class MeshData(object):
    """Polygon soup with per-face-corner UVs grouped in shells. Units: cm, UV: metres."""

    def __init__(self):
        self.verts = []
        self.faces = []      # list of [vid, ...]
        self.uvs = []        # list of [(u, v), ...] matching faces
        self.shells = []     # shell id per face
        self._next_shell = 0

    def new_shell(self):
        self._next_shell += 1
        return self._next_shell - 1

    def add_vert(self, p):
        self.verts.append(tuple(float(c) for c in p))
        return len(self.verts) - 1

    def add_face(self, vids, uvs, shell):
        # drop consecutive duplicates (collapsed pole corners)
        fv, fu = [], []
        for v, uv in zip(vids, uvs):
            if fv and fv[-1] == v:
                continue
            fv.append(v)
            fu.append(uv)
        if len(fv) > 1 and fv[0] == fv[-1]:
            fv.pop()
            fu.pop()
        if len(fv) >= 3:
            self.faces.append(fv)
            self.uvs.append(fu)
            self.shells.append(shell)

    def add_planar_face(self, vids, shell=None, basis=None):
        pts = [self.verts[v] for v in vids]
        t, b = basis or planar_basis(newell_normal(pts))
        uvs = [(_dot(p, t) / 100.0, _dot(p, b) / 100.0) for p in pts]
        self.add_face(vids, uvs, self.new_shell() if shell is None else shell)

    def drop_ground_faces(self, eps=0.01):
        """Remove faces lying flat on the ground (never visible) and unused vertices."""
        keep = [k for k, f in enumerate(self.faces) if not all(self.verts[v][1] <= eps for v in f)]
        self.faces = [self.faces[k] for k in keep]
        self.uvs = [self.uvs[k] for k in keep]
        self.shells = [self.shells[k] for k in keep]
        used = sorted(set(v for f in self.faces for v in f))
        remap = dict((old, new) for new, old in enumerate(used))
        self.verts = [self.verts[v] for v in used]
        self.faces = [[remap[v] for v in f] for f in self.faces]
        return self

    def merge(self, other):
        off, soff = len(self.verts), self._next_shell
        self.verts.extend(other.verts)
        self.faces.extend([[v + off for v in f] for f in other.faces])
        self.uvs.extend([list(f) for f in other.uvs])
        self.shells.extend([s + soff for s in other.shells])
        self._next_shell += other._next_shell
        return self

    # --- transforms (proper rotations only, so face winding stays outward) ----
    def _map(self, fn):
        self.verts = [fn(p) for p in self.verts]
        return self

    def translate(self, x=0.0, y=0.0, z=0.0):
        return self._map(lambda p: (p[0] + x, p[1] + y, p[2] + z))

    def rotate_y(self, deg):
        c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
        return self._map(lambda p: (p[0] * c + p[2] * s, p[1], -p[0] * s + p[2] * c))

    def rotate_x(self, deg):
        c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
        return self._map(lambda p: (p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c))

    def place_radial(self, radius, angle_deg):
        """Local +Z points away from the fountain centre; angle 0 = front (+Z)."""
        return self.translate(z=radius).rotate_y(angle_deg)


def merge_all(meshes):
    out = MeshData()
    for m in meshes:
        out.merge(m)
    return out


def revolve(profile, a0=0.0, a1=360.0, segs=32):
    """Revolve a closed (r, y) profile around Y. Angle 0 = +Z, increasing toward +X.
    UVs: horizontal faces -> planar shell per profile edge, other edges -> strip shells
    (u = arc length, v = profile length), partial revolves get planar end caps."""
    prof = [(float(r), float(y)) for r, y in profile]
    if signed_area_2d(prof) < 0:
        prof.reverse()
    full = abs(a1 - a0) >= 360.0 - 1e-6
    segs = max(1, int(segs))
    ncol = segs if full else segs + 1
    thetas = [math.radians(a0 + (a1 - a0) * j / float(segs)) for j in range(segs + 1)]
    m = MeshData()
    eps = 1e-6
    vid = []
    for r, y in prof:
        if r < eps:
            pole = m.add_vert((0.0, y, 0.0))
            vid.append([pole] * ncol)
        else:
            vid.append([m.add_vert((r * math.sin(thetas[j]), y, r * math.cos(thetas[j])))
                        for j in range(ncol)])

    def col(j):
        return j % ncol

    n = len(prof)
    edges = [(i, (i + 1) % n) for i in range(n)]
    # start walking at a horizontal edge so strips are not cut at index 0
    start = next((k for k, (i, k2) in enumerate(edges) if abs(prof[i][1] - prof[k2][1]) < eps), 0)
    order = edges[start:] + edges[:start]

    strips, cur = [], []
    for i, k in order:
        (r0, y0), (r1, y1) = prof[i], prof[k]
        if r0 < eps and r1 < eps:            # edge on the axis: no faces
            if cur:
                strips.append(cur); cur = []
            continue
        if abs(y1 - y0) < eps:               # horizontal -> planar shell
            if cur:
                strips.append(cur); cur = []
            shell = m.new_shell()
            basis = planar_basis((0.0, 1.0 if r1 < r0 else -1.0, 0.0))
            for j in range(segs):
                m.add_planar_face([vid[i][col(j)], vid[i][col(j + 1)], vid[k][col(j + 1)], vid[k][col(j)]],
                                  shell, basis)
            continue
        ravg = 0.5 * (r0 + r1)
        if cur:
            ci, ck = cur[0]
            cr = 0.5 * (prof[ci][0] + prof[ck][0])
            same_dir = (prof[ck][1] - prof[ci][1]) * (y1 - y0) > 0
            if not same_dir or abs(ravg - cr) > 0.3 * max(cr, ravg):
                strips.append(cur); cur = []
        cur.append((i, k))
    if cur:
        strips.append(cur)

    for strip in strips:
        shell = m.new_shell()
        radii = [0.5 * (prof[i][0] + prof[k][0]) for i, k in strip]
        R = sum(radii) / len(radii)
        sgn = 1.0 if prof[strip[0][1]][1] > prof[strip[0][0]][1] else -1.0
        vcum = 0.0
        for i, k in strip:
            seg_len = math.hypot(prof[k][0] - prof[i][0], prof[k][1] - prof[i][1])
            v0, v1 = sgn * vcum / 100.0, sgn * (vcum + seg_len) / 100.0
            vcum += seg_len
            for j in range(segs):
                u0 = sgn * (thetas[j] - thetas[0]) * R / 100.0
                u1 = sgn * (thetas[j + 1] - thetas[0]) * R / 100.0
                # A(i,j) D(i,j+1) C(k,j+1) B(k,j) -> outward for a CCW profile
                m.add_face([vid[i][col(j)], vid[i][col(j + 1)], vid[k][col(j + 1)], vid[k][col(j)]],
                           [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], shell)

    if not full:
        cap0 = []
        cap1 = []
        for row in vid:
            if not cap0 or cap0[-1] != row[0]:
                cap0.append(row[0])
            if not cap1 or cap1[-1] != row[-1]:
                cap1.append(row[-1])
        m.add_planar_face(cap0)
        m.add_planar_face(cap1[::-1])
    return m


def prism(outline_xz, y0, y1):
    """Extrude a closed (x, z) outline from y0 to y1. Side = one strip shell, caps planar."""
    pts = [(float(x), float(z)) for x, z in outline_xz]
    if signed_area_2d([(x, -z) for x, z in pts]) < 0:   # want CCW seen from +Y
        pts.reverse()
    m = MeshData()
    bot = [m.add_vert((x, y0, z)) for x, z in pts]
    top = [m.add_vert((x, y1, z)) for x, z in pts]
    n = len(pts)
    shell = m.new_shell()
    ucum = 0.0
    for k in range(n):
        k1 = (k + 1) % n
        seg = math.hypot(pts[k1][0] - pts[k][0], pts[k1][1] - pts[k][1])
        u0, u1 = ucum / 100.0, (ucum + seg) / 100.0
        ucum += seg
        m.add_face([bot[k], bot[k1], top[k1], top[k]],
                   [(u0, y0 / 100.0), (u1, y0 / 100.0), (u1, y1 / 100.0), (u0, y1 / 100.0)], shell)
    m.add_planar_face(top)
    m.add_planar_face(bot[::-1])
    return m


def box(w, d, y0, y1, x=0.0, z=0.0):
    hw, hd = w / 2.0, d / 2.0
    return prism([(x - hw, z - hd), (x + hw, z - hd), (x + hw, z + hd), (x - hw, z + hd)], y0, y1)


def tube_profile(r_in, r_out, y0, y1):
    return [(r_in, y0), (r_out, y0), (r_out, y1), (r_in, y1)]


def ring_blocks(r_in, r_out, y0, y1, count, segs_full, phase_deg, rng, jitter=1.5):
    """Ring split into stone blocks with mortar gaps and small size variation."""
    out = []
    step = 360.0 / count
    gap_deg = math.degrees(BLOCK_GAP_CM / r_out)
    per = max(2, int(round(segs_full / float(count))))
    for b in range(count):
        a0 = phase_deg + b * step + gap_deg / 2.0
        a1 = a0 + step - gap_deg
        ro = r_out + rng.uniform(-jitter, jitter)
        top = y1 + rng.uniform(-jitter, jitter * 0.5)
        out.append(revolve(tube_profile(r_in, ro, y0, top), a0, a1, per))
    return out


def ring(r_in, r_out, y0, y1, lod, blocks=0, courses=1, rng=None):
    """Full ring: one piece on low, `courses` x `blocks` stones on high."""
    if not (lod["blocks"] and blocks):
        return [revolve(tube_profile(r_in, r_out, y0, y1), segs=lod["segs"])]
    out = []
    ch = (y1 - y0) / float(courses)
    for c in range(courses):
        cy0 = y0 + c * ch + (BLOCK_GAP_CM / 2.0 if c else 0.0)
        cy1 = y0 + (c + 1) * ch - (BLOCK_GAP_CM / 2.0 if c < courses - 1 else 0.0)
        phase = (180.0 / blocks) * (c % 2) + rng.uniform(0, 3)
        out += ring_blocks(r_in, r_out, cy0, cy1, blocks, lod["segs"], phase, rng)
    return out


def stacked_box(w, d, y0, y1, lod, courses, rng):
    """Box on low; stacked stone courses with slight size variation on high."""
    if not lod["blocks"]:
        return [box(w, d, y0, y1)]
    out = []
    ch = (y1 - y0) / float(courses)
    for c in range(courses):
        cy0 = y0 + c * ch + (BLOCK_GAP_CM / 2.0 if c else 0.0)
        cy1 = y0 + (c + 1) * ch - (BLOCK_GAP_CM / 2.0 if c < courses - 1 else 0.0)
        out.append(box(w + rng.uniform(-2.5, 2.5), d + rng.uniform(-2.5, 2.5), cy0, cy1,
                       x=rng.uniform(-0.8, 0.8), z=rng.uniform(-0.8, 0.8)))
    return out


# =============================================================================
# 3. PARTS
# =============================================================================
def part_base(lod, rng):
    """Plinth disc (= main pool floor), pool wall, wall cap."""
    p, w, c = PLINTH, POOL_WALL, WALL_CAP
    if lod["blocks"]:
        parts = [revolve([(0, 0), (w["r_in"] + 5, 0), (w["r_in"] + 5, p["h"] - 0.5), (0, p["h"] - 0.5)],
                         segs=lod["segs"])]
        parts += ring(w["r_in"] - 5, p["r"], 0.0, p["h"], lod, blocks=22, rng=rng)
    else:
        parts = [revolve([(0, 0), (p["r"], 0), (p["r"], p["h"]), (0, p["h"])], segs=lod["segs"])]
    parts += ring(w["r_in"], w["r_out"], w["y0"], w["y1"], lod, blocks=18, courses=2, rng=rng)
    parts += ring(c["r_in"], c["r_out"], c["y0"], c["y1"], lod, blocks=26, rng=rng)
    return parts


def part_buttresses(lod, rng):
    b = BUTTRESS
    out = []
    for ang in b["angles"]:
        pieces = stacked_box(b["w"], b["d"], 0.0, b["h"], lod, 3, rng)
        pieces.append(box(b["cap_w"], b["cap_d"], b["h"] - 1.0, b["h"] + b["cap_h"]))
        out += [m.place_radial(b["radius"], ang) for m in pieces]
    return out


def part_planters(lod, rng):
    """Stone planter boxes only (flowers are separate props)."""
    p = PLANTER
    s, t, h = p["size"], p["rim_t"], p["h"]
    out = []
    for ang in p["angles"]:
        pieces = stacked_box(s, s, 0.0, h, lod, 3, rng)
        y0, y1 = h - 1.0, h + p["rim_h"]
        half = s / 2.0 - t / 2.0
        pieces += [box(s, t, y0, y1, z=half), box(s, t, y0, y1, z=-half),
                   box(t, s - 2 * t, y0, y1, x=half), box(t, s - 2 * t, y0, y1, x=-half)]
        out += [m.place_radial(p["radius"], ang) for m in pieces]
    return out


def stair_profile(st):
    """Stepped side profile (radial, height) of the front stairs, outer edge first."""
    n, rise, tread = st["steps"], st["rise"], st["tread"]
    r_foot = POOL_WALL["r_out"] + n * tread + 5.0
    pts = [(st["r_start"], 0.0), (r_foot, 0.0)]
    for k in range(1, n + 1):
        r = r_foot - (k - 1) * tread
        pts += [(r, rise * k), (r - tread, rise * k)]
    pts[-1] = (st["r_start"], rise * n)
    return pts


def _x_prism(outline_zy, x0, x1):
    """Prism with outline in the (z, y) plane, extruded along X (cyclic axis swap = proper rotation)."""
    m = prism(outline_zy, x0, x1)
    return m._map(lambda p: (p[1], p[2], p[0]))


def part_stairs(lod, rng):
    """Front stairs (+Z): one stepped prism (3 stone runs on high) + side cheeks."""
    st = STAIRS
    prof = stair_profile(st)
    hw = st["width"] / 2.0
    out = []
    if lod["blocks"]:
        n = 3
        wseg = st["width"] / n
        for b in range(n):
            x0 = -hw + b * wseg + (BLOCK_GAP_CM / 2.0 if b else 0.0)
            x1 = -hw + (b + 1) * wseg - (BLOCK_GAP_CM / 2.0 if b < n - 1 else 0.0)
            jit = [(r + rng.uniform(-0.8, 0.8), y if y == 0 else y + rng.uniform(-0.6, 0.0)) for r, y in prof]
            out.append(_x_prism(jit, x0, x1))
    else:
        out.append(_x_prism(prof, -hw, hw))
    r0 = st["r_start"]
    cx = hw + st["cheek_w"] / 2.0 - 1.0          # 1 cm into the stairs, no coplanar faces
    top_y = WALL_CAP["y1"] + 25.0
    r_mid = r0 + 100.0
    r_end = prof[1][0] + 5.0
    for sx in (-1.0, 1.0):
        out += [box(st["cheek_w"], r_mid - r0, 0.0, top_y, x=sx * cx, z=(r0 + r_mid) / 2.0),
                box(st["cheek_w"], r_end - r_mid, 0.0, top_y * 0.55, x=sx * cx, z=(r_mid + r_end) / 2.0)]
    return out


def part_pedestal(lod, rng):
    """Central stepped pedestal + upper basin (Ø5 m) + top tier."""
    if not lod["blocks"]:
        return [revolve(PEDESTAL_PROFILE, segs=lod["segs"])]
    segs = lod["segs"]
    out = []
    out += ring(80, 170, FLOOR_Y, 100, lod, blocks=12, rng=rng)
    out += ring(70, 140, 100, 170, lod, blocks=10, rng=rng)
    out += ring(50, 105, 170, 250, lod, blocks=8, courses=2, rng=rng)
    out.append(revolve([(60, 250), (105, 250), (255, 315), (255, 364), (222, 364), (222, 345), (60, 345)],
                       segs=segs))                                      # basin bowl
    out += ring(221, 265, 364, 385, lod, blocks=18, rng=rng)            # basin rim stones
    out += ring(50, 125, 345, 400, lod, blocks=8, rng=rng)              # top tier
    out.append(revolve([(0, 399), (92, 399), (92, 422), (0, 422)], segs=segs))
    return out


def part_arches(lod, rng):
    """Two broken crescent arcs around the diamond, in the XY plane (facing +Z)."""
    a = ARCH
    hd = a["depth"] / 2.0
    out = []
    for sign in (1.0, -1.0):
        lo, hi = a["gap_top_deg"], a["end_deg"]
        a0, a1 = (lo, hi) if sign > 0 else (-hi, -lo)
        if lod["blocks"]:
            step = (a1 - a0) / a["blocks"]
            gap = math.degrees(BLOCK_GAP_CM * 1.5 / a["r_out"])
            pieces = []
            for b in range(a["blocks"]):
                b0 = a0 + b * step + (gap / 2.0 if b else 0.0)
                b1 = a0 + (b + 1) * step - (gap / 2.0 if b < a["blocks"] - 1 else 0.0)
                ri = a["r_in"] + rng.uniform(-3, 2)
                ro = a["r_out"] + rng.uniform(-4, 3)
                d = hd + rng.uniform(-1.5, 1.5)
                pieces.append(revolve([(ri, -d), (ro, -d), (ro, d), (ri, d)], b0, b1, 8))
        else:
            pieces = [revolve([(a["r_in"], -hd), (a["r_out"], -hd), (a["r_out"], hd), (a["r_in"], hd)],
                              a0, a1, a["segs_low"])]
        # revolve axis Y -> Z: angle 0 points up, arc lies in the XY plane
        out += [m.rotate_x(-90.0).translate(y=a["center_y"]) for m in pieces]
    return out


def _xy_prism(outline_xy, z0, z1):
    """Prism whose outline lies in the XY plane, extruded along Z."""
    return prism([(x, -y) for x, y in outline_xy], z0, z1).rotate_x(90.0)


def part_gem(lod, rng):
    """Diamond stone frame + faceted gem + small plinth under it."""
    g = GEM
    cy = g["center_y"]
    outer = [(0, g["outer_h"]), (g["outer_w"], 0), (0, -g["outer_h"]), (-g["outer_w"], 0)]
    inner = [(0, g["inner_h"]), (g["inner_w"], 0), (0, -g["inner_h"]), (-g["inner_w"], 0)]
    hd = g["frame_depth"] / 2.0
    out = []
    for k in range(4):
        k1 = (k + 1) % 4
        quad = [outer[k], outer[k1], inner[k1], inner[k]]
        out.append(_xy_prism([(x, y + cy) for x, y in quad], -hd, hd))
    # bipyramid gem filling the frame opening
    m = MeshData()
    ring_ids = [m.add_vert((x, y + cy, 0.0)) for x, y in inner]
    front = m.add_vert((0.0, cy, g["gem_depth"]))
    back = m.add_vert((0.0, cy, -g["gem_depth"]))
    for k in range(4):
        k1 = (k + 1) % 4
        # inner outline is clockwise seen from +Z, so (front, k1, k) faces outward
        m.add_planar_face([front, ring_ids[k1], ring_ids[k]])
        m.add_planar_face([back, ring_ids[k], ring_ids[k1]])
    out.append(m)
    p = GEM_PLINTH
    out.append(box(p["w"], p["d"], p["y0"], p["y0"] + p["h"]))
    return out


PARTS = [part_base, part_buttresses, part_planters, part_stairs, part_pedestal, part_arches, part_gem]


def build_parts(lod_name):
    lod = LODS[lod_name]
    rng = random.Random(SEED)
    meshes = []
    for fn in PARTS:
        meshes += fn(lod, rng)
    return merge_all(meshes)


# =============================================================================
# 4. MAYA
# =============================================================================
def create_maya_mesh(md, name):
    import maya.cmds as cmds
    import maya.api.OpenMaya as om

    points = om.MPointArray()
    for p in md.verts:
        points.append(om.MPoint(p[0], p[1], p[2]))
    counts = om.MIntArray([len(f) for f in md.faces])
    connects = om.MIntArray([v for f in md.faces for v in f])

    uv_index, us, vs, uv_ids = {}, om.MFloatArray(), om.MFloatArray(), om.MIntArray()
    for face, face_uvs, shell in zip(md.faces, md.uvs, md.shells):
        for v, (u, w) in zip(face, face_uvs):
            key = (shell, v, round(u, 5), round(w, 5))
            idx = uv_index.get(key)
            if idx is None:
                idx = len(us)
                uv_index[key] = idx
                us.append(u)
                vs.append(w)
            uv_ids.append(idx)

    fn = om.MFnMesh()
    transform = fn.create(points, counts, connects, us, vs)
    fn.assignUVs(counts, uv_ids)
    path = om.MFnDagNode(transform).fullPathName()
    return cmds.rename(path, name)


def ensure_material():
    import maya.cmds as cmds
    if not cmds.objExists(MATERIAL):
        cmds.shadingNode("lambert", asShader=True, name=MATERIAL)
        cmds.setAttr(MATERIAL + ".color", *STONE_RGB, type="double3")
    sg = MATERIAL + "SG"
    if not cmds.objExists(sg):
        cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name=sg)
        cmds.connectAttr(MATERIAL + ".outColor", sg + ".surfaceShader", force=True)
    return sg


def hard_edges(mesh, angle):
    import maya.cmds as cmds
    cmds.select(mesh + ".e[*]", r=True)
    cmds.polySelectConstraint(mode=2, type=0x8000, angle=1, anglebound=(angle, 180))
    edges = cmds.ls(sl=True)
    cmds.polySelectConstraint(disable=True)
    cmds.select(clear=True)
    return edges


def finish_low(mesh):
    import maya.cmds as cmds
    cmds.polySoftEdge(mesh, angle=40, ch=False)
    try:
        cmds.u3dLayout(mesh, res=2048, scl=1, spc=0.004, mar=0.004)
    except Exception as exc:  # older Maya / plugin missing
        print("u3dLayout failed (%s), falling back to polyLayoutUV" % exc)
        cmds.polyLayoutUV(mesh, layout=2, scale=1, rotateForBestFit=2, percentageSpace=0.4)
    cmds.delete(mesh, ch=True)


def finish_high(mesh):
    import maya.cmds as cmds
    edges = hard_edges(mesh, 35)
    if edges:
        try:
            cmds.polyBevel3(edges, offsetAsFraction=False, offset=HIGH_BEVEL_CM,
                            segments=HIGH_BEVEL_SEGS, chamfer=False, autoFit=True)
        except TypeError:
            cmds.polyBevel3(edges, offsetAsFraction=False, offset=HIGH_BEVEL_CM, segments=HIGH_BEVEL_SEGS)
    cmds.delete(mesh, ch=True)
    cmds.polySoftEdge(mesh, angle=60, ch=False)


def scene_is_blank():
    import maya.cmds as cmds
    defaults = {"persp", "top", "front", "side", ROOT}
    scene = cmds.file(q=True, sceneName=True).replace("\\", "/").lower()
    return scene in ("", OUT_PATH.lower()) and not (set(cmds.ls(assemblies=True)) - defaults)


def build():
    import maya.cmds as cmds
    if cmds.currentUnit(q=True, linear=True) != "cm":
        print("WARNING: scene unit is %s; geometry is authored in cm." % cmds.currentUnit(q=True, linear=True))
    if cmds.objExists(ROOT):
        cmds.delete(ROOT)
    sg = ensure_material()

    low = create_maya_mesh(build_parts("low").drop_ground_faces(), LOW_NAME)
    high = create_maya_mesh(build_parts("high"), HIGH_NAME)
    finish_low(low)
    finish_high(high)
    for m in (low, high):
        cmds.sets(m, edit=True, forceElement=sg)

    root = cmds.group(empty=True, name=ROOT)
    cmds.parent(low, high, root)
    low, high = ROOT + "|" + LOW_NAME, ROOT + "|" + HIGH_NAME

    blank = scene_is_blank()
    cmds.select(root, r=True)
    if blank:
        cmds.file(rename=OUT_PATH)
        cmds.file(save=True, type="mayaAscii", force=True)
    else:  # keep the user's current scene untouched, export only the fountain
        cmds.file(OUT_PATH, exportSelected=True, type="mayaAscii", force=True,
                  constructionHistory=False, shader=True)

    for m in (low, high):
        print("%-24s tris=%-7d verts=%-7d uvShells=%d" % (
            m.split("|")[-1], cmds.polyEvaluate(m, triangle=True), cmds.polyEvaluate(m, vertex=True),
            cmds.polyEvaluate(m, uvShell=True)))
    print("Saved: %s (%s)" % (OUT_PATH, "scene save" if blank else "export selected"))
    return low, high


if __name__ == "__main__":
    build()
