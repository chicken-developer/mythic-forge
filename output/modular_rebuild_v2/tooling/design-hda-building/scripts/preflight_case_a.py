#!/usr/bin/env python3
"""Validate Case A architecture intent; optionally draw a 2D diagnostic PNG.

The preview is NOT a Houdini mesh/render, roof-intersection solution, or bake QA.
The script uses only Python stdlib for rules; Pillow is optional for a PNG.
"""

import argparse
import copy
import hashlib
import json
import math
import sys
from pathlib import Path


DEMO = Path(__file__).resolve().parent.parent / "assets" / "case_a_semantic_demo.json"
EPS = 1.0e-6
SUPPORTED_ROOFS = {"gable", "hip"}
SUPPORTED_JOINTS = {"valley", "ridge", "abutment"}


def is_number(value):
    return type(value) in (float, int) and math.isfinite(value)


def length(wall):
    p, q = wall["start_xz"], wall["end_xz"]
    return math.hypot(q[0] - p[0], q[1] - p[1])


def bounds(patch):
    points = patch["footprint_xz"]
    return (
        min(p[0] for p in points), min(p[1] for p in points),
        max(p[0] for p in points), max(p[1] for p in points)
    )


def overlaps_box(a, b):
    """Coarse XY-plan overlap: a conservative preflight, not roof CSG."""
    ax0, az0, ax1, az1 = bounds(a)
    bx0, bz0, bx1, bz1 = bounds(b)
    return min(ax1, bx1) - max(ax0, bx0) > EPS and min(az1, bz1) - max(az0, bz0) > EPS


def _unique_id(records, label, problems):
    ids = set()
    for obj in records:
        key = obj.get("id") if isinstance(obj, dict) else None
        if not isinstance(key, str) or not key:
            problems.append(label + " has invalid/missing id")
        elif key in ids:
            problems.append(label + " duplicate id " + key)
        else:
            ids.add(key)
    return ids


