"""Step 4: three lighting sets, cameras, stand-ins and the state Blueprint on L_Kingdom_Main.

DAY is the map's own lighting (KD_Sun, KD_SkyLight, KD_SkyAtmosphere, KD_Fog, KD_Clouds):
those actors only receive tags. Everything this step creates lives in the Outliner folder
DreamDemo, which Tools/Kingdom/build_kingdom_map.py leaves alone. That tool respawns the
KD_ lighting without tags, so rerun build_all.sh after it.

Run:  bash Tools/DreamDemo/ue_run.sh cmd Tools/DreamDemo/ue/step4_lighting_states.py dd_step4
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                "Tools", "DreamDemo", "ue"))
import dd_common as dd  # noqa: E402
import dd_layout  # noqa: E402

STEP = "DreamDemo.Step4"
REPORT = {"actors": [], "notes": [], "day_lighting": {}}

# ---- tunables ----------------------------------------------------------------
# Exposure is locked in NIGHT and DREAM (manual, no physical camera): display = luminance * 2^bias.
NIGHT = dict(
    moon_intensity=0.45, moon_color=(0.72, 0.80, 1.0), moon_elevation=42.0, moon_azimuth=15.0,    # azimuth relative to the view direction
    sky_light_intensity=2.0, exposure_bias=1.6,
    fog_color=(0.006, 0.012, 0.030), fog_density=0.012,
)
DREAM = dict(
    key_intensity=1.8,           # 30 % of the day sun (KD_Sun is 6.0)
    key_color="#7FD6D0", key_pitch=-80.0, key_yaw=30.0, key_source_angle=8.0,
    sky_light_intensity=3.0, exposure_bias=0.15, saturation=0.9,
    fog_color="#2E5F7A", fog_luminance_scale=0.35, fog_density=0.018,
    bloom_intensity=0.9, vignette=0.35,
)
XENON_SCALE = 1.6
WEAK_SPOT_GLOW = 1.0
# -------------------------------------------------------------------------------


def lookat(src, dst):
    dx, dy, dz = dst[0] - src[0], dst[1] - src[1], dst[2] - src[2]
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return (pitch, yaw, 0.0)


def record(actor, note=""):
    loc = actor.get_actor_location()
    REPORT["actors"].append({"label": actor.get_actor_label(), "class": actor.get_class().get_name(),
                             "loc": [round(loc.x), round(loc.y), round(loc.z)],
                             "tags": [str(t) for t in actor.tags], "note": note})
    return actor


def color8(rgb):
    return unreal.Color(r=int(rgb[0] * 255), g=int(rgb[1] * 255), b=int(rgb[2] * 255), a=255)


def hex_color8(hex_color):
    h = hex_color.lstrip("#")
    return unreal.Color(r=int(h[0:2], 16), g=int(h[2:4], 16), b=int(h[4:6], 16), a=255)


# ---------------------------------------------------------------------------
# Small shared materials
# ---------------------------------------------------------------------------

def build_basic_materials():
    g = dd.new_material("M_DreamEmissive", shading_model=unreal.MaterialShadingModel.MSM_UNLIT)
    color = g.vector("Color", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = g.scalar("Glow", 1.0)
    mul = g.node(unreal.MaterialExpressionMultiply)
    g.connect(color, mul, "A")
    g.connect(glow, mul, "B")
    g.output(mul, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()
    emissive = g.material

    g = dd.new_material("M_DreamSolid")
    g.output(g.vector("Color", unreal.LinearColor(0.05, 0.05, 0.055, 1.0)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(g.scalar("Roughness", 0.8), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()
    solid = g.material

    return {
        "weak_spot": dd.new_instance("MI_WeakSpot_Orange", emissive,
                                     vectors={"Color": unreal.LinearColor(1.0, 0.20, 0.015, 1.0)},
                                     scalars={"Glow": WEAK_SPOT_GLOW}),
        "xenon": dd.new_instance("MI_XenonBody", solid,
                                 vectors={"Color": unreal.LinearColor(0.012, 0.012, 0.016, 1.0)},
                                 scalars={"Roughness": 0.85}),
    }


# ---------------------------------------------------------------------------
# Map
# ---------------------------------------------------------------------------

def open_or_create_map():
    world = dd.open_demo_map()
    if "L_Kingdom_Main" not in world.get_path_name():
        raise RuntimeError("wrong world loaded: %s" % world.get_path_name())
    return world


def tag_day_lighting():
    """The map's own lighting becomes the DAY set: tags only, nothing else is changed."""
    found = {}
    day_classes = (unreal.DirectionalLight, unreal.SkyLight, unreal.ExponentialHeightFog,
                   unreal.PostProcessVolume, unreal.SkyAtmosphere, unreal.VolumetricCloud)
    for actor in dd.all_actors():
        if actor.actor_has_tag("DreamDemo") or not isinstance(actor, day_classes):
            continue
        dd.add_tags(actor, [dd.TAG_DAY])
        found[actor.get_actor_label()] = actor.get_class().get_name()
        if isinstance(actor, unreal.DirectionalLight):
            comp = actor.light_component
            REPORT["day_lighting"]["sun"] = {
                "intensity": comp.intensity, "mobility": str(comp.mobility),
                "rotation": [round(actor.get_actor_rotation().pitch, 1), round(actor.get_actor_rotation().yaw, 1)]}
        if isinstance(actor, unreal.SkyLight):
            comp = actor.light_component
            REPORT["day_lighting"]["sky_light"] = {
                "intensity": comp.intensity, "source": str(comp.source_type),
                "real_time_capture": bool(comp.real_time_capture)}
    REPORT["day_lighting"]["actors"] = found
    REPORT["day_lighting"]["post_process_volume"] = any(v == "PostProcessVolume" for v in found.values())
    return found


