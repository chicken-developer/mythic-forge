"""NetBuilder - creates nodes on a readable grid and wraps each module in a network box."""
from . import conventions as cv


def _hou():
    import hou
    return hou


class Module:
    """A group of nodes that does one job. Ends with an OUT_<NAME> null."""

    def __init__(self, builder, name, family, origin, note=""):
        self.b, self.name, self.family, self.origin, self.note = builder, name.upper(), family, origin, note
        self.nodes = []
        self.out = None

    def node(self, typ, role, col, row, inputs=(), color_role=None):
        """Create <module>_<role> at grid cell (col,row) inside this module and wire inputs.
        inputs: list of nodes (None leaves an input empty)."""
        hou = _hou()
        n = self.b.parent.createNode(typ, cv.node_name(self.name, role))
        x0, y0 = self.origin
        n.setPosition((x0 + col * cv.GRID_X, y0 - row * cv.GRID_Y))
        for i, src in enumerate(inputs):
            if src is not None:
                n.setInput(i, src)
        if color_role:
            n.setColor(hou.Color(cv.ROLE_COLORS[color_role]))
        self.nodes.append(n)
        return n

    def wrangle(self, role, col, row, snippet, run_over="detail", inputs=(), color_role="generate"):
        classes = {"detail": 0, "primitives": 1, "points": 2, "vertices": 3}
        n = self.node("attribwrangle", role, col, row, inputs, color_role)
        n.parm("class").set(classes[run_over])
        n.parm("snippet").set(snippet)
        return n

    def finish(self, last, col=0, row=None):
        """Add OUT_<NAME> under `last` and wrap all module nodes in a network box."""
        hou = _hou()
        if row is None:
            x0, y0 = self.origin
            row = round((y0 - last.position()[1]) / cv.GRID_Y) + 1
        out = self.b.parent.createNode("null", cv.out_name(self.name))
        x0, y0 = self.origin
        out.setPosition((x0 + col * cv.GRID_X, y0 - row * cv.GRID_Y))
        out.setInput(0, last)
        out.setColor(hou.Color(cv.ROLE_COLORS["out"]))
        self.nodes.append(out)
        self.out = out
        box = self.b.parent.createNetworkBox("nb_" + self.name.lower())
        box.setComment(self.name + (" - " + self.note if self.note else ""))
        box.setColor(hou.Color(cv.BOX_COLORS.get(self.family, (0.5, 0.5, 0.5))))
        for n in self.nodes:
            box.addNode(n)
        box.fitAroundContents()
        self.box = box
        return out


class NetBuilder:
    """Places modules left-to-right in rows: builder.module(...) returns a Module."""

    def __init__(self, parent):
        self.parent = parent
        self.modules = []

    def module(self, name, family, col, row, note=""):
        origin = (col * (cv.GRID_X * 1.0), -row * cv.GRID_Y)
        m = Module(self, name, family, origin, note)
        self.modules.append(m)
        return m

    def sticky(self, text, pos, size=(10.0, 3.0)):
        s = self.parent.createStickyNote("readme")
        s.setText(text)
        s.setPosition(pos)
        s.setSize(size)
        return s