def validate(data):
    """Return (errors, warnings). Do not mutate input."""
    problems, warnings = [], []
    if not isinstance(data, dict) or data.get("schema_version") != 1:
        return ["Expected schema_version=1 object"], warnings
    if data.get("target") != "unreal_editor_bake":
        problems.append("Expected target=unreal_editor_bake")
    if not isinstance(data.get("building_id"), str) or not data["building_id"]:
        problems.append("Missing building_id")
    if type(data.get("seed")) is not int:
        problems.append("seed must be an integer")
    for p in ("course_height_m", "module_length_m", "min_piece_m"):
        if not is_number(data.get(p)) or data[p] <= EPS:
            problems.append(p + " must be finite and positive")
    if problems:
        return problems, warnings

    walls = data.get("walls", [])
    openings = data.get("openings", [])
    roofs = data.get("roof_patches", [])
    junctions = data.get("roof_junctions", [])
    for label, records in (("walls", walls), ("openings", openings), ("roofs", roofs), ("junctions", junctions)):
        if not isinstance(records, list):
            problems.append(label + " must be a list")
    if problems:
        return problems, warnings
    if not walls or not roofs:
        problems.append("At least one wall and one roof patch are required")
    wall_ids = _unique_id(walls, "wall", problems)
    opening_ids = _unique_id(openings, "opening", problems)
    roof_ids = _unique_id(roofs, "roof", problems)
    _unique_id(junctions, "junction", problems)
    if wall_ids & opening_ids or wall_ids & roof_ids or opening_ids & roof_ids:
        problems.append("IDs must be unique across wall/opening/roof feature kinds")

    wall_by_id = {}
    for wall in walls:
        try:
            for attr in ("start_xz", "end_xz"):
                pt = wall[attr]
                if len(pt) != 2 or any(not is_number(v) for v in pt):
                    raise ValueError(attr + " must be [x,z]")
            if not is_number(wall["height_m"]) or wall["height_m"] <= EPS:
                raise ValueError("Invalid height")
            if not is_number(wall["thickness_m"]) or wall["thickness_m"] <= EPS:
                raise ValueError("Invalid thickness")
            if length(wall) <= EPS:
                raise ValueError("Zero-length wall")
            if wall.get("surface") not in ("wood_planks", "stone", "plaster", "brick"):
                raise ValueError("Unsupported wall surface")
            wall_by_id[wall["id"]] = wall
        except (KeyError, TypeError, ValueError, IndexError) as exc:
            problems.append("Invalid wall " + str(wall.get("id")) + ": " + str(exc))

    opening_by_wall = {name: [] for name in wall_by_id}
    for opening in openings:
        host = opening.get("wall_id")
        if host not in wall_by_id:
            problems.append("Dangling wall_id for " + str(opening.get("id")))
            continue
        wall = wall_by_id[host]
        values = ("u_m", "width_m", "sill_m", "height_m")
        if any(not is_number(opening.get(key)) for key in values):
            problems.append("Invalid opening coordinates: " + str(opening.get("id")))
            continue
        a, w, y, h = (opening[k] for k in values)
        if opening.get("kind") not in ("door", "window"):
            problems.append("Unsupported opening type: " + str(opening.get("id")))
        if a < 0 or w <= EPS or y < 0 or h <= EPS or a + w > length(wall) + EPS or y + h > wall["height_m"] + EPS:
            problems.append("Opening outside wall: " + str(opening.get("id")))
        if opening.get("kind") == "door" and y > EPS:
            warnings.append("Raised door sill (may not be accessible): " + str(opening.get("id")))
        opening_by_wall[host].append(opening)

    for wall_id, items in opening_by_wall.items():
        for i, a in enumerate(items):
            for b in items[i + 1:]:
                horiz = min(a["u_m"] + a["width_m"], b["u_m"] + b["width_m"]) - max(a["u_m"], b["u_m"])
                vert = min(a["sill_m"] + a["height_m"], b["sill_m"] + b["height_m"]) - max(a["sill_m"], b["sill_m"])
                if horiz > EPS and vert > EPS:
                    problems.append("Overlapping openings on " + wall_id + ": " + a["id"] + " + " + b["id"])

    roof_by_id = {}
    for roof in roofs:
        try:
            if roof.get("roof_type") not in SUPPORTED_ROOFS:
                raise ValueError("Unsupported roof type: " + str(roof.get("roof_type")))
            points = roof["footprint_xz"]
            if len(points) != 4 or any(len(pt) != 2 or any(not is_number(v) for v in pt) for pt in points):
                raise ValueError("Diagnostic supports rectangular footprint_xz only")
            x0, z0, x1, z1 = bounds(roof)
            expected = {(x0,z0), (x1,z0), (x1,z1), (x0,z1)}
            if set(tuple(p) for p in points) != expected or x1-x0 < EPS or z1-z0 < EPS:
                raise ValueError("Diagnostic roof patch must be axis-aligned rectangle")
            if not is_number(roof.get("pitch_deg")) or not 5 <= roof["pitch_deg"] <= 75:
                raise ValueError("Invalid pitch_deg")
            if not is_number(roof.get("eave_m")) or roof["eave_m"] < 0:
                raise ValueError("Invalid eave_m")
            roof_by_id[roof["id"]] = roof
        except (KeyError, TypeError, ValueError) as exc:
            problems.append("Invalid roof " + str(roof.get("id")) + ": " + str(exc))

    treated = set()
    for junction in junctions:
        pair = frozenset((junction.get("a"), junction.get("b")))
        if len(pair) != 2 or any(v not in roof_by_id for v in pair):
            problems.append("Unknown/self roof junction: " + str(junction.get("id")))
        if junction.get("type") not in SUPPORTED_JOINTS:
            problems.append("Unsupported roof junction type: " + str(junction.get("id")))
        if pair in treated:
            problems.append("Duplicate roof junction: " + str(junction.get("id")))
        treated.add(pair)

    for i, a in enumerate(roofs):
        for b in roofs[i + 1:]:
            if a["id"] in roof_by_id and b["id"] in roof_by_id and overlaps_box(a, b):
                if frozenset((a["id"], b["id"])) not in treated:
                    problems.append("Missing roof junction: " + a["id"] + " + " + b["id"])
    warnings.append("Roof AABB checks are a coarse 2D preflight; true valley meshes need Houdini cook")
    return problems, warnings


