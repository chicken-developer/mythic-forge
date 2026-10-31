"""Check fresh HDA instances, manual debugging, live finish controls and relocation."""
import json
from pathlib import Path
import shutil
import tempfile

import hou

ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "houdini_output"
REPORTS = ROOT / "reports"
deployment = json.loads((REPORTS/"deployment.json").read_text(encoding="utf-8"))
asset_file = ROOT / deployment["hda"]
scene_file = ROOT / deployment["scene"]
OUTPUT.mkdir(exist_ok=True)


def check_cook(node):
    node.cook(force=True)
    problems = [(n.path(),list(n.errors()),list(n.warnings()))
                for n in [node]+list(node.allSubChildren(recurse_in_locked_nodes=False))
                if n.errors() or n.warnings()]
    if problems:
        raise AssertionError(str(problems))
    if not node.geometry().prims():
        raise AssertionError("Empty output")
    if any(n.type().name()=="python" for n in node.allSubChildren(recurse_in_locked_nodes=False)):
        raise AssertionError("The final graph must not contain Python SOPs")


with tempfile.TemporaryDirectory(prefix="relocation_",dir=OUTPUT) as directory:
    assert Path(directory).resolve().is_relative_to(OUTPUT.resolve())
    relocated = Path(directory)/"moved_project"
    (relocated/"asset").mkdir(parents=True)
    copied_asset = relocated/"asset"/"gothic_building.hda"
    copied_scene = relocated/"gothic_building.hip"
    shutil.copy2(asset_file,copied_asset)
    shutil.copy2(scene_file,copied_scene)
    hou.hda.installFile(str(copied_asset),change_oplibraries_file=False)
    definition = next(d for d in hou.hda.definitionsInFile(str(copied_asset))
                      if d.nodeTypeName()==deployment["type_name"])
    definition.setIsPreferred(True)
    obj = hou.node("/obj").createNode("geo","portability_probe",run_init_scripts=False)
    node = obj.createNode(deployment["type_name"],"generator",exact_type_name=True)
    check_cook(node)
    assert node.parm("building_type") is not None
    node.parm("building_preset").set("variant_09")
    node.parm("building_preset").pressButton()
    check_cook(node)
    assert node.evalParm("building_type")=="gatehouse"
    generated = node.node("construction/generated")
    marker = node.node("construction").createNode("xform","manual_debug_marker")
    marker.setInput(0,generated)
    marker.parmTuple("t").set((.05,0,0))
    marker.setDisplayFlag(True)
    marker.setRenderFlag(True)
    manual = node.node("MANUAL_EDIT")
    assert manual.isEditableInsideLockedHDA()
    manual.setUserData("preservation_probe","keep")
    identifier = generated.sessionId()
    count = len(node.geometry().prims())
    node.parm("auto_rebuild").set(0)
    original_spire = node.evalParm("spire_height")
    node.parm("spire_height").set(original_spire+.1)
    node.parm("spire_height").pressButton()
    assert node.node("construction/generated").sessionId()==identifier
    assert len(node.geometry().prims())==count
    assert "press Rebuild" in node.evalParm("rebuild_status")
    node.parm("spire_height").set(original_spire)
    node.parm("rebuild_native").pressButton()
    assert marker == node.node("construction/manual_debug_marker")
    assert marker.input(0)==node.node("construction/generated")
    assert marker.isDisplayFlagSet()
    assert max(abs(a-b) for a,b in zip(marker.parmTuple("t").eval(),(.05,0,0)))<1e-8
    assert node.node("MANUAL_EDIT").userData("preservation_probe")=="keep"
    check_cook(node)
    identifier = node.node("construction/generated").sessionId()
    before = node.geometry().boundingBox()
    node.parm("global_scale").set(2.)
    check_cook(node)
    after = node.geometry().boundingBox()
    assert max(abs(a-2*b) for a,b in zip(list(after.minvec())+list(after.maxvec()),
                                       list(before.minvec())+list(before.maxvec())))<1e-4
    assert node.node("construction/generated").sessionId()==identifier
    node.parm("stone_palette").set("basalt")
    check_cook(node)
    stone = next(p for p in node.geometry().prims() if p.attribValue("material_id")=="stone")
    assert max(stone.attribValue("Cd"))<.3
    assert node.node("construction/generated").sessionId()==identifier
    assert node.geometry().findVertexAttrib("uv") is not None
    assert node.geometry().findVertexAttrib("N") is not None
    obj.destroy()
    # Load the renamed/copied HIP and remove all project libraries from this process.
    # This exercises the full embedded definition rather than an on-disk fallback.
    hou.hipFile.load(str(copied_scene),suppress_save_prompt=True,ignore_load_warnings=False)
    for library in list(hou.hda.loadedFiles()):
        normalized = str(library).replace("\\","/").lower()
        if normalized != "embedded" and (
            normalized.startswith(ROOT.as_posix().lower()+"/")
            or normalized.startswith(relocated.as_posix().lower()+"/")):
            hou.hda.uninstallFile(library)
    node = hou.node(deployment["asset"])
    assert node.type().definition().libraryFilePath()=="Embedded"
    assert node.type().definition().isInstalled()
    check_cook(node)
    assert len(node.geometry().prims())==deployment["validation"]["primitives"]
    # Embedded callbacks must be able to rebuild editable native modules too.
    node.parm("rebuild_native").pressButton()
    check_cook(node)
    assert len(node.geometry().prims())==deployment["validation"]["primitives"]
    assert node.evalParm("building_preset")==deployment["preset"]
    assert all(p.normal().dot(hou.Vector3(p.vertices()[0].attribValue("N"))) >= -1e-7
               for p in node.geometry().prims())
    result = dict(status="passed",houdini=hou.applicationVersionString(),graph_version="2.2",
        fresh_hda_instance=True,preset_callback=True,editable_native_modules=True,
        manual_edits_preserved=True,auto_rebuild_can_be_disabled=True,
        scale_and_palette_without_rebuild=True,vertex_uv_and_normals=True,
        relocated_scene_reopened=True,external_project_libraries_uninstalled=True,
        embedded_callback_rebuild=True,python_sops=0,
        saved_preset=node.evalParm("building_preset"),errors=[],warnings=[])
    REPORTS.mkdir(exist_ok=True)
    (REPORTS/"deployment_verification.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    print(json.dumps(result,indent=2),flush=True)
