"""Dry-run the generator + asset pipeline against tests/mock_hou.py (no Houdini needed).
Checks Python structure, network layout, naming and wiring; writes network.json for render_network.py.

  python scripts/tests/dry_run.py [out_dir]
"""
import os
import sys
import json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import mock_hou                                   # noqa: E402
sys.modules["hou"] = mock_hou

from hbuild.generators import building           # noqa: E402
from hbuild import netdump, validate, presets as PR, scene   # noqa: E402

out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "_dry_run")
os.makedirs(out_dir, exist_ok=True)

example = os.path.join(HERE, "..", "..", "assets", "presets", "example_townhouse.json")
if not os.path.exists(example):
    example = os.path.join(HERE, "..", "..", "presets", "example_townhouse.json")
preset = PR.load_file(example)
assert not PR.validate(preset, building.PARMS), PR.validate(preset, building.PARMS)

node = building.build(os.path.join(out_dir, "gen.hda"), presets={"example": preset})
net = netdump.dump(node)
json.dump(net, open(os.path.join(out_dir, "network.json"), "w"), indent=1)
rep = validate.check(node)

geo, inst, out = scene.instance(building.TYPE_NAME, "example")
cams = scene.cameras(out, "example")
rops = scene.opengl_rops("example", cams, out_dir)
names = [n["name"] for n in net["nodes"]]
print(json.dumps({"nodes": len(names), "boxes": [b["comment"] for b in net["boxes"]],
                  "errors": rep["errors"], "warnings": rep["warnings"],
                  "cams": sorted(cams), "rops": sorted(rops)}, indent=1))
