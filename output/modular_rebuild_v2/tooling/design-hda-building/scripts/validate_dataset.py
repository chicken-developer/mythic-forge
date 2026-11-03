#!/usr/bin/env python3
"""Validate HDA batch config structure, not Houdini cook or visual fidelity."""
import argparse
import json
import math
import re
from pathlib import Path

SAFE_ID = re.compile(r"^[A-Za-z0-9_-]+$")
PNG = b"\x89PNG\r\n\x1a\n"


def relative_path(root, value):
    if not isinstance(value, str) or not value or "\\" in value:
        raise ValueError("Expected nonempty POSIX relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or ":" in value:
        raise ValueError("Unsafe path: " + value)
    target = (root / path).resolve()
    if not target.is_relative_to(root.resolve()):
        raise ValueError("Path escapes deliverable: " + value)
    return target


def load_json(path):
    def unique(pairs):
        obj = {}
        for key, value in pairs:
            if key in obj:
                raise ValueError("Duplicate JSON key: " + key)
            obj[key] = value
        return obj
    value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)
    if not isinstance(value, dict) or type(value.get("schema_version")) is not int or value["schema_version"] != 1:
        raise ValueError("Expected object with integer schema_version=1: " + str(path))
    return value


def valid_value(value, spec):
    kind = spec.get("type")
    types = {"integer": lambda v: type(v) is int,
             "number": lambda v: type(v) in (int, float) and math.isfinite(v),
             "boolean": lambda v: type(v) is bool,
             "string": lambda v: isinstance(v, str)}
    if kind not in types or not types[kind](value):
        return False
    if "enum" in spec and value not in spec["enum"]:
        return False
    if kind in ("integer", "number"):
        return ("min" not in spec or value >= spec["min"]) and ("max" not in spec or value <= spec["max"])
    return True