def hide_district_labels():
    """KD_Label_* text actors are editor aids of the Kingdom tool; in Play they hang in the sky."""
    count = 0
    for actor in dd.all_actors():
        if isinstance(actor, unreal.TextRenderActor) and actor.get_actor_label().startswith("KD_Label_"):
            actor.set_actor_hidden_in_game(True)
            count += 1
    REPORT["notes"].append("%d KD_Label actors set to hidden in game" % count)


def sky_sphere_scale():
    for actor in dd.all_actors():
        if actor.get_class().get_name().startswith("BP_Sky_Sphere"):
            s = actor.get_actor_scale3d()
            return (s.x, s.y, s.z)
    return (400.0, 400.0, 400.0)


def spawn_sky(label, material, tags, folder):
    mesh = dd.load("/Engine/EngineSky/SM_SkySphere")
    actor = dd.spawn(mesh, label, (0, 0, 0), step_tag=STEP, tags=tags, folder=folder, scale=sky_sphere_scale())
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_material(0, material)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_cast_shadow(False)
    comp.set_editor_property("affect_distance_field_lighting", False)
    comp.set_editor_property("affect_dynamic_indirect_lighting", False)
    return record(actor)


def spawn_directional(label, intensity, color, pitch, yaw, source_angle, tags, folder):
    actor = dd.spawn(unreal.DirectionalLight, label, (0, 0, 800), rotation=(pitch, yaw, 0.0),
                     step_tag=STEP, tags=tags, folder=folder)
    comp = actor.light_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("intensity", intensity)
    comp.set_editor_property("light_color", color)
    comp.set_editor_property("light_source_angle", source_angle)
    comp.set_editor_property("atmosphere_sun_light", False)
    return record(actor)


def spawn_sky_light(label, intensity, cubemap, tags, folder, real_time=False):
    actor = dd.spawn(unreal.SkyLight, label, (0, 0, 900), step_tag=STEP, tags=tags, folder=folder)
    comp = actor.light_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("intensity", intensity)
    comp.set_editor_property("real_time_capture", real_time)
    comp.set_editor_property("lower_hemisphere_is_black", real_time)
    if cubemap is not None:
        comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        comp.set_editor_property("cubemap", cubemap)
    else:
        comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
    return record(actor)


def spawn_fog(label, color, density, tags, folder):
    actor = dd.spawn(unreal.ExponentialHeightFog, label, (0, 0, 100), step_tag=STEP, tags=tags, folder=folder)
    comp = actor.component
    comp.set_editor_property("fog_density", density)
    comp.set_editor_property("fog_height_falloff", 0.2)
    comp.set_editor_property("fog_inscattering_luminance", color)
    comp.set_editor_property("start_distance", 300.0)
    return record(actor)


