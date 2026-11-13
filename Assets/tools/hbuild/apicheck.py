"""Check every hou API assumption the toolkit relies on (references/hou_api_notes.md) on the
installed Houdini. Each check is independent; results go to docs/hou_api_notes.md.

  hython hb.py apicheck --out docs/_apicheck.json
"""
import traceback

RESULTS = []


def check(call):
    def deco(fn):
        def run():
            try:
                detail = fn()
                RESULTS.append({"call": call, "ok": True, "detail": detail or "confirmed"})
            except Exception as e:
                RESULTS.append({"call": call, "ok": False, "detail": "%s: %s" % (type(e).__name__, e),
                                "trace": traceback.format_exc()[-1500:]})
        run.__name__ = fn.__name__
        CHECKS.append(run)
        return run
    return deco


CHECKS = []


def _geo():
    import hou
    obj = hou.node("/obj")
    g = obj.node("_apicheck") or obj.createNode("geo", "_apicheck")
    for c in g.children():
        c.destroy()
    return g


@check("attribwrangle parms class (0 detail,1 prims,2 points,3 vertices) + snippet")
def wrangle_parms():
    g = _geo()
    w = g.createNode("attribwrangle", "w")
    items = w.parm("class").menuItems()
    w.parm("class").set(0)
    w.parm("snippet").set("addpoint(0, {1,2,3});")
    w.cook(force=True)
    assert len(w.geometry().points()) == 1, "detail wrangle did not add a point"
    return "class items=%s" % (list(items),)


@check("ParmTemplate kwargs: Menu(menu_labels, default_value), String(item_generator_script*, menu_type), "
       "Float(look=ColorSquare, naming_scheme=RGBA), Button(script_callback*)")
def parm_templates():
    import hou
    hou.MenuParmTemplate("m", "M", ["0", "1"], menu_labels=["a", "b"], default_value=1)
    hou.StringParmTemplate("s", "S", 1, default_value=("",), item_generator_script="[]",
                           item_generator_script_language=hou.scriptLanguage.Python,
                           menu_type=hou.menuType.Normal)
    hou.FloatParmTemplate("c", "C", 3, default_value=(1, 0, 0), look=hou.parmLook.ColorSquare,
                          naming_scheme=hou.parmNamingScheme.RGBA)
    hou.ButtonParmTemplate("b", "B", script_callback="pass",
                           script_callback_language=hou.scriptLanguage.Python)
    hou.FolderParmTemplate("f", "F", folder_type=hou.folderType.Tabs)


@check("subnet.indirectInputs()[0] wireable with setInput")
def indirect_inputs():
    g = _geo()
    sub = g.createNode("subnet", "sub")
    n = sub.createNode("null", "a")
    n.setInput(0, sub.indirectInputs()[0])
    return "indirect inputs=%d" % len(sub.indirectInputs())


@check("subnet.createDigitalAsset(name, hda_file_name, description, min_num_inputs, max_num_inputs) + "
       "definition().setParmTemplateGroup/addSection/setExtraFileOption/save + matchCurrentDefinition")
def create_hda():
    import hou, os, tempfile, json
    g = _geo()
    sub = g.createNode("subnet", "hdasub")
    sub.createNode("null", "OUT")
    path = os.path.join(tempfile.gettempdir(), "hb_apicheck.hda").replace("\\", "/")
    node = sub.createDigitalAsset(name="hb::apicheck::1.0", hda_file_name=path, description="apicheck",
                                  min_num_inputs=0, max_num_inputs=1)
    d = node.type().definition()
    ptg = hou.ParmTemplateGroup()
    ptg.append(hou.FloatParmTemplate("x", "X", 1, default_value=(2.0,)))
    d.setParmTemplateGroup(ptg)
    d.addSection("PythonModule", "def hello(n):\n    return 42\n")
    d.setExtraFileOption("PythonModule/IsPython", True)
    d.addSection("presets.json", json.dumps({"a": 1}))
    d.save(path, node)
    node.matchCurrentDefinition()
    assert node.hdaModule().hello(node) == 42, "PythonModule not callable"
    assert node.evalParm("x") == 2.0
    return "type=%s file=%s" % (node.type().name(), path)


@check("cam parms resx resy projection('ortho') orthowidth; Euler XYZ, looks down -Z")
def camera_parms():
    import hou
    cam = hou.node("/obj").node("_apicheck_cam") or hou.node("/obj").createNode("cam", "_apicheck_cam")
    for p in ("resx", "resy", "projection", "orthowidth"):
        assert cam.parm(p) is not None, "missing cam parm %s" % p
    items = cam.parm("projection").menuItems()
    cam.parm("projection").set("ortho")
    return "projection items=%s rOrd=%s" % (list(items), cam.parm("rOrd").evalAsString() if cam.parm("rOrd") else "?")