def _subtract(interval, blockers):
    """Subtract clipped wall-local opening intervals from one course module."""
    result = [interval]
    for left, right in sorted(blockers):
        updated = []
        for a, b in result:
            if right <= a + EPS or left >= b - EPS:
                updated.append((a, b))
            else:
                if left > a + EPS:
                    updated.append((a, min(left, b)))
                if right < b - EPS:
                    updated.append((max(right, a), b))
        result = updated
    return [(round(a, 5), round(b, 5)) for a, b in result if b - a > EPS]


def courses_for_wall(data, wall_id):
    """Produce deterministic *2D intervals*, not triangles or Houdini geometry."""
    wall = next(w for w in data["walls"] if w["id"] == wall_id)
    wall_length = length(wall)
    pitch = data["course_height_m"]
    module = data["module_length_m"]
    n = int(math.ceil(wall["height_m"] / pitch))
    if n > 256:
        raise ValueError("Course count above diagnostic limit")
    openings = [op for op in data["openings"] if op["wall_id"] == wall_id]
    pieces = []
    for row in range(n):
        low, high = row * pitch, min(wall["height_m"], (row + 1) * pitch)
        blockers = [(o["u_m"], o["u_m"] + o["width_m"]) for o in openings
                    if o["sill_m"] < high - EPS and o["sill_m"] + o["height_m"] > low + EPS]
        phase = module * 0.5 if row % 2 else 0.0
        cuts = [0.0]
        x = phase
        while x < wall_length - EPS:
            if x > EPS:
                cuts.append(x)
            x += module
        cuts.append(wall_length)
        for cell, (start, end) in enumerate(zip(cuts, cuts[1:])):
            for sub, (a, b) in enumerate(_subtract((start, end), blockers)):
                pieces.append({
                    "wall_id": wall_id,
                    "row": row,
                    "cell": cell,
                    "sub": sub,
                    "u0": a,
                    "u1": b,
                    "y0": round(low, 5),
                    "y1": round(high, 5)
                })
    return pieces


