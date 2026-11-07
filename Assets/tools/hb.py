#!/usr/bin/env hython
"""hb.py - command line for the hbuild toolkit. Run with Houdini's hython.

  hython hb.py env
  hython hb.py docs                                   # parameter table (markdown)
  hython hb.py generator --hda hda/bldg_building_generator.hda --presets-dir presets
  hython hb.py asset --hda hda/bldg_building_generator.hda --preset presets/townhouse_a.json \
                     --hip hip/townhouse_a.hip --renders renders/townhouse_a
  hython hb.py validate --hip hip/townhouse_a.hip --node /obj/townhouse_a/generator
  hython hb.py regress --hda hda/bldg_building_generator.hda --presets-dir presets --renders renders/_regress
  hython hb.py selftest --project . [--no-render]     # first run on a new machine
  hython hb.py apicheck --out docs/_apicheck.json      # check hou API assumptions on this Houdini
  hython hb.py inspect --hip X.hip --out X_dump.json   # read-only deep dump of an existing hip

Every command prints a JSON summary on the last line (easy for Claude to parse).
"""
import argparse
import glob
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from hbuild import VERSION                                  # noqa: E402
from hbuild import presets as PR, scene, netdump, validate  # noqa: E402
from hbuild.generators import building                     # noqa: E402

GENERATORS = {building.TYPE_NAME: building}


def _abs(p):
    return os.path.abspath(p).replace("\\", "/")


def _summary(d):
    print(json.dumps(d))


def _load_presets_dir(d):
    out = {}
    for f in sorted(glob.glob(os.path.join(d, "*.json"))):
        p = PR.load_file(f)
        out[p.get("name") or os.path.splitext(os.path.basename(f))[0]] = p
    return out


def cmd_env(a):
    import hou
    _summary({"hbuild": VERSION, "houdini": hou.applicationVersionString(), "python": sys.version.split()[0],
              "hip": hou.hipFile.path(), "generators": list(GENERATORS)})


def cmd_docs(a):
    from hbuild.parms import docs_markdown
    print(docs_markdown(building.PARMS))


def cmd_generator(a):
    import hou
    hou.hipFile.clear(suppress_save_prompt=True)
    os.makedirs(os.path.dirname(_abs(a.hda)), exist_ok=True)
    presets = _load_presets_dir(a.presets_dir) if a.presets_dir else {}
    for name, p in presets.items():
        issues = PR.validate(p, building.PARMS)
        if issues:
            print("[preset %s] %s" % (name, "; ".join(issues)))
    node = building.build(_abs(a.hda), presets=presets)
    try:
        node.cook(force=True)
    except Exception:
        pass                       # node errors are collected by validate.check below
    hip = a.hip or os.path.join(os.path.dirname(_abs(a.hda)), "_generator_dev.hip")
    hou.hipFile.save(_abs(hip))
    rep = validate.check(node)
    _summary({"ok": not rep["errors"], "hda": _abs(a.hda), "type": building.TYPE_NAME,
              "presets": list(presets), "errors": rep["errors"][:20], "warnings": rep["warnings"][:20],
              "dev_hip": _abs(hip)})


def run_asset(hda, preset_path, hip, renders, render=True, res=(1280, 960)):
    import hou
    hou.hipFile.clear(suppress_save_prompt=True)
    hou.hda.installFile(_abs(hda))
    preset = PR.load_file(preset_path)
    name = preset.get("name") or os.path.splitext(os.path.basename(preset_path))[0]
    gen_type = preset.get("generator", building.TYPE_NAME)
    mod = GENERATORS[gen_type]
    issues = PR.validate(preset, mod.PARMS)
    geo, node, out = scene.instance(gen_type, name)
    missing = PR.apply_values(node, preset.get("parms", {}))
    try:
        out.cook(force=True)
    except Exception:
        pass                       # node errors are collected by validate.check below
    renders = _abs(renders)
    os.makedirs(renders, exist_ok=True)
    try:
        cams = scene.cameras(out, name, res=res)
    except Exception:
        cams = {}
    rops = scene.opengl_rops(name, cams, renders) if render else {}   # --no-render: no ROP nodes at all
    os.makedirs(os.path.dirname(_abs(hip)), exist_ok=True)
    hou.hipFile.save(_abs(hip))
    rep = validate.check(node)
    netdump.write(node, os.path.join(renders, "network.json"))
    with open(os.path.join(renders, "validate.json"), "w") as f:
        json.dump(rep, f, indent=1)
    rendered = []
    if render and not rep["errors"]:
        t0 = time.time()
        scene.render(rops)
        rendered = sorted(glob.glob(os.path.join(renders, "*.png")))
        rep["info"]["render_seconds"] = round(time.time() - t0, 1)
    return {"ok": not rep["errors"], "asset": name, "hip": _abs(hip), "renders": rendered,
            "network_json": _abs(os.path.join(renders, "network.json")),
            "preset_issues": issues, "missing_parms": missing,
            "errors": rep["errors"][:20], "warnings": rep["warnings"][:20], "info": rep["info"]}


