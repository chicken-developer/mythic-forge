"""Asset scene for verification: /obj/<asset>/<generator HDA> + cameras + OpenGL ROPs."""
import math

VIEWS = ("front", "side", "persp", "top")


def instance(hda_type, asset):
    import hou
    obj = hou.node("/obj")
    if obj.node(asset):
        obj.node(asset).destroy()
    geo = obj.createNode("geo", asset)
    for c in geo.children():
        c.destroy()
    node = geo.createNode(hda_type, "generator")
    out = geo.createNode("null", "OUT")
    out.setInput(0, node)
    node.setPosition((0, 0))
    out.setPosition((0, -1.5))
    out.setDisplayFlag(True)
    out.setRenderFlag(True)
    return geo, node, out


def _cam(obj, name, pos, rot, res, ortho_width=None):
    cam = obj.createNode("cam", name)
    cam.parmTuple("t").set(pos)
    cam.parmTuple("r").set(rot)
    cam.parm("resx").set(res[0])
    cam.parm("resy").set(res[1])
    if ortho_width:
        cam.parm("projection").set("ortho")
        cam.parm("orthowidth").set(ortho_width)
    return cam


def look_at_rot(pos, target):
    """Euler (XYZ order) for a Houdini camera at pos looking at target (camera looks down -Z)."""
    d = [target[i] - pos[i] for i in range(3)]
    ln = math.sqrt(sum(c * c for c in d)) or 1.0
    rx = math.degrees(math.asin(d[1] / ln))
    ry = math.degrees(math.atan2(-d[0], -d[2]))
    return (rx, ry, 0.0)


def cameras(asset_out, prefix, res=(1280, 960), persp_dir=(1.0, 0.65, 1.3)):
    """Front (+Z), side (+X), top and a 3/4 perspective camera framed on the cooked bbox."""
    import hou
    obj = hou.node("/obj")
    for v in VIEWS:
        old = obj.node("%s_cam_%s" % (prefix, v))
        if old:
            old.destroy()
    bb = asset_out.geometry().boundingBox()
    mn, mx = bb.minvec(), bb.maxvec()
    c = [(mn[i] + mx[i]) / 2 for i in range(3)]
    size = [mx[i] - mn[i] for i in range(3)]
    aspect = res[0] / float(res[1])
    pad = 1.15
    dist = max(size) * 3 + 10
    cams = {
        "front": _cam(obj, prefix + "_cam_front", (c[0], c[1], mx[2] + dist), (0, 0, 0), res,
                      max(size[0], size[1] * aspect) * pad),
        "side": _cam(obj, prefix + "_cam_side", (mx[0] + dist, c[1], c[2]), (0, 90, 0), res,
                     max(size[2], size[1] * aspect) * pad),
        "top": _cam(obj, prefix + "_cam_top", (c[0], mx[1] + dist, c[2]), (-90, 0, 0), res,
                    max(size[0], size[2] * aspect) * pad),
    }
    n = math.sqrt(sum(v * v for v in persp_dir))
    r = math.sqrt(sum(s * s for s in size)) * 1.7          # whole asset in frame with the default lens
    aim = (c[0], mn[1] + size[1] * 0.42, c[2])             # aim a bit low: the ground line stays in view
    pos = tuple(aim[i] + persp_dir[i] / n * r for i in range(3))
    cams["persp"] = _cam(obj, prefix + "_cam_persp", pos, look_at_rot(pos, aim), res)
    return cams


def opengl_rops(prefix, cams, out_dir):
    import hou
    out = hou.node("/out")
    rops = {}
    for v, cam in cams.items():
        name = "%s_gl_%s" % (prefix, v)
        if out.node(name):
            out.node(name).destroy()
        rop = out.createNode("opengl", name)
        rop.parm("camera").set(cam.path())
        rop.parm("picture").set("%s/%s.png" % (out_dir, v))
        rops[v] = rop
    return rops


def render(rops):
    for v, rop in rops.items():
        rop.render()
