"""Bake one asset: LOW + HIGH FBX and a HIGH -> LOW tangent-space normal map (hython, no render ROP).

  hython tools/bake_asset.py <project_root> <asset> [--resolution 4096] [--dest <dir>]

Source: hip/<asset>.hip (generator HDA, output 1 = LOW, output 2 = HIGH).
Output (default <project_root>/_render/SourceAssets/HBuild/<asset>/):
  SM_<asset>_LOW.fbx    LOW, triangulated. UV0 = unique packed UVs (normal map), UV1 = metric tile UVs (Substance)
  SM_<asset>_HIGH.fbx   HIGH (PolyBevel on hardsurface parts), triangulated, same UV channels
  T_<asset>_N.png       normal map, DirectX (green down), MikkT, linear, baked HIGH -> LOW on UV0
  T_<asset>_hits.png    ray hit mask of the bake (white = traced)
  bake.json             counts, bounds, material slots, colours (linear, from the preset), hashes
Material slots: one per s@mat_id, named M_<mat_id> (via shop_materialpath -> /mat/M_<mat_id>).

Recipe (same as the Medieval15 bake that was verified on Houdini 22.0.368): face-local UVs packed by
uvlayout::3.0, Copernicus bakegeometrytextures::2.0 with LOW and HIGH translated apart per connected piece
(identical offsets on both; i@piece comes from the generator before the HIGH bevel, so LOW and HIGH agree),
so the inner and outer faces of a wall and touching boxes never catch each other's rays.
FBX goes through the filmboxfbx export ROP (geometry export, not a render); positions are scaled to cm.
"""
import argparse
import hashlib
import json
import math
import os
import sys
import time

import hou


def node(parent, typ, name, inp=None, **parms):
    n = parent.createNode(typ, name)
    if inp is not None:
        if isinstance(inp, tuple):
            n.setInput(0, inp[0], inp[1])
        else:
            n.setInput(0, inp)
    for k, v in parms.items():
        p = n.parm(k)
        if p is None:
            raise ValueError((typ, k))
        p.set(v)
    return n


def cook(n):
    n.cook(force=True)
    errs = [(c.path(), c.errors()) for c in (n,) if c.errors()]
    if errs:
        raise RuntimeError(errs)
    return n.geometry()


def sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


TILE_UV = r'''v@uv2 = v@uv;      // metric tile UVs move to UV1 (Substance), UV0 becomes the unique bake layout'''
FACE_UV = r'''int pr = @primnum;
int pts[] = primpoints(0, pr);
vector n = normalize(prim_normal(0, pr, 0.5, 0.5)), o = point(0, "P", pts[0]);
vector u = 0;
float longest = 0.0;
for (int i = 0; i < len(pts); i++) {
    vector e = vector(point(0, "P", pts[(i + 1) % len(pts)])) - vector(point(0, "P", pts[i]));
    if (length2(e) > longest) { u = normalize(e); longest = length2(e); }
}
vector v = normalize(cross(n, u));
vector q = v@P - o;
v@uv = set(dot(q, u), dot(q, v), 0.0);'''
DEGEN = r'''if (primintrinsic(0, "measuredarea", @primnum) < 1e-10) removeprim(0, @primnum, 1);'''
MATPATH = r'''s@shop_materialpath = "/mat/M_" + s@mat_id;'''
# Unreal 5.8 Interchange asserts on a non-finite UV (NaN != NaN): count and zero them, same for N
FINITE = r'''int bad = 0;
string names[] = {"uv", "uv2"};
foreach (string nm; names) {
    vector t = vertex(0, nm, @vtxnum);
    if (isnan(t.x) || isnan(t.y) || abs(t.x) > 1e8 || abs(t.y) > 1e8) { setvertexattrib(0, nm, -1, @vtxnum, {0,0,0}); bad = 1; }
}
vector n = v@N;
if (isnan(n.x) || isnan(n.y) || isnan(n.z) || length(n) < 0.5) { v@N = prim_normal(0, @primnum, 0.5, 0.5); bad = 1; }
i@fixed_vtx = bad;'''
HIGH_CLEAN = DEGEN
EXPLODE = r'''int id = i@piece;              // point attribute from the generator, set before the HIGH bevel (same on LOW/HIGH)
v@P += set(id % 16, (id / 16) % 16, id / 256) * 64.0;'''


