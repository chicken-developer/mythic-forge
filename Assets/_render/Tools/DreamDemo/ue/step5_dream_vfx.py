"""Step 5: dream VFX without Niagara: star fish, memory bubbles, cosmic dust, floating
props, ink darkness and lantern star motes. Everything moves by world position offset.
The instanced fields are BP_DreamInstancedField actors; this script places their instances.

Run:  bash Tools/DreamDemo/ue_run.sh cmd Tools/DreamDemo/ue/step5_dream_vfx.py dd_step5
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                "Tools", "DreamDemo", "ue"))
import dd_common as dd  # noqa: E402

STEP = "DreamDemo.Step5"
REPORT = {"actors": [], "notes": [], "materials": {}}
mel = dd.mel
CM = unreal.CustomMaterialOutputType

# ---- tunables ----------------------------------------------------------------
FISH_SCHOOLS = [   # radius range (cm), height range (cm), arc start (deg), orbit speed (rad/s)
    dict(radius=(950.0, 1250.0), height=(900.0, 1100.0), arc_start=10.0, speed=0.085, seed=11),
    dict(radius=(1500.0, 1900.0), height=(1200.0, 1500.0), arc_start=135.0, speed=0.06, seed=23),
    dict(radius=(650.0, 850.0), height=(800.0, 950.0), arc_start=250.0, speed=0.11, seed=37),
]
FISH_PER_SCHOOL = 8
BUBBLE_COUNT = 10
BUBBLE_RISE_HEIGHT = 700.0
DUST_COUNT = 420
INK_ANGLE_DEG = 45.0          # from the view direction, toward the right of CAM_Wide
INK_DISTANCE = 0.78            # of the arena radius
INK_RADIUS = 380.0
# Floating props: light market things. Searched by name in these content folders; the pack
# materials are copied to /Game/DreamDemo/Materials/Float and the copies get the float offset.
FLOAT_SEARCH_ROOTS = ["/Game/Stylized_Village"]
FLOAT_NAME_PATTERN = r"barrel|crate|basket|bucket|wheel|stool|chair|sack|pot|jug|box|bench|vase|keg"
FLOAT_NAME_EXCLUDE = r"lantern|lamp|candle|torch|fire"      # nothing warm may float in the dream
FLOAT_MAX_SIZE_CM = 220.0
FLOAT_MAX_KINDS = 6
FLOAT_FALLBACK = ["/Game/Stylized_Forest/Meshes/stones/SM_small_stone_01",
                  "/Game/Stylized_Forest/Meshes/stones/SM_small_stone_03",
                  "/Game/Stylized_Forest/Meshes/trees/SM_small_branch_02"]
LANTERN_ANGLES = [35.0, 78.0, 160.0, 215.0, 275.0, 325.0]
# -------------------------------------------------------------------------------


def enum_value(enum_cls, *candidates):
    for name in candidates:
        if hasattr(enum_cls, name):
            return getattr(enum_cls, name)
    raise RuntimeError("%s has none of %s; available: %s" % (
        enum_cls.__name__, candidates, [n for n in dir(enum_cls) if n.isupper()]))


def record(actor, note=""):
    loc = actor.get_actor_location()
    REPORT["actors"].append({"label": actor.get_actor_label(), "class": actor.get_class().get_name(),
                             "loc": [round(loc.x), round(loc.y), round(loc.z)],
                             "tags": [str(t) for t in actor.tags], "note": note})
    return actor


# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------

FISH_WPO = r"""
float3 rel = P - C;
float2 radial = normalize(rel.xy + float2(1e-3, 0.0));
float phase = Rand * 6.2831853;
// swim: sine wave along the body (UV0.x: 0 nose .. 1 tail), growing toward the tail
float wave = sin(UV.x * WaveFreq - T * WaveSpeed + phase) * WaveAmp * pow(saturate(UV.x), 1.5);
rel.xy += radial * wave;
rel.z += sin(T * 0.4 + phase) * BobAmp;
rel.xy *= 1.0 + 0.04 * sin(T * 0.23 + phase * 1.7);
// school: rigid rotation around the actor, so the fish keep heading along the tangent
float a = T * OrbitSpeed;
float sn = sin(a); float cs = cos(a);
float3 moved = float3(rel.x * cs - rel.y * sn, rel.x * sn + rel.y * cs, rel.z);
return moved - (P - C);
"""

FISH_EMISSIVE = r"""
return Color.rgb * (VC.r * Glow + Body + F * 0.35 * Glow) * Blend;
"""


def build_fish_material():
    g = dd.new_material("M_StarFish", shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True,
                        used_with_instanced_static_meshes=True, blend_mode=unreal.BlendMode.BLEND_MASKED)
    wpo = g.custom(FISH_WPO, {
        "P": g.node(unreal.MaterialExpressionWorldPosition),
        "C": g.node(unreal.MaterialExpressionActorPositionWS),
        "UV": g.node(unreal.MaterialExpressionTextureCoordinate),
        "Rand": g.node(unreal.MaterialExpressionPerInstanceRandom),
        "T": g.time(),
        "OrbitSpeed": g.scalar("OrbitSpeed", 0.08),
        "WaveAmp": g.scalar("WaveAmplitude", 7.0),
        "WaveFreq": g.scalar("WaveFrequency", 7.0),
        "WaveSpeed": g.scalar("WaveSpeed", 4.5),
        "BobAmp": g.scalar("BobAmplitude", 50.0),
    }, CM.CMOT_FLOAT3, "FishSwimOrbit")
    g.output(wpo, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    emissive = g.custom(FISH_EMISSIVE, {
        "Color": g.vector("Color", dd.linear_from_hex("#7FD6D0")),
        "VC": g.node(unreal.MaterialExpressionVertexColor),
        "F": g.node(unreal.MaterialExpressionFresnel, exponent=2.5, base_reflect_fraction=0.02),
        "Glow": g.scalar("FinGlow", 1.6),
        "Body": g.scalar("BodyGlow", 0.22),
        "Blend": g.collection("WorldBlend"),
    }, CM.CMOT_FLOAT3, "FishGlow")
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # reduced motion hides the fish: the Blueprint sets MotionReduced, the mask clips them away
    mask = g.custom("return 1.0 - saturate(Reduced);", {"Reduced": g.collection("MotionReduced")},
                    CM.CMOT_FLOAT1, "ReducedMotionMask")
    g.output(mask, unreal.MaterialProperty.MP_OPACITY_MASK)
    g.finish()
    instances = []
    for index, school in enumerate(FISH_SCHOOLS):
        instances.append(dd.new_instance("MI_StarFish_%d" % index, g.material,
                                         scalars={"OrbitSpeed": school["speed"]}))
    return instances


RISE_WPO = r"""
float ph = frac(T * Speed / max(RiseHeight, 1.0) * (0.7 + 0.6 * Rand) + Rand * 7.31);
float z = (ph - 0.5) * RiseHeight;
float sx = sin(T * 0.31 + Rand * 40.0) * Sway + sin(T * 0.13 + Rand * 12.0) * Sway * 0.6;
float sy = cos(T * 0.27 + Rand * 23.0) * Sway;
return float3(sx, sy, z);
"""

RISE_FADE = r"""
float ph = frac(T * Speed / max(RiseHeight, 1.0) * (0.7 + 0.6 * Rand) + Rand * 7.31);
return smoothstep(0.0, 0.14, ph) * (1.0 - smoothstep(0.86, 1.0, ph));
"""

BUBBLE_MEMORY_UV = r"""
// lens-like projection that always faces the camera
float2 uv = float2(Nv.x, -Nv.y) * 0.5 / max(MemScale, 0.05) + 0.5;
return float3(uv, floor(Rand * 3.999));
"""

BUBBLE_FILM_UV = r"""
return float2(saturate(F * 0.9 + (Noise - 0.5) * 0.35 + 0.05), 0.5);
"""

BUBBLE_EMISSIVE = r"""
float inside = 1.0 - saturate(length(Nv.xy) / max(MemScale, 0.05));
float3 mem = Mem.rgb * smoothstep(0.0, 0.25, inside);
return (Film.rgb * F * RimGlow + mem * MemGlow) * Fade * Blend;
"""

BUBBLE_OPACITY = r"""
float o = lerp(Core, Rim, saturate(F)) + Mem.a * 0.25;
return saturate(o) * Fade * (1.0 - Dark) * Blend * saturate(Depth / 150.0);
"""


def build_bubble_material():
    tlm = enum_value(unreal.TranslucencyLightingMode, "TLM_SURFACE")
    g = dd.new_material("M_MemoryBubble", blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT,
                        translucency_lighting_mode=tlm, used_with_instanced_static_meshes=True)
    rand = g.node(unreal.MaterialExpressionPerInstanceRandom)
    t = g.time()
    speed = g.scalar("RiseSpeed", 9.0)
    height = g.scalar("RiseHeight", BUBBLE_RISE_HEIGHT)
    wpo = g.custom(RISE_WPO, {"T": t, "Rand": rand, "Speed": speed, "RiseHeight": height,
                              "Sway": g.scalar("Sway", 18.0)}, CM.CMOT_FLOAT3, "BubbleRise")
    g.output(wpo, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    fade = g.custom(RISE_FADE, {"T": t, "Rand": rand, "Speed": speed, "RiseHeight": height},
                    CM.CMOT_FLOAT1, "BubbleLoopFade")

    fresnel = g.node(unreal.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.03)
    normal_ws = g.node(unreal.MaterialExpressionVertexNormalWS)
    to_view = g.node(unreal.MaterialExpressionTransform,
                     transform_source_type=enum_value(unreal.MaterialVectorCoordTransformSource,
                                                      "TRANSFORMSOURCE_WORLD"),
                     transform_type=enum_value(unreal.MaterialVectorCoordTransform, "TRANSFORM_VIEW"))
    g.connect(normal_ws, to_view, "")
    mem_scale = g.scalar("MemoryScale", 0.62)

    memory_uv = g.custom(BUBBLE_MEMORY_UV, {"Nv": to_view, "Rand": rand, "MemScale": mem_scale},
                         CM.CMOT_FLOAT3, "MemoryUV")
    memory = g.texture_sample(dd.load(dd.ROOT + "/Textures/T_MemoryArray"), memory_uv)

    pan = g.custom("return Nv.xy * 0.35 + float2(T * 0.021, T * 0.013) + Rand * 3.0;",
                   {"Nv": to_view, "T": t, "Rand": rand}, CM.CMOT_FLOAT2, "FilmNoiseUV")
    noise = g.texture_sample(dd.load(dd.ROOT + "/Textures/T_CloudNoise"), pan)
    film_uv = g.custom(BUBBLE_FILM_UV, {"F": fresnel, "Noise": (noise, "R")}, CM.CMOT_FLOAT2, "FilmUV")
    film = g.texture_sample(dd.load(dd.ROOT + "/Textures/T_ThinFilmRamp"), film_uv)

    blend = g.collection("WorldBlend")
    emissive = g.custom(BUBBLE_EMISSIVE, {
        "Film": film, "F": fresnel, "Mem": (memory, "RGBA"), "Nv": to_view, "MemScale": mem_scale,
        "RimGlow": g.scalar("RimGlow", 0.55), "MemGlow": g.scalar("MemoryGlow", 0.4),
        "Fade": fade, "Blend": blend,
    }, CM.CMOT_FLOAT3, "BubbleEmissive")
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    pos = g.node(unreal.MaterialExpressionWorldPosition)
    dark = g.function_call(dd.load(dd.ROOT + "/Materials/MF_DreamDarknessMask"), pos)
    opacity = g.custom(BUBBLE_OPACITY, {
        "F": fresnel, "Mem": (memory, "RGBA"), "Core": g.scalar("CoreOpacity", 0.2),
        "Rim": g.scalar("RimOpacity", 0.6), "Fade": fade, "Dark": dark, "Blend": blend,
        "Depth": g.node(unreal.MaterialExpressionPixelDepth),
    }, CM.CMOT_FLOAT1, "BubbleOpacity")
    g.output(opacity, unreal.MaterialProperty.MP_OPACITY)

    tint = g.node(unreal.MaterialExpressionMultiply)
    g.connect(film, tint, "A")
    g.connect(g.const(0.25), tint, "B")
    g.output(tint, unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(g.const(0.08), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()
    return g.material


DUST_EMISSIVE = r"""
float ndv = saturate(dot(normalize(N), normalize(V)));
float tw = 0.6 + 0.4 * sin(T * (1.0 + 2.0 * Rand) + Rand * 50.0);
float nearFade = saturate(Depth / 300.0);      // fades out within 3 m of the camera
return Color.rgb * Glow * ndv * ndv * Fade * tw * nearFade * Blend;
"""


def build_dust_material():
    g = dd.new_material("M_CosmicDust", shading_model=unreal.MaterialShadingModel.MSM_UNLIT,
                        blend_mode=unreal.BlendMode.BLEND_ADDITIVE, used_with_instanced_static_meshes=True)
    rand = g.node(unreal.MaterialExpressionPerInstanceRandom)
    t = g.time()
    speed_mul = g.node(unreal.MaterialExpressionMultiply)
    g.connect(g.scalar("RiseSpeed", 14.0), speed_mul, "A")
    g.connect(g.collection("DustSpeed"), speed_mul, "B")
    height = g.scalar("RiseHeight", 900.0)
    wpo = g.custom(RISE_WPO, {"T": t, "Rand": rand, "Speed": speed_mul, "RiseHeight": height,
                              "Sway": g.scalar("Sway", 10.0)}, CM.CMOT_FLOAT3, "DustRise")
    g.output(wpo, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    fade = g.custom(RISE_FADE, {"T": t, "Rand": rand, "Speed": speed_mul, "RiseHeight": height},
                    CM.CMOT_FLOAT1, "DustLoopFade")
    emissive = g.custom(DUST_EMISSIVE, {
        "N": g.node(unreal.MaterialExpressionVertexNormalWS),
        "V": g.node(unreal.MaterialExpressionCameraVectorWS),
        "Depth": g.node(unreal.MaterialExpressionPixelDepth),
        "Rand": rand, "T": t, "Fade": fade,
        "Color": g.vector("Color", unreal.LinearColor(1.0, 0.93, 0.72, 1.0)),
        "Glow": g.scalar("Glow", 3.0),
        "Blend": g.collection("WorldBlend"),
    }, CM.CMOT_FLOAT3, "DustGlow")
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()
    return g.material


FLOAT_WPO = r"""
// every prop gets its own lift, period and phase from where it stands
float h1 = frac(sin(dot(O.xy, float2(12.9898, 78.233))) * 43758.5453);
float h2 = frac(sin(dot(O.xy, float2(39.346, 11.135))) * 24634.6345);
float amount = FloatAmount * Blend * (1.0 - Dark);      // ink makes the prop sink back
float period = lerp(4.0, 7.0, h1);
float ph = h2 * 6.2831853;
float lift = lerp(LiftMin, LiftMax, h2) + sin(T * 6.2831853 / period + ph) * 6.0;
float ang = radians(6.0) * sin(T * 6.2831853 / (period * 1.3) + ph * 2.0) * amount;
float3 axis = float3(cos(h1 * 6.2831853), sin(h1 * 6.2831853), 0.0);
float3 rel = P - O;
float3 rot = rel * cos(ang) + cross(axis, rel) * sin(ang) + axis * dot(axis, rel) * (1.0 - cos(ang));
return (rot - rel) + float3(0.0, 0.0, lift * amount);
"""


def add_float_to_material(material):
    """Adds (or refreshes) the DreamFloat world position offset on a duplicated base material."""
    prop = unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET
    existing = mel.get_material_property_input_node(material, prop)
    if isinstance(existing, unreal.MaterialExpressionCustom) and             str(existing.get_editor_property("description")) == "DreamFloat":
        # ours from an earlier run: drop it and build it again, the input list may have changed
        mel.delete_material_expression(material, existing)
        existing = None
    g = dd.Graph(material=material)
    g._row = 60
    obj = g.node(unreal.MaterialExpressionObjectPositionWS)
    wpo = g.custom(FLOAT_WPO, {
        "P": g.node(unreal.MaterialExpressionWorldPosition),
        "O": obj,
        "T": g.time(),
        "FloatAmount": g.collection("FloatAmount"),
        "LiftMin": g.collection("FloatLiftMin"),
        "LiftMax": g.collection("FloatLiftMax"),
        "Blend": g.collection("WorldBlend"),
        "Dark": g.function_call(dd.load(dd.ROOT + "/Materials/MF_DreamDarknessMask"), obj),
    }, CM.CMOT_FLOAT3, "DreamFloat")
    if existing is not None:
        add = g.node(unreal.MaterialExpressionAdd)
        out_name = mel.get_material_property_input_node_output_name(material, prop)
        g.connect((existing, out_name), add, "A")
        g.connect(wpo, add, "B")
        g.output(add, prop)
        note = "added to existing WPO"
    else:
        g.output(wpo, prop)
        note = "new WPO"
    mel.recompile_material(material)
    dd.save(material)
    return note


def find_float_meshes():
    """Small market props by name; falls back to forest stones if the pack has none."""
    import re
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    seen_kinds = set()
    for root in FLOAT_SEARCH_ROOTS:
        for data in registry.get_assets_by_path(root, recursive=True):
            name = str(data.asset_name)
            match = re.search(FLOAT_NAME_PATTERN, name, re.I)
            if str(data.asset_class_path.asset_name) != "StaticMesh" or not match:
                continue
            if re.search(FLOAT_NAME_EXCLUDE, name, re.I):
                continue
            kind = match.group(0).lower()
            if kind in seen_kinds:
                continue
            mesh = dd.load(str(data.package_name))
            box = mesh.get_bounding_box()
            size = box.max - box.min
            if max(size.x, size.y, size.z) > FLOAT_MAX_SIZE_CM or min(size.x, size.y, size.z) < 8.0:
                continue
            seen_kinds.add(kind)
            found.append(mesh)
            if len(found) >= FLOAT_MAX_KINDS:
                return found
    if len(found) < 3:
        REPORT["notes"].append("few market props found (%d); adding forest stones and branches" % len(found))
        found.extend(dd.load(p) for p in FLOAT_FALLBACK if dd.exists(p))
    return found


def build_float_instances():
    """Float copies of the prop materials, one per material slot. Pack materials stay untouched."""
    result = {}
    bases = {}
    for mesh in find_float_meshes():
        slot_materials = []
        for slot in range(len(mesh.static_materials)):
            source = mesh.get_material(slot)
            if source is None:
                slot_materials.append(None)
                continue
            base = source
            while isinstance(base, unreal.MaterialInstance):
                base = base.get_editor_property("parent")
            base_name = base.get_name()
            if base_name not in bases:
                dst = "%s/Materials/Float/%s_Float" % (dd.ROOT, base_name)
                if not dd.exists(dst):
                    dd.eal.duplicate_asset(base.get_path_name().split(".")[0], dst)
                bases[base_name] = dd.load(dst)
                REPORT["materials"][base_name + "_Float"] = add_float_to_material(bases[base_name])
            if isinstance(source, unreal.MaterialInstanceConstant):
                dst = "%s/Materials/Float/%s_Float" % (dd.ROOT, source.get_name().replace("_Inst", ""))
                if not dd.exists(dst):
                    dd.eal.duplicate_asset(source.get_path_name().split(".")[0], dst)
                instance = dd.load(dst)
                mel.set_material_instance_parent(instance, bases[base_name])
                mel.update_material_instance(instance)
                dd.save(instance)
                slot_materials.append(instance)
            else:
                slot_materials.append(bases[base_name])
        result[mesh.get_name()] = (mesh, [m for m in slot_materials if m is not None])
        REPORT["materials"]["prop " + mesh.get_name()] = [m.get_name() for m in slot_materials if m is not None]
    return result


def build_ink_material():
    g = dd.new_material("M_Decal_DreamInk", material_domain=unreal.MaterialDomain.MD_DEFERRED_DECAL,
                        blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT)
    pos = g.node(unreal.MaterialExpressionWorldPosition)
    dark = g.function_call(dd.load(dd.ROOT + "/Materials/MF_DreamDarknessMask"), pos)
    opacity = g.node(unreal.MaterialExpressionMultiply)
    g.connect(dark, opacity, "A")
    g.connect(g.collection("WorldBlend"), opacity, "B")
    g.output(opacity, unreal.MaterialProperty.MP_OPACITY)
    g.output(g.vector("InkColor", dd.linear_from_hex("#0A0B1A")), unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(g.scalar("InkRoughness", 0.05), unreal.MaterialProperty.MP_ROUGHNESS)
    g.output(g.const(0.0), unreal.MaterialProperty.MP_METALLIC)
    g.output(g.const(0.5), unreal.MaterialProperty.MP_SPECULAR)
    g.output(g.const3(unreal.LinearColor(0.0, 0.0, 1.0, 0.0)), unreal.MaterialProperty.MP_NORMAL)
    g.finish()
    return g.material


def build_lantern_materials():
    emissive = dd.load(dd.ROOT + "/Materials/M_DreamEmissive")
    solid = dd.load(dd.ROOT + "/Materials/M_DreamSolid")
    return {
        "post": dd.new_instance("MI_LanternPost", solid,
                                vectors={"Color": unreal.LinearColor(0.035, 0.024, 0.016, 1.0)},
                                scalars={"Roughness": 0.7}),
        "star": dd.new_instance("MI_LanternStar", emissive,
                                vectors={"Color": unreal.LinearColor(0.72, 0.90, 1.0, 1.0)},
                                scalars={"Glow": 3.0}),
    }


# ---------------------------------------------------------------------------
# Actors
# ---------------------------------------------------------------------------

def set_mpc_darkness(position, radius):
    mpc = dd.load(dd.ROOT + "/MPC_Dream")
    vectors = list(mpc.get_editor_property("vector_parameters"))
    for param in vectors:
        if str(param.get_editor_property("parameter_name")) == "DarknessSource0":
            param.set_editor_property("default_value",
                                      unreal.LinearColor(position[0], position[1], position[2], radius))
    mpc.set_editor_property("vector_parameters", vectors)
    scalars = list(mpc.get_editor_property("scalar_parameters"))
    for param in scalars:
        if str(param.get_editor_property("parameter_name")) == "DarknessCount":
            param.set_editor_property("default_value", 1.0)
    mpc.set_editor_property("scalar_parameters", scalars)
    dd.save(mpc)


def spawn_field(label, location, tags, folder, mesh, material, count, seed, inner_radius, outer_radius,
                min_height, max_height, min_scale=1.0, max_scale=1.0, arc_start_degrees=0.0, arc_degrees=360.0,
                face_tangent=False, random_yaw=False):
    """A BP_DreamInstancedField actor (Blueprint with one instanced mesh component, no graph)."""
    actor = dd.spawn(dd.load(dd.BP_FIELD), label, location, step_tag=STEP, tags=tags, folder=folder)
    count = dd.place_instances(actor, mesh, material, count, seed, inner_radius, outer_radius, min_height,
                               max_height, min_scale, max_scale, arc_start_degrees, arc_degrees,
                               face_tangent, random_yaw)
    return record(actor, "%d instances" % count)


def lantern_positions(world, layout):
    cx, cy, cz = layout["arena_center"]
    radius = layout["arena_radius"]
    yaw = math.radians(layout["view_yaw"])
    out = []
    for angle in LANTERN_ANGLES:
        a = yaw + math.radians(angle)
        x, y = cx + math.cos(a) * radius * 0.9, cy + math.sin(a) * radius * 0.9
        out.append((x, y, dd.ground_z(world, x, y, cz)))
    # one lantern beside the rock wall, so CAM_Wall sees warm light at night
    wall = layout["wall"]
    x = wall["mid"][0] - wall["radial"][0] * 330.0 + wall["tangent"][0] * 300.0
    y = wall["mid"][1] - wall["radial"][1] * 330.0 + wall["tangent"][1] * 300.0
    out.append((x, y, dd.ground_z(world, x, y, cz)))
    return out


def build_actors(world, layout):
    cx, cy, cz = layout["arena_center"]
    radius = layout["arena_radius"]
    yaw = math.radians(layout["view_yaw"])
    dream = [dd.TAG_DREAM]

    # --- 1. star fish -----------------------------------------------------------
    fish_mesh = dd.load(dd.ROOT + "/Meshes/SM_StarFish")
    for index, (school, material) in enumerate(zip(FISH_SCHOOLS, build_fish_material())):
        spawn_field("StarFishSchool_%d" % index, (cx, cy, cz), dream + ["FX_Fish"], "DreamDemo/FX_Dream",
                    mesh=fish_mesh, material=material, count=FISH_PER_SCHOOL, seed=school["seed"],
                    inner_radius=school["radius"][0], outer_radius=school["radius"][1],
                    arc_start_degrees=school["arc_start"], arc_degrees=26.0,
                    min_height=school["height"][0], max_height=school["height"][1],
                    min_scale=1.0, max_scale=1.7, face_tangent=True)

    # --- 2. memory bubbles: only outside 70 % of the arena radius ----------------
    spawn_field("MemoryBubbles", (cx, cy, cz), dream + ["FX_Bubbles"], "DreamDemo/FX_Dream",
                mesh=dd.load(dd.ROOT + "/Meshes/SM_Bubble"), material=build_bubble_material(),
                count=BUBBLE_COUNT, seed=5, inner_radius=radius * 0.74, outer_radius=radius * 1.08,
                min_height=BUBBLE_RISE_HEIGHT * 0.5 + 40.0, max_height=BUBBLE_RISE_HEIGHT * 0.5 + 220.0,
                min_scale=0.5, max_scale=2.0, random_yaw=True)

    # --- 3. cosmic dust ----------------------------------------------------------
    spawn_field("CosmicDust", (cx, cy, cz), dream + ["FX_Dust"], "DreamDemo/FX_Dream",
                mesh=dd.load(dd.ROOT + "/Meshes/SM_DustMote"), material=build_dust_material(),
                count=DUST_COUNT, seed=9, inner_radius=0.0, outer_radius=radius * 1.25,
                min_height=420.0, max_height=560.0, min_scale=0.18, max_scale=0.5)

    # --- 5. ink darkness (before the floaters, two of them sit inside it) --------
    ink_angle = yaw + math.radians(INK_ANGLE_DEG)
    ink = (cx + math.cos(ink_angle) * radius * INK_DISTANCE, cy + math.sin(ink_angle) * radius * INK_DISTANCE)
    ink_z = dd.ground_z(world, ink[0], ink[1], cz)
    set_mpc_darkness((ink[0], ink[1], ink_z), INK_RADIUS)
    decal = dd.spawn(unreal.DecalActor, "Decal_DreamInk", (cx, cy, cz), rotation=(-90.0, 0.0, 0.0),
                     step_tag=STEP, tags=dream + ["FX_Ink"], folder="DreamDemo/FX_Dream")
    decal.decal.set_decal_material(build_ink_material())
    decal.decal.set_editor_property("decal_size", unreal.Vector(500.0, radius * 1.2, radius * 1.2))
    record(decal, "darkness source at %s radius %.0f" % ([round(ink[0]), round(ink[1])], INK_RADIUS))

    # --- 4. floating props at the rim --------------------------------------------
    rng = random.Random(17)
    floats = build_float_instances()
    names = list(floats.keys())
    wall = layout["wall"]
    spots = []
    for k in range(9):       # around the rim
        a = yaw + math.radians(20.0 + k * 38.0 + rng.uniform(-8.0, 8.0))
        r = radius * rng.uniform(0.80, 1.04)
        spots.append((cx + math.cos(a) * r, cy + math.sin(a) * r, "rim"))
    for k in range(2):       # inside the ink, on its outer half: these must stay on the ground
        r = INK_RADIUS * (0.15 + 0.3 * k)
        side = INK_RADIUS * 0.3 * (1 if k else -1)
        spots.append((ink[0] + math.cos(ink_angle) * r - math.sin(ink_angle) * side,
                      ink[1] + math.sin(ink_angle) * r + math.cos(ink_angle) * side, "inside ink"))
    for offset in (-230.0, 210.0):   # in front of the house wall, for CAM_Wall
        x = wall["mid"][0] - wall["radial"][0] * 130.0 + wall["tangent"][0] * offset
        y = wall["mid"][1] - wall["radial"][1] * 130.0 + wall["tangent"][1] * offset
        spots.append((x, y, "wall camera"))
    for index, (x, y, why) in enumerate(spots):
        name = names[index % len(names)]
        mesh, materials = floats[name]
        box = mesh.get_bounding_box()
        z = dd.ground_z(world, x, y, cz) - box.min.z
        actor = dd.spawn(mesh, "Float_%02d_%s" % (index, name.replace("SM_", "")), (x, y, z),
                         rotation=(0.0, rng.uniform(0.0, 360.0), 0.0), step_tag=STEP,
                         tags=dream + ["FX_Float"], folder="DreamDemo/FX_Dream/Floaters")
        comp = actor.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        for slot, material in enumerate(materials):
            comp.set_material(slot, material)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
        comp.set_editor_property("bounds_scale", 1.8)
        record(actor, why)

    # --- 6. lantern stand-ins: post (shared) and a star mote (dream only) --------
    mats = build_lantern_materials()
    cylinder = dd.load("/Engine/BasicShapes/Cylinder")
    sphere = dd.load("/Engine/BasicShapes/Sphere")
    for index, (x, y, z) in enumerate(lantern_positions(world, layout)):
        post = dd.spawn(cylinder, "Lantern_%d_Post" % index, (x, y, z + 105.0), step_tag=STEP,
                        tags=["DreamDemo.Lantern"], folder="DreamDemo/Lanterns", scale=(0.07, 0.07, 2.2))
        post.static_mesh_component.set_material(0, mats["post"])
        post.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        record(post)
        star = dd.spawn(sphere, "Lantern_%d_Star" % index, (x, y, z + 232.0), step_tag=STEP,
                        tags=dream + ["DreamDemo.LanternStar"], folder="DreamDemo/Lanterns",
                        scale=(0.16, 0.16, 0.16))
        comp = star.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_material(0, mats["star"])
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_cast_shadow(False)
        record(star)

    layout["ink"] = {"center": [round(ink[0], 1), round(ink[1], 1), round(ink_z, 1)], "radius": INK_RADIUS}
    import json
    with open(dd.LAYOUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(layout, fh, indent=1)


def main():
    world = dd.open_demo_map()
    layout = dd.load_layout()
    REPORT["notes"].append("removed %d actors from a previous run" % dd.remove_step_actors(STEP))
    build_actors(world, layout)
    dd.apply_day()
    dd.save_demo_map()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    dd.write_report_json("step5_build.json", REPORT)
    dd.log("step5 done")


main()
