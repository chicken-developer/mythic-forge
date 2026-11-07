"""Building generator: interface (PARMS) and facade module registry (MODULES).

To add a facade module (e.g. 'B' balcony):
  1. write vex/mod_b.vfl that builds ONE bay in module space (see references/module_library.md)
  2. add a ModuleDef to MODULES below and any new parms to PARMS (folder "Modules")
  3. rebuild the HDA (hb.py generator), then re-render every preset (hb.py regress) to catch regressions

Versioning: every parm added after 1.0 defaults to "off" / the old behaviour, so presets written for the
first version still produce the same building (the type stays ::1.0, see references/conventions.md).
"""
from ...parms import ParmSpec as P

TYPE_NAME = "bldg::building_generator::1.0"
LABEL = "Building Generator"

# material ids: one viewport colour (Look) + one Unreal material (Unreal) each
MATS = ("wall", "trim", "frame", "glass", "door", "roof", "stone", "timber", "fabric", "fabric2", "metal")
MAT_COLORS = ((0.78, 0.68, 0.55), (0.88, 0.85, 0.78), (0.30, 0.22, 0.16), (0.35, 0.50, 0.60),
              (0.45, 0.18, 0.12), (0.45, 0.20, 0.16), (0.55, 0.52, 0.47), (0.25, 0.15, 0.08),
              (0.75, 0.25, 0.18), (0.85, 0.80, 0.68), (0.20, 0.20, 0.22))


class ModuleDef:
    def __init__(self, token, label, vex, nominal, note):
        self.token, self.label, self.vex, self.nominal, self.note = token, label, vex, nominal, note


MODULES = [
    ModuleDef("W0", "Plain wall bay", "mod_w0.vfl", "floor_h", "solid wall, used to fill and on blind facades"),
    ModuleDef("W", "Window bay", "mod_w.vfl", "floor_h", "rect opening, frame, mullion, glass, sill, optional header"),
    ModuleDef("D", "Door bay", "mod_d.vfl", "ground_h", "door opening to the ground, frame, door slab"),
    ModuleDef("S", "Shopfront bay", "mod_s.vfl", "ground_h", "piers, bulkhead, large glazing, sign band"),
    ModuleDef("WA", "Arched window bay", "mod_wa.vfl", "floor_h", "W with a round / pointed arch head"),
    ModuleDef("DA", "Arched door bay", "mod_da.vfl", "ground_h", "door / portal with a round / pointed arch"),
    ModuleDef("WR", "Rose window bay", "mod_wr.vfl", "floor_h", "round window, frame ring, spokes"),
]

ROOF_TYPES = ("Flat + parapet", "Gable", "Hip")
SIDES = ("Front", "Right", "Back", "Left")
DEFAULT_BEVEL_PARTS = ("frame mullion sill header door bulkhead sign plinth band cornice pilaster coping "
                       "chimney chimney_cap timber_* verge ridge_beam buttress pinnacle tower_band "
                       "arch_frame rose_frame rose_spoke rose_hub beam post rail baluster bracket balcony_deck")


def _wing(s, on=0):
    f = "Wings"
    return [
        P(f, "wing_%s" % s, "Wing %s" % s.upper(), "toggle", on),
        P(f, "wing_%s_side" % s, "Wing %s Side" % s.upper(), "menu", 0, menu=SIDES,
          help="main-body edge it stands against (relative to the front edge)"),
        P(f, "wing_%s_w" % s, "Wing %s Width" % s.upper(), "float", 4.0, 1.0, 30.0, help="along that wall"),
        P(f, "wing_%s_d" % s, "Wing %s Depth" % s.upper(), "float", 3.0, 0.5, 30.0, help="out of that wall"),
        P(f, "wing_%s_off" % s, "Wing %s Offset" % s.upper(), "float", 0.0, -30.0, 30.0,
          help="shift of the wing centre along the wall"),
        P(f, "wing_%s_floors" % s, "Wing %s Storeys" % s.upper(), "int", 1, 1, 12, help="incl. the ground storey"),
        P(f, "wing_%s_ground" % s, "Wing %s Ground Pattern" % s.upper(), "string", "W D W",
          help="outer face, ground storey; side faces use Ground/Upper Side Pattern"),
        P(f, "wing_%s_upper" % s, "Wing %s Upper Pattern" % s.upper(), "string", "W"),
        P(f, "wing_%s_roof" % s, "Wing %s Roof" % s.upper(), "menu", 0,
          menu=("Cross gable", "Shed", "Gable parallel", "Flat")),
        P(f, "wing_%s_pitch" % s, "Wing %s Pitch" % s.upper(), "float", 40.0, 5.0, 70.0),
        P(f, "wing_%s_back" % s, "Wing %s Roof Back" % s.upper(), "float", 1.5, 0.0, 20.0,
          help="cross gable: how far the wing roof runs into the main roof"),
    ]