def build(geo, gen, area_scale):
    low_src = node(geo, "null", "_bake_low_src", (gen, 1))
    high_src = node(geo, "null", "_bake_high_src", (gen, 2))
    # LOW: tile uv -> uv2, unique uv -> uv, pack, triangulate, pieces
    t = node(geo, "attribwrangle", "_bake_tile_uv", low_src, **{"class": 3, "snippet": TILE_UV})
    f = node(geo, "attribwrangle", "_bake_face_uv", t, **{"class": 3, "snippet": FACE_UV})
    area = sum(p.intrinsicValue("measuredarea") for p in cook(f).prims())
    lay = node(geo, "uvlayout::3.0", "_bake_uv_layout", f, separatoredges="*", correctareas=1, scaling="custom",
               scale=math.sqrt(area_scale / max(area, 0.01)), rotstep="none", padding=1, resolution="res3",
               iterations=1, packincavities=0, generatenonpackedpoly=1)
    tri = node(geo, "divide", "_bake_low_tri", lay, usemaxsides=1, numsides=3)
    cl = node(geo, "attribwrangle", "_bake_low_clean", tri, **{"class": 1, "snippet": DEGEN})
    mp = node(geo, "attribwrangle", "_bake_low_mat", cl, **{"class": 1, "snippet": MATPATH})
    ln = node(geo, "normal", "_bake_low_n", mp)
    low = node(geo, "attribwrangle", "_bake_low_finite", ln, **{"class": 3, "snippet": FINITE})
    # HIGH: tile uv -> uv2 (the HIGH keeps its own uv as UV0, it is not textured by the bake)
    th = node(geo, "attribwrangle", "_bake_high_tile_uv", high_src, **{"class": 3, "snippet": TILE_UV})
    htri = node(geo, "divide", "_bake_high_tri", th, usemaxsides=1, numsides=3)
    hp = node(geo, "attribwrangle", "_bake_high_clean", htri, **{"class": 1, "snippet": HIGH_CLEAN})
    hm = node(geo, "attribwrangle", "_bake_high_mat", hp, **{"class": 1, "snippet": MATPATH})
    hn = node(geo, "normal", "_bake_high_n", hm)
    high = node(geo, "attribwrangle", "_bake_high_finite", hn, **{"class": 3, "snippet": FINITE})
    ex_low = node(geo, "attribwrangle", "_bake_explode_low", low, **{"class": 2, "snippet": EXPLODE})
    ex_high = node(geo, "attribwrangle", "_bake_explode_high", high, **{"class": 2, "snippet": EXPLODE})
    return low, high, ex_low, ex_high


def materials(mat_ids):
    mat = hou.node("/mat")
    for m in mat_ids:
        if mat.node("M_" + m) is None:
            mat.createNode("principledshader::2.0", "M_" + m)


def export_fbx(sop, path):
    obj = hou.node("/obj").createNode("geo", "_FBX_EXPORT", run_init_scripts=False)
    om = node(obj, "object_merge", "mesh", objpath1=sop.path())
    cl = node(obj, "attribdelete", "export_attribs", om, vtxdel="fixed_vtx", ptdel="piece high_fixed")
    sc = node(obj, "xform", "metres_to_cm", cl, scale=100.0)
    sc.setDisplayFlag(True)
    sc.setRenderFlag(True)
    rop = node(hou.node("/out"), "filmboxfbx", "_EXPORT_FBX", startnode=obj.path(), trange=0, sopoutput=path)
    try:
        rop.render()
    finally:
        rop.destroy()
        obj.destroy()


def bake_normal(ex_low, ex_high, out_n, out_hits, res):
    cop = hou.node("/img").createNode("copnet", "_HBUILD_NORMAL_BAKE")
    try:
        imp = [node(cop, "sopimport", nm, usesoppath=1, soppath=s.path()) for nm, s in (("low", ex_low), ("high", ex_high))]
        baker = node(cop, "bakegeometrytextures", "high_to_low", tracingmode="surfacenormal",
                     raylengthmissing="custom", raylength=0.075, raybias=0.001, enablematchpieces=0,
                     pieceattrib="piece", bakenormal=1, normaltype="offset", flipN="down", bakeocclusion=0,
                     bakealpha=1, enableuvfilling=1, edgepadding=3.0 / res, exterior="diffuse",
                     visibilityculling="noculling")
        baker.setInput(1, imp[0])
        baker.setInput(2, imp[1])
        for path, idx in ((out_n, 0), (out_hits, 10)):
            w = node(cop, "rop_image", "write_%d" % idx, baker, copoutput=path, setres=1, res1=res, res2=res,
                     colorconversion="raw", savebackground=0)
            w.setInput(0, baker, idx)
            w.render()
        if baker.errors():
            raise RuntimeError(baker.errors())
        return baker.type().name()
    finally:
        cop.destroy()


