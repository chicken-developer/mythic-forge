"""T_AuroraStrip (2048x256, 1 channel, seamless in U) and T_AuroraFlow (512x512, RG flow map).

T_AuroraFlow must be imported into Unreal with sRGB OFF.
"""
import os

import numpy as np

import _bake_common as bc

# ---- parameters -------------------------------------------------------------
STRIP_W = 2048
STRIP_H = 256
SEED = 5.0
FOLDS = 6            # slow folds per tile (integer)
STREAKS = 90         # fine vertical streaks per tile (integer)
EDGE = 0.80          # v of the bright lower edge (0 = top row)
EDGE_AMP = 0.10
RAMP_POW = 2.6       # how fast the curtain brightens toward the lower edge
FLOW_RES = 512
FLOW_CELLS = 4       # swirl cells per tile (integer)
FLOW_SWIRL = 0.35
# -----------------------------------------------------------------------------

STRIP_SNIPPET = r"""
int W = chi("W"); int H = chi("H");
int ix = @ptnum % W; int iy = @ptnum / W;
float u = (ix + 0.5) / W; float v = (iy + 0.5) / H;
float seed = chf("seed");
int F = chi("folds"); int S = chi("streaks");

float fold = pnoise(set(u * F, 0.37, seed), F, 64, 64);
float fold2 = pnoise(set(u * F * 3, 1.7, seed), F * 3, 64, 64);
float edge = chf("edge") + chf("edge_amp") * ((fold - 0.5) * 2.0 + 0.4 * (fold2 - 0.5) * 2.0);

float st = pnoise(set(u * S, v * 1.5, seed + 5.0), S, 64, 64);
float st2 = pnoise(set(u * S * 4, v * 3.0, seed + 9.0), S * 4, 64, 64);
float streak = 0.3 + 0.7 * pow(clamp(st * 1.35, 0.0, 1.0), 2.0);
streak *= 0.65 + 0.35 * clamp(st2 * 1.3, 0.0, 1.0);

float body = 1.0 - smooth(edge - 0.03, edge + 0.012, v);      // 0 below the lower edge
float ramp = pow(clamp(v / max(edge, 0.01), 0.0, 1.0), chf("ramp_pow"));
float top = smooth(0.0, 0.3, v);                              // soft upper edge
f@val = clamp(body * ramp * top * streak * (0.55 + 0.9 * fold), 0.0, 1.0);
"""

FLOW_SNIPPET = r"""
int W = chi("W");
int ix = @ptnum % W; int iy = @ptnum / W;
float u = (ix + 0.5) / W; float v = (iy + 0.5) / W;
int P = chi("cells"); float seed = chf("seed");
float eps = 1.0 / W;
float phi(float a; float b; int P; float seed) {
    return pnoise(set(a * P, b * P, seed), P, P, 64)
         + 0.5 * pnoise(set(a * P * 2, b * P * 2, seed + 3.0), P * 2, P * 2, 64);
}
float dpu = (phi(u + eps, v, P, seed) - phi(u - eps, v, P, seed)) / (2.0 * eps);
float dpv = (phi(u, v + eps, P, seed) - phi(u, v - eps, P, seed)) / (2.0 * eps);
vector2 curl = set(dpv, -dpu) / P;
vector2 flow = set(1.0, 0.0) + chf("swirl") * curl * 2.0;
flow = normalize(flow);
v@col = set(flow.x * 0.5 + 0.5, flow.y * 0.5 + 0.5, 0.0);
"""


def main():
    timer = bc.BakeTimer("T_Aurora")
    strip = bc.eval_vex_image(STRIP_W, STRIP_H, STRIP_SNIPPET, {
        "seed": SEED, "folds": FOLDS, "streaks": STREAKS, "edge": EDGE,
        "edge_amp": EDGE_AMP, "ramp_pow": RAMP_POW,
    }, attrib="val", size=1)[:, :, 0]
    strip = strip / max(float(strip.max()), 1e-5)
    out = os.path.join(bc.RAW_DIR, "T_AuroraStrip.png")
    bc.save_png(out, strip)
    timer.output(out)
    timer.output(bc.save_tile_check("T_AuroraStrip", strip, repeat=2))
    seam = bc.seam_report(strip)
    timer.note("strip_seam_u", {"inner": seam["inner_step_x"], "wrap": seam["wrap_step_x"]})

    flow = bc.eval_vex_image(FLOW_RES, FLOW_RES, FLOW_SNIPPET, {
        "seed": SEED, "cells": FLOW_CELLS, "swirl": FLOW_SWIRL,
    })
    out = os.path.join(bc.RAW_DIR, "T_AuroraFlow.png")
    bc.save_png(out, flow)
    timer.output(out)
    timer.note("flow_seam", bc.seam_report(flow[:, :, :2]))
    timer.note("flow_mean_rg", [float(flow[:, :, 0].mean()), float(flow[:, :, 1].mean())])
    timer.note("flow_import", "sRGB must be OFF in Unreal")
    timer.finish()


main()
