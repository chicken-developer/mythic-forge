"""Step 7 (level checks): things a script can verify without rendering.

  1. Nothing warm (light or emissive) is active in DREAM.
  2. No memory bubble or floating prop inside 70 % of the arena radius.
  3. Custom stencil values follow the table in TECH_DREAM_VISUALS 4.3.
  4. Lantern flames belong to Day/Night only, lantern stars to Dream only.

Run:  bash Tools/DreamDemo/ue_run.sh cmd Tools/DreamDemo/ue/step7_checks.py dd_step7
Writes RenderTests/DreamDemo/step7_level_checks.json. Read-only: the map is not saved.
"""
import colorsys
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                "Tools", "DreamDemo", "ue"))
import dd_common as dd  # noqa: E402

WARM_HUE_RANGE = (15.0, 60.0)
WARM_MIN_SATURATION = 0.4
STATE_TAGS = (dd.TAG_DAY, dd.TAG_NIGHT, dd.TAG_DREAM)


def is_warm(r, g, b):
    h, s, v = colorsys.rgb_to_hsv(max(r, 0.0), max(g, 0.0), max(b, 0.0))
    hue = h * 360.0
    return v > 0.01 and WARM_HUE_RANGE[0] <= hue <= WARM_HUE_RANGE[1] and s > WARM_MIN_SATURATION, round(hue, 1), round(s, 2)


def active_in_dream(actor):
    tags = [str(t) for t in actor.tags]
    if not any(t in tags for t in STATE_TAGS):
        return True                      # shared actors are visible in every state
    return dd.TAG_DREAM in tags


def emissive_color(material):
    """Colour and glow of materials built on M_DreamEmissive; None for anything else."""
    base = material
    while isinstance(base, unreal.MaterialInstance):
        base = base.get_editor_property("parent")
    if base is None or base.get_name() != "M_DreamEmissive":
        return None
    if isinstance(material, unreal.MaterialInstanceConstant):
        color = dd.mel.get_material_instance_vector_parameter_value(material, "Color")
        glow = dd.mel.get_material_instance_scalar_parameter_value(material, "Glow")
        return color, glow
    return unreal.LinearColor(1, 1, 1, 1), 1.0


def main():
    dd.open_demo_map()
    layout = dd.load_layout()
    cx, cy, _ = layout["arena_center"]
    radius = layout["arena_radius"]
    limit = radius * 0.7
    result = {}

    # 1. warm sources active in DREAM
    warm, lights_checked, emissive_checked = [], 0, 0
    for actor in dd.all_actors():
        if not active_in_dream(actor):
            continue
        for comp in actor.get_components_by_class(unreal.LightComponent):
            # judged by state tags, not by current visibility (the map is saved in the DAY state)
            if isinstance(comp, unreal.SkyLightComponent):
                continue
            lights_checked += 1
            c = comp.get_editor_property("light_color")
            hit, hue, sat = is_warm(c.r / 255.0, c.g / 255.0, c.b / 255.0)
            if hit:
                warm.append({"actor": actor.get_actor_label(), "kind": comp.get_class().get_name(),
                             "hue": hue, "saturation": sat})
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            for index in range(comp.get_num_materials()):
                material = comp.get_material(index)
                info = emissive_color(material) if material else None
                if info is None:
                    continue
                emissive_checked += 1
                color, glow = info
                hit, hue, sat = is_warm(color.r, color.g, color.b)
                # the Xenon weak spot is gameplay colour (stencil 3), not environment lighting
                if hit and glow > 0.0 and not actor.get_actor_label().startswith("Xenon_WeakSpot"):
                    warm.append({"actor": actor.get_actor_label(), "kind": "emissive " + material.get_name(),
                                 "hue": hue, "saturation": sat})
    result["dream_warm_sources"] = {
        "pass": not warm, "offenders": warm, "lights_checked": lights_checked,
        "emissive_materials_checked": emissive_checked,
        "rule": "hue %d-%d deg and saturation > %.1f" % (WARM_HUE_RANGE[0], WARM_HUE_RANGE[1], WARM_MIN_SATURATION),
        "note": "Xenon_WeakSpot is excluded: it is a gameplay colour, visible in every state",
    }

    # 2. 70 % rule
    inside, floaters, bubbles = [], 0, 0
    min_float, min_bubble = 1e9, 1e9
    for actor in dd.all_actors():
        if actor.actor_has_tag("FX_Float"):
            floaters += 1
            loc = actor.get_actor_location()
            d = math.hypot(loc.x - cx, loc.y - cy)
            min_float = min(min_float, d)
            if d < limit:
                inside.append({"actor": actor.get_actor_label(), "distance_cm": round(d)})
        if actor.actor_has_tag("FX_Bubbles"):
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                for i in range(comp.get_instance_count()):
                    t = comp.get_instance_transform(i, True)
                    bubbles += 1
                    d = math.hypot(t.translation.x - cx, t.translation.y - cy)
                    min_bubble = min(min_bubble, d)
                    if d < limit:
                        inside.append({"actor": "%s[%d]" % (actor.get_actor_label(), i), "distance_cm": round(d)})
    result["arena_center_clear"] = {
        "pass": not inside, "limit_cm": limit, "offenders": inside,
        "floaters": floaters, "bubbles": bubbles,
        "nearest_floater_cm": round(min_float), "nearest_bubble_cm": round(min_bubble),
        "note": "bubbles sway up to 30 cm sideways by world position offset; the margin covers it",
    }

    # 3. stencil table
    expected = {"Golem_StandIn": 1, "Xenon_StandIn": 2, "Xenon_WeakSpot": 3}
    stencil, bad = [], []
    for actor in dd.all_actors():
        label = actor.get_actor_label()
        for prefix, value in expected.items():
            if label.startswith(prefix):
                for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
                    entry = {"actor": label, "render_custom_depth": bool(comp.render_custom_depth),
                             "stencil": comp.custom_depth_stencil_value, "expected": value}
                    stencil.append(entry)
                    if not entry["render_custom_depth"] or entry["stencil"] != value:
                        bad.append(entry)
    result["stencil_table"] = {"pass": bool(stencil) and not bad, "entries": stencil, "offenders": bad}

    # 4. lantern tags
    lantern_bad, flames, stars = [], 0, 0
    for actor in dd.all_actors():
        tags = [str(t) for t in actor.tags]
        if "DreamDemo.LanternFlame" in tags or "DreamDemo.LanternLight" in tags or "DreamDemo.FestivalBulb" in tags:
            flames += 1
            if dd.TAG_DREAM in tags:
                lantern_bad.append(actor.get_actor_label())
        if "DreamDemo.LanternStar" in tags:
            stars += 1
            if dd.TAG_DAY in tags or dd.TAG_NIGHT in tags or dd.TAG_DREAM not in tags:
                lantern_bad.append(actor.get_actor_label())
    result["lantern_states"] = {"pass": not lantern_bad and flames > 0 and stars > 0,
                                "warm_actors": flames, "star_actors": stars, "offenders": lantern_bad}

    dd.write_report_json("step7_level_checks.json", result)
    dd.log("step7 checks: " + ", ".join("%s=%s" % (k, v["pass"]) for k, v in result.items()))


main()
