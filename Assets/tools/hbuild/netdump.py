"""Dump a network (children, positions, wires, boxes, stickies) to JSON for render_network.py."""
import json


def dump(parent):
    nodes = []
    for n in parent.children():
        nodes.append({
            "name": n.name(), "type": n.type().name(), "pos": list(n.position()),
            "color": list(n.color().rgb()),
            "inputs": [i.name() if i is not None else None for i in n.inputs()],
            "errors": list(n.errors()), "warnings": list(n.warnings()),
            "flags": {"display": _flag(n, "isDisplayFlagSet")},
        })
    boxes = []
    for b in parent.networkBoxes():
        boxes.append({"name": b.name(), "comment": b.comment(), "color": list(b.color().rgb()),
                      "nodes": [n.name() for n in b.nodes()]})
    notes = [{"text": s.text(), "pos": list(s.position())} for s in parent.stickyNotes()]
    return {"path": parent.path(), "nodes": nodes, "boxes": boxes, "notes": notes}


def _flag(n, fn):
    try:
        return bool(getattr(n, fn)())
    except Exception:
        return False


def write(parent, path):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(dump(parent), f, indent=1)
