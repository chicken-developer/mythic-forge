"""T_ThinFilmRamp (256x8) and T_MemoryPlaceholder_0..3 (512x512 RGBA).

The memory images are placeholders for the 2D artist: dark silhouettes of rook-headed
golems with banners and weapons on a sepia ground. Alpha holds the soft round mask.
Drawn with Pillow inside hython; no Houdini nodes are needed for these two.
"""
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import _bake_common as bc

# ---- parameters -------------------------------------------------------------
RAMP_W = 256
RAMP_H = 8
RAMP_CYCLES = 1.6        # interference orders across the ramp
RAMP_PASTEL = 0.55       # 0 = white, 1 = saturated
MEMORY_RES = 512
MEMORY_COUNT = 4
MEMORY_SEED = 1000
SEPIA_LIGHT = (214, 170, 96)
SEPIA_DARK = (96, 62, 28)
SILHOUETTE = (28, 18, 12)
# -----------------------------------------------------------------------------


def bake_ramp(timer):
    t = np.linspace(0.0, 1.0, RAMP_W, dtype=np.float32)
    phase = np.array([0.0, 0.33, 0.67], dtype=np.float32)
    rainbow = 0.5 + 0.5 * np.cos(2.0 * math.pi * (t[:, None] * RAMP_CYCLES + phase[None, :]))
    fade = (0.35 + 0.65 * t)[:, None]            # weaker tint near the bubble centre
    row = 1.0 - RAMP_PASTEL * fade * (1.0 - rainbow)
    img = np.repeat(row[None, :, :], RAMP_H, axis=0)
    out = os.path.join(bc.RAW_DIR, "T_ThinFilmRamp.png")
    bc.save_png(out, img)
    timer.output(out)


def golem(draw, x, ground, h, rng, facing):
    """Blocky body with a castle-rook head. (x, ground) is the middle of the feet."""
    w = h * 0.52
    body_top = ground - h * 0.62
    draw.rectangle([x - w / 2, body_top, x + w / 2, ground], fill=SILHOUETTE)
    draw.rectangle([x - w * 0.62, body_top + h * 0.05, x - w / 2, body_top + h * 0.4], fill=SILHOUETTE)
    draw.rectangle([x + w / 2, body_top + h * 0.05, x + w * 0.62, body_top + h * 0.4], fill=SILHOUETTE)
    hw = w * 0.62
    head_top = body_top - h * 0.34
    draw.rectangle([x - hw / 2, head_top + h * 0.08, x + hw / 2, body_top], fill=SILHOUETTE)
    step = hw / 5.0
    for i in (0, 2, 4):
        draw.rectangle([x - hw / 2 + i * step, head_top, x - hw / 2 + (i + 1) * step, head_top + h * 0.1],
                       fill=SILHOUETTE)
    hand_x = x + facing * w * 0.66
    hand_y = body_top + h * 0.3
    kind = rng.choice(["banner", "hammer", "staff", "bow"])
    if kind == "banner":
        top = hand_y - h * 1.15
        draw.line([hand_x, ground, hand_x, top], fill=SILHOUETTE, width=max(2, int(h * 0.035)))
        fw = h * 0.5 * facing
        draw.polygon([(hand_x, top), (hand_x + fw, top + h * 0.12), (hand_x, top + h * 0.3)],
                     fill=SILHOUETTE)
    elif kind == "hammer":
        top = hand_y - h * 0.6
        draw.line([hand_x, hand_y + h * 0.25, hand_x, top], fill=SILHOUETTE, width=max(2, int(h * 0.05)))
        draw.rectangle([hand_x - h * 0.16, top - h * 0.1, hand_x + h * 0.16, top + h * 0.08], fill=SILHOUETTE)
    elif kind == "staff":
        top = hand_y - h * 0.85
        draw.line([hand_x, ground, hand_x, top], fill=SILHOUETTE, width=max(2, int(h * 0.035)))
        r = h * 0.08
        draw.polygon([(hand_x, top - 2 * r), (hand_x + r, top - r), (hand_x, top), (hand_x - r, top - r)],
                     fill=SILHOUETTE)
    else:
        r = h * 0.42
        box = [hand_x - r, hand_y - r, hand_x + r, hand_y + r]
        start = -70 if facing > 0 else 110
        draw.arc(box, start, start + 140, fill=SILHOUETTE, width=max(2, int(h * 0.04)))


def bake_memory(index, timer):
    rng = random.Random(MEMORY_SEED + index)
    size = MEMORY_RES
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    cx = size * (0.5 + rng.uniform(-0.12, 0.12))
    cy = size * rng.uniform(0.3, 0.42)
    glow = np.clip(1.0 - np.hypot(xx - cx, yy - cy) / (size * 0.75), 0.0, 1.0) ** 1.5
    light = np.array(SEPIA_LIGHT, dtype=np.float32) / 255.0
    dark = np.array(SEPIA_DARK, dtype=np.float32) / 255.0
    bg = dark[None, None, :] + (light - dark)[None, None, :] * glow[:, :, None]
    canvas = Image.fromarray(bc.to_u8(bg), "RGB")
    draw = ImageDraw.Draw(canvas)

    ground = size * rng.uniform(0.70, 0.76)
    pts = [(0, size)]
    for i in range(9):
        pts.append((size * i / 8.0, ground + rng.uniform(-10, 10)))
    pts.append((size, size))
    draw.polygon(pts, fill=SILHOUETTE)

    # distant gate / wall on some images
    if index % 2 == 0:
        gx = size * rng.uniform(0.55, 0.7)
        gw = size * 0.2
        gh = size * 0.3
        far = tuple(int(c * 0.6 + d * 0.4) for c, d in zip(SILHOUETTE, SEPIA_DARK))
        draw.rectangle([gx - gw / 2, ground - gh, gx + gw / 2, ground + 12], fill=far)
        draw.pieslice([gx - gw * 0.22, ground - gh * 0.55, gx + gw * 0.22, ground + gh * 0.1], 180, 360,
                      fill=tuple(int(v) for v in SEPIA_DARK))
        for i in range(4):
            draw.rectangle([gx - gw / 2 + i * gw / 3.5, ground - gh - 12,
                            gx - gw / 2 + i * gw / 3.5 + gw / 7, ground - gh], fill=far)

    count = rng.randint(3, 5)
    xs = sorted(size * (0.16 + 0.68 * (i + rng.uniform(0.15, 0.85)) / count) for i in range(count))
    for x in xs:
        h = size * rng.uniform(0.2, 0.3)
        golem(draw, x, ground + 4, h, rng, 1 if rng.random() < 0.6 else -1)

    canvas = canvas.filter(ImageFilter.GaussianBlur(1.2))
    rgb = np.asarray(canvas, dtype=np.float32) / 255.0
    r = np.hypot(xx - size / 2, yy - size / 2) / (size / 2)
    alpha = np.clip((1.0 - r) / 0.35, 0.0, 1.0)
    alpha = alpha * alpha * (3.0 - 2.0 * alpha)
    rgba = np.concatenate([rgb * alpha[:, :, None], alpha[:, :, None]], axis=2)
    out = os.path.join(bc.RAW_DIR, "T_MemoryPlaceholder_%d.png" % index)
    bc.save_png(out, rgba)
    timer.output(out)


def main():
    timer = bc.BakeTimer("T_ThinFilm_Memory")
    bake_ramp(timer)
    for i in range(MEMORY_COUNT):
        bake_memory(i, timer)
    timer.note("memory", "placeholders for the 2D artist; alpha is the soft round mask")
    timer.finish()


main()
