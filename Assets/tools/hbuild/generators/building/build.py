"""Build the Building Generator HDA.

Network inside the HDA (each line = one network box; read top to bottom, left to right):

  row 0  FOOTPRINT  rect / HDA input 1 -> normalize (prim 0 ground outline, prim 1 jettied upper outline)
         WING_PLAN  OUT_FOOTPRINT -> wing footprints (prims with their own patterns / roof settings)
         LAYOUT     OUT_FOOTPRINT + OUT_WING_PLAN -> one point per facade bay (s@module, i@floor ...)
         MOD_<T>    one bay in module space, copied onto the LAYOUT points whose s@module == T
  row 1  TRIMS CORE ROOF WINGS DORMERS CANOPY BALCONY TOWER TIMBER BUTTRESS   (each -> OUT_<NAME>)
  row 2  OUTPUT     merge(all OUT_*) -> ground-storey material -> Cd + unreal_material -> UV -> normals = LOW
         HIGH       OUT_OUTPUT -> hardsurface group -> PolyBevel -> normals
         PUBLISH    switch(LOW, HIGH by preview_quality) -> outputs 0 selected / 1 LOW / 2 HIGH

Every value comes from the HDA interface (spec.PARMS) through ch("../name").
"""
import os
from .spec import TYPE_NAME, LABEL, PARMS, MODULES
from ... import hda as H
from ...net import NetBuilder

VEX_DIR = os.path.join(os.path.dirname(__file__), "vex")


def vex(name, helpers=True):
    with open(os.path.join(VEX_DIR, name), "r", encoding="utf-8") as f:
        body = f.read()
    if not helpers:
        return body
    with open(os.path.join(VEX_DIR, "_helpers.vfl"), "r", encoding="utf-8") as f:
        return f.read() + "\n" + body


README = """BUILDING GENERATOR  (%s)
Modules read only OUT_* nulls and HDA parms (ch("../name")). Data flows top -> bottom.
FOOTPRINT: prim 0 = ground outline, prim 1 = upper outline (jetty). WING_PLAN: one polygon per wing.
LAYOUT: one point per bay (main + wings): s@module W0 W D S WA DA WR, i@floor i@edge i@bay i@nbays i@poly
  f@bay_width, orient, scale. Each MOD_<T> builds ONE bay in module space and is copied onto its points.
Row 2 modules add volumes, roofs, overlays (timber, buttress) and attachments (canopy, balcony, tower).
Output prims: s@mat_id s@part v@Cd s@unreal_material, vertex uv + N. Outputs: 0 selected, 1 LOW, 2 HIGH.
HIGH = LOW + PolyBevel on parts listed in 'HIGH Bevel Parts' (walls / roof planes stay flat for Substance).
Source of truth: <project>/tools/hbuild/generators/building (do not hand-edit VEX only here).""" % TYPE_NAME

ROW1 = (  # name, family, vex, note, inputs (OUT names of other modules, wired to wrangle inputs 1..3)
    ("TRIMS", "trim", "trims.vfl", "plinth, bands, cornice, pilasters", ("FOOTPRINT",)),
    ("CORE", "core", "core.vfl", "inset closed volumes (ground + upper)", ("FOOTPRINT",)),
    ("ROOF", "roof", "roof.vfl", "flat/gable/hip, verge, ridge, chimneys", ("FOOTPRINT",)),
    ("WINGS", "core", "wings.vfl", "wing volumes + roofs", ("WING_PLAN",)),
    ("DORMERS", "roof", "dormers.vfl", "gabled dormers on the main roof", ("FOOTPRINT",)),
    ("CANOPY", "module", "canopy.vfl", "porch roofs, fabric awnings, pergolas", ("FOOTPRINT",)),
    ("BALCONY", "module", "balcony.vfl", "balcony deck + railing", ("FOOTPRINT",)),
    ("TOWER", "core", "tower.vfl", "towers / turrets / flues with caps", ("FOOTPRINT",)),
    ("TIMBER", "trim", "timber.vfl", "half-timber overlay + gable trusses", ("LAYOUT", "ROOF", "WINGS")),
    ("BUTTRESS", "trim", "buttress.vfl", "stepped buttresses + pinnacles", ("LAYOUT", "FOOTPRINT")),
)