def validate(manifest_path, require_images=False):
    root = manifest_path.resolve().parent
    manifest = load_json(manifest_path)
    schema = load_json(relative_path(root, manifest["parameter_schema"]))
    parameters = schema.get("parameters")
    if not isinstance(parameters, dict) or not parameters:
        raise ValueError("Empty parameter registry")
    for name, spec in parameters.items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", name) or not isinstance(spec, dict):
            raise ValueError("Invalid parameter spec: " + name)
        if "default" not in spec or not valid_value(spec["default"], spec):
            raise ValueError("Invalid parameter default/type/range: " + name)
    generator = relative_path(root, manifest["generator"])
    if not generator.is_file():
        raise ValueError("Missing generator: " + str(generator))
    references = manifest.get("references")
    if not isinstance(references, list) or not references:
        raise ValueError("Missing references")
    refs = {}
    for ref in references:
        if not isinstance(ref, dict) or not isinstance(ref.get("reference_id"), str):
            raise ValueError("Invalid reference record")
        key = ref["reference_id"]
        if not SAFE_ID.fullmatch(key) or key in refs:
            raise ValueError("Invalid/duplicate reference ID: " + key)
        if ref.get("role") not in ("baseline", "variant", "style"):
            raise ValueError("Invalid reference role: " + key)
        if "path" in ref:
            if not relative_path(root, ref["path"]).is_file():
                raise ValueError("Missing reference image: " + key)
        elif not isinstance(ref.get("source"), str) or not ref["source"]:
            raise ValueError("Reference requires path or source: " + key)
        refs[key] = ref
    if not any(r["role"] == "baseline" for r in refs.values()):
        raise ValueError("Baseline reference missing")
    files = manifest.get("datasets")
    if not isinstance(files, list) or not 5 <= len(files) <= 10:
        raise ValueError("Expected 5–10 individual dataset records")
    ids, used_files, outputs, tiers, used_variant_refs = set(), set(), set(), set(), set()
    warnings = []
    records = []
    for entry in files:
        path = relative_path(root, entry)
        if path in used_files:
            raise ValueError("Duplicate dataset path: " + entry)
        used_files.add(path)
        rec = load_json(path)
        key = rec.get("variant_id")
        if not isinstance(key, str) or not SAFE_ID.fullmatch(key) or key in ids:
            raise ValueError("Invalid/duplicate variant ID")
        ids.add(key)
        tier = rec.get("complexity")
        if tier not in ("simple", "medium", "complex"):
            raise ValueError("Invalid complexity: " + key)
        tiers.add(tier)
        if rec.get("source_kind") not in ("input", "designed"):
            raise ValueError("Invalid source_kind: " + key)
        if rec.get("comparison_kind") not in ("baseline", "preset", "sweep"):
            raise ValueError("Invalid comparison_kind: " + key)
        mapping = rec.get("reference_ids")
        if not isinstance(mapping, list) or not mapping or any(not isinstance(r, str) or r not in refs for r in mapping):
            raise ValueError("Missing/unknown reference mapping: " + key)
        variant_refs = {r for r in mapping if refs[r]["role"] == "variant"}
        if rec["source_kind"] == "input" and not variant_refs:
            raise ValueError("Input variant must map to variant reference: " + key)
        if rec["source_kind"] == "designed" and variant_refs:
            raise ValueError("Designed variant cannot claim input variant reference: " + key)
        if used_variant_refs & variant_refs:
            raise ValueError("Input reference reused by multiple records: " + key)
        used_variant_refs.update(variant_refs)
        values = rec.get("parameters")
        if not isinstance(values, dict) or set(values) != set(parameters):
            raise ValueError("Config must contain exactly all schema params: " + key)
        for name, value in values.items():
            if not valid_value(value, parameters[name]):
                raise ValueError("Invalid value for " + key + "." + name)
        seeds = rec.get("seeds")
        if not isinstance(seeds, dict) or not seeds or any(type(v) is not int for v in seeds.values()):
            raise ValueError("Seeds must be nonempty integer mapping: " + key)
        if not isinstance(rec.get("expected_features"), dict) or not rec["expected_features"]:
            raise ValueError("Missing expected_features: " + key)
        if not isinstance(rec.get("module_dependencies"), list) or not isinstance(rec.get("assumptions"), list):
            raise ValueError("Missing dependency/assumption lists: " + key)
        out = rec.get("outputs")
        if not isinstance(out, dict) or not {"geometry", "hero", "comparison", "result"} <= set(out):
            raise ValueError("Missing output paths: " + key)
        for kind, value in out.items():
            target = relative_path(root, value)
            if target in outputs:
                raise ValueError("Reused output path: " + value)
            outputs.add(target)
            if kind in ("hero", "comparison"):
                if target.suffix.lower() != ".png":
                    raise ValueError("Expected PNG output: " + value)
                if require_images:
                    with target.open("rb") as image:
                        if image.read(8) != PNG:
                            raise ValueError("Invalid PNG signature: " + value)
        records.append(rec)
    for rec in records:
        if rec["comparison_kind"] == "sweep":
            baseline = rec.get("baseline_variant_id")
            changed = rec.get("changed_parameters")
            if baseline not in ids or baseline == rec["variant_id"] or not isinstance(changed, list) or not changed:
                raise ValueError("Sweep needs valid baseline and changed params: " + rec["variant_id"])
            if any(name not in parameters for name in changed):
                raise ValueError("Unknown changed sweep parameter")
            base = next(r for r in records if r["variant_id"] == baseline)
            actual = {n for n in parameters if rec["parameters"][n] != base["parameters"][n]}
            if actual != set(changed) or rec["seeds"] != base["seeds"]:
                raise ValueError("Sweep differs from declared changes/seeds: " + rec["variant_id"])
    uncovered = {r for r in refs if refs[r]["role"] == "variant"} - used_variant_refs
    if uncovered:
        raise ValueError("Unmapped input variants: " + ", ".join(sorted(uncovered)))
    missing_tiers = {"simple", "medium", "complex"} - tiers
    if missing_tiers:
        warnings.append("Missing complexity coverage: " + ", ".join(sorted(missing_tiers)))
    return len(ids), warnings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--require-images", action="store_true")
    args = parser.parse_args()
    try:
        count, warnings = validate(args.manifest, args.require_images)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(1, "FAIL: " + str(exc) + "\n")
    print("PASS: {} config records (structure only; not Houdini/visual validation)".format(count))
    for warning in warnings:
        print("WARNING: " + warning)


if __name__ == "__main__":
    main()
