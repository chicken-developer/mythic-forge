"""Tile a few images into one sheet (run with hython):  hython tile_images.py out.png cell_width cols a.png b.png ..."""
import sys
from PIL import Image

out, cell_w, cols = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
paths = sys.argv[4:]
images = [Image.open(p).convert("RGB") for p in paths]
cell_h = cell_w * images[0].height // images[0].width
rows = (len(images) + cols - 1) // cols
sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (0, 0, 0))
for i, img in enumerate(images):
    sheet.paste(img.resize((cell_w, cell_h), Image.LANCZOS), ((i % cols) * cell_w, (i // cols) * cell_h))
sheet.save(out)
print(out)
