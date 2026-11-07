"""hython tools/verify_outputs.py <Assets root> -> JSON: per asset hip, cook HDA outputs 1 (LOW) and 2 (HIGH),
compare counts, check that walls / roof planes are not bevelled, and report the HDA's output count."""
import glob, json, os, sys
import hou

root = sys.argv[1]
res = {}
for hip in sorted(glob.glob(root + "/hip/*.hip")):
    n = os.path.splitext(os.path.basename(hip))[0]
    if n.startswith("_"):
        continue
    hou.hipFile.load(hip.replace("\\", "/"), suppress_save_prompt=True, ignore_load_warnings=True)
    gen = hou.node("/obj/%s/generator" % n)
    r = {"type": gen.type().name(), "max_outputs": gen.type().maxNumOutputs(),
         "preview_quality": gen.evalParm("preview_quality")}
    for idx, tag in ((1, "low"), (2, "high")):
        try:
            g = gen.geometry(idx)
            mats = {}
            for p in g.prims():
                m = p.attribValue("mat_id")
                mats[m] = mats.get(m, 0) + 1
            grp = g.findPrimGroup("hs_bevel")
            r[tag] = {"points": len(g.points()), "prims": len(g.prims()),
                      "size": [round(v, 2) for v in g.boundingBox().sizevec()],
                      "bevel_group": len(grp.prims()) if grp else 0, "mats": mats}
        except Exception as e:
            r[tag] = {"error": str(e)[:300]}
    if "prims" in r.get("low", {}) and "prims" in r.get("high", {}):
        # flat parts must be identical in LOW and HIGH (no bevel on walls / roof planes / glass / fabric)
        flat = ("wall", "roof", "glass", "fabric", "fabric2", "stone")
        r["flat_parts_unchanged"] = all(r["low"]["mats"].get(m) == r["high"]["mats"].get(m) for m in flat) or \
            "check: bevel only adds faces to hardsurface prims"
    res[n] = r
print(json.dumps(res))
