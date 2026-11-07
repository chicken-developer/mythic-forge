#!/usr/bin/env python3
"""Concept vs render contact sheet for visual review (any Python with matplotlib).

  python contact_sheet.py --concept concepts/townhouse_a/front.png --renders renders/townhouse_a \
                          --out renders/townhouse_a/contact_sheet.png --title "townhouse_a v3"
"""
import argparse
import glob
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt     # noqa: E402
import matplotlib.image as mpimg   # noqa: E402

ORDER = ("persp", "front", "side", "top")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--concept", nargs="+", required=True)
    ap.add_argument("--renders", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--title", default="")
    a = ap.parse_args()
    shots = [os.path.join(a.renders, v + ".png") for v in ORDER if os.path.exists(os.path.join(a.renders, v + ".png"))]
    imgs = list(a.concept) + shots
    cols = max(len(a.concept), len(shots), 1)
    fig, axes = plt.subplots(2, cols, figsize=(5 * cols, 9), dpi=110, squeeze=False)
    fig.patch.set_facecolor("#1e2028")
    for r, row in enumerate((a.concept, shots)):
        for c in range(cols):
            ax = axes[r][c]
            ax.axis("off")
            if c < len(row):
                ax.imshow(mpimg.imread(row[c]))
                label = ("concept: " if r == 0 else "render: ") + os.path.splitext(os.path.basename(row[c]))[0]
                ax.set_title(label, color="#d8dbe6", fontsize=11, loc="left")
    fig.suptitle(a.title, color="white", fontsize=15, x=0.01, ha="left")
    plt.tight_layout()
    plt.savefig(a.out, facecolor=fig.get_facecolor())
    print(a.out)


if __name__ == "__main__":
    main()
