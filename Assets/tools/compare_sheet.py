#!/usr/bin/env python3
"""Concept vs Houdini viewport screenshots, side by side (PIL only; any Python with Pillow).

  python compare_sheet.py --concept img_refs/medievals_refs/X.png --shots renders/<asset> \
                          --out renders/<asset>/compare.png --title "<asset> round 1"

Top row: concept | persp (HIGH). Bottom row: front | side | persp LOW. Transparent viewport
backgrounds are composited on a neutral grey so silhouettes read like the concept sheets.
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFont

BG = (46, 48, 56)
SHOT_BG = (196, 196, 192)


def _font(sz):
    for f in ("DejaVuSans.ttf", "arial.ttf", "Arial.ttf"):
        try:
            return ImageFont.truetype(f, sz)
        except OSError:
            pass
    return ImageFont.load_default()


def _load(path, box):
    im = Image.open(path)
    if im.mode in ("RGBA", "LA") or "transparency" in im.info:
        im = im.convert("RGBA")
        bg = Image.new("RGBA", im.size, SHOT_BG + (255,))
        bg.alpha_composite(im)
        im = bg
    im = im.convert("RGB")
    im.thumbnail(box, Image.LANCZOS)
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--concept", nargs="+", required=True)
    ap.add_argument("--shots", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--title", default="")
    ap.add_argument("--concept-label", default="concept", help="label of the reference cell (e.g. moodboard)")
    a = ap.parse_args()
    W, H, pad, th = 2400, 1500, 16, 56
    sheet = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(sheet)
    d.text((pad, 12), a.title, fill=(240, 240, 240), font=_font(30))
    top_h = int((H - th) * 0.58)
    bot_h = H - th - top_h - 3 * pad
    cells = [(a.concept[0], a.concept_label, (pad, th, W // 2 - 1.5 * pad, top_h)),
             (os.path.join(a.shots, "persp.png"), "Houdini viewport - persp (HIGH)", (W // 2 + pad * 0.5, th, W // 2 - 1.5 * pad, top_h))]
    bw = (W - 4 * pad) / 3
    for i, (v, lab) in enumerate((("front", "front (HIGH)"), ("side", "side (HIGH)"), ("persp_low", "persp (LOW)"))):
        cells.append((os.path.join(a.shots, v + ".png"), lab, (pad + i * (bw + pad), th + top_h + 2 * pad, bw, bot_h)))
    f = _font(22)
    for path, lab, (x, y, w, h) in cells:
        x, y, w, h = int(x), int(y), int(w), int(h)
        d.rectangle([x, y, x + w, y + h], fill=(36, 38, 44))
        if os.path.exists(path):
            im = _load(path, (w, h - 30))
            sheet.paste(im, (x + (w - im.width) // 2, y + 30 + (h - 30 - im.height) // 2))
        else:
            d.text((x + 10, y + 40), "missing: " + os.path.basename(path), fill=(255, 120, 120), font=f)
        d.text((x + 8, y + 4), lab, fill=(220, 224, 235), font=f)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    sheet.save(a.out, optimize=True)
    print(a.out)


if __name__ == "__main__":
    main()
