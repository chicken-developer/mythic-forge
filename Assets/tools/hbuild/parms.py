"""Parameter registry: a list of ParmSpec is the single source of truth for
the HDA interface, preset validation and documentation (see hb.py docs)."""


class ParmSpec:
    def __init__(self, folder, name, label, kind, default, lo=None, hi=None, menu=None, help="",
                 callback=None, menu_script=None):
        self.folder, self.name, self.label, self.kind = folder, name, label, kind
        self.default, self.lo, self.hi, self.menu, self.help = default, lo, hi, menu, help
        self.callback, self.menu_script = callback, menu_script

    def to_dict(self):
        return dict(folder=self.folder, name=self.name, label=self.label, kind=self.kind,
                    default=self.default, lo=self.lo, hi=self.hi, menu=self.menu, help=self.help)


def _template(hou, p):
    if p.kind == "float":
        t = hou.FloatParmTemplate(p.name, p.label, 1, default_value=(float(p.default),),
                                  min=p.lo if p.lo is not None else 0.0, max=p.hi if p.hi is not None else 10.0,
                                  min_is_strict=p.lo is not None and p.lo >= 0)
    elif p.kind == "int":
        t = hou.IntParmTemplate(p.name, p.label, 1, default_value=(int(p.default),),
                                min=p.lo if p.lo is not None else 0, max=p.hi if p.hi is not None else 10)
    elif p.kind == "toggle":
        t = hou.ToggleParmTemplate(p.name, p.label, default_value=bool(p.default))
    elif p.kind == "menu":
        tokens = [str(i) for i in range(len(p.menu))]
        t = hou.MenuParmTemplate(p.name, p.label, tokens, menu_labels=list(p.menu), default_value=int(p.default))
    elif p.kind == "string":
        kw = {}
        if p.menu_script:
            kw = dict(item_generator_script=p.menu_script,
                      item_generator_script_language=hou.scriptLanguage.Python,
                      menu_type=hou.menuType.Normal)
        t = hou.StringParmTemplate(p.name, p.label, 1, default_value=(str(p.default),), **kw)
    elif p.kind == "color":
        t = hou.FloatParmTemplate(p.name, p.label, 3, default_value=tuple(p.default),
                                  look=hou.parmLook.ColorSquare, naming_scheme=hou.parmNamingScheme.RGBA)
    elif p.kind == "button":
        t = hou.ButtonParmTemplate(p.name, p.label, script_callback=p.callback or "",
                                   script_callback_language=hou.scriptLanguage.Python)
    else:
        raise ValueError("unknown parm kind %s" % p.kind)
    if p.help:
        t.setHelp(p.help)
    return t


def build_template_group(specs):
    """Folders appear in the order of their first parm."""
    import hou
    ptg = hou.ParmTemplateGroup()
    folders, order = {}, []
    for p in specs:
        if p.folder not in folders:
            folders[p.folder] = []
            order.append(p.folder)
        folders[p.folder].append(_template(hou, p))
    for f in order:
        ptg.append(hou.FolderParmTemplate("fld_" + f.lower().replace(" ", "_"), f, parm_templates=folders[f],
                                          folder_type=hou.folderType.Tabs))
    return ptg


def preset_keys(specs):
    """Parameters a preset may set (buttons and the preset selector itself are excluded)."""
    return [p.name for p in specs if p.kind != "button" and p.folder != "Presets"]


def docs_markdown(specs):
    lines, folder = [], None
    for p in specs:
        if p.folder != folder:
            folder = p.folder
            lines += ["", "### " + folder, "", "| parm | type | default | meaning |", "|---|---|---|---|"]
        d = p.default if p.kind != "menu" else "%s (%s)" % (p.default, p.menu[int(p.default)])
        lines.append("| `%s` | %s | %s | %s |" % (p.name, p.kind, d, p.help))
    return "\n".join(lines)
