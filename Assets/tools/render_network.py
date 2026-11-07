#!/usr/bin/env python3
"""Draw network.json (from hb.py) like the Houdini network editor -> PNG.
Runs in any Python with matplotlib (system python is fine; hython may not have matplotlib).

  python render_network.py renders/townhouse_a/network.json renders/townhouse_a/network.png
"""
import json
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt                       # noqa: E402
from matplotlib.patches import FancyBboxPatch, Rectangle  # noqa: E402

NW, NH = 2.25, 0.62


def main(src, dst):
    d = json.load(open(src))
    nodes = {n["name"]: n for n in d["nodes"]}
    if not nodes:
        raise SystemExit("empty network")
    xs = [n["pos"][0] for n in nodes.values()]
    ys = [n["pos"][1] for n in nodes.values()]
    w = max(xs) - min(xs) + 6
    h = max(ys) - min(ys) + 6
    fig = plt.figure(figsize=(max(12, w * 0.55), max(8, h * 0.55)), dpi=110)
    ax = fig.add_axes([0.01, 0.01, 0.98, 0.93])
    fig.patch.set_facecolor("#2b2d33")
    ax.set_facecolor("#33363d")
    for b in d["boxes"]:
        ns = [nodes[n] for n in b["nodes"] if n in nodes]
        if not ns:
            continue
        bx = [n["pos"][0] for n in ns]
        by = [n["pos"][1] for n in ns]
        x0, x1, y0, y1 = min(bx) - .4, max(bx) + NW + .4, min(by) - .4, max(by) + NH + 1.0
        c = b["color"]
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=c, alpha=.18, ec=c, lw=2))
        ax.text(x0 + .2, y1 - .4, b["comment"], color=c, fontsize=10, fontweight="bold", va="center")
    for n in nodes.values():
        for i, src_name in enumerate(n["inputs"]):
            s = nodes.get(src_name) if src_name else None
            if s is None:
                continue
            ax.plot([s["pos"][0] + NW / 2, n["pos"][0] + NW / 2 + (i - (len(n["inputs"]) - 1) / 2) * .4],
                    [s["pos"][1], n["pos"][1] + NH], color="#bfc4cf", lw=1, alpha=.7, zorder=2)
    for n in nodes.values():
        err = bool(n.get("errors"))
        ax.add_patch(FancyBboxPatch((n["pos"][0], n["pos"][1]), NW, NH,
                                    boxstyle="round,pad=0,rounding_size=.12", fc=n["color"],
                                    ec="#ff4040" if err else "#10121a", lw=2.5 if err else 1, zorder=3))
        ax.text(n["pos"][0] + NW / 2, n["pos"][1] + NH / 2, n["name"], ha="center", va="center",
                fontsize=7.4, color="white", zorder=4,
                fontweight="bold" if n["name"].startswith("OUT") else None)
    for s in d.get("notes", []):
        ax.text(s["pos"][0], s["pos"][1], s["text"], color="#e8d9a8", fontsize=7.5, va="top", family="monospace",
                bbox=dict(fc="#4a4532", ec="#8a7f55"))
    ax.set_xlim(min(xs) - 2.5, max(xs) + NW + 2.5)
    ax.set_ylim(min(ys) - 1.5, max(ys) + NH + 6)
    ax.set_aspect("equal")
    ax.axis("off")
    errs = sum(1 for n in nodes.values() if n.get("errors"))
    fig.text(0.01, 0.965, "%s  ·  %d nodes  ·  %d boxes  ·  %d with errors"
             % (d["path"], len(nodes), len(d["boxes"]), errs), color="white", fontsize=14, fontweight="bold")
    plt.savefig(dst, facecolor=fig.get_facecolor())
    print(dst)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