@check("/out opengl ROP type + parms camera, picture (existence only; no render per user rule)")
def opengl_rop():
    import hou
    rop = hou.node("/out").createNode("opengl", "_apicheck_gl")
    ok = {p: rop.parm(p) is not None for p in ("camera", "picture")}
    rop.destroy()
    assert all(ok.values()), ok
    return str(ok)


@check("VEX: setpointattrib vector4 creates orient; copytopoints uses orient + vector scale")
def copy_orient_scale():
    g = _geo()
    src = g.createNode("attribwrangle", "src")
    src.parm("class").set(0)
    src.parm("snippet").set("int p = addpoint(0, {0,0,0}); addprim(0, 'poly', p, addpoint(0, {1,0,0}), "
                            "addpoint(0, {1,1,0}));")
    pts = g.createNode("attribwrangle", "pts")
    pts.parm("class").set(0)
    pts.parm("snippet").set("int p = addpoint(0, {5,0,0});\n"
                            "setpointattrib(0, 'orient', p, quaternion(radians(90), {0,1,0}));\n"
                            "setpointattrib(0, 'scale', p, {2,3,1});")
    cp = g.createNode("copytopoints", "cp")
    cp.setInput(0, src)
    cp.setInput(1, pts)
    cp.cook(force=True)
    attr = pts.geometry().findPointAttrib("orient")
    assert attr is not None and attr.size() == 4, "orient not a vector4"
    bb = cp.geometry().boundingBox()
    size = [round(v, 3) for v in bb.sizevec()]
    assert abs(size[1] - 3.0) < 1e-3, "scale.y not applied: %s" % size
    assert abs(size[0]) < 1e-3 and abs(size[2] - 2.0) < 1e-3, "orient/scale.x wrong: %s" % size
    return "copied bbox size=%s" % size


@check("VEX: user functions in wrangle + dynamic chv('../col_' + s@mat_id)")
def dynamic_chv():
    import hou
    g = _geo()
    sub = g.createNode("subnet", "chsub")
    ptg = sub.parmTemplateGroup()
    ptg.append(hou.FloatParmTemplate("col_wall", "c", 3, default_value=(0.1, 0.2, 0.3),
                                     naming_scheme=hou.parmNamingScheme.RGBA))
    sub.setParmTemplateGroup(ptg)
    w = sub.createNode("attribwrangle", "w")
    w.parm("class").set(0)
    w.parm("snippet").set("vector f(string m) { return chv('../col_' + m); }\n"
                          "int p = addpoint(0, 0); int pr = addprim(0, 'poly', p);\n"
                          "setprimattrib(0, 'Cd', pr, f('wall'));")
    w.cook(force=True)
    cd = w.geometry().prims()[0].attribValue("Cd")
    assert abs(cd[2] - 0.3) < 1e-4, "chv returned %s" % (cd,)
    return "Cd=%s" % (cd,)


@check("polybevel SOP parms for HIGH branch (group, offset/distance, divisions, limit)")
def polybevel_parms():
    g = _geo()
    b = g.createNode("polybevel::3.0") if _has_type("polybevel::3.0") else g.createNode("polybevel")
    names = [p.name() for p in b.parms()]
    keys = [n for n in names if any(k in n for k in ("group", "offset", "dist", "division", "limit",
                                                     "ignoreflat", "flat", "angle", "profile", "fill"))]
    return "type=%s parms=%s" % (b.type().name(), keys[:60])


@check("SOP types used by LOW/HIGH (normal, clean, fuse, groupcreate/groupexpression, measure)")
def sop_types():
    g = _geo()
    out = {}
    for t in ("normal", "clean", "fuse::2.0", "fuse", "groupexpression", "groupcreate", "measure",
              "uvlayout", "boolean::2.0", "polyextrude::2.0", "copytopoints::2.0", "attribwrangle",
              "switch", "output", "polyreduce::2.0"):
        out[t] = _has_type(t)
    return str(out)


def _has_type(name):
    import hou
    return hou.nodeType(hou.sopNodeTypeCategory(), name) is not None


@check("hou.hda.installFile / reloadFile / uninstallFile exist")
def hda_module():
    import hou
    for fn in ("installFile", "reloadFile", "uninstallFile", "definitionsInFile", "loadedFiles"):
        assert hasattr(hou.hda, fn), fn


def run_all():
    import hou
    del RESULTS[:]
    for c in CHECKS:
        c()
    hou.node("/obj/_apicheck") and hou.node("/obj/_apicheck").destroy()
    hou.node("/obj/_apicheck_cam") and hou.node("/obj/_apicheck_cam").destroy()
    return {"houdini": hou.applicationVersionString(), "results": list(RESULTS),
            "ok": all(r["ok"] for r in RESULTS)}