def build_network(parent):
    """Populate `parent` (a SOP subnet) with the generator network. Returns the OUT node."""
    b = NetBuilder(parent)
    b.sticky(README, (-2.0, 5.5), (22.0, 4.6))
    hda_in = parent.indirectInputs()[0]
    outs = {}

    # FOOTPRINT ---------------------------------------------------------------
    m = b.module("FOOTPRINT", "input", 0, 0, "rect or input curve -> ground + upper outline")
    rect = m.wrangle("rect", 0, 0, vex("footprint_rect.vfl", False))
    norm = m.wrangle("normalize", 0, 1, vex("footprint_normalize.vfl", False),
                     inputs=[None, rect, hda_in], color_role="filter")
    outs["FOOTPRINT"] = m.finish(norm, col=0, row=2)

    # WING_PLAN ----------------------------------------------------------------
    m = b.module("WING_PLAN", "layout", 2.4, 0, "wing footprints from the wing slots")
    wp = m.wrangle("geo", 0, 0, vex("wing_plan.vfl", False), inputs=[None, outs["FOOTPRINT"]], color_role="filter")
    outs["WING_PLAN"] = m.finish(wp, col=0, row=1)

    # LAYOUT -----------------------------------------------------------------
    m = b.module("LAYOUT", "layout", 4.8, 0, "one point per facade bay (main + wings)")
    lay = m.wrangle("points", 0, 0, vex("layout.vfl", False),
                    inputs=[None, outs["FOOTPRINT"], outs["WING_PLAN"]], color_role="filter")
    outs["LAYOUT"] = m.finish(lay, col=0, row=1)

    # MODULES ----------------------------------------------------------------
    merge_list = []
    for i, md in enumerate(MODULES):
        name = "MOD_" + md.token
        m = b.module(name, "module", 7.2 + i * 2.4, 0, md.label)
        geo = m.wrangle("geo", 0, 0, vex(md.vex))
        pts = m.wrangle("pts", 0.5, 1,
                        '// keep only the bays that use module "%s"\nif (s@module != "%s") removepoint(0, @ptnum);\n'
                        % (md.token, md.token), run_over="points", inputs=[outs["LAYOUT"]], color_role="filter")
        cp = m.node("copytopoints::2.0", "copy", 0, 2, inputs=[geo, pts], color_role="copy")
        merge_list.append(m.finish(cp, col=0, row=3))

    # ROW 1 ------------------------------------------------------------------
    for j, (name, fam, file, note, ins) in enumerate(ROW1):
        m = b.module(name, fam, j * 2.4, 7, note)
        g = m.wrangle("geo", 0, 0, vex(file), inputs=[None] + [outs[k] for k in ins])
        outs[name] = m.finish(g, col=0, row=1)
        merge_list.append(outs[name])

    # OUTPUT (LOW) -------------------------------------------------------------
    m = b.module("OUTPUT", "output", 0, 11, "LOW: material by storey, Cd + unreal_material, UV, normals")
    mg = m.node("merge", "merge", 0, 0, inputs=merge_list, color_role="merge")
    fm = m.wrangle("floor_mat", 0, 1, vex("floor_mat.vfl", False), run_over="primitives", inputs=[mg],
                   color_role="attrib")
    look = m.wrangle("look", 0, 2, vex("look.vfl", False), run_over="primitives", inputs=[fm], color_role="attrib")
    uv = m.wrangle("uv", 0, 3, vex("uv.vfl", False), run_over="vertices", inputs=[look], color_role="attrib")
    nrm = m.node("normal", "normals", 0, 4, inputs=[uv], color_role="attrib")
    low = m.finish(nrm, col=0, row=5)

    # HIGH ---------------------------------------------------------------------
    m = b.module("HIGH", "output", 2.4, 11, "LOW + PolyBevel on hardsurface parts")
    grp = m.wrangle("group", 0, 0, vex("high_group.vfl", False), run_over="primitives", inputs=[low],
                    color_role="attrib")
    bev = m.node("polybevel::3.0", "bevel", 0, 1, inputs=[grp], color_role="generate")
    bev.parm("group").set("hs_bevel")
    bev.parm("offset").setExpression('ch("../bevel_w")')
    bev.parm("divisions").setExpression('ch("../bevel_div")')
    bev.parm("ignoreflatedges").setExpression('ch("../bevel_ignore_flat")')
    hn = m.node("normal", "normals", 0, 2, inputs=[bev], color_role="attrib")
    high = m.finish(hn, col=0, row=3)

    # PUBLISH ------------------------------------------------------------------
    m = b.module("PUBLISH", "output", 4.8, 11, "outputs: 0 selected (preview_quality), 1 LOW, 2 HIGH")
    sw = m.node("switch", "select", 0, 0, inputs=[low, high], color_role="merge")
    sw.parm("input").setExpression('ch("../preview_quality")')
    for k, (role, src) in enumerate((("output_selected", sw), ("output_low", low), ("output_high", high))):
        o = m.node("output", role, k, 2, inputs=[src], color_role="out")
        o.parm("outputidx").set(k)
    pub = m.finish(sw, col=0, row=1)

    out = parent.createNode("null", "OUT")
    ox, oy = pub.position()
    out.setPosition((ox, oy - 2.6))
    out.setInput(0, pub)
    out.setDisplayFlag(True)
    out.setRenderFlag(True)
    return out


def build(hda_path, presets=None, container="/obj/_hbuild_generator"):
    """Create the subnet, populate it, convert to HDA at hda_path. Returns the HDA node."""
    import hou
    obj = hou.node("/obj")
    name = container.rsplit("/", 1)[-1]
    if obj.node(name):
        obj.node(name).destroy()
    geo = obj.createNode("geo", name)
    for c in geo.children():
        c.destroy()
    sub = geo.createNode("subnet", "building_generator")
    build_network(sub)
    help_text = ("= Building Generator =\n\nSee the README sticky note inside, docs/ in the project and the hbuild "
                 "skill docs. Outputs: 0 selected (Output 0 Quality), 1 LOW, 2 HIGH.")
    node = H.make_hda(sub, TYPE_NAME, LABEL, hda_path, PARMS, presets=presets, max_inputs=1, help_text=help_text,
                      max_outputs=3)
    node.setDisplayFlag(True)
    return node