def _canopy(s):
    f = "Canopies"
    return [
        P(f, "canopy_%s" % s, "Canopy %s" % s.upper(), "toggle", 0),
        P(f, "canopy_%s_side" % s, "Canopy %s Side" % s.upper(), "menu", 0, menu=SIDES),
        P(f, "canopy_%s_kind" % s, "Canopy %s Kind" % s.upper(), "menu", 0,
          menu=("Roof slab", "Fabric awning", "Pergola")),
        P(f, "canopy_%s_w" % s, "Canopy %s Width" % s.upper(), "float", 3.0, 0.5, 30.0),
        P(f, "canopy_%s_off" % s, "Canopy %s Offset" % s.upper(), "float", 0.0, -30.0, 30.0),
        P(f, "canopy_%s_d" % s, "Canopy %s Depth" % s.upper(), "float", 1.5, 0.2, 10.0),
        P(f, "canopy_%s_h" % s, "Canopy %s Wall Height" % s.upper(), "float", 3.0, 1.5, 15.0),
        P(f, "canopy_%s_drop" % s, "Canopy %s Drop" % s.upper(), "float", 0.6, 0.0, 5.0),
        P(f, "canopy_%s_posts" % s, "Canopy %s Posts" % s.upper(), "toggle", 0),
    ]


def _tower(s):
    f = "Towers"
    return [
        P(f, "tower_%s" % s, "Tower %s" % s.upper(), "toggle", 0),
        P(f, "tower_%s_mode" % s, "Tower %s Placement" % s.upper(), "menu", 0,
          menu=("Single (u, v)", "Pair (+-u, v)", "Every corner")),
        P(f, "tower_%s_u" % s, "Tower %s U" % s.upper(), "float", 0.0, -1.0, 1.0,
          help="along the front edge, fraction of its length from the middle"),
        P(f, "tower_%s_v" % s, "Tower %s V" % s.upper(), "float", 0.0, -20.0, 20.0,
          help="metres out of the front wall (corner mode: out along the corner diagonal)"),
        P(f, "tower_%s_shape" % s, "Tower %s Shape" % s.upper(), "menu", 0, menu=("Square", "Polygon")),
        P(f, "tower_%s_sides" % s, "Tower %s Sides" % s.upper(), "int", 8, 5, 24),
        P(f, "tower_%s_w" % s, "Tower %s Width" % s.upper(), "float", 3.0, 0.5, 20.0),
        P(f, "tower_%s_h" % s, "Tower %s Shaft Height" % s.upper(), "float", 12.0, 1.0, 80.0),
        P(f, "tower_%s_levels" % s, "Tower %s Levels" % s.upper(), "int", 3, 1, 12),
        P(f, "tower_%s_win_w" % s, "Tower %s Window Width" % s.upper(), "float", 0.3, 0.0, 0.9, help="of the face"),
        P(f, "tower_%s_win_h" % s, "Tower %s Window Height" % s.upper(), "float", 0.5, 0.0, 0.9, help="of a level"),
        P(f, "tower_%s_cap" % s, "Tower %s Cap" % s.upper(), "menu", 1, menu=("Flat slab", "Pyramid / spire")),
        P(f, "tower_%s_cap_h" % s, "Tower %s Cap Height" % s.upper(), "float", 4.0, 0.0, 60.0),
        P(f, "tower_%s_cap_over" % s, "Tower %s Cap Overhang" % s.upper(), "float", 0.3, 0.0, 3.0),
        P(f, "tower_%s_pinnacles" % s, "Tower %s Corner Pinnacles" % s.upper(), "toggle", 0),
        P(f, "tower_%s_mat" % s, "Tower %s Material" % s.upper(), "menu", 1, menu=("Wall", "Stone")),
    ]


