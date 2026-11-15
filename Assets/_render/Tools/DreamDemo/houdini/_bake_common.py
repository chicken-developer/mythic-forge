"""Shared helpers for the DreamDemo Houdini bakes.

Every bake script runs in its own hython process (see run_all_bakes.py). Images are
evaluated with VEX on a point cloud (one point per pixel), so neither Copernicus nor
OpenGL is involved and the AMD driver crash recorded in the main repo cannot occur.
"""
import json
import os
import sys
import time

import hou
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
RAW_DIR = os.path.join(PROJECT_ROOT, "SourceAssets", "DreamDemo")
CHECK_DIR = os.path.join(RAW_DIR, "_checks")


def ensure_dirs():
    os.makedirs(RAW_DIR, exist_ok=True)
    os.makedirs(CHECK_DIR, exist_ok=True)


def srgb_hex_to_linear(hex_color):
    """'#7FD6D0' -> linear float triple."""
    h = hex_color.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return tuple(out)


def _add_parms(node, parms):
    group = node.parmTemplateGroup()
    for name, value in parms.items():
        if isinstance(value, bool):
            value = int(value)
        if isinstance(value, int):
            group.append(hou.IntParmTemplate(name, name, 1, default_value=(value,)))
        elif isinstance(value, float):
            group.append(hou.FloatParmTemplate(name, name, 1, default_value=(value,)))
        elif isinstance(value, (tuple, list)):
            group.append(hou.FloatParmTemplate(name, name, len(value),
                                               default_value=tuple(float(v) for v in value)))
        else:
            raise TypeError("unsupported parm %s=%r" % (name, value))
    node.setParmTemplateGroup(group)


def eval_vex_image(width, height, snippet, parms, attrib="col", size=3):
    """Run a point wrangle over width*height points; return float32 array (H, W, size).

    The snippet derives the pixel from @ptnum, so the point order never matters:
      int ix = @ptnum % W; int iy = @ptnum / W;   (row 0 is the top of the image)
    """
    obj = hou.node("/obj").createNode("geo", "bake")
    gen = obj.createNode("pointgenerate", "pixels")
    gen.parm("npts").set(width * height)
    wrangle = obj.createNode("attribwrangle", "shade")
    wrangle.setInput(0, gen)
    all_parms = {"W": int(width), "H": int(height)}
    all_parms.update(parms)
    _add_parms(wrangle, all_parms)
    wrangle.parm("snippet").set(snippet)
    geo = wrangle.geometry()
    errors = wrangle.errors()
    if errors:
        raise RuntimeError("VEX error: %s" % (errors,))
    if geo.findPointAttrib(attrib) is None:
        raise RuntimeError("attribute %s missing; warnings: %s" % (attrib, wrangle.warnings()))
    data = np.frombuffer(geo.pointFloatAttribValuesAsString(attrib), dtype=np.float32)
    if data.size != width * height * size:
        raise RuntimeError("unexpected value count %d" % data.size)
    result = data.reshape(height, width, size).copy()
    obj.destroy()
    return result


def to_u8(array):
    return (np.clip(array, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)


def save_png(path, array):
    """array: (H, W) or (H, W, 1|3|4) float 0..1, written as 8-bit PNG."""
    from PIL import Image
    data = to_u8(array)
    if data.ndim == 3 and data.shape[2] == 1:
        data = data[:, :, 0]
    Image.fromarray(data).save(path)


def save_exr_half(path, array):
    import OpenImageIO as oiio
    height, width, channels = array.shape
    spec = oiio.ImageSpec(width, height, channels, oiio.HALF)
    spec.attribute("compression", "zip")
    out = oiio.ImageOutput.create(path)
    if out is None:
        raise RuntimeError("OIIO cannot create %s: %s" % (path, oiio.geterror()))
    out.open(path, spec)
    out.write_image(np.ascontiguousarray(array, dtype=np.float32))
    out.close()


def save_hdr(path, array):
    """Radiance RGBE (.hdr), float input."""
    import OpenImageIO as oiio
    height, width, channels = array.shape
    spec = oiio.ImageSpec(width, height, channels, oiio.FLOAT)
    out = oiio.ImageOutput.create(path)
    if out is None:
        raise RuntimeError("OIIO cannot create %s: %s" % (path, oiio.geterror()))
    out.open(path, spec)
    out.write_image(np.ascontiguousarray(array, dtype=np.float32))
    out.close()


def seam_report(array):
    """Compare the wrap-around step with the typical neighbour step (tileability check)."""
    a = array.astype(np.float64)
    if a.ndim == 2:
        a = a[:, :, None]
    inner_x = np.abs(np.diff(a, axis=1)).mean()
    inner_y = np.abs(np.diff(a, axis=0)).mean()
    wrap_x = np.abs(a[:, 0] - a[:, -1]).mean()
    wrap_y = np.abs(a[0] - a[-1]).mean()
    return {"inner_step_x": float(inner_x), "wrap_step_x": float(wrap_x),
            "inner_step_y": float(inner_y), "wrap_step_y": float(wrap_y)}


def save_tile_check(name, array, repeat=2):
    path = os.path.join(CHECK_DIR, name + "_tile%dx%d.png" % (repeat, repeat))
    tiled = np.tile(array if array.ndim == 3 else array[:, :, None], (repeat, repeat, 1))
    save_png(path, tiled)
    return path


class BakeTimer:
    """Writes a small JSON record next to the checks so the runner can build the report."""

    def __init__(self, name):
        self.name = name
        self.start = time.time()
        self.record = {"name": name, "outputs": [], "notes": {}}
        ensure_dirs()

    def output(self, path):
        self.record["outputs"].append(os.path.relpath(path, PROJECT_ROOT).replace("\\", "/"))

    def note(self, key, value):
        self.record["notes"][key] = value

    def finish(self):
        self.record["seconds"] = round(time.time() - self.start, 2)
        self.record["houdini"] = hou.applicationVersionString()
        with open(os.path.join(CHECK_DIR, self.name + ".json"), "w", encoding="utf-8") as fh:
            json.dump(self.record, fh, indent=1)
        print("BAKE_OK %s %.2fs" % (self.name, self.record["seconds"]))
        sys.stdout.flush()
