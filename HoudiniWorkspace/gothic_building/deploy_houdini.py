"""Build the portable native-SOP HDA and the canonical gothic_building.hip.

hython validate_native.py --all
hython deploy_houdini.py --prepare
hython verify_houdini.py
"""
import argparse
import datetime
import json
from pathlib import Path
import shutil
import sys

import hou
ROOT = Path(globals().get("__file__") or sys.argv[0]).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0,str(ROOT))
import build_hda as B
import native_network as N
from validate_native import configure, set_config, validate
SCENE = ROOT / "gothic_building.hip"
ASSET = ROOT / "asset" / "gothic_building.hda"
REPORTS = ROOT / "reports"
OUTPUT = ROOT / "houdini_output"
TYPE_NAME = "quynh::gothic_building::3.0"
OBJECT_NAME = "gothic_building_v2_1"


def backup_file(path):
    path = Path(path)
    if not path.exists():
        return None
    destination = OUTPUT / "backups" / (path.stem + "_" +
        datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f") + path.suffix)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)
    return destination.relative_to(ROOT).as_posix()


def embedded_module():
    return ((ROOT / "native_network.py").read_text(encoding="utf-8")
            + "\nCORE_SOURCE = " + repr(N.CORE_SOURCE)
            + "\nPRODUCTION_SOURCE = " + repr(N.PRODUCTION_SOURCE)
            + "\nPRESETS = json.loads(" + repr(json.dumps(N.PRESETS)) + ")\n")


def save_scene(path):
    # Temporarily allow definition embedding, restoring the user's preference.
    previous = "true" in next(line for line in hou.hscript("otconfig")[0].splitlines()
                             if line.startswith("Save Operator Definitions to Hip File:"))
    hou.hscript("otconfig -s 1")
    try:
        hou.hipFile.save(str(path))
    finally:
        hou.hscript("otconfig -s " + str(int(previous)))


def publish_definition(records):
    baseline = B.normalize(records[0])
    scratch = hou.node("/obj").createNode("geo", "__gothic_export", run_init_scripts=False)
    node = scratch.createNode("subnet", "generator")
    templates = N.interface(baseline)
    node.setParmTemplateGroup(templates)
    set_config(node, baseline)
    N.rebuild(node)
    node.cook(force=True)
    if node.errors():
        raise RuntimeError(str(node.errors()))
    staging = OUTPUT / "staging" / ("gothic_" +
        datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".hda")
    staging.parent.mkdir(parents=True, exist_ok=True)
    asset = node.createDigitalAsset(name=TYPE_NAME, hda_file_name=str(staging),
        description="Gothic Building - Native SOP Modules", min_num_inputs=0, max_num_inputs=0)
    definition = asset.type().definition()
    definition.setParmTemplateGroup(templates)
    definition.addSection("PythonModule", embedded_module())
    definition.addSection("EditableNodes", "construction MANUAL_EDIT final_boundary_alignment")
    definition.addSection("GothicGraphVersion", "3.0")
    ASSET.parent.mkdir(exist_ok=True)
    asset_backup = backup_file(ASSET)
    definition.copyToHDAFile(str(ASSET))
    hou.hda.installFile(str(ASSET), change_oplibraries_file=False)
    packaged = next(d for d in hou.hda.definitionsInFile(str(ASSET)) if d.nodeTypeName() == TYPE_NAME)
    packaged.setIsPreferred(True)
    scratch.destroy()
    hou.hda.uninstallFile(str(staging))
    return asset_backup


def read_saved_config(node):
    if node is None or node.parm("building_type") is None:
        return None, "baseline"
    config = {"parameters": {}, "seeds": {}}
    for name, schema in B.SCHEMA.items():
        parm = node.parm(name)
        value = schema['default'] if parm is None else parm.evalAsString() if schema["type"] == "string" else parm.eval()
        config["parameters"][name] = bool(value) if schema["type"] == "boolean" else value
    config["seeds"] = {key: node.evalParm("seed_" + key) for key in ("layout", "decoration")}
    preset = node.evalParm("building_preset") if node.parm("building_preset") else "baseline"
    return B.normalize(config), preset


def install_in_scene(config, preset):
    parent = hou.node("/obj")
    obj = parent.node(OBJECT_NAME)
    if obj is None:
        obj = parent.createNode("geo", OBJECT_NAME, run_init_scripts=False)
    old = obj.node("generator")
    if old is not None and old.type().name() != TYPE_NAME:
        old.destroy()
        old = None
    asset = old or obj.createNode(TYPE_NAME, "generator", exact_type_name=True)
    asset.setUserData("owner", "quynh_gothic_deployment")
    set_config(asset, config)
    asset.parm("building_preset").set(preset if preset in N.PRESETS else "baseline")
    asset.hdaModule().rebuild(asset)
    asset.setDisplayFlag(True)
    asset.setRenderFlag(True)
    obj.layoutChildren()
    asset.setCurrent(True, clear_all_selected=True)
    # Retire only unused definitions of this project's old asset in the HIP.
    for definition in hou.hda.definitionsInFile("Embedded"):
        if (definition.nodeTypeName().startswith("quynh::gothic_building::")
                and definition.nodeTypeName() != TYPE_NAME and not definition.nodeType().instances()):
            definition.destroy()
    asset.type().definition().copyToHDAFile("Embedded")
    embedded = next(d for d in hou.hda.definitionsInFile("Embedded") if d.nodeTypeName() == TYPE_NAME)
    embedded.setIsPreferred(True)
    return asset


def prepare():
    records = configure()
    if SCENE.exists():
        hou.hipFile.load(str(SCENE), suppress_save_prompt=True, ignore_load_warnings=True)
    current = hou.node("/obj/" + OBJECT_NAME + "/generator")
    config, preset = read_saved_config(current)
    config = config or B.normalize(records[0])
    backup = backup_file(SCENE)
    asset_backup = publish_definition(records)
    asset = install_in_scene(config, preset)
    result = validate(asset, config)
    # Saving happens only after the replacement graph cooks and passes geometry checks.
    save_scene(SCENE)
    REPORTS.mkdir(exist_ok=True)
    report = dict(status="prepared",houdini=hou.applicationVersionString(),graph_version="2.2",
        scene="gothic_building.hip",hda="asset/gothic_building.hda",
        type_name=TYPE_NAME,asset=asset.path(),preset=asset.evalParm("building_preset"),
        config=config,scene_backup=backup,hda_backup=asset_backup,validation=result)
    (REPORTS/"deployment.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k != "validation"},indent=2),flush=True)
    return asset


def load_into_session():
    configure()
    if not ASSET.exists():
        raise FileNotFoundError("Run hython deploy_houdini.py --prepare first")
    hou.hda.installFile(str(ASSET), change_oplibraries_file=False)
    definition = next(d for d in hou.hda.definitionsInFile(str(ASSET)) if d.nodeTypeName()==TYPE_NAME)
    definition.setIsPreferred(True)
    current = hou.node("/obj/" + OBJECT_NAME + "/generator")
    config, preset = read_saved_config(current)
    config = config or B.normalize(N.PRESETS["baseline"])
    # Preserve the current session before changing its project asset.
    if hou.node("/obj").children():
        path = hou.hipFile.path()
        snapshot = OUTPUT/"backups"/("session_" +
            datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")+".hip")
        snapshot.parent.mkdir(parents=True, exist_ok=True)
        save_scene(snapshot)
        hou.hipFile.setName(path)
    asset = install_in_scene(config,preset)
    validate(asset,config)
    save_scene(SCENE)
    return asset


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare",action="store_true")
    parser.add_argument("--live",action="store_true")
    args = parser.parse_args()
    if args.prepare:
        prepare()
    elif args.live:
        load_into_session()
    else:
        parser.error("Choose --prepare or --live")