PARMS = [
    # ---------------- Presets
    P("Presets", "preset", "Preset", "string", "", menu_script="hou.phm().preset_menu(kwargs['node'])",
      help="Presets embedded in the HDA (section presets.json)"),
    P("Presets", "apply_preset", "Apply Preset", "button", None,
      callback="hou.phm().apply_preset(kwargs['node'])"),
    # ---------------- Output (LOW / HIGH)
    P("Output", "preview_quality", "Output 0 Quality", "menu", 0, menu=("LOW", "HIGH"),
      help="what output 0 / the display shows; output 1 is always LOW, output 2 always HIGH"),
    P("Output", "bevel_w", "HIGH Bevel Width", "float", 0.015, 0.0, 0.2,
      help="PolyBevel offset on hardsurface parts (group hs_bevel)"),
    P("Output", "bevel_div", "HIGH Bevel Divisions", "int", 2, 1, 8),
    P("Output", "bevel_ignore_flat", "HIGH Ignore Flat Edges", "toggle", 1),
    P("Output", "bevel_parts", "HIGH Bevel Parts", "string", DEFAULT_BEVEL_PARTS,
      help="s@part patterns that get bevelled; walls, roof planes, glass and fabric stay flat"),
    # ---------------- Footprint
    P("Footprint", "footprint_mode", "Footprint", "menu", 0, menu=("Rectangle", "Input curve"),
      help="Input curve: closed/open polyline on input 1 (e.g. a spline from Unreal)"),
    P("Footprint", "width", "Width (X)", "float", 10.0, 2.0, 60.0, help="rectangle width, metres"),
    P("Footprint", "depth", "Depth (Z)", "float", 8.0, 2.0, 60.0, help="rectangle depth, metres"),
    # ---------------- Floors
    P("Floors", "ground_h", "Ground Floor Height", "float", 3.6, 2.4, 12.0),
    P("Floors", "floor_n", "Upper Floors", "int", 2, 0, 12),
    P("Floors", "floor_h", "Upper Floor Height", "float", 3.0, 2.4, 12.0),
    P("Floors", "wall_t", "Wall Thickness", "float", 0.3, 0.1, 1.0, help="core is inset by this"),
    P("Floors", "jetty", "Upper Floor Jetty", "float", 0.0, 0.0, 1.5,
      help="upper storeys step out by this on every edge (medieval overhang); 0 = flush"),
    P("Floors", "ground_mat", "Ground Storey Material", "menu", 0, menu=("Wall", "Stone"),
      help="Stone: ground-storey walls use the stone material"),
    # ---------------- Facade
    P("Facade", "bay_w", "Bay Width", "float", 2.2, 0.8, 6.0, help="target width; bays are fitted per facade"),
    P("Facade", "front_edge", "Front Facade Edge", "int", 0, 0, 16, help="footprint edge index of the front"),
    P("Facade", "ground_front", "Ground Front Pattern", "string", "S D S",
      help="tokens per bay: W0 W D S WA DA WR; repeated to fill; exact if count matches"),
    P("Facade", "ground_side", "Ground Side Pattern", "string", "W0"),
    P("Facade", "upper_front", "Upper Front Pattern", "string", "W"),
    P("Facade", "upper_side", "Upper Side Pattern", "string", "W W0"),
    P("Facade", "center_door", "Door On Centre Bay", "toggle", 1,
      help="if the ground pattern has one D and fewer tokens than bays, put D on the centre bay"),
    # ---------------- Modules
    P("Modules", "win_w", "Window Width (of bay)", "float", 0.55, 0.1, 0.95),
    P("Modules", "win_h", "Window Height (of floor)", "float", 0.55, 0.1, 0.9),
    P("Modules", "sill_h", "Sill Height", "float", 0.9, 0.0, 2.0),
    P("Modules", "frame_w", "Frame Width", "float", 0.08, 0.02, 0.3),
    P("Modules", "frame_d", "Frame Depth (out)", "float", 0.06, 0.0, 0.3),
    P("Modules", "sill_out", "Sill Overhang", "float", 0.1, 0.0, 0.4),
    P("Modules", "win_header", "Window/Door Header", "toggle", 1),
    P("Modules", "door_w", "Door Width", "float", 1.2, 0.6, 4.0),
    P("Modules", "door_h", "Door Height", "float", 2.4, 1.8, 7.0, help="arched door: floor to apex"),
    P("Modules", "shop_pier", "Shop Pier Width", "float", 0.2, 0.05, 1.0),
    P("Modules", "shop_bulkhead", "Shop Bulkhead Height", "float", 0.6, 0.0, 1.5),
    P("Modules", "shop_sign_h", "Shop Sign Band", "float", 0.7, 0.0, 2.0),
    P("Modules", "arch_shape", "Arch Shape (WA/DA)", "menu", 0, menu=("Round", "Pointed")),
    P("Modules", "arch_seg", "Arch Segments", "int", 8, 2, 32),
    P("Modules", "rose_r", "Rose Window Radius", "float", 1.2, 0.3, 6.0),
    P("Modules", "rose_y", "Rose Window Centre (of floor)", "float", 0.55, 0.2, 0.8),
    P("Modules", "rose_spokes", "Rose Window Spokes", "int", 12, 0, 32),
    # ---------------- Trims
    P("Trims", "plinth_h", "Plinth Height", "float", 0.5, 0.0, 2.0, help="0 disables"),
    P("Trims", "plinth_out", "Plinth Out", "float", 0.08, 0.0, 0.5),
    P("Trims", "plinth_mat", "Plinth Material", "menu", 0, menu=("Trim", "Stone")),
    P("Trims", "band_h", "Floor Band Height", "float", 0.22, 0.0, 1.0),
    P("Trims", "band_out", "Floor Band Out", "float", 0.08, 0.0, 0.5),
    P("Trims", "cornice_h", "Cornice Height", "float", 0.45, 0.0, 2.0),
    P("Trims", "cornice_out", "Cornice Out", "float", 0.3, 0.0, 1.0),
    P("Trims", "pilaster_w", "Corner Pilaster Width", "float", 0.45, 0.0, 2.0, help="0 disables; bays start after it"),
    P("Trims", "pilaster_out", "Corner Pilaster Out", "float", 0.08, 0.0, 0.5),
    P("Trims", "pilaster_mat", "Pilaster Material", "menu", 0, menu=("Trim", "Stone"), help="Stone = quoins"),
    # ---------------- Timber
    P("Timber", "timber", "Half-Timber Overlay", "toggle", 0),
    P("Timber", "timber_w", "Member Width", "float", 0.18, 0.05, 0.5),
    P("Timber", "timber_d", "Member Depth (out)", "float", 0.06, 0.01, 0.3),
    P("Timber", "timber_from", "From Storey", "int", 0, 0, 12, help="0 = ground storey, 1 = upper storeys only"),
    P("Timber", "timber_brace", "Braces In Plain Bays", "menu", 1, menu=("None", "Diagonal", "Cross", "Chevron")),
    P("Timber", "timber_rail", "Sill Rail On Window Bays", "toggle", 1),
    P("Timber", "timber_gable", "Gable Trusses", "toggle", 1),
    P("Timber", "timber_collar", "Collar Height (of gable)", "float", 0.5, 0.2, 0.8),
    # ---------------- Buttress
    P("Buttress", "buttress", "Buttresses", "toggle", 0),
    P("Buttress", "buttress_edges", "On Edges", "menu", 0, menu=("Side walls", "All walls")),
    P("Buttress", "buttress_wings", "Also On Wings", "toggle", 0),
    P("Buttress", "buttress_w", "Width", "float", 0.6, 0.1, 3.0),
    P("Buttress", "buttress_d", "Depth", "float", 0.9, 0.1, 4.0),
    P("Buttress", "buttress_hr", "Height (of wall)", "float", 0.85, 0.1, 1.2),
    P("Buttress", "buttress_steps", "Steps", "int", 2, 1, 5),
    P("Buttress", "buttress_taper", "Step Taper", "float", 0.4, 0.0, 0.9),
    P("Buttress", "buttress_mat", "Material", "menu", 1, menu=("Wall", "Stone", "Trim")),
    P("Buttress", "pinnacle", "Pinnacles", "toggle", 0),
    P("Buttress", "pinnacle_h", "Pinnacle Height", "float", 1.5, 0.0, 6.0),
    # ---------------- Roof
    P("Roof", "roof_type", "Roof Type", "menu", 1, menu=ROOF_TYPES,
      help="gable/hip use the oriented bounding rectangle of the footprint (edge 0)"),
    P("Roof", "roof_axis", "Ridge Axis", "menu", 0, menu=("Auto (longer side)", "Along edge 0", "Across edge 0")),
    P("Roof", "roof_pitch", "Pitch (deg)", "float", 38.0, 5.0, 70.0),
    P("Roof", "roof_overhang", "Overhang", "float", 0.4, 0.0, 2.0),
    P("Roof", "roof_t", "Roof Slab Thickness", "float", 0.0, 0.0, 0.6, help="0 = thin double-sided plane"),
    P("Roof", "verge_w", "Verge Board Width", "float", 0.0, 0.0, 0.8, help="timber boards on the gable rakes; 0 = none"),
    P("Roof", "verge_t", "Verge Board Thickness", "float", 0.06, 0.01, 0.3),
    P("Roof", "ridge_w", "Ridge Beam Size", "float", 0.0, 0.0, 0.6, help="0 = none"),
    P("Roof", "parapet_h", "Parapet Height (flat)", "float", 0.8, 0.0, 3.0),
    P("Roof", "chimney", "Chimney", "toggle", 1),
    P("Roof", "chimney_n", "Chimney Count", "int", 1, 1, 2, help="2 = second one mirrored along the ridge"),
    P("Roof", "chimney_u", "Chimney Along Ridge", "float", 0.75, 0.0, 1.0),
    P("Roof", "chimney_v", "Chimney Across (of span)", "float", 0.18, -0.5, 0.5),
    P("Roof", "chimney_size", "Chimney Size", "float", 0.8, 0.2, 3.0),
    P("Roof", "chimney_h", "Chimney Above Ridge", "float", 0.8, 0.0, 5.0),
    P("Roof", "chimney_mat", "Chimney Material", "menu", 0, menu=("Trim", "Stone")),
    P("Roof", "chimney_cap", "Chimney Cap", "toggle", 0),
    P("Roof", "chimney_cap_h", "Chimney Cap Height", "float", 0.15, 0.02, 1.0),
    # ---------------- Dormers
    P("Dormers", "dormer_n", "Dormers", "int", 0, 0, 8),
    P("Dormers", "dormer_side", "On Slope", "menu", 0, menu=("Front", "Back", "Both")),
    P("Dormers", "dormer_w", "Width", "float", 1.4, 0.5, 5.0),
    P("Dormers", "dormer_h", "Front Wall Height", "float", 1.2, 0.4, 4.0),
    P("Dormers", "dormer_pitch", "Pitch", "float", 45.0, 15.0, 70.0),
    P("Dormers", "dormer_pos", "Front Wall Position", "float", 0.6, 0.1, 0.95,
      help="fraction of the half span, ridge (0) -> eave line (1)"),
    P("Dormers", "dormer_off", "Offset Along Ridge", "float", 0.0, -20.0, 20.0),
    P("Dormers", "dormer_ov", "Dormer Overhang", "float", 0.15, 0.0, 1.0),
] + _wing("a") + _wing("b") + [
    # ---------------- Canopies (porch roofs, awnings, pergolas) / balcony
    P("Canopies", "canopy_t", "Slab Thickness", "float", 0.08, 0.0, 0.5),
    P("Canopies", "stripe_w", "Awning Stripe Width", "float", 0.5, 0.1, 6.0, help="a stripe wider than the awning = plain fabric"),
    P("Canopies", "valance_h", "Awning Valance", "float", 0.25, 0.0, 1.0),
    P("Canopies", "post_w", "Post / Beam Size", "float", 0.18, 0.05, 0.6),
    P("Canopies", "post_gap", "Max Post Spacing", "float", 2.5, 0.5, 10.0),
] + _canopy("a") + _canopy("b") + _canopy("c") + [
    P("Balcony", "balcony", "Balcony", "toggle", 0),
    P("Balcony", "balcony_side", "Side", "menu", 0, menu=SIDES),
    P("Balcony", "balcony_floor", "Storey", "int", 1, 1, 12),
    P("Balcony", "balcony_w", "Width", "float", 3.0, 0.5, 30.0),
    P("Balcony", "balcony_off", "Offset", "float", 0.0, -30.0, 30.0),
    P("Balcony", "balcony_d", "Depth", "float", 0.9, 0.2, 4.0),
    P("Balcony", "balcony_t", "Deck Thickness", "float", 0.15, 0.03, 0.6),
    P("Balcony", "balcony_rail_h", "Railing Height", "float", 1.0, 0.3, 2.0),
    P("Balcony", "balcony_rail_w", "Rail Size", "float", 0.08, 0.02, 0.3),
    P("Balcony", "balcony_gap", "Baluster Spacing", "float", 0.18, 0.05, 1.0),
] + _tower("a") + _tower("b") + [
    # ---------------- Look (viewport Cd per material id, used to compare against the concept)
] + [P("Look", "col_" + m, m.title() + " Colour", "color", c) for m, c in zip(MATS, MAT_COLORS)] + [
    P("Look", "uv_tile", "UV Tile Size (m)", "float", 2.0, 0.1, 20.0, help="world-space box UVs: metres per 0-1 UV"),
    # ---------------- Unreal (Houdini Engine)
] + [P("Unreal", "ue_" + m, m.title() + " Material", "string", "/Game/Env/Materials/MI_%s.MI_%s" % (m.title(), m.title()),
       help="written to prim attribute unreal_material") for m in MATS]
