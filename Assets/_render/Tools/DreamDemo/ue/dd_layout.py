"""Chooses where the demo arena sits on L_Kingdom_Main and writes layout.json.

The arena is a clear patch of the central plaza, between the fountain and the houses, so
CAM_Wide looks across it toward the fountain and the cathedral. Everything else (cameras,
stand-ins, effects) is placed relative to it.
Delete layout.json to choose again; edit it by hand to move the arena.
"""
import json
import math
import os

import unreal

import dd_common as dd

ARENA_RADIUS = 1500.0          # cm, wanted radius (w of MPC_Dream.ArenaCenterRadius); shrinks to fit
SEARCH_CENTER = (3000.0, -2250.0)
SEARCH_HALF = 2400.0
SEARCH_STEP = 150.0
LOOK_AT = (0.0, 0.0)           # the fountain; CAM_Wide looks from the arena toward it
BUILDING_PREFIXES = ("SM_med_", "SM_goth_")


def _obstacles():
    """(label, mesh name, origin xy, half extent xy) of everything that stands on the ground."""
    out = []
    for actor in dd.all_actors():
        if not isinstance(actor, unreal.StaticMeshActor) or actor.actor_has_tag("DreamDemo"):
            continue
        if str(actor.get_folder_path()).startswith("DreamDemo") or dd.is_ground(actor):
            continue
        mesh = actor.static_mesh_component.static_mesh
        if mesh is None:
            continue
        origin, extent = actor.get_actor_bounds(False)
        if extent.z < 15.0 or max(extent.x, extent.y) > 8000.0:      # flat decals, sea, city walls
            continue
        out.append((actor.get_actor_label(), mesh.get_name(), (origin.x, origin.y), (extent.x, extent.y)))
    return out


def _clearance(x, y, obstacle):
    (ox, oy), (ex, ey) = obstacle[2], obstacle[3]
    dx = max(abs(x - ox) - ex, 0.0)
    dy = max(abs(y - oy) - ey, 0.0)
    return math.hypot(dx, dy)


def compute(world):
    obstacles = _obstacles()
    best = None
    steps = int(SEARCH_HALF / SEARCH_STEP)
    for ix in range(-steps, steps + 1):
        for iy in range(-steps, steps + 1):
            x = SEARCH_CENTER[0] + ix * SEARCH_STEP
            y = SEARCH_CENTER[1] + iy * SEARCH_STEP
            clear = min((_clearance(x, y, o) for o in obstacles), default=1e6)
            score = min(clear, ARENA_RADIUS) - math.hypot(x - SEARCH_CENTER[0], y - SEARCH_CENTER[1]) * 0.08
            if best is None or score > best[0]:
                best = (score, x, y, clear)
    _, cx, cy, clear = best
    # the plaza is busy: shrink the arena to the clear patch instead of running into houses
    radius = min(ARENA_RADIUS, max(600.0, math.floor(clear / 50.0) * 50.0))

    heights = [dd.ground_z(world, cx, cy, None)]
    for k in range(12):
        a = 2.0 * math.pi * k / 12
        heights.append(dd.ground_z(world, cx + math.cos(a) * radius, cy + math.sin(a) * radius, None))
    hits = [h for h in heights if h is not None]
    cz = heights[0] if heights[0] is not None else (hits[0] if hits else 0.0)

    buildings = [o for o in obstacles if o[1].startswith(BUILDING_PREFIXES)]
    wall = min(buildings, key=lambda o: _clearance(cx, cy, o)) if buildings else None
    inside = [o[0] for o in obstacles if _clearance(cx, cy, o) < radius]

    layout = {
        "arena_center": [round(cx, 1), round(cy, 1), round(cz, 1)],
        "arena_radius": radius,
        "view_yaw": round(math.degrees(math.atan2(LOOK_AT[1] - cy, LOOK_AT[0] - cx)), 1),
        "nearest_obstacle_cm": round(clear, 1),
        "obstacles_inside_arena": inside,
        "ground_traces_hit": len(hits),
        "height_range_cm": [round(min(hits), 1), round(max(hits), 1)] if hits else None,
        "obstacle_count": len(obstacles),
    }
    if wall is not None:
        layout["wall_building"] = {"label": wall[0], "mesh": wall[1], "origin": [round(v, 1) for v in wall[2]],
                                   "extent": [round(v, 1) for v in wall[3]],
                                   "distance_cm": round(_clearance(cx, cy, wall), 1)}
    with open(dd.LAYOUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(layout, fh, indent=1)
    dd.log("layout %s" % json.dumps(layout))
    return layout


def ensure(world=None):
    if os.path.exists(dd.LAYOUT_JSON):
        return dd.load_layout()
    if world is None:
        world = dd.open_demo_map()
    return compute(world)