def stats(g):
    bb = g.boundingBox()
    mats = {}
    for p in g.prims():
        m = p.attribValue("mat_id")
        mats[m] = mats.get(m, 0) + 1
    fa = g.findVertexAttrib("fixed_vtx")
    fixed = sum(g.vertexIntAttribValues("fixed_vtx")) if fa else 0
    import math
    bad_p = sum(1 for v in g.pointFloatAttribValues("P") if not math.isfinite(v))
    if bad_p:
        raise RuntimeError("%d non-finite position values in the export mesh (Unreal shows them as fans)" % bad_p)
    ha = g.findPointAttrib("high_fixed")
    high_fixed = sum(g.pointIntAttribValues("high_fixed")) if ha else 0
    pa = g.findPointAttrib("piece")
    pieces = g.pointIntAttribValues("piece") if pa else []
    return {"points": len(g.points()), "triangles": len(g.prims()), "fixed_nonfinite_vertices": fixed, "high_fixed_points": high_fixed,
            "pieces": len(set(pieces)), "piece0_points": sum(1 for x in pieces if x == 0), "bounds_m": [round(x, 4) for x in bb.sizevec()],
            "bbox_min_m": [round(x, 4) for x in bb.minvec()], "materials": mats,
            "uv_sets": [a.name() for a in g.vertexAttribs() if a.name().startswith("uv")]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("asset")
    ap.add_argument("--resolution", type=int, default=4096)
    ap.add_argument("--dest", default=None)
    ap.add_argument("--area-scale", type=float, default=0.45, help="UV area fraction target for the packed layout")
    a = ap.parse_args()
    root = a.root.replace("\\", "/")
    dest = (a.dest or os.path.join(root, "_render", "SourceAssets", "HBuild", a.asset)).replace("\\", "/")
    os.makedirs(dest, exist_ok=True)
    t0 = time.time()
    hip = "%s/hip/%s.hip" % (root, a.asset)
    hou.hipFile.load(hip, suppress_save_prompt=True, ignore_load_warnings=True)
    geo = hou.node("/obj/" + a.asset)
    gen = geo.node("generator")
    low, high, ex_low, ex_high = build(geo, gen, a.area_scale)
    lg, hg = cook(low), cook(high)
    materials(sorted(set(lg.primStringAttribValues("mat_id")) | set(hg.primStringAttribValues("mat_id"))))
    lg, hg = cook(low), cook(high)
    st_low, st_high = stats(lg), stats(hg)            # raises before export on non-finite positions
    files = {"low": "%s/SM_%s_LOW.fbx" % (dest, a.asset), "high": "%s/SM_%s_HIGH.fbx" % (dest, a.asset),
             "normal": "%s/T_%s_N.png" % (dest, a.asset), "hits": "%s/T_%s_hits.png" % (dest, a.asset)}
    export_fbx(low, files["low"])
    export_fbx(high, files["high"])
    print("FBX_DONE", round(time.time() - t0, 1), flush=True)
    backend = bake_normal(ex_low, ex_high, files["normal"], files["hits"], a.resolution)
    print("BAKE_DONE", round(time.time() - t0, 1), flush=True)
    preset = json.load(open("%s/presets/%s.json" % (root, a.asset)))["parms"]
    cols = {m: preset.get("col_" + m) for m in lg.primStringAttribValues("mat_id")}
    rep = {"asset": a.asset, "hip": hip, "hip_sha256": sha(hip), "houdini": hou.applicationVersionString(),
           "hda": gen.type().definition().libraryFilePath(), "type": gen.type().name(),
           "low": st_low, "high": st_high, "colors_linear": cols,
           "bake": {"backend": "Copernicus " + backend, "resolution": a.resolution,
                    "normal_space": "tangent, DirectX (green down), MikkT, linear", "uv": "UV0 (unique packed)",
                    "ray_length_m": 0.075, "isolation": "connected pieces (generator i@piece, kept through the bevel) translated apart, same offsets low/high"},
           "fbx_units": "cm (positions x100)", "material_slots": ["M_" + m for m in sorted(cols)],
           "files": {k: v for k, v in files.items()},
           "sha256": {k: sha(v) for k, v in files.items() if os.path.exists(v)},
           "seconds": round(time.time() - t0, 1)}
    with open(dest + "/bake.json", "w") as f:
        json.dump(rep, f, indent=1)
    print(json.dumps({"ok": True, "dest": dest, "low_tris": rep["low"]["triangles"], "high_tris": rep["high"]["triangles"],
                      "seconds": rep["seconds"]}))


if __name__ == "__main__":
    main()
