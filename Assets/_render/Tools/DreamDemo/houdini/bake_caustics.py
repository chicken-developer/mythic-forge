"""T_Caustics: seamless pond-floor caustic network, 1 channel, 8-bit PNG.

The veins are deliberately broad: M_PP_DreamCaustics takes min() of two layers drifting in
opposite directions, and the overlap of two soft networks is what gives the sharp lines.
"""
import os

import _bake_common as bc

# ---- parameters -------------------------------------------------------------
RESOLUTION = 1024
SEED = 3.0
CELLS = 7            # Voronoi cells per tile (integer keeps the texture seamless)
WARP = 0.42          # domain warp in cell units
LINE_WIDTH = 0.30    # width of the bright lines (in d2-d1 units)
LINE_SHARPNESS = 1.8
FINE_LAYER = 0.25    # weight of the second, finer network
GLOW = 0.12          # soft glow around the lines
# -----------------------------------------------------------------------------

SNIPPET = r"""
int W = chi("W");
int ix = @ptnum % W; int iy = @ptnum / W;
float u = (ix + 0.5) / W; float v = (iy + 0.5) / W;
int P = chi("cells");
float seed = chf("seed");
vector p = set(u * P, v * P, seed);
float warp = chf("warp");
vector q = p;
q.x += warp * (pnoise(p + set(11.3, 4.7, 0.0), P, P, 64) - 0.5) * 2.0;
q.y += warp * (pnoise(p + set(-5.2, 9.1, 17.0), P, P, 64) - 0.5) * 2.0;
q.x += warp * 0.35 * (pnoise(p * 2.0 + set(3.3, 1.1, 9.0), 2 * P, 2 * P, 64) - 0.5) * 2.0;
q.y += warp * 0.35 * (pnoise(p * 2.0 + set(7.9, 6.5, 2.0), 2 * P, 2 * P, 64) - 0.5) * 2.0;

int s; float d1; float d2;
wnoise(q, s, d1, d2, P, P, 64);
float e = d2 - d1;
float line = pow(clamp(1.0 - e / chf("width"), 0.0, 1.0), chf("sharp"));
float glow = pow(clamp(1.0 - e / (chf("width") * 3.0), 0.0, 1.0), 2.0);

vector q2 = q * 2.0 + set(3.7, 1.9, 5.0);
float d1b; float d2b;
wnoise(q2, s, d1b, d2b, 2 * P, 2 * P, 64);
float e2 = d2b - d1b;
float line2 = pow(clamp(1.0 - e2 / (chf("width") * 1.2), 0.0, 1.0), chf("sharp") * 1.3);

float val = line + chf("fine") * line2 * (0.35 + 0.65 * glow) + chf("glow") * glow;
f@val = clamp(val, 0.0, 1.0);
"""


def main():
    timer = bc.BakeTimer("T_Caustics")
    img = bc.eval_vex_image(RESOLUTION, RESOLUTION, SNIPPET, {
        "cells": CELLS, "seed": SEED, "warp": WARP, "width": LINE_WIDTH,
        "sharp": LINE_SHARPNESS, "fine": FINE_LAYER, "glow": GLOW,
    }, attrib="val", size=1)[:, :, 0]
    out = os.path.join(bc.RAW_DIR, "T_Caustics.png")
    bc.save_png(out, img)
    timer.output(out)
    timer.output(bc.save_tile_check("T_Caustics", img))
    timer.note("seam", bc.seam_report(img))
    timer.note("mean", float(img.mean()))
    timer.finish()


main()
