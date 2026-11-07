"""Find geometry that pokes through a pitched roof (hython).

  hython tools/check_roof_poke.py <project_root> [asset ...]

For every asset hip (hip/<asset>.hip): takes the LOW output of the generator, shoots a vertical ray at every
point of every non-roof primitive and measures how far it sits ABOVE the upper face of a roof slab (parts
slope / hip) right over it. Points 1 mm .. 0.6 m above count as poking through (further up = standing on the
roof on purpose, e.g. chimneys). Prints one JSON line: {asset: {part: [prims, max_excess_m, sample_xyz]}}.
"""
import json
import os
import sys

import hou

SKIP = ("slope hip soffit verge ridge_beam chimney chimney_cap dormer_* tower_* pinnacle* canopy awning* "
        "core_top parapet coping")

VEX = r'''
float lim = 0.6;
vector down = {0, -1, 0};
string own = s@part;
string skip[] = split(chs("skip"));
foreach (string sk; skip) if (match(sk, own)) return;
float worst = 0;
vector where = 0;
foreach (int pt; primpoints(0, @primnum)) {
    vector P = point(0, "P", pt);
    vector hp[]; int hpr[]; vector huv[];
    int n = intersect_all(1, "", P - down * (lim + 1.0), down * (2 * lim + 2.0), hp, hpr, huv, 0.0001, 0.00001);
    float top = -1e30;
    for (int i = 0; i < n; i++) {
        vector nn = prim_normal(1, hpr[i], huv[i].x, huv[i].y);
        if (nn.y <= 0.05) continue;
        top = max(top, hp[i].y);
    }
    if (top < -1e29) continue;
    float ex = P.y - top;
    if (ex > 0.001 && ex < lim && ex > worst) { worst = ex; where = P; }
}
f@poke = worst;
v@poke_at = where;
'''


def check(root, asset):
    hou.hipFile.load(os.path.join(root, "hip", asset + ".hip").replace("\\", "/"), suppress_save_prompt=True,
                     ignore_load_warnings=True)
    geo = hou.node("/obj/" + asset)
    gen = geo.node("generator")
    sel = geo.createNode("null", "_poke_low")
    sel.setInput(0, gen, 1)                     # output 1 = LOW
    rf = geo.createNode("attribwrangle", "_poke_roofs")      # rays only against the roof faces
    rf.setInput(0, sel)
    rf.parm("class").set(1)
    rf.parm("snippet").set('if (s@part != "slope" && s@part != "hip") removeprim(0, @primnum, 1);')
    w = geo.createNode("attribwrangle", "_poke_check")
    w.setInput(0, sel)
    w.setInput(1, rf)
    w.parm("class").set(1)
    w.parm("snippet").set(VEX)
    tg = hou.StringParmTemplate("skip", "skip", 1)
    ptg = w.parmTemplateGroup()
    ptg.append(tg)
    w.setParmTemplateGroup(ptg)
    w.parm("skip").set(gen.evalParm("roof_clip_skip") if gen.parm("roof_clip_skip") else SKIP)
    g = w.geometry()
    res = {}
    for p in g.prims():
        e = p.attribValue("poke")
        if e > 0.001:
            part = p.attribValue("part")
            r = res.setdefault(part, [0, 0.0, None])
            r[0] += 1
            if e > r[1]:
                r[1] = round(e, 4)
                r[2] = [round(x, 2) for x in p.attribValue("poke_at")]
    return res


def main():
    root = sys.argv[1]
    names = sys.argv[2:] or sorted(f[:-4] for f in os.listdir(os.path.join(root, "hip"))
                                   if f.endswith(".hip") and not f.startswith("_"))
    out = {}
    for n in names:
        try:
            out[n] = check(root, n)
        except Exception as e:
            out[n] = {"error": str(e)}
    print(json.dumps(out))


if __name__ == "__main__":
    main()
