"""Build the comparison sheets from a capture folder (run with hython, which has Pillow).

  hython make_sheets.py <capture_dir> [<out_dir>]

Writes:
  compare_full.png      3 x 3 grid, rows = cameras, columns = Day / Night / Dream
  compare_320_gray.png  CAM_Wide and CAM_Wall for NIGHT and DREAM at 320 px, greyscale,
                        side by side, no labels, order shuffled by a fixed seed
                        (the answer key is in compare_320_gray_key.txt)
"""
import os
import random
import sys

from PIL import Image, ImageDraw

CAMERAS = ["CAM_Wide", "CAM_Wall", "CAM_Sky"]
STATES = ["Day", "Night", "Dream"]


def build_full(capture_dir, out_dir, cell_w=960):
    cell_h = cell_w * 9 // 16
    pad, head = 8, 28
    sheet = Image.new("RGB", (pad + 3 * (cell_w + pad), head + pad + 3 * (cell_h + pad)), (18, 18, 22))
    draw = ImageDraw.Draw(sheet)
    for col, state in enumerate(STATES):
        draw.text((pad + col * (cell_w + pad) + 6, 8), state.upper(), fill=(235, 235, 235))
    for row, camera in enumerate(CAMERAS):
        for col, state in enumerate(STATES):
            path = os.path.join(capture_dir, "%s_%s.png" % (camera, state))
            if not os.path.exists(path):
                continue
            img = Image.open(path).convert("RGB").resize((cell_w, cell_h), Image.LANCZOS)
            sheet.paste(img, (pad + col * (cell_w + pad), head + pad + row * (cell_h + pad)))
    out = os.path.join(out_dir, "compare_full.png")
    sheet.save(out)
    return out


def build_gray(capture_dir, out_dir):
    items = []
    for camera in ("CAM_Wide", "CAM_Wall"):
        pair = [(camera, "Night"), (camera, "Dream")]
        random.Random(len(camera) * 7919).shuffle(pair)
        items.extend(pair)
    width, height, pad = 320, 180, 10
    sheet = Image.new("L", (pad + 2 * (width + pad), pad + 2 * (height + pad)), 40)
    key = []
    for index, (camera, state) in enumerate(items):
        path = os.path.join(capture_dir, "%s_%s.png" % (camera, state))
        if not os.path.exists(path):
            continue
        img = Image.open(path).convert("L").resize((width, height), Image.LANCZOS)
        col, row = index % 2, index // 2
        sheet.paste(img, (pad + col * (width + pad), pad + row * (height + pad)))
        key.append("row %d, column %d: %s %s" % (row + 1, col + 1, camera, state))
    out = os.path.join(out_dir, "compare_320_gray.png")
    sheet.save(out)
    with open(os.path.join(out_dir, "compare_320_gray_key.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(key) + "\n")
    return out


def main():
    capture_dir = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else capture_dir
    os.makedirs(out_dir, exist_ok=True)
    cell = int(sys.argv[3]) if len(sys.argv) > 3 else 960
    print(build_full(capture_dir, out_dir, cell))
    print(build_gray(capture_dir, out_dir))


if __name__ == "__main__":
    main()
