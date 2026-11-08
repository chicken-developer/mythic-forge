"""Point the live Scene Viewer at a spot (review helper, GUI only).

    exec(open(r'E:/Repos/mythic-forge/Assets/tools/hgui/look.py').read())
    look((-12, 6, 0.7), (-6, 4.2, 0.7))   # eye, target in world space (metres)
    unlook()                               # back to the default perspective camera, helper camera removed
"""
import hou

_LOOK_CAM = "/obj/_hb_look_cam"


def _viewport():
    sv = hou.ui.paneTabOfType(hou.paneTabType.SceneViewer)
    return sv, sv.curViewport()


def look(eye, target, focal=35.0):
    cam = hou.node(_LOOK_CAM)
    if cam is None:
        cam = hou.node("/obj").createNode("cam", "_hb_look_cam")
    e = hou.Vector3(eye)
    t = hou.Vector3(target)
    z = (e - t).normalized()                 # camera looks down its -Z
    x = hou.Vector3(0, 1, 0).cross(z).normalized()
    y = z.cross(x)
    m = hou.Matrix4(((x[0], x[1], x[2], 0), (y[0], y[1], y[2], 0), (z[0], z[1], z[2], 0), (e[0], e[1], e[2], 1)))
    cam.setWorldTransform(m)
    cam.parm("focal").set(focal)
    cam.parm("resx").set(1280)
    cam.parm("resy").set(1080)
    sv, vp = _viewport()
    vp.setCamera(cam)
    vp.draw()
    return cam.path()


def unlook():
    sv, vp = _viewport()
    vp.useDefaultCamera()
    cam = hou.node(_LOOK_CAM)
    if cam is not None:
        cam.destroy()
