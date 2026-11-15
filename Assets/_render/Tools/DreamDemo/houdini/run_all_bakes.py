"""Run every DreamDemo bake, each in its own hython process, then build the preview sheet.

  python run_all_bakes.py [--hython PATH] [--only bake_caustics bake_meshes ...]

The preview sheet is built in one more hython process, because Pillow only loads
reliably inside hython on this machine.
"""
import argparse
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
RAW_DIR = os.path.join(PROJECT_ROOT, "SourceAssets", "DreamDemo")
REPORT_DIR = os.path.join(PROJECT_ROOT, "RenderTests", "DreamDemo")
DEFAULT_HYTHON = r"E:\SideFX\Houdini 22.0.368\bin\hython.exe"
BAKES = ["bake_caustics", "bake_nebula", "bake_aurora", "bake_stars_noise",
         "bake_ramp_memory", "bake_meshes"]


def run_bakes(hython, names):
    results = []
    for name in names:
        script = os.path.join(HERE, name + ".py")
        start = time.time()
        proc = subprocess.run([hython, script], cwd=HERE, capture_output=True, text=True)
        seconds = round(time.time() - start, 1)
        ok = proc.returncode == 0 and "BAKE_OK" in proc.stdout
        results.append({"script": name, "ok": ok, "returncode": proc.returncode,
                        "process_seconds": seconds,
                        "stderr_tail": proc.stderr.strip().splitlines()[-8:] if not ok else []})
        print("%-20s %s %6.1fs" % (name, "OK " if ok else "FAIL", seconds))
        if not ok:
            print(proc.stdout[-2000:])
            print(proc.stderr[-4000:])
    return results


def build_preview():
    import numpy as np
    from PIL import Image, ImageDraw

    names = ["T_Caustics.png", "T_DreamNebula.exr", "T_AuroraStrip.png", "T_AuroraFlow.png",
             "T_StarField.png", "T_CloudNoise.png", "T_ThinFilmRamp.png",
             "T_MemoryPlaceholder_0.png", "T_MemoryPlaceholder_1.png",
             "T_MemoryPlaceholder_2.png", "T_MemoryPlaceholder_3.png"]
    cell_w, cell_h, pad, cols = 400, 260, 12, 4
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (cell_w + pad) + pad, rows * (cell_h + pad) + pad), (24, 24, 28))
    draw = ImageDraw.Draw(sheet)
    for i, name in enumerate(names):
        path = os.path.join(RAW_DIR, name)
        if name.endswith(".exr"):
            path = os.path.join(RAW_DIR, "_checks", "T_DreamNebula_preview.png")
        if not os.path.exists(path):
            continue
        img = Image.open(path)
        if img.mode == "RGBA":
            bg = Image.new("RGB", img.size, (10, 12, 26))
            bg.paste(img, mask=img.split()[3])
            img = bg
        img = img.convert("RGB")
        scale = min(cell_w / img.width, (cell_h - 22) / img.height)
        size = (max(1, int(img.width * scale)), max(1, int(img.height * scale)))
        resample = Image.NEAREST if img.height <= 16 else Image.LANCZOS
        img = img.resize(size, resample)
        x = pad + (i % cols) * (cell_w + pad)
        y = pad + (i // cols) * (cell_h + pad)
        sheet.paste(img, (x + (cell_w - size[0]) // 2, y + 20))
        draw.text((x + 2, y + 2), name, fill=(230, 230, 230))
    os.makedirs(REPORT_DIR, exist_ok=True)
    out = os.path.join(REPORT_DIR, "STEP_2_preview.png")
    sheet.save(out)
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--hython", default=DEFAULT_HYTHON)
    parser.add_argument("--only", nargs="*")
    parser.add_argument("--preview-only", action="store_true")
    args = parser.parse_args()
    if args.preview_only:
        print("preview:", build_preview())
        return 0
    names = args.only or BAKES
    results = run_bakes(args.hython, names)
    records = {}
    check_dir = os.path.join(RAW_DIR, "_checks")
    if os.path.isdir(check_dir):
        for fname in sorted(os.listdir(check_dir)):
            if fname.endswith(".json") and fname != "bake_summary.json":
                with open(os.path.join(check_dir, fname), encoding="utf-8") as fh:
                    records[fname[:-5]] = json.load(fh)
    proc = subprocess.run([args.hython, os.path.abspath(__file__), "--preview-only"],
                          cwd=HERE, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr[-3000:])
    preview = os.path.join(REPORT_DIR, "STEP_2_preview.png")
    summary = {"runs": results, "records": records,
               "preview": os.path.relpath(preview, PROJECT_ROOT).replace("\\", "/")}
    with open(os.path.join(check_dir, "bake_summary.json"), "w", encoding="utf-8") as fh:
        json.dump(summary, fh, indent=1)
    print("preview:", preview)
    return 0 if all(r["ok"] for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
