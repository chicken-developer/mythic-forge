"""Step 6: finish the NIGHT state: warm lantern flames and lights with a slight flicker,
festival bulbs. The forest pack has no windows, lamps or lanterns, so these are stand-ins
built from engine basic shapes (see STEP_6.md).

Run:  bash Tools/DreamDemo/ue_run.sh cmd Tools/DreamDemo/ue/step6_night.py dd_step6
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                "Tools", "DreamDemo", "ue"))
import dd_common as dd  # noqa: E402

STEP = "DreamDemo.Step6"
REPORT = {"actors": [], "notes": [], "budget": {}}

# ---- tunables ----------------------------------------------------------------
LIGHT_INTENSITY_CD = 30.0
LIGHT_COLOR = (255, 166, 84)
LIGHT_RADIUS = 1200.0
MAX_SHADOW_CASTERS = 4          # budget: point lights with shadows
FLAME_COLOR = unreal.LinearColor(1.0, 0.50, 0.14, 1.0)
FLAME_GLOW = 5.0
BULB_COLORS = [unreal.LinearColor(1.0, 0.62, 0.20, 1.0), unreal.LinearColor(1.0, 0.36, 0.16, 1.0),
               unreal.LinearColor(1.0, 0.80, 0.42, 1.0)]
BULB_GLOW = 3.0
BULBS_PER_STRING = 11
STRING_PAIRS = [(0, 1), (5, 0), (2, 6)]    # lantern indices joined by a string of bulbs
# -------------------------------------------------------------------------------


def record(actor, note=""):
    loc = actor.get_actor_location()
    REPORT["actors"].append({"label": actor.get_actor_label(), "class": actor.get_class().get_name(),
                             "loc": [round(loc.x), round(loc.y), round(loc.z)],
                             "tags": [str(t) for t in actor.tags], "note": note})
    return actor


def build_materials():
    g = dd.new_material("M_LF_LanternFlicker", material_domain=unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    flicker = g.custom(
        # slight flicker; in daylight the lantern is lit but much weaker than at night
        "return (Base + Amount * (0.6 * sin(T * 11.0) + 0.4 * sin(T * 23.7 + 1.3))) * lerp(DayScale, 1.0, saturate(Night));",
        {"T": g.time(), "Base": g.scalar("Base", 0.92), "Amount": g.scalar("FlickerAmount", 0.06),
         "DayScale": g.scalar("DayScale", 0.15), "Night": g.collection("NightBlend")},
        unreal.CustomMaterialOutputType.CMOT_FLOAT1, "Flicker")
    g.output(flicker, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()
    emissive = dd.load(dd.ROOT + "/Materials/M_DreamEmissive")
    mats = {
        "flicker": g.material,
        "flame": dd.new_instance("MI_LanternFlame", emissive, vectors={"Color": FLAME_COLOR},
                                 scalars={"Glow": FLAME_GLOW}),
        "bulbs": [dd.new_instance("MI_FestivalBulb_%d" % i, emissive, vectors={"Color": color},
                                  scalars={"Glow": BULB_GLOW}) for i, color in enumerate(BULB_COLORS)],
    }
    return mats


def lantern_posts():
    posts = {}
    for actor in dd.all_actors():
        label = actor.get_actor_label()
        if label.startswith("Lantern_") and label.endswith("_Post"):
            loc = actor.get_actor_location()
            posts[int(label.split("_")[1])] = (loc.x, loc.y, loc.z - 105.0)   # post is centred 105 cm up
    return [posts[k] for k in sorted(posts)]


def main():
    dd.open_demo_map()
    layout = dd.load_layout()
    REPORT["notes"].append("removed %d actors from a previous run" % dd.remove_step_actors(STEP))
    mats = build_materials()
    sphere = dd.load("/Engine/BasicShapes/Sphere")
    warm_tags = [dd.TAG_DAY, dd.TAG_NIGHT]
    lanterns = lantern_posts()
    if not lanterns:
        raise RuntimeError("no lantern posts: run step 5 first")

    # Shadows only for the lanterns nearest to CAM_Wide and the one at the rock wall.
    cam = dd.find_actor("CAM_Wide").get_actor_location()
    order = sorted(range(len(lanterns)), key=lambda i: math.hypot(lanterns[i][0] - cam.x, lanterns[i][1] - cam.y))
    shadow_set = set(order[:MAX_SHADOW_CASTERS - 1]) | {len(lanterns) - 1}

    for index, (x, y, z) in enumerate(lanterns):
        flame = dd.spawn(sphere, "Lantern_%d_Flame" % index, (x, y, z + 232.0), step_tag=STEP,
                         tags=warm_tags + ["DreamDemo.LanternFlame"], folder="DreamDemo/Lanterns",
                         scale=(0.12, 0.12, 0.17))
        comp = flame.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_material(0, mats["flame"])
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_cast_shadow(False)
        record(flame)

        light = dd.spawn(unreal.PointLight, "Lantern_%d_Light" % index, (x, y, z + 250.0), step_tag=STEP,
                         tags=warm_tags + ["DreamDemo.LanternLight"], folder="DreamDemo/Lanterns")
        comp = light.point_light_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        comp.set_editor_property("intensity", LIGHT_INTENSITY_CD)
        comp.set_editor_property("light_color", unreal.Color(r=LIGHT_COLOR[0], g=LIGHT_COLOR[1],
                                                             b=LIGHT_COLOR[2], a=255))
        comp.set_editor_property("attenuation_radius", LIGHT_RADIUS)
        comp.set_editor_property("source_radius", 6.0)
        comp.set_editor_property("cast_shadows", index in shadow_set)
        comp.set_editor_property("light_function_material", mats["flicker"])
        record(light, "shadows" if index in shadow_set else "no shadows")

    # Festival strings: emissive bulbs only, no extra lights. NIGHT only.
    bulbs = 0
    for a, b in STRING_PAIRS:
        if a >= len(lanterns) or b >= len(lanterns):
            continue
        pa, pb = lanterns[a], lanterns[b]
        for k in range(1, BULBS_PER_STRING + 1):
            t = k / float(BULBS_PER_STRING + 1)
            sag = 4.0 * t * (1.0 - t) * 70.0
            loc = (pa[0] + (pb[0] - pa[0]) * t, pa[1] + (pb[1] - pa[1]) * t,
                   pa[2] + (pb[2] - pa[2]) * t + 225.0 - sag)
            bulb = dd.spawn(sphere, "FestivalBulb_%d_%d_%02d" % (a, b, k), loc, step_tag=STEP,
                            tags=[dd.TAG_NIGHT, "DreamDemo.FestivalBulb"], folder="DreamDemo/Lanterns/Bulbs",
                            scale=(0.07, 0.07, 0.09))
            comp = bulb.static_mesh_component
            comp.set_mobility(unreal.ComponentMobility.MOVABLE)
            comp.set_material(0, mats["bulbs"][k % len(mats["bulbs"])])
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            comp.set_cast_shadow(False)
            bulbs += 1

    REPORT["budget"] = {
        "point_lights": len(lanterns),
        "point_lights_with_shadows": len(shadow_set),
        "shadow_budget": MAX_SHADOW_CASTERS,
        "festival_bulbs_emissive_only": bulbs,
        "attenuation_radius_cm": LIGHT_RADIUS,
    }
    REPORT["notes"].append("no window meshes or lamp assets in the Stylized_Forest pack: emissive windows skipped, "
                           "lanterns and bulbs are basic-shape stand-ins")
    dd.apply_day()
    dd.save_demo_map()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    dd.write_report_json("step6_build.json", REPORT)
    dd.log("step6 done")


main()