def signature(records):
    raw = json.dumps(records, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def make_comparison(data, shift):
    problems, warnings = validate(data)
    if problems:
        raise ValueError("; ".join(problems))
    before = {w["id"]: courses_for_wall(data, w["id"]) for w in data["walls"]}
    moved = copy.deepcopy(data)
    changed = next((o for o in moved["openings"] if o["id"] == "window_01"), None)
    if changed is None:
        raise ValueError("Demo/sweep requires opening_id window_01")
    changed["u_m"] = round(changed["u_m"] + shift, 5)
    new_problems, new_warnings = validate(moved)
    if new_problems:
        raise ValueError("Moved opening invalid: " + "; ".join(new_problems))
    after = {w["id"]: courses_for_wall(moved, w["id"]) for w in data["walls"]}
    changed_walls = [key for key in before if signature(before[key]) != signature(after[key])]
    if changed_walls != ["front"]:
        raise ValueError("Expected only front wall to change; got " + repr(changed_walls))
    report = {
        "schema_version": 1,
        "status": "pass",
        "preview_provenance": "semantic_rule_preview",
        "scope": "2D wall-course interval splitting and coarse roof junction intent only",
        "input_building_id": data["building_id"],
        "window_moved_m": shift,
        "wall_count": len(data["walls"]),
        "opening_count": len(data["openings"]),
        "roof_patch_count": len(data["roof_patches"]),
        "roof_junction_count": len(data["roof_junctions"]),
        "changed_walls": changed_walls,
        "unchanged_wall_hashes": {k: signature(before[k]) for k in before if k not in changed_walls},
        "front_course_count_before": len(before["front"]),
        "front_course_count_after": len(after["front"]),
        "front_signature_before": signature(before["front"]),
        "front_signature_after": signature(after["front"]),
        "warnings": warnings + new_warnings,
        "houdini_cooked": "not_run",
        "geometry_exported": "not_run",
        "rendered": "not_run",
        "hda_packaged": "not_run",
        "unreal_baked": "not_run"
    }
    return moved, before, after, report


def self_test():
    source = json.loads(DEMO.read_text(encoding="utf-8"))
    assert validate(source)[0] == [], validate(source)
    moved, before, after, report = make_comparison(source, 0.55)
    assert source["openings"][1]["u_m"] == 3.05, "Mutation of input fixture"
    assert report["changed_walls"] == ["front"]
    assert before["east"] == after["east"]
    assert signature(before["front"]) != signature(after["front"])
    assert signature(before["front"]) == signature(courses_for_wall(source, "front"))

    def should_fail(mutator, token):
        candidate = copy.deepcopy(source)
        mutator(candidate)
        errors, _ = validate(candidate)
        assert any(token in e for e in errors), (token, errors)

    should_fail(lambda d: d["openings"][1].update(wall_id="missing"), "Dangling wall_id")
    should_fail(lambda d: d["openings"][1].update(u_m=7.6), "outside wall")
    should_fail(lambda d: d["openings"][1].update(u_m=5.4), "Overlapping openings")
    should_fail(lambda d: d["roof_junctions"].clear(), "Missing roof junction")
    should_fail(lambda d: d["roof_patches"][0].update(roof_type="cone"), "Unsupported roof type")
    should_fail(lambda d: d["walls"][1].update(id="front"), "duplicate id")
    should_fail(lambda d: d["roof_junctions"][0].update(a="other"), "Unknown/self")
    assert moved["openings"][1]["u_m"] == 3.6
    print("PASS: 10 semantic checks (rules only; no Houdini geometry produced)")


def _font(size, bold=False):
    from PIL import ImageFont
    fn = "/usr/share/fonts/truetype/dejavu/DejaVuSans" + ("-Bold.ttf" if bold else ".ttf")
    try:
        return ImageFont.truetype(fn, size)
    except OSError:
        return ImageFont.load_default()


def _rounded(draw, box, fill, outline=None, radius=18, width=2):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def render_png(source, moved, before, after, report, output_file):
    """Draw planning diagrams; deliberately no mesh generation/rendering."""
    from PIL import Image, ImageDraw

    bg = (235, 231, 220)
    ink = (43, 53, 54)
    img = Image.new("RGB", (1800, 1230), bg)
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, 0, 1800, 140), fill=(28, 49, 54))
    draw.text((66, 31), "CASE A  /  CONTEXT-AWARE BUILDING", font=_font(37, True), fill=(247, 238, 219))
    draw.text((69, 91), "Semantic-rule diagnostic  |  NOT a Houdini render or geometry cook", font=_font(19), fill=(183, 209, 204))

    def elevation(x, y, title, data, pieces, accent):
        bx = (x, y, x + 818, y + 515)
        _rounded(draw, bx, (249, 247, 240), (208, 203, 191), radius=22)
        draw.text((x + 33, y + 24), title, font=_font(26, True), fill=ink)
        draw.text((x + 34, y + 65), "Front facade  |  planks split around live opening intervals", font=_font(15), fill=(96, 105, 102))
        left, right = x + 40, x + 778
        wall_top, wall_bottom = y + 174, y + 428
        scale_x = (right - left) / 8.0
        scale_y = (wall_bottom - wall_top) / 4.6

        # A simple elevation *symbol*, not solved roof/mesh topology.
        roof_peak = (left + right) * .5
        draw.polygon([(left - 19, wall_top), (roof_peak, wall_top - 88), (right + 19, wall_top)],
                     fill=(69, 90, 92))
        for rx in range(int(left + 10), int(right), 26):
            roof_y = wall_top - 88 + abs(rx - roof_peak) * 88 / (right-left) * 2
            draw.line([(rx, roof_y + 4), (rx + 11, roof_y + 13)], fill=(105, 127, 127), width=2)
        draw.rectangle((left, wall_top, right, wall_bottom), fill=(201, 179, 141))

        # Each rectangle represents a 2D course interval, not a generated 3D plank.
        for part in pieces:
            x0 = left + part["u0"] * scale_x + 1
            x1 = left + part["u1"] * scale_x - 1
            yy0 = wall_bottom - part["y1"] * scale_y + 1
            yy1 = wall_bottom - part["y0"] * scale_y - 1
            tone = (171, 122, 79) if part["row"] % 2 else (189, 143, 94)
            if x1 - x0 >= 1 and yy1 - yy0 >= 1:
                draw.rectangle((x0, yy0, x1, yy1), fill=tone, outline=(109, 83, 61), width=1)

        for opening in [o for o in data["openings"] if o["wall_id"] == "front"]:
            x0 = left + opening["u_m"] * scale_x
            x1 = left + (opening["u_m"] + opening["width_m"]) * scale_x
            top = wall_bottom - (opening["sill_m"] + opening["height_m"]) * scale_y
            bottom = wall_bottom - opening["sill_m"] * scale_y
            draw.rectangle((x0 - 5, top - 5, x1 + 5, bottom + 1), fill=(226, 199, 149))
            draw.rectangle((x0, top, x1, bottom), fill=(42, 65, 74), outline=(243, 218, 158), width=4)
            if opening["kind"] == "window":
                mid = (x0 + x1) * .5
                draw.line((mid, top + 2, mid, bottom - 2), fill=(222, 203, 158), width=4)
                draw.line((x0 + 1, top + (bottom-top)*.47, x1 - 1, top + (bottom-top)*.47), fill=(222, 203, 158), width=4)
                draw.text((x0 - 7, bottom + 9), opening["id"], font=_font(12), fill=accent)
            else:
                draw.ellipse((x1-14, top + (bottom-top)*.55, x1-8, top + (bottom-top)*.55 + 6), fill=(238, 213, 138))
        draw.line((left - 20, wall_bottom, right + 20, wall_bottom), fill=(90, 104, 98), width=5)
        draw.text((left + 3, y + 461), "8.0 m facade width", font=_font(15), fill=(92, 98, 96))
        draw.text((right - 200, y + 461), "Wall: front", font=_font(16, True), fill=accent)

    elevation(59, 157, "BEFORE  /  window at 3.05 m", source, before["front"], (130, 102, 65))
    elevation(924, 157, "AFTER   /  window at 3.60 m", moved, after["front"], (45, 121, 115))

    _rounded(draw, (59, 698, 876, 1154), (249, 247, 240), (208, 203, 191), radius=22)
    draw.text((89, 721), "TOP VIEW  /  semantic roof patches", font=_font(25, True), fill=ink)
    draw.text((90, 762), "Overlap demands a declared valley solver; this picture does not solve one.", font=_font(15), fill=(96, 105, 102))
    plan_o = (175, 830)
    ps = 37.0

    def plan_point(pt):
        return (plan_o[0] + pt[0] * ps, plan_o[1] + pt[1] * ps)

    for patch, fill, outline in [
        (source["roof_patches"][0], (185, 204, 202), (48, 105, 111)),
        (source["roof_patches"][1], (228, 186, 147), (158, 91, 69))
    ]:
        draw.polygon([plan_point(p) for p in patch["footprint_xz"]], fill=fill)
        draw.line([plan_point(p) for p in patch["footprint_xz"]] + [plan_point(patch["footprint_xz"][0])], fill=outline, width=5, joint="curve")
        draw.text((plan_point(patch["footprint_xz"][0])[0] + 10, plan_point(patch["footprint_xz"][0])[1] + 10),
                  patch["id"], font=_font(15, True), fill=outline)
    # Diagrammatic overlap marker: declared relationship, not computed valley curve.
    draw.line((plan_o[0]+5*ps, plan_o[1]+4*ps, plan_o[0]+8*ps, plan_o[1]+4*ps),
              fill=(160, 68, 69), width=6)
    draw.text((505, 956), "roof junction", font=_font(18, True), fill=(160, 68, 69))
    draw.text((505, 987), "type: valley", font=_font(17), fill=ink)
    draw.text((505, 1020), "status: intent validated", font=_font(17), fill=ink)
    draw.text((505, 1053), "mesh: pending Houdini", font=_font(17), fill=(151, 80, 65))

    _rounded(draw, (924, 698, 1741, 1154), (249, 247, 240), (208, 203, 191), radius=22)
    draw.text((959, 726), "PRE-HOUDINI  /  VALIDATION", font=_font(24, True), fill=ink)
    stats = [
        ("Semantic references", "PASS", (38, 112, 91)),
        ("Opening bounds / overlaps", "PASS", (38, 112, 91)),
        ("Declared roof junction", "PASS", (38, 112, 91)),
        ("Changed walls", ", ".join(report["changed_walls"]), (38, 112, 91)),
        ("Unrelated east wall", "UNCHANGED", (38, 112, 91)),
        ("Native SOP geometry cook", "NOT RUN", (157, 95, 65)),
        ("Roof valley / UV / collision", "NOT RUN", (157, 95, 65)),
        ("Unreal Editor bake", "NOT RUN", (157, 95, 65)),
    ]
    cy = 788
    for label, value, color in stats:
        draw.text((963, cy), label, font=_font(18), fill=ink)
        draw.text((1393, cy), value, font=_font(17, True), fill=color)
        cy += 40
    draw.line((960, 1117, 1700, 1117), fill=(210, 205, 194), width=2)
    draw.text((966, 1124), "Preview evidence: 2D semantic intervals only", font=_font(14), fill=(113, 116, 108))
    img.save(output_file, "PNG", optimize=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    src = parser.add_mutually_exclusive_group()
    src.add_argument("--demo", action="store_true", help="Use bundled synthetic golem blacksmith fixture")
    src.add_argument("--input", type=Path, help="Path to a semantic Case A JSON fixture")
    parser.add_argument("--output-dir", type=Path, help="Output directory for report and optional PNG")
    parser.add_argument("--shift-window-m", type=float, default=0.55, help="Synthetic move sweep of window_01")
    parser.add_argument("--self-test", action="store_true", help="Run deterministic rule tests")
    args = parser.parse_args(argv)
    if args.self_test:
        self_test()
    if not (args.demo or args.input):
        return 0 if args.self_test else parser.error("Use --demo, --input or --self-test")
    if args.output_dir is None:
        parser.error("--output-dir is required for --demo or --input")
    if not math.isfinite(args.shift_window_m):
        parser.error("--shift-window-m must be finite")
    source_path = DEMO if args.demo else args.input
    try:
        source = json.loads(source_path.read_text(encoding="utf-8"))
        moved, before, after, report = make_comparison(source, args.shift_window_m)
    except (ValueError, OSError, KeyError, TypeError, IndexError) as exc:
        print("FAIL: " + str(exc), file=sys.stderr)
        return 1
    args.output_dir.mkdir(parents=True, exist_ok=True)
    png_path = args.output_dir / "case_a_semantic_preview.png"
    try:
        render_png(source, moved, before, after, report, png_path)
        report["diagnostic_png"] = png_path.name
    except ImportError:
        report["diagnostic_png"] = "not_run (Pillow missing)"
    (args.output_dir / "semantic_preflight.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("PASS: semantic preflight; changed walls: " + ",".join(report["changed_walls"]))
    print("Evidence: " + str(args.output_dir / "semantic_preflight.json"))
    print("Preview: " + str(report["diagnostic_png"]))
    print("Houdini/Unreal: NOT RUN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
