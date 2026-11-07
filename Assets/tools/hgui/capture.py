"""Viewport captures from the live Houdini GUI (no ROP, no Karma/Mantra): SceneViewer flipbook of the
current frame through the verification cameras that `hb.py asset` puts in every asset hip.

Run inside a Houdini GUI session (main thread), e.g. from hbgui: gjob("<job>") where the job calls
    capture.capture_asset(hip, asset, out_dir)
Writes <out_dir>/<view>.png for persp/front/side (output 0 as saved, normally HIGH) plus persp_low.png
(output 0 switched to LOW), cutaway.png (front half clipped away, interior check), and capture.json with the geometry counts seen by the GUI, so they can be
compared with validate.json from hython (detects a stale HDA definition in the GUI session).
"""
import json
import os
import time

import hou


def _viewer():
    sv = hou.ui.paneTabOfType(hou.paneTabType.SceneViewer)
    if sv is None:
        raise RuntimeError("no Scene Viewer pane in this Houdini session")
    return sv


def _setup_viewport(sv, vp):
    """Smooth shaded, no grid / reference plane, so the capture shows only the asset with its Cd."""
    try:
        sv.referencePlane().setIsVisible(False)
    except Exception:
        pass
    s = vp.settings()
    for fn, val in (("setDisplayOrthoGrid", False), ("showsName", False)):
        try:
            getattr(s, fn)(val)
        except Exception:
            pass
    try:
        ds = s.displaySet(hou.displaySetType.DisplayModel)
        ds.setShadedMode(hou.glShadingType.Smooth)
    except Exception:
        pass


def _flip(sv, vp, cam, path, res):
    vp.setCamera(cam)
    vp.lockCameraToView(False)
    fb = sv.flipbookSettings().stash()
    seq = path.replace(".png", ".$F4.png")
    fb.frameRange((1, 1))
    fb.output(seq)
    fb.outputToMPlay(False)
    fb.useResolution(True)
    fb.resolution(res)
    sv.flipbook(vp, fb)
    produced = path.replace(".png", ".0001.png")
    for _ in range(50):                       # the file appears when the flipbook finishes
        if os.path.exists(produced):
            break
        time.sleep(0.1)
    if os.path.exists(produced):
        if os.path.exists(path):
            os.remove(path)
        os.replace(produced, path)
    return os.path.exists(path)


def _cutaway(geo, out, tmp, sv, vp, asset, out_dir, res):
    """Interior check: a temporary Clip SOP removes the half of the building nearest to the persp camera
    (vertical plane through the bbox centre, normal = horizontal view direction); flipbook through persp."""
    cam = hou.node("/obj/%s_cam_persp" % asset)
    if cam is None:
        return "no camera"
    bb = out.geometry().boundingBox()
    c = bb.center()
    eye = cam.worldTransform().extractTranslates()
    fwd = hou.Vector3(c[0] - eye[0], 0.0, c[2] - eye[2]).normalized()
    clip = geo.node("_capture_cutaway") or geo.createNode("clip", "_capture_cutaway")
    clip.setInput(0, out)
    tmp.setInput(0, clip)
    ok = False
    try:
        for name, val in (("origin", (c[0], c[1], c[2])), ("dir", (fwd[0], fwd[1], fwd[2]))):
            clip.parmTuple(name).set(val)
        p = clip.parm("clipop")
        labels = [x.lower() for x in p.menuLabels()]
        keep = [i for i, x in enumerate(labels) if "above" in x]
        p.set(p.menuItems()[keep[0]])
        clip.cook(force=True)
        ok = _flip(sv, vp, cam, os.path.join(out_dir, "cutaway.png").replace("\\", "/"), res)
    finally:
        tmp.setInput(0, out)
        clip.destroy()
    return ok


def capture_asset(hip, asset, out_dir, hda=None, views=("persp", "front", "side"), res=(1600, 1200)):
    hou.hipFile.load(hip, suppress_save_prompt=True, ignore_load_warnings=True)
    if hda:                                   # always use the HDA file on disk, never a cached definition
        hou.hda.installFile(hda)
        hou.hda.reloadFile(hda)
    hou.setFrame(1)
    geo = hou.node("/obj/" + asset)
    gen = geo.node("generator")
    out = geo.node("OUT")
    out.setDisplayFlag(True)
    sv = _viewer()
    sv.setPwd(geo)
    vp = sv.curViewport()
    _setup_viewport(sv, vp)
    os.makedirs(out_dir, exist_ok=True)
    rep = {"hip": hip, "asset": asset, "views": {}, "time": time.strftime("%Y-%m-%d %H:%M:%S")}
    out.cook(force=True)
    # display-only helper (not saved): drop vertex uv so the viewport shows plain Cd, no UV checker
    tmp = geo.node("_capture_display") or geo.createNode("attribdelete", "_capture_display")
    tmp.setInput(0, out)
    tmp.parm("vtxdel").set("uv")
    tmp.setDisplayFlag(True)
    g = out.geometry()
    rep["points"], rep["prims"] = len(g.points()), len(g.prims())
    rep["size"] = list(g.boundingBox().sizevec())
    rep["preview_quality"] = gen.evalParm("preview_quality")
    for v in views:
        cam = hou.node("/obj/%s_cam_%s" % (asset, v))
        if cam is None:
            rep["views"][v] = "no camera"
            continue
        rep["views"][v] = _flip(sv, vp, cam, os.path.join(out_dir, v + ".png").replace("\\", "/"), res)
    # LOW persp for comparison with the HIGH bevel
    q = gen.parm("preview_quality")
    old = q.eval()
    q.set(0)
    cam = hou.node("/obj/%s_cam_persp" % asset)
    if cam is not None:
        rep["views"]["persp_low"] = _flip(sv, vp, cam, os.path.join(out_dir, "persp_low.png").replace("\\", "/"), res)
    q.set(old)
    try:
        rep["views"]["cutaway"] = _cutaway(geo, out, tmp, sv, vp, asset, out_dir, res)
    except Exception as e:                    # never lose the other views over the interior check
        rep["views"]["cutaway"] = "error: %s" % e
    out.setDisplayFlag(True)
    tmp.destroy()
    with open(os.path.join(out_dir, "capture.json"), "w") as f:
        json.dump(rep, f, indent=1)
    return rep
