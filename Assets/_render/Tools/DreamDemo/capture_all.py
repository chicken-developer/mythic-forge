"""Step 7: capture every camera in every state on L_Kingdom_Main and run the image checks.

Run with hython (it ships Pillow and numpy):
  hython Tools/DreamDemo/capture_all.py [--quick] [--no-gate] [--skip-capture]

What happens:
  1. The editor is started with ue/pie_tour.py. It plays the map (PIE) and calls the events
     of BP_DreamWorldState (ApplyState, ViewWide/ViewWall/ViewSky, Gate, ToggleReducedMotion),
     taking 1920x1080 high-res screenshots. No DreamDemo C++ is involved.
  2. compare_full.png and compare_320_gray.png are built from the captures.
  3. The stencil check and the gate frames are analysed.
Outputs go to RenderTests/DreamDemo/ (captures in RenderTests/DreamDemo/captures/).
GPU cost is not measured here: the old C++ timer went away with the Blueprint port.
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(HERE, "..", ".."))
REPORT_DIR = os.path.join(PROJECT_DIR, "RenderTests", "DreamDemo")
CAPTURE_DIR = os.path.join(REPORT_DIR, "captures")
UE_ROOT = os.environ.get("UE_ROOT", r"C:\Program Files\Epic Games\UE_5.8")

sys.path.insert(0, os.path.join(HERE, "tools"))


def run_capture(quick, gate):
    for name in os.listdir(CAPTURE_DIR) if os.path.isdir(CAPTURE_DIR) else []:
        if name.endswith((".png", ".json")):
            os.remove(os.path.join(CAPTURE_DIR, name))
    os.makedirs(CAPTURE_DIR, exist_ok=True)
    config = os.path.join(CAPTURE_DIR, "tour_config.json")
    with open(config, "w", encoding="utf-8") as fh:
        json.dump({"out_dir": CAPTURE_DIR.replace("\\", "/"), "quick": bool(quick), "gate": bool(gate)}, fh)
    env = dict(os.environ)
    env["UE_PYTHONPATH"] = os.path.join(HERE, "ue", "autorun")
    env["DREAMDEMO_AUTORUN"] = os.path.join(HERE, "ue", "pie_tour.py")
    env["DREAMDEMO_TOUR"] = config
    args = [os.path.join(UE_ROOT, "Engine", "Binaries", "Win64", "UnrealEditor.exe"),
            os.path.join(PROJECT_DIR, "TDKProps.uproject"), "-nosplash", "-unattended", "-log=dd_capture.log"]
    return subprocess.call(args, env=env)


def load(name):
    path = os.path.join(CAPTURE_DIR, name)
    if not os.path.exists(path):
        return None
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0


def erode(mask, pixels):
    out = mask.copy()
    for _ in range(pixels):
        m = out
        out = m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1)
    return out


def stencil_check():
    """Pixels with custom stencil != 0 must look the same with WorldBlend 0 and 1."""
    a, b, m = load("check_caustics_blend0.png"), load("check_caustics_blend1.png"), load("check_stencil_mask.png")
    if a is None or b is None or m is None:
        return {"pass": None, "note": "not run: capture was started with --quick"}
    mask = erode(m.min(axis=2) > 0.75, 4)
    environment = erode(m.max(axis=2) < 0.5, 4)
    environment[: a.shape[0] // 2] = False            # lower half: ground, where the caustics are
    diff = np.abs(a - b).mean(axis=2)
    on_mask = float(diff[mask].mean()) * 255.0 if mask.any() else None
    on_env = float(diff[environment].mean()) * 255.0 if environment.any() else None
    ok = on_mask is not None and on_env is not None and on_mask < 2.0 and on_env > on_mask * 3.0
    return {
        "pass": bool(ok),
        "stencil_pixels": int(mask.sum()),
        "mean_abs_diff_on_stencil_8bit": None if on_mask is None else round(on_mask, 3),
        "mean_abs_diff_on_ground_8bit": None if on_env is None else round(on_env, 3),
        "rule": "difference on stencil pixels < 2/255 and ground difference at least 3x larger",
        "note": "the residual on stencil pixels is temporal noise (TSR, Lumen), not caustics",
    }


def gray_stats():
    """Numbers only; whether NIGHT and DREAM read apart at a glance is for people to judge."""
    out = {}
    for camera in ("CAM_Wide", "CAM_Wall"):
        for state in ("Night", "Dream"):
            path = os.path.join(CAPTURE_DIR, "%s_%s.png" % (camera, state))
            if not os.path.exists(path):
                continue
            g = np.asarray(Image.open(path).convert("L").resize((320, 180), Image.LANCZOS), dtype=np.float32) / 255.0
            edges = np.abs(np.diff(g, axis=1)).mean() + np.abs(np.diff(g, axis=0)).mean()
            out["%s_%s" % (camera, state)] = {
                "mean": round(float(g.mean()), 4), "std": round(float(g.std()), 4),
                "edge_energy": round(float(edges), 4),
                "lower_half_mean": round(float(g[90:].mean()), 4),
                "lower_half_std": round(float(g[90:].std()), 4),
            }
    return out


def gate_analysis():
    """How much of each gate frame is covered by the veil, and a contact sheet."""
    frames = sorted(n for n in os.listdir(CAPTURE_DIR) if n.startswith(("gate_", "wake_")))
    if not frames:
        return {"frames": 0, "note": "not run: no gate frames captured"}
    rows = []
    thumbs = []
    for name in frames:
        img = Image.open(os.path.join(CAPTURE_DIR, name)).convert("RGB")
        small = np.asarray(img.resize((160, 90), Image.BILINEAR), dtype=np.float32) / 255.0
        r, g, b = small[..., 0], small[..., 1], small[..., 2]
        # the veil is a bright, flat teal: green and blue high, red clearly lower, little detail
        veil = (g > 0.55) & (b > 0.5) & (r < g - 0.08) & (np.abs(g - b) < 0.18)
        rows.append({"frame": name, "veil_fraction": round(float(veil.mean()), 3),
                     "mean_luma": round(float(small.mean()), 3)})
        thumbs.append(img.resize((320, 180), Image.LANCZOS))
    cols = 8
    sheet = Image.new("RGB", (cols * 320, ((len(thumbs) + cols - 1) // cols) * 180), (0, 0, 0))
    for i, thumb in enumerate(thumbs):
        sheet.paste(thumb, ((i % cols) * 320, (i // cols) * 180))
    sheet_path = os.path.join(REPORT_DIR, "STEP_8_gate_frames.png")
    sheet.save(sheet_path)
    covered = [r["frame"] for r in rows if r["veil_fraction"] > 0.9]
    return {"frames": len(rows), "fully_covered_frames": covered, "per_frame": rows,
            "sheet": os.path.relpath(sheet_path, PROJECT_DIR).replace("\\", "/"),
            "note": "frames are 0.1 s apart; whether any uncovered frame shows unconverged Lumen is a visual check"}


def performance():
    return {"note": "not measured: GPU timing needed the removed C++ controller. "
                    "Numbers of the forest prototype are in forest_prototype/STEP_7.md."}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--quick", action="store_true", help="screenshots only, no measurements or checks")
    parser.add_argument("--no-gate", action="store_true")
    parser.add_argument("--skip-capture", action="store_true", help="reuse the images already in captures/")
    args = parser.parse_args()

    code = 0
    if not args.skip_capture:
        code = run_capture(args.quick, not args.no_gate)

    import make_sheets
    sheets = [make_sheets.build_full(CAPTURE_DIR, REPORT_DIR), make_sheets.build_gray(CAPTURE_DIR, REPORT_DIR)]
    expected = ["%s_%s.png" % (c, s) for c in make_sheets.CAMERAS for s in make_sheets.STATES]
    results = {
        "capture_exit_code": code,
        "captures_present": {n: os.path.exists(os.path.join(CAPTURE_DIR, n)) for n in expected},
        "sheets": [os.path.relpath(p, PROJECT_DIR).replace("\\", "/") for p in sheets],
        "performance": performance(),
        "stencil_check": stencil_check(),
        "gray_320_stats": gray_stats(),
        "gate": gate_analysis(),
    }
    out = os.path.join(REPORT_DIR, "step7_results.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(results, fh, indent=1)
    print("written", out)
    tour = os.path.join(CAPTURE_DIR, "tour_log.json")
    if os.path.exists(tour):
        with open(tour, encoding="utf-8") as fh:
            results["tour_errors"] = json.load(fh).get("errors")
        with open(out, "w", encoding="utf-8") as fh:
            json.dump(results, fh, indent=1)
        print("tour errors:", results["tour_errors"])
    print(json.dumps(results["stencil_check"], indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
