"""Audit repository source/config paths without importing Houdini or Unreal.

Run with Python 3.10+ from any directory. Saved state, reports, third-party
plugins, generated files and archived authoring copies are excluded.
"""
from __future__ import annotations

import argparse
import ast
from collections import Counter
import json
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SKIP = {".git", ".vs", ".idea", "__pycache__", "Binaries", "Intermediate",
        "DerivedDataCache", "Saved", "Plugins", "backup", "backups",
        "native_release", "relocated", "manual_refactor"}
RETIRED = re.compile(r"HoudiniWorkspace[/\\]|SubstanceWorkspace[/\\]|"
                     r"(?<![A-Za-z_])TDKProps[/\\](?!TDKProps\.(?:Build\.cs|cpp|h))|"
                     r"TDKKingdomMap[/\\]")
REQUIRED = (
    "ThreeDreamGolems/ThreeDreamGolems.uproject",
    "Assets/_render/TDKProps.uproject",
    "Assets/_composition/TDKKingdomMap.uproject",
    "Assets/_render/Source/TDKProps/TDKProps.Build.cs",
    "Assets/hda_gothic_building/gothic_building.hip",
    "Assets/hda_medieval_building/AGENTS.md",
    "Assets/hda_medieval_building/build_hda.py",
    "Assets/hda_medieval_building/asset/medieval_house_layout.hda",
    "Assets/hda_samples/AllHDAs/House.hdalc",
    "Assets/texture_gothic/material_manifest.json",
    ".agents/skills/design-hda-building/SKILL.md",
    "MEMORY.md",
)
PATH_VARIABLES = {"ROOT", "REPO", "PROJECT", "P", "SOURCE", "SRC", "SAMPLES",
                  "S", "HERE", "TOOLS", "_ROOT", "_PROJECT_ROOT"}


def source_files():
    for folder in (ROOT / "Assets", ROOT / "ThreeDreamGolems"):
        for current, dirs, names in os.walk(folder):
            dirs[:] = [d for d in dirs if d not in SKIP and not d.startswith("relocated_")]
            for name in names:
                path = Path(current) / name
                if path.suffix.lower() in {".py", ".json", ".ini", ".uproject"}:
                    yield path


def active_json(path):
    rel = path.relative_to(ROOT).as_posix()
    if "/results/" in rel or "/reports/" in rel or "/RenderTests/" in rel:
        return False
    return ("/SourceAssets/" in rel or "/function_refactor/project/" in rel
            or path.name in {"manifest.json", "parameter_schema.json", "materials.json",
                             "material_manifest.json", "skytown_library.json"})


def evaluate(node, values):
    """Resolve literal pathlib expressions, without executing source code."""
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.Name):
        return values[node.id]
    if isinstance(node, ast.Call) and not node.keywords:
        if isinstance(node.func, ast.Name) and node.func.id == "Path" and len(node.args) == 1:
            return Path(evaluate(node.args[0], values))
        if isinstance(node.func, ast.Attribute) and node.func.attr == "resolve" and not node.args:
            return evaluate(node.func.value, values).resolve()
    if isinstance(node, ast.Attribute) and node.attr in {"parent", "parents"}:
        return getattr(evaluate(node.value, values), node.attr)
    if isinstance(node, ast.Subscript):
        return evaluate(node.value, values)[evaluate(node.slice, values)]
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Div):
        return evaluate(node.left, values) / evaluate(node.right, values)
    raise ValueError("Dynamic expression")


def json_paths(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for item in value.values():
            yield from json_paths(item)
    elif isinstance(value, list):
        for item in value:
            yield from json_paths(item)


def audit():
    failures, root_checks, counts = [], [], Counter()
    for relative in REQUIRED:
        if not (ROOT / relative).exists():
            failures.append({"file": relative, "issue": "missing required entry point"})
    for path in source_files():
        relative = path.relative_to(ROOT).as_posix()
        text = path.read_text(encoding="utf-8-sig")
        if path.suffix == ".py":
            counts["python_files"] += 1
            try:
                tree = ast.parse(text, filename=relative)
            except SyntaxError as exc:
                failures.append({"file": relative, "line": exc.lineno, "issue": str(exc)})
                continue
            values = {"__file__": str(path)}
            for statement in tree.body:
                if not isinstance(statement, ast.Assign):
                    continue
                try:
                    value = evaluate(statement.value, values)
                except (KeyError, ValueError, TypeError, IndexError, AttributeError):
                    continue
                for target in statement.targets:
                    if not isinstance(target, ast.Name):
                        continue
                    values[target.id] = value
                    if target.id in PATH_VARIABLES and isinstance(value, Path):
                        exists = value.exists()
                        root_checks.append({"file": relative, "name": target.id,
                                            "path": str(value), "exists": exists})
                        if not exists:
                            failures.append({"file": relative, "line": statement.lineno,
                                             "issue": "missing source/root", "path": str(value)})
            for number, line in enumerate(text.splitlines(), 1):
                if RETIRED.search(line):
                    failures.append({"file": relative, "line": number,
                                     "issue": "retired path reference", "text": line.strip()[:240]})
                if relative.startswith("Assets/_render/Tools/") and re.search(
                        r"P\.parent\s*/\s*row\['file'\]|relative_to\(P\.parent\)", line):
                    failures.append({"file": relative, "line": number,
                                     "issue": "mesh manifest paths must use the repository root, P.parent.parent"})
        elif path.suffix in {".json", ".uproject"}:
            counts["json_files"] += 1
            try:
                data = json.loads(text)
            except ValueError as exc:
                failures.append({"file": relative, "issue": str(exc)})
                continue
            if active_json(path):
                counts["active_json_files"] += 1
                for number, line in enumerate(text.splitlines(), 1):
                    if RETIRED.search(line):
                        failures.append({"file": relative, "line": number,
                                         "issue": "retired path in active manifest"})
                for value in json_paths(data):
                    normalized = value.replace("\\", "/")
                    target = None
                    if normalized.startswith(ROOT.as_posix() + "/"):
                        target = Path(value)
                    elif normalized.startswith("Assets/"):
                        target = ROOT / normalized
                    if target is not None:
                        counts["manifest_file_paths"] += 1
                        if not target.exists():
                            failures.append({"file": relative, "issue": "missing manifest resource",
                                             "path": value})
    return {"status": "passed" if not failures else "failed", "root": str(ROOT),
            "counts": dict(counts), "required_entry_points": len(REQUIRED),
            "resolved_source_roots": len(root_checks), "root_checks": root_checks,
            "failures": failures,
            "limits": "Static source/config audit only; no Houdini cook, Unreal editor or binary asset validation."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, help="Write the complete JSON audit report")
    args = parser.parse_args()
    report = audit()
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "root_checks"}, indent=2, ensure_ascii=False))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