def cmd_asset(a):
    _summary(run_asset(a.hda, a.preset, a.hip, a.renders, render=not a.no_render))


def cmd_validate(a):
    import hou
    hou.hipFile.load(_abs(a.hip), suppress_save_prompt=True, ignore_load_warnings=True)
    rep = validate.check(hou.node(a.node))
    _summary({"ok": not rep["errors"], **rep})


def cmd_netdump(a):
    import hou
    hou.hipFile.load(_abs(a.hip), suppress_save_prompt=True, ignore_load_warnings=True)
    netdump.write(hou.node(a.node), _abs(a.out))
    _summary({"ok": True, "out": _abs(a.out)})


def cmd_regress(a):
    stamp = time.strftime("%Y%m%d_%H%M%S")
    root = os.path.join(_abs(a.renders), stamp)
    results = []
    for f in sorted(glob.glob(os.path.join(a.presets_dir, "*.json"))):
        n = os.path.splitext(os.path.basename(f))[0]
        r = run_asset(a.hda, f, os.path.join(root, n + ".hip"), os.path.join(root, n))
        results.append({"asset": n, "ok": r["ok"], "errors": r["errors"][:3]})
    _summary({"ok": all(r["ok"] for r in results), "root": root, "results": results})


def cmd_selftest(a):
    proj = _abs(a.project)
    hda = os.path.join(proj, "hda", "bldg_building_generator.hda")
    example = os.path.join(HERE, "..", "assets", "presets", "example_townhouse.json")
    if not os.path.exists(example):
        example = os.path.join(proj, "presets", "example_townhouse.json")
    import hou
    hou.hipFile.clear(suppress_save_prompt=True)
    os.makedirs(os.path.dirname(hda), exist_ok=True)
    building.build(hda, presets={"example_townhouse": PR.load_file(example)})
    r = run_asset(hda, example, os.path.join(proj, "hip", "_selftest.hip"),
                  os.path.join(proj, "renders", "_selftest"), render=not a.no_render)
    _summary(r)


def cmd_apicheck(a):
    from hbuild import apicheck
    import hou
    hou.hipFile.clear(suppress_save_prompt=True)
    res = apicheck.run_all()
    os.makedirs(os.path.dirname(_abs(a.out)), exist_ok=True)
    with open(_abs(a.out), "w", encoding="utf-8") as f:
        json.dump(res, f, indent=1)
    _summary({"ok": res["ok"], "out": _abs(a.out), "houdini": res["houdini"],
              "failed": [r["call"] for r in res["results"] if not r["ok"]]})


def cmd_inspect(a):
    """Read-only: loads the hip, dumps it, never saves (hipFile.clear at the end)."""
    import hou
    from hbuild import inspect_hip
    hou.hipFile.load(_abs(a.hip), suppress_save_prompt=True, ignore_load_warnings=True)
    os.makedirs(os.path.dirname(_abs(a.out)), exist_ok=True)
    r = inspect_hip.write(_abs(a.out), roots=tuple(a.roots), cook=not a.no_cook)
    hou.hipFile.clear(suppress_save_prompt=True)
    _summary({"ok": True, "out": _abs(a.out), **r})


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    sp.add_parser("env").set_defaults(fn=cmd_env)
    sp.add_parser("docs").set_defaults(fn=cmd_docs)
    p = sp.add_parser("generator"); p.set_defaults(fn=cmd_generator)
    p.add_argument("--hda", required=True); p.add_argument("--presets-dir"); p.add_argument("--hip")
    p = sp.add_parser("asset"); p.set_defaults(fn=cmd_asset)
    p.add_argument("--hda", required=True); p.add_argument("--preset", required=True)
    p.add_argument("--hip", required=True); p.add_argument("--renders", required=True)
    p.add_argument("--no-render", action="store_true")
    p = sp.add_parser("validate"); p.set_defaults(fn=cmd_validate)
    p.add_argument("--hip", required=True); p.add_argument("--node", required=True)
    p = sp.add_parser("netdump"); p.set_defaults(fn=cmd_netdump)
    p.add_argument("--hip", required=True); p.add_argument("--node", required=True); p.add_argument("--out", required=True)
    p = sp.add_parser("regress"); p.set_defaults(fn=cmd_regress)
    p.add_argument("--hda", required=True); p.add_argument("--presets-dir", required=True); p.add_argument("--renders", required=True)
    p = sp.add_parser("selftest"); p.set_defaults(fn=cmd_selftest)
    p.add_argument("--project", default=".")
    p.add_argument("--no-render", action="store_true")
    p = sp.add_parser("apicheck"); p.set_defaults(fn=cmd_apicheck)
    p.add_argument("--out", required=True)
    p = sp.add_parser("inspect"); p.set_defaults(fn=cmd_inspect)
    p.add_argument("--hip", required=True); p.add_argument("--out", required=True)
    p.add_argument("--roots", nargs="+", default=["/obj"]); p.add_argument("--no-cook", action="store_true")
    a = ap.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
