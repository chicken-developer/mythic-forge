"""T_StarField (1024, 1 channel, seamless) and T_CloudNoise (1024, 1 channel, seamless).

T_CloudNoise is an addition to the prompt list: the NIGHT sky needs drifting clouds and
the bubble film needs a slow noise, and neither should reuse the dream nebula.
"""
import os

import _bake_common as bc

# ---- parameters -------------------------------------------------------------
RESOLUTION = 1024
SEED = 21.0
STAR_LAYERS = ((96, 0.20, 1.0), (40, 0.16, 0.9), (14, 0.11, 1.0))  # cells, max radius, weight
CLOUD_CELLS = 4
# -----------------------------------------------------------------------------

STAR_SNIPPET = r"""
int W = chi("W");
int ix = @ptnum % W; int iy = @ptnum / W;
float u = (ix + 0.5) / W; float v = (iy + 0.5) / W;
float seed = chf("seed");
float total = 0.0;
int cells[] = array(chi("c0"), chi("c1"), chi("c2"));
float radii[] = array(chf("r0"), chf("r1"), chf("r2"));
float weights[] = array(chf("w0"), chf("w1"), chf("w2"));
for (int i = 0; i < 3; i++) {
    int C = cells[i];
    vector p = set(u * C, v * C, seed + 3.7 * i);
    int s; float d1; float d2;
    wnoise(p, s, d1, d2, C, C, 64);
    float r = rand(s * 1.0 + 0.5);
    float size = lerp(0.35, 1.0, pow(r, 5.0)) * radii[i];
    float b = lerp(0.25, 1.0, pow(rand(s * 1.0 + 7.1), 2.5));
    float core = 1.0 - smooth(size * 0.15, size, d1);
    total += weights[i] * b * core * core;
}
f@val = clamp(total, 0.0, 1.0);
"""

CLOUD_SNIPPET = r"""
int W = chi("W");
int ix = @ptnum % W; int iy = @ptnum / W;
float u = (ix + 0.5) / W; float v = (iy + 0.5) / W;
int P = chi("cells"); float seed = chf("seed");
float a = 1.0; float s = 0.0; float n = 0.0; int f = P;
for (int i = 0; i < 6; i++) {
    s += a * pnoise(set(u * f, v * f, seed + 5.0 * i), f, f, 64);
    n += a; a *= 0.55; f *= 2;
}
f@val = s / n;
"""


def main():
    timer = bc.BakeTimer("T_StarField")
    parms = {"seed": SEED}
    for i, (cells, radius, weight) in enumerate(STAR_LAYERS):
        parms["c%d" % i] = int(cells)
        parms["r%d" % i] = float(radius)
        parms["w%d" % i] = float(weight)
    stars = bc.eval_vex_image(RESOLUTION, RESOLUTION, STAR_SNIPPET, parms,
                              attrib="val", size=1)[:, :, 0]
    out = os.path.join(bc.RAW_DIR, "T_StarField.png")
    bc.save_png(out, stars)
    timer.output(out)
    timer.output(bc.save_tile_check("T_StarField", stars))
    timer.note("stars_seam", bc.seam_report(stars))
    timer.note("stars_coverage_over_0.5", float((stars > 0.5).mean()))

    cloud = bc.eval_vex_image(RESOLUTION, RESOLUTION, CLOUD_SNIPPET,
                              {"seed": SEED, "cells": CLOUD_CELLS}, attrib="val", size=1)[:, :, 0]
    lo, hi = float(cloud.min()), float(cloud.max())
    cloud = (cloud - lo) / max(hi - lo, 1e-5)
    out = os.path.join(bc.RAW_DIR, "T_CloudNoise.png")
    bc.save_png(out, cloud)
    timer.output(out)
    timer.output(bc.save_tile_check("T_CloudNoise", cloud))
    timer.note("cloud_seam", bc.seam_report(cloud))
    timer.finish()


main()
