"""Read-only deep dump of an existing hip (for porting hand-built networks into hbuild).

Writes one JSON with, for every node under the given roots (recursively, including locked HDA
contents): path, type, inputs, network box, comment, flags, non-default parms (value or expression),
wrangle / python code, spare parms, and the interface of every HDA definition used in the scene.
Optionally cooks the display node of each top-level geo object and records bbox / counts / time.

Never saves the hip. Run from hython:  hython hb.py inspect --hip X --out Y.json
"""
import json
import time

CODE_PARMS = ("snippet", "python", "code", "vexsrc", "script")
MAX_STR = 20000


def _s(v):
    s = v if isinstance(v, str) else repr(v)
    return s if len(s) <= MAX_STR else s[:MAX_STR] + "...<truncated %d chars>" % (len(s) - MAX_STR)


def _parm_record(p):
    rec = {}
    try:
        rec["expr"] = _s(p.expression())
        rec["lang"] = str(p.expressionLanguage()).split(".")[-1]
    except Exception:
        pass
    try:
        rec["value"] = _s(p.unexpandedString()) if p.parmTemplate().type().name() == "String" else p.eval()
    except Exception:
        try:
            rec["value"] = _s(p.eval())
        except Exception as e:
            rec["value"] = "<eval error %s>" % e
    if isinstance(rec.get("value"), float):
        rec["value"] = round(rec["value"], 6)
    return rec


def node_record(n, boxes):
    import hou
    t = n.type()
    rec = {
        "path": n.path(), "name": n.name(), "type": t.name(), "category": t.category().name(),
        "box": boxes.get(n.path()), "comment": n.comment(), "color": [round(c, 3) for c in n.color().rgb()],
        "inputs": [i.path() if i is not None else None for i in n.inputs()],
        "n_children": len(n.children()),
        "locked_hda": bool(t.definition()) and not n.isEditable() if hasattr(n, "isEditable") else False,
    }
    try:
        rec["hda"] = t.definition().libraryFilePath() if t.definition() else None
    except Exception:
        rec["hda"] = None
    flags = {}
    for f in ("isDisplayFlagSet", "isRenderFlagSet", "isBypassed", "isTemplateFlagSet"):
        try:
            flags[f] = bool(getattr(n, f)())
        except Exception:
            pass
    rec["flags"] = flags
    parms, code = {}, {}
    for p in n.parms():
        try:
            pt = p.parmTemplate()
            if pt.type() in (hou.parmTemplateType.Folder, hou.parmTemplateType.FolderSet,
                             hou.parmTemplateType.Separator, hou.parmTemplateType.Label):
                continue
            is_expr = True
            try:
                p.expression()
            except Exception:
                is_expr = False
            if p.name() in CODE_PARMS or p.name().startswith("snippet"):
                v = p.unexpandedString() if pt.type() == hou.parmTemplateType.String else p.eval()
                if v:
                    code[p.name()] = _s(v)
                continue
            if is_expr or not p.isAtDefault():
                parms[p.name()] = _parm_record(p)
        except Exception as e:
            parms[p.name()] = {"error": str(e)}
    rec["parms"] = parms
    if code:
        rec["code"] = code
    try:
        spare = [pt.name() for pt in n.spareParms()]
        if spare:
            rec["spare_parms"] = spare
    except Exception:
        pass
    try:
        rec["errors"] = list(n.errors())
        rec["warnings"] = list(n.warnings())
    except Exception:
        pass
    return rec


def hda_definitions():
    import hou
    out = []
    for f in hou.hda.loadedFiles():
        try:
            defs = hou.hda.definitionsInFile(f)
        except Exception:
            continue
        for d in defs:
            try:
                ptg = d.parmTemplateGroup()
                names = [(t.name(), t.type().name(), t.label()) for t in ptg.entriesWithoutFolders()]
            except Exception:
                names = []
            out.append({"type": d.nodeTypeName(), "category": d.nodeTypeCategory().name(),
                        "file": f, "description": d.description(),
                        "sections": sorted(d.sections().keys()), "parms": names[:600]})
    return out


def dump(roots=("/obj",), cook=True):
    import hou
    nodes, boxes, stickies = [], {}, []
    for r in roots:
        root = hou.node(r)
        if root is None:
            continue
        all_nodes = [root] + list(root.allSubChildren(recurse_in_locked_nodes=True))
        for parent in all_nodes:
            try:
                for b in parent.networkBoxes():
                    for m in b.nodes():
                        boxes[m.path()] = b.comment() or b.name()
                for s in parent.stickyNotes():
                    stickies.append({"parent": parent.path(), "text": _s(s.text())})
            except Exception:
                pass
        for n in all_nodes[1:]:
            nodes.append(node_record(n, boxes))
    cooks = []
    if cook:
        for r in roots:
            root = hou.node(r)
            if root is None:
                continue
            for obj in root.children():
                if obj.type().name() not in ("geo", "subnet") and not obj.type().definition():
                    continue
                disp = obj.displayNode() if hasattr(obj, "displayNode") else None
                if disp is None:
                    continue
                t0 = time.time()
                rec = {"object": obj.path(), "display": disp.path()}
                try:
                    disp.cook(force=True)
                    g = disp.geometry()
                    bb = g.boundingBox()
                    rec.update({"ok": True, "seconds": round(time.time() - t0, 2),
                                "points": g.intrinsicValue("pointcount"),
                                "prims": g.intrinsicValue("primitivecount"),
                                "size": [round(v, 3) for v in bb.sizevec()],
                                "bbox_min": [round(v, 3) for v in bb.minvec()],
                                "prim_attribs": [a.name() for a in g.primAttribs()],
                                "point_attribs": [a.name() for a in g.pointAttribs()],
                                "detail_attribs": [a.name() for a in g.globalAttribs()],
                                "errors": list(disp.errors())})
                except Exception as e:
                    rec.update({"ok": False, "error": str(e)[:2000]})
                cooks.append(rec)
    return {"hip": hou.hipFile.path(), "houdini": hou.applicationVersionString(),
            "n_nodes": len(nodes), "nodes": nodes, "stickies": stickies,
            "hda_definitions": hda_definitions(), "cooks": cooks}


def write(path, roots=("/obj",), cook=True):
    data = dump(roots, cook)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, default=str)
    return {"n_nodes": data["n_nodes"], "cooks": data["cooks"],
            "hda_types": [d["type"] for d in data["hda_definitions"]]}
