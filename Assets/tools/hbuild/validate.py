"""Lint a generated network against conventions.py and check that it cooks.

Checks
  E cook errors on any node / on OUT
  E nodes outside a network box (except OUT and sticky notes)
  W default-looking names (box1, attribwrangle3 ...)
  W modules without an OUT_<MODULE> null
  W wires that read from inside another module (not from its OUT_ null)
  W hard-coded values: non-default parms without an expression (wrangle snippet/class excluded)
  I geometry stats + bbox (compare against the preset's intended size)
"""
import re

ALLOWED_CONSTANT_PARMS = {"snippet", "class", "vex_cwdpath", "group", "outputidx",   # code / wiring, not shape
                          "booleanop", "operation",                                   # Boolean mode (subtract)
                          "attribname"}                                               # attribute names
DEFAULT_NAME = re.compile(r"^[a-z]+[a-z_]*\d+$")


def _expr(parm):
    try:
        parm.expression()
        return True
    except Exception:
        return False


def check(parent, out_name="OUT"):
    import hou
    rep = {"errors": [], "warnings": [], "info": {}}
    E, W = rep["errors"], rep["warnings"]
    out = parent.node(out_name)
    if out is None:
        E.append("no %s node in %s" % (out_name, parent.path()))
    else:
        try:
            out.cook(force=True)
        except Exception as e:
            E.append("cook failed: %s" % e)
    boxed = {}
    for b in parent.networkBoxes():
        for n in b.nodes():
            boxed[n.name()] = b.comment().split(" ")[0]
    for n in parent.children():
        for msg in n.errors():
            E.append("%s: %s" % (n.name(), msg))
        for msg in n.warnings():
            W.append("%s: %s" % (n.name(), msg))
        if n.name() not in boxed and n.name() != out_name and not n.name().startswith("MERGE_ALL"):
            E.append("%s is not inside a module network box" % n.name())
        if DEFAULT_NAME.match(n.name()):
            W.append("%s looks like a default name" % n.name())
        for i in n.inputs():
            if i is None:
                continue
            same = boxed.get(i.name()) == boxed.get(n.name())
            if not same and not i.name().startswith("OUT_") and i.name() != out_name:
                W.append("%s reads %s from inside module %s (use its OUT_ null)"
                         % (n.name(), i.name(), boxed.get(i.name())))
        for p in n.parms():
            if (p.name() in ALLOWED_CONSTANT_PARMS or p.name().startswith("profileramp")
                    or p.parmTemplate().type() == hou.parmTemplateType.Button):   # ramp keys of a default ramp
                continue
            try:
                if not p.isAtDefault() and not _expr(p):
                    W.append("%s.%s = %r is hard-coded (promote it to an HDA parm or reference one)"
                             % (n.name(), p.name(), p.eval()))
            except Exception:
                pass
    for comment in set(boxed.values()):
        if comment and parent.node("OUT_" + comment) is None and comment not in ("README",):
            W.append("module %s has no OUT_%s null" % (comment, comment))
    if out is not None and not E:
        g = out.geometry()
        bb = g.boundingBox()
        rep["info"] = {
            "points": len(g.points()), "prims": len(g.prims()),
            "bbox_min": list(bb.minvec()), "bbox_max": list(bb.maxvec()),
            "size": list(bb.sizevec()),
            "prim_attribs": [a.name() for a in g.primAttribs()],
        }
    return rep
