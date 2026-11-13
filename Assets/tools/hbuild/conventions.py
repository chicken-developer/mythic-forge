"""House style for every network hbuild creates. Change it here, nowhere else.

Naming
    MODULE boxes  : UPPER_CASE comment, e.g. FOOTPRINT, MOD_W, ROOF
    node names    : <module>_<role>  in lower case, e.g. roof_geo, mod_w_copy
    module output : OUT_<MODULE>  (null)  - the only node other modules may read from
    final output  : OUT  (null, display + render flag)
    no default names (box1, attribwrangle3): validate.py reports them.

Data flow rule
    A module reads only from OUT_* nulls of other modules and from the HDA parameters
    (ch("../name") inside the HDA). Never wire into the middle of another module.

Hard-coded values
    Every number that shapes the asset is an HDA parameter. Literal numbers inside VEX are allowed
    only for tiny structural tolerances (1e-4, 0.5 for halving ...), and must be commented.
"""

GRID_X = 2.6        # horizontal spacing between node columns (network units)
GRID_Y = 1.15       # vertical spacing between node rows
BOX_GAP_X = 1.8     # gap between neighbouring module boxes
BOX_GAP_Y = 2.2

# node colour per role (RGB 0-1) - matches the legend of the network diagram
ROLE_COLORS = {
    "input":    (0.55, 0.55, 0.55),
    "generate": (0.62, 0.43, 0.82),   # VEX wrangles that create geometry
    "filter":   (0.29, 0.64, 0.64),   # wrangles that select/prepare points
    "copy":     (0.29, 0.64, 0.64),
    "attrib":   (0.85, 0.57, 0.23),   # attribute / material / uv
    "merge":    (0.42, 0.66, 0.42),
    "out":      (0.15, 0.15, 0.15),
}

# network box colour per module family
BOX_COLORS = {
    "input":   (0.45, 0.45, 0.55),
    "layout":  (0.50, 0.40, 0.65),
    "module":  (0.55, 0.45, 0.35),
    "trim":    (0.60, 0.55, 0.35),
    "core":    (0.45, 0.45, 0.45),
    "roof":    (0.60, 0.35, 0.30),
    "output":  (0.30, 0.50, 0.75),
}


def node_name(module, role):
    return ("%s_%s" % (module, role)).lower()


def out_name(module):
    return "OUT_%s" % module.upper()
