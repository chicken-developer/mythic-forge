#!/usr/bin/env python3
"""Dominant colours of a concept image (or a cropped region) -> values for the Look parms.

  python sample_palette.py concepts/townhouse_a/front.png --k 8
  python sample_palette.py concepts/townhouse_a/front.png --box 0.1 0.3 0.4 0.7   # x0 y0 x1 y1 (0-1)

Prints sRGB hex + linear RGB (Houdini Cd / Look parms are linear).
"""
import argparse

import numpy as np
import matplotlib.image as mpimg


def srgb_to_linear(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def kmeans(x, k, it=20, seed=0):
    rng = np.random.default_rng(seed)
    c = x[rng.choice(len(x), k, replace=False)]
    for _ in range(it):
        lab = ((x[:, None, :] - c[None]) ** 2).sum(-1).argmin(1)
        for j in range(k):
            if (lab == j).any():
                c[j] = x[lab == j].mean(0)
    return c, np.bincount(lab, minlength=k)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--k", type=int, default=8)
    ap.add_argument("--box", type=float, nargs=4)
    a = ap.parse_args()
    img = mpimg.imread(a.image)[..., :3].astype(float)
    if img.max() > 1.5:
        img /= 255.0
    if a.box:
        h, w = img.shape[:2]
        x0, y0, x1, y1 = a.box
        img = img[int(y0 * h):int(y1 * h), int(x0 * w):int(x1 * w)]
    px = img.reshape(-1, 3)
    if len(px) > 60000:
        px = px[np.random.default_rng(1).choice(len(px), 60000, replace=False)]
    cent, cnt = kmeans(px, a.k)
    for i in np.argsort(-cnt):
        s = cent[i]
        lin = srgb_to_linear(s)
        print("%5.1f%%  #%02x%02x%02x  linear [%.3f, %.3f, %.3f]" % (
            100 * cnt[i] / cnt.sum(), *(int(round(v * 255)) for v in s), *lin))


if __name__ == "__main__":
    main()
