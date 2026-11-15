"""Crop (and optionally scale) an image, run with hython:  hython crop_image.py in.png out.png x y w h [scale]"""
import sys
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
x, y, w, h = (int(v) for v in sys.argv[3:7])
scale = float(sys.argv[7]) if len(sys.argv) > 7 else 1.0
img = Image.open(src).convert("RGB").crop((x, y, x + w, y + h))
if scale != 1.0:
    img = img.resize((int(w * scale), int(h * scale)), Image.LANCZOS)
img.save(dst)
print(dst)
