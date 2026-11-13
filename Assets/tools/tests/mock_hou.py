"""Minimal fake `hou` for dry-running hbuild without Houdini (structure only - nothing cooks).
Usage:  python tests/dry_run.py   (inserts this module as `hou`)"""
import types


class _Parm:
    def __init__(self, node, name):
        self.node, self._name, self.value, self.expr = node, name, None, None
    def name(self): return self._name
    def set(self, v): self.value = v
    def setExpression(self, e, language=None): self.expr = e
    def eval(self): return self.value
    def expression(self):
        if self.expr is None: raise RuntimeError("no expression")
        return self.expr
    def pressButton(self): pass


class _PTuple(list):
    def __init__(self, node, name):
        super().__init__([_Parm(node, name)])
        self._name = name
    def set(self, vals): self[0].value = tuple(vals)
    def eval(self): return self[0].value if isinstance(self[0].value, tuple) else (self[0].value,)
    def name(self): return self._name


class Color:
    def __init__(self, rgb): self._rgb = tuple(rgb)
    def rgb(self): return self._rgb


class _BBox:
    def minvec(self): return (-5.0, 0.0, -4.0)
    def maxvec(self): return (5.0, 12.0, 4.0)
    def sizevec(self): return (10.0, 12.0, 8.0)


class _Geo:
    def boundingBox(self): return _BBox()
    def points(self): return []
    def prims(self): return []
    def primAttribs(self): return []


class _Box:
    def __init__(self, name): self._name, self._nodes, self._comment, self._color = name, [], "", Color((.5, .5, .5))
    def name(self): return self._name
    def addNode(self, n): self._nodes.append(n)
    def nodes(self): return list(self._nodes)
    def fitAroundContents(self): pass
    def setComment(self, c): self._comment = c
    def comment(self): return self._comment
    def setColor(self, c): self._color = c
    def color(self): return self._color


class _Sticky:
    def __init__(self): self._text, self._pos = "", (0, 0)
    def setText(self, t): self._text = t
    def text(self): return self._text
    def setPosition(self, p): self._pos = tuple(p)
    def position(self): return self._pos
    def setSize(self, s): pass


class _Indirect:
    def name(self): return "(subnet input 1)"


class _Defn:
    def __init__(self, node): self.node, self.secs, self.ptg = node, {}, None
    def setParmTemplateGroup(self, g): self.ptg = g
    def addSection(self, name, contents): self.secs[name] = types.SimpleNamespace(contents=lambda c=contents: c)
    def sections(self): return self.secs
    def setExtraFileOption(self, *a): pass
    def save(self, *a, **k): pass
    def libraryFilePath(self): return "/mock.hda"


class _Type:
    def __init__(self, name, defn=None): self._name, self._defn = name, defn
    def name(self): return self._name
    def definition(self): return self._defn


class Node:
    def __init__(self, parent, typ, name):
        self.parent, self._type, self._name = parent, _Type(typ), name
        self._inputs, self._pos, self.kids, self.boxes, self.notes = {}, (0, 0), [], [], []
        self._parms, self._color = {}, Color((.8, .8, .8))
    # hierarchy
    def name(self): return self._name
    def type(self): return self._type
    def path(self): return (self.parent.path() if self.parent else "") + "/" + self._name if self._name else ""
    def createNode(self, typ, name=None):
        name = name or typ
        if any(k._name == name for k in self.kids):
            i = 1
            while any(k._name == "%s%d" % (name, i) for k in self.kids): i += 1
            name = "%s%d" % (name, i)
        n = Node(self, typ, name); self.kids.append(n); return n
    def node(self, rel):
        if rel.startswith("/"): return ROOT.node(rel[1:])
        cur = self
        for part in rel.split("/"):
            if not part: continue
            cur = next((k for k in cur.kids if k._name == part), None)
            if cur is None: return None
        return cur
    def children(self): return list(self.kids)
    def destroy(self): self.parent.kids.remove(self)
    def indirectInputs(self): return [_Indirect() for _ in range(4)]
    # wiring / layout
    def setInput(self, i, n): self._inputs[i] = n
    def inputs(self):
        if not self._inputs: return ()
        return tuple(self._inputs.get(i) if not isinstance(self._inputs.get(i), _Indirect) else None
                     for i in range(max(self._inputs) + 1))
    def setPosition(self, p): self._pos = tuple(p)
    def position(self): return self._pos
    def setColor(self, c): self._color = c
    def color(self): return self._color
    # parms
    def parm(self, name):
        if name not in self._parms: self._parms[name] = _Parm(self, name)
        return self._parms[name]
    def parmTuple(self, name): return _PTuple(self, name)
    def parms(self): return []
    def parmTuples(self): return []
    def evalParm(self, n): return self.parm(n).value
    # flags / cook
    def setDisplayFlag(self, v): pass
    def setRenderFlag(self, v): pass
    def isDisplayFlagSet(self): return False
    def cook(self, force=False): pass
    def errors(self): return ()
    def warnings(self): return ()
    def geometry(self): return _Geo()
    def render(self, *a, **k): pass
    # network items
    def createNetworkBox(self, name): b = _Box(name); self.boxes.append(b); return b
    def networkBoxes(self): return list(self.boxes)
    def createStickyNote(self, name): s = _Sticky(); self.notes.append(s); return s
    def stickyNotes(self): return list(self.notes)
    # HDA
    def createDigitalAsset(self, name, hda_file_name, description, min_num_inputs=0, max_num_inputs=1):
        self._type = _Type(name, _Defn(self)); return self
    def matchCurrentDefinition(self): pass


ROOT = Node(None, "root", "")
ROOT.kids = [Node(ROOT, "obj", "obj"), Node(ROOT, "out", "out")]


def node(path): return ROOT.node(path.lstrip("/"))


class _Tmpl:
    def __init__(self, *a, **k): self.a, self.k = a, k
    def setHelp(self, h): pass


class ParmTemplateGroup:
    def __init__(self): self.items = []
    def append(self, t): self.items.append(t)


FloatParmTemplate = IntParmTemplate = ToggleParmTemplate = MenuParmTemplate = _Tmpl
StringParmTemplate = ButtonParmTemplate = FolderParmTemplate = _Tmpl
scriptLanguage = types.SimpleNamespace(Python="python")
menuType = types.SimpleNamespace(Normal="normal")
parmLook = types.SimpleNamespace(ColorSquare="color")
parmNamingScheme = types.SimpleNamespace(RGBA="rgba")
folderType = types.SimpleNamespace(Tabs="tabs")
parmTemplateType = types.SimpleNamespace(Button="button")
exprLanguage = types.SimpleNamespace(Hscript="hscript")
hipFile = types.SimpleNamespace(save=lambda *a, **k: None, clear=lambda **k: None, path=lambda: "/mock.hip",
                                load=lambda *a, **k: None)
hda = types.SimpleNamespace(installFile=lambda p: None)
def applicationVersionString(): return "mock"