def spawn_post_process(label, tags, folder, bias, bloom=None, vignette=None, blendables=(), saturation=None):
    actor = dd.spawn(unreal.PostProcessVolume, label, (0, 0, 300), step_tag=STEP, tags=tags, folder=folder)
    actor.set_editor_property("unbound", True)
    s = actor.settings
    s.set_editor_property("override_auto_exposure_method", True)
    s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    s.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
    s.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", bias)
    if bloom is not None:
        s.set_editor_property("override_bloom_intensity", True)
        s.set_editor_property("bloom_intensity", bloom)
    if vignette is not None:
        s.set_editor_property("override_vignette_intensity", True)
        s.set_editor_property("vignette_intensity", vignette)
    if saturation is not None:
        s.set_editor_property("override_color_saturation", True)
        s.set_editor_property("color_saturation", unreal.Vector4(saturation, saturation, saturation, 1.0))
    if blendables:
        s.set_editor_property("weighted_blendables", unreal.WeightedBlendables(
            array=[unreal.WeightedBlendable(weight=1.0, object=m) for m in blendables]))
    actor.set_editor_property("settings", s)
    return record(actor)


def build_lighting(layout):
    # ---- NIGHT: moon, warm lights come in step 6 --------------------------------
    folder = "DreamDemo/Lighting_Night"
    tags = [dd.TAG_NIGHT]
    # the moon sits a little to the side of the view direction, so CAM_Sky has it in frame
    moon_azimuth = layout["view_yaw"] + NIGHT["moon_azimuth"]
    el, az = math.radians(NIGHT["moon_elevation"]), math.radians(moon_azimuth)
    moon_dir = (math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))
    spawn_directional("DL_Moon", NIGHT["moon_intensity"], color8(NIGHT["moon_color"]),
                      -NIGHT["moon_elevation"], moon_azimuth + 180.0, 1.0, tags, folder)
    night_sky = dd.new_instance("MI_NightSky", dd.load(dd.ROOT + "/Materials/M_NightSky"),
                                vectors={"MoonDirection": unreal.LinearColor(moon_dir[0], moon_dir[1], moon_dir[2], 0.0)})
    spawn_sky("Sky_Night", night_sky, tags, folder)
    # Real-time capture sees only the sky dome (M_NightSky is an "Is Sky" material), so the
    # night ambient can never contain a stale capture of the sunlit day scene.
    spawn_sky_light("SL_Night", NIGHT["sky_light_intensity"], None, tags, folder, real_time=True)
    spawn_fog("Fog_Night", unreal.LinearColor(*NIGHT["fog_color"], 1.0), NIGHT["fog_density"], tags, folder)
    spawn_post_process("PPV_Night", tags, folder, NIGHT["exposure_bias"], bloom=0.7, vignette=0.4)

    # ---- DREAM ------------------------------------------------------------------
    folder = "DreamDemo/Lighting_Dream"
    tags = [dd.TAG_DREAM]
    spawn_directional("DL_Dream", DREAM["key_intensity"], hex_color8(DREAM["key_color"]),
                      DREAM["key_pitch"], DREAM["key_yaw"], DREAM["key_source_angle"], tags, folder)
    spawn_sky("Sky_Dream", dd.load(dd.ROOT + "/Materials/MI_DreamSky"), tags + ["FX_Sky"], folder)
    spawn_sky_light("SL_Dream", DREAM["sky_light_intensity"], dd.load(dd.ROOT + "/Textures/T_DreamNebula"),
                    tags, folder)
    fog = dd.linear_from_hex(DREAM["fog_color"])
    k = DREAM["fog_luminance_scale"]
    spawn_fog("Fog_Dream", unreal.LinearColor(fog.r * k, fog.g * k, fog.b * k, 1.0), DREAM["fog_density"],
              tags, folder)
    spawn_post_process("PPV_Dream", tags, folder, DREAM["exposure_bias"], bloom=DREAM["bloom_intensity"],
                       vignette=DREAM["vignette"], saturation=DREAM["saturation"],
                       blendables=[dd.load(dd.ROOT + "/Materials/M_PP_DreamCaustics")])


# ---------------------------------------------------------------------------
# Scene dressing, stand-ins, cameras
# ---------------------------------------------------------------------------

def find_idle_animation():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path("/Game/Characters/Mannequins/Anims", recursive=True):
        name = str(data.asset_name)
        if str(data.asset_class_path.asset_name) == "AnimSequence" and "Idle" in name and "Unarmed" in str(data.package_name):
            return dd.load(str(data.package_name)), name
    return None, None


