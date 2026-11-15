"""Runs the generator and saves the two Blueprints (commandlet, inside the DreamBPGen project)."""
import json
import os

import unreal

PATH = "/Game/DreamDemo/Blueprints"
names = ["BP_DreamInstancedField", "BP_DreamWorldState"]
for name in names:
    if unreal.EditorAssetLibrary.does_asset_exist("%s/%s" % (PATH, name)):
        unreal.EditorAssetLibrary.delete_asset("%s/%s" % (PATH, name))
errors = unreal.DreamBlueprintBuilder.build_dream_blueprints(PATH)
result = {"errors": [e for e in str(errors).split("\n") if e], "saved": {}}
for name in names:
    result["saved"][name] = bool(unreal.EditorAssetLibrary.save_asset("%s/%s" % (PATH, name), only_if_is_dirty=False))
out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "generate_result.json")
with open(out, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=1)
unreal.log("DREAMBPGEN %s" % json.dumps(result))
