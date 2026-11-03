"""Read-only file-reference audit. Run in a separate Houdini hython process.

Loads two authoring scenes and five delivered HDAs in Manual update mode.
Does not save scenes, alter definitions, force-cook geometry or render images.
"""
import argparse
import json
from pathlib import Path
import re

import hou

ROOT = Path(__file__).resolve().parents[1]
RETIRED = re.compile(r"HoudiniWorkspace|SubstanceWorkspace|[/\\]TDKProps[/\\]")
SCENES = ("Assets/hda_medieval_building/medieval_building.hip",
          "Assets/hda_gothic_building/gothic_building.hip")
HDAS = (
    "Assets/hda_medieval_building/asset/medieval_house_functions.hda",
    "Assets/hda_medieval_building/asset/medieval_house_procedural.hda",
    "Assets/hda_gothic_building/asset/gothic_building.hda",
    "Assets/_render/SourceAssets/Houdini/Medieval/medieval_building.hda",
    "Assets/_render/SourceAssets/Houdini/Gothic/gothic_building.hda",
)


def references():
    failures, checked = [], 0
    for parm, value in hou.fileReferences():
        if not parm:
            continue
        record = {"parm": parm.path(), "value": value}
        if RETIRED.search(value):
            failures.append({**record, "issue": "retired file reference"})
        # Scene-local textures are the external inputs repaired by this review.
        if "/assets/textures/" in value.replace("\\", "/"):
            checked += 1
            if not Path(hou.text.expandString(value)).is_file():
                failures.append({**record, "issue": "missing texture"})
    return failures, checked


def audit():
    if hou.isUIAvailable():
        raise RuntimeError("Run this audit in a separate hython process, outside the open GUI scene.")
    hou.setUpdateMode(hou.updateMode.Manual)
    records = []
    for relative in SCENES:
        hou.hipFile.load(str(ROOT / relative), suppress_save_prompt=True, ignore_load_warnings=True)
        failures, textures = references()
        records.append({"file": relative, "kind": "scene", "textures_checked": textures,
                        "failures": failures})
    for relative in HDAS:
        hou.hipFile.clear(suppress_save_prompt=True)
        hou.hda.installFile(str(ROOT / relative))
        for definition in hou.hda.definitionsInFile(str(ROOT / relative)):
            definition.setIsPreferred(True)
            host = hou.node("/obj").createNode("geo", "path_audit", run_init_scripts=False)
            host.createNode(definition.nodeTypeName(), "asset", run_init_scripts=False)
            failures, textures = references()
            records.append({"file": relative, "kind": "hda_instance",
                            "type": definition.nodeTypeName(), "textures_checked": textures,
                            "failures": failures})
            host.destroy()
    return {"status": "failed" if any(r["failures"] for r in records) else "passed",
            "runtime": hou.applicationVersionString(), "files": records,
            "limits": "Manual file-reference audit only; geometry cook, rendering and Unreal editor not run."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = audit()
    output = json.dumps(report, indent=2, ensure_ascii=False)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output + "\n", encoding="utf-8")
    print(output)
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