def build_scene(world, layout):
    cx, cy, cz = layout["arena_center"]
    yaw = math.radians(layout["view_yaw"])
    fwd = (math.cos(yaw), math.sin(yaw))
    right = (-math.sin(yaw), math.cos(yaw))

    def at(forward, side, up=0.0):
        x = cx + fwd[0] * forward + right[0] * side
        y = cy + fwd[1] * forward + right[1] * side
        return (x, y, dd.ground_z(world, x, y, cz) + up)

    mats = build_basic_materials()
    folder = "DreamDemo/StandIns"

    # Three Golem stand-ins, seen from behind by CAM_Wide. Custom stencil 1.
    idle, idle_name = find_idle_animation()
    REPORT["notes"].append("golem idle animation: %s" % idle_name)
    meshes = ["/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
              "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple",
              "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"]
    for index, side in enumerate((-130.0, 0.0, 130.0)):
        loc = at(-450.0 - abs(side) * 0.25, side)
        # mannequin meshes face +Y, so yaw -90 makes them look along the view direction
        actor = dd.spawn(dd.load(meshes[index]), "Golem_StandIn_%d" % index, loc,
                         rotation=(0.0, layout["view_yaw"] - 90.0, 0.0), step_tag=STEP, folder=folder)
        comp = actor.skeletal_mesh_component
        comp.set_render_custom_depth(True)
        comp.set_custom_depth_stencil_value(1)
        comp.set_editor_property("receives_decals", False)
        if idle is not None:
            comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            comp.override_animation_data(idle, True, True, index * 0.7, 1.0)
        record(actor, "stencil 1")

    # Xenon stand-in: a dark boulder about three golems tall. Custom stencil 2.
    stone_path = "/Game/Stylized_Forest/Meshes/stones/SM_stone_05"
    scale = XENON_SCALE
    if dd.exists(stone_path):
        stone = dd.load(stone_path)
    else:
        stone, scale = dd.load("/Engine/BasicShapes/Sphere"), 5.0
        REPORT["notes"].append("Xenon stand-in: SM_stone_05 missing, using a sphere")
    box = stone.get_bounding_box()
    xen = at(260.0, 0.0)
    xen_z = xen[2] - box.min.z * scale - 45.0
    actor = dd.spawn(stone, "Xenon_StandIn", (xen[0], xen[1], xen_z), rotation=(0.0, 25.0, 0.0),
                     step_tag=STEP, folder=folder, scale=(scale,) * 3)
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_material(0, mats["xenon"])
    comp.set_render_custom_depth(True)
    comp.set_custom_depth_stencil_value(2)
    comp.set_editor_property("receives_decals", False)
    record(actor, "stencil 2")
    top = xen_z + box.max.z * scale

    # Weak spot: the brightest thing in the frame. Custom stencil 3. Found by tracing the boulder.
    spot_z = xen[2] + (top - xen[2]) * 0.5
    start = unreal.Vector(xen[0] - fwd[0] * 900.0, xen[1] - fwd[1] * 900.0, spot_z)
    end = unreal.Vector(xen[0] + fwd[0] * 300.0, xen[1] + fwd[1] * 300.0, spot_z)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 True, [], unreal.DrawDebugTrace.NONE, True)
    surface = None
    if hit is not None:
        parts = hit.to_tuple()
        if any(p == actor for p in parts if isinstance(p, unreal.Actor)):
            surface = next(p for p in parts if isinstance(p, unreal.Vector))
    if surface is None:
        half = (box.max.x - box.min.x) * 0.5 * scale
        surface = unreal.Vector(xen[0] - fwd[0] * half, xen[1] - fwd[1] * half, spot_z)
        REPORT["notes"].append("weak spot: trace missed the boulder, placed from bounds")
    spot = (surface.x - fwd[0] * 6.0, surface.y - fwd[1] * 6.0, spot_z)
    actor = dd.spawn(dd.load("/Engine/BasicShapes/Sphere"), "Xenon_WeakSpot", spot,
                     rotation=(0.0, layout["view_yaw"], 0.0), step_tag=STEP, folder=folder,
                     scale=(0.3, 0.8, 0.8))
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_material(0, mats["weak_spot"])
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_cast_shadow(False)
    comp.set_render_custom_depth(True)
    comp.set_custom_depth_stencil_value(3)
    comp.set_editor_property("receives_decals", False)
    record(actor, "stencil 3")

    # CAM_Wall looks at the facade of the house nearest to the arena: wall, windows, ground, no sky.
    wb = layout.get("wall_building")
    if wb is None:
        raise RuntimeError("layout has no wall_building")
    ox, oy = wb["origin"]
    ex, ey = wb["extent"]
    dx, dy = cx - ox, cy - oy
    length = math.hypot(dx, dy)
    toward = (dx / length, dy / length)                       # from the house toward the arena
    reach = min(ex / max(abs(toward[0]), 1e-3), ey / max(abs(toward[1]), 1e-3))
    facade = (ox + toward[0] * reach, oy + toward[1] * reach)
    facade_z = dd.ground_z(world, facade[0] + toward[0] * 150.0, facade[1] + toward[1] * 150.0, cz)
    radial = (-toward[0], -toward[1])
    tangent = (-radial[1], radial[0])
    cam_wall = (facade[0] + toward[0] * 620.0, facade[1] + toward[1] * 620.0, facade_z + 165.0)

    folder = "DreamDemo/Cameras"
    golems = at(-450.0, 0.0)
    cam_wide = (golems[0] - fwd[0] * 400.0, golems[1] - fwd[1] * 400.0, golems[2] + 200.0)
    cameras = []
    for label, loc, rot, fov in (
        ("CAM_Wide", cam_wide, lookat(cam_wide, (xen[0], xen[1], xen[2] + 190.0)), 90.0),
        ("CAM_Wall", cam_wall, lookat(cam_wall, (facade[0], facade[1], facade_z + 120.0)), 60.0),
        ("CAM_Sky", at(-200.0, -900.0, 170.0), (30.0, layout["view_yaw"] + 10.0, 0.0), 90.0),
    ):
        actor = dd.spawn(unreal.CameraActor, label, loc, rotation=rot, step_tag=STEP, folder=folder)
        cam = actor.camera_component
        cam.set_editor_property("field_of_view", fov)
        cam.set_editor_property("constrain_aspect_ratio", False)
        cameras.append(record(actor, "fov %.0f rot %s" % (fov, [round(v, 1) for v in rot])))

    layout["wall"] = {"mid": [round(facade[0], 1), round(facade[1], 1), round(facade_z, 1)],
                      "radial": list(radial), "tangent": list(tangent)}
    layout["xenon"] = [round(v, 1) for v in xen]
    layout["golems"] = [round(v, 1) for v in golems]
    import json
    with open(dd.LAYOUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(layout, fh, indent=1)
    return cameras


def build_controller(cameras):
    """Places BP_DreamWorldState (pure Blueprint, see Tools/DreamDemo/bp_generator)."""
    blueprint = dd.load(dd.BP_WORLD_STATE)
    if blueprint is None:
        raise RuntimeError("%s missing" % dd.BP_WORLD_STATE)
    actor = dd.spawn(blueprint, "BP_DreamWorldState", (0, 0, 400), step_tag=STEP, folder="DreamDemo")
    actor.set_editor_property("DreamCollection", dd.load(dd.ROOT + "/MPC_Dream"))
    actor.set_editor_property("CamWide", cameras[0])
    actor.set_editor_property("CamWall", cameras[1])
    actor.set_editor_property("CamSky", cameras[2])
    actor.set_editor_property("InitialState", 0)
    actor.set_editor_property("auto_receive_input", unreal.AutoReceiveInput.PLAYER0)
    gate = dd.find_actor("PPV_DreamGate")       # made by step 8; keep the link when step 4 is rerun
    if gate is not None:
        actor.set_editor_property("GateVolume", gate)
    record(actor)
    return actor


def main():
    world = open_or_create_map()
    layout = dd_layout.ensure(world)
    removed = dd.remove_step_actors(STEP)
    REPORT["notes"].append("removed %d actors from a previous run" % removed)
    tag_day_lighting()
    hide_district_labels()
    build_lighting(layout)
    cameras = build_scene(world, layout)
    build_controller(cameras)
    dd.apply_day()
    dd.save_demo_map()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    REPORT["layout"] = layout
    dd.write_report_json("step4_build.json", REPORT)
    dd.log("step4 done")


main()
