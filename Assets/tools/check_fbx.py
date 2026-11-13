"""Check an exported ASCII FBX mesh before it goes to Unreal (plain Python 3 + numpy, or hython).

    python tools/check_fbx.py <file.fbx> [<file.fbx> ...]

Prints one JSON line per file and exits 1 when a file has a defect Unreal shows as broken geometry:
  nonfinite_P      vertex positions that are NaN / inf (Unreal draws long fans from them)
  nonfinite_N/UV   normals / UVs that are NaN / inf (Interchange asserts on a NaN UV)
  bad_index        polygon indices outside the vertex array
  non_tri          polygons that are not triangles (the bake exports triangles only)
Information only: long_thin (triangles longer than 2.5 m and thinner than 3 % of that length; long trims
are normal, a jump in the count between two exports is not), degenerate (index-degenerate triangles).
"""
import json
import re
import sys

import numpy as np


def _arrays(txt, key):
    out = []
    for m in re.finditer(r"\n\s*" + key + r": \*(\d+) \{\s*a: ([^}]*)\}", txt):
        vals = m.group(2).replace("\n", "").replace("\r", "").split(",")
        out.append(np.array([float(v) if ("nan" not in v and "inf" not in v) else np.nan for v in vals]))
    return out


def check(path):
    txt = open(path, encoding="latin1").read()
    if not txt.startswith("; FBX"):
        return {"file": path, "error": "not an ASCII FBX"}
    rep = {"file": path, "meshes": 0, "verts": 0, "tris": 0, "nonfinite_P": 0, "nonfinite_N": 0, "nonfinite_UV": 0,
           "bad_index": 0, "non_tri": 0, "degenerate": 0, "long_thin": 0}
    for v in _arrays(txt, "Normals"):
        rep["nonfinite_N"] += int(np.isnan(v).sum())
    for v in _arrays(txt, "UV"):
        rep["nonfinite_UV"] += int(np.isnan(v).sum())
    for V, I in zip(_arrays(txt, "Vertices"), _arrays(txt, "PolygonVertexIndex")):
        V = V.reshape(-1, 3)
        I = I.astype(np.int64)
        rep["meshes"] += 1
        rep["verts"] += len(V)
        rep["nonfinite_P"] += int(np.isnan(V).any(1).sum())
        ends = np.where(I < 0)[0]
        starts = np.concatenate([[0], ends[:-1] + 1])
        sizes = ends - starts + 1
        idx = np.where(I < 0, -I - 1, I)
        rep["bad_index"] += int(((idx < 0) | (idx >= len(V))).sum())
        rep["non_tri"] += int((sizes != 3).sum())
        if (sizes == 3).all() and rep["bad_index"] == 0:
            tri = idx.reshape(-1, 3)
            rep["tris"] += len(tri)
            rep["degenerate"] += int(((tri[:, 0] == tri[:, 1]) | (tri[:, 1] == tri[:, 2]) | (tri[:, 0] == tri[:, 2])).sum())
            P = np.nan_to_num(V[tri])
            e = np.linalg.norm(P - P[:, [1, 2, 0]], axis=2).max(1)
            area = np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1) / 2
            unit = 100.0 if np.ptp(V[np.isfinite(V).all(1)], axis=0).max() > 100 else 1.0     # cm or m
            rep["long_thin"] += int(((e > 2.5 * unit) & (area / np.maximum(e, 1e-9) ** 2 < 0.03)).sum())
    rep["ok"] = not any(rep[k] for k in ("nonfinite_P", "nonfinite_N", "nonfinite_UV", "bad_index", "non_tri"))
    return rep


def main():
    ok = True
    for p in sys.argv[1:]:
        r = check(p)
        ok = ok and r.get("ok", False)
        print(json.dumps(r))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
