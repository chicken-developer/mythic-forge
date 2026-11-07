# hou API notes — Houdini 22.0.368 (this machine)

Checked on 2026-11-07 with `hython tools/hb.py apicheck --out docs/_apicheck.json` (raw results in that file) and `hb.py selftest --no-render`.
Each line: `H22.0.368: <call> — <result>`. The 21-era assumptions from the skill all held; nothing in `tools/hbuild` had to change for the API.

## Assumptions from the skill (references/hou_api_notes.md)

- H22.0.368: attribwrangle parms class (0 detail,1 prims,2 points,3 vertices) + snippet — confirmed OK (class items=['detail', 'primitive', 'point', 'vertex', 'number'])
- H22.0.368: ParmTemplate kwargs: Menu(menu_labels, default_value), String(item_generator_script*, menu_type), Float(look=ColorSquare, naming_scheme=RGBA), Button(script_callback*) — confirmed OK (confirmed)
- H22.0.368: subnet.indirectInputs()[0] wireable with setInput — confirmed OK (indirect inputs=4)
- H22.0.368: subnet.createDigitalAsset(name, hda_file_name, description, min_num_inputs, max_num_inputs) + definition().setParmTemplateGroup/addSection/setExtraFileOption/save + matchCurrentDefinition — confirmed OK (type=hb::apicheck::1.0 file=C:/Users/quynh/AppData/Local/Temp/hb_apicheck.hda)
- H22.0.368: cam parms resx resy projection('ortho') orthowidth; Euler XYZ, looks down -Z — confirmed OK (projection items=['perspective', 'ortho', 'sphere', 'cylinder', 'lens'] rOrd=xyz)
- H22.0.368: /out opengl ROP type + parms camera, picture (existence only; no render per user rule) — confirmed OK ({'camera': True, 'picture': True})
- H22.0.368: VEX: setpointattrib vector4 creates orient; copytopoints uses orient + vector scale — confirmed OK (copied bbox size=[0.0, 3.0, 2.0])
- H22.0.368: VEX: user functions in wrangle + dynamic chv('../col_' + s@mat_id) — confirmed OK (Cd=(0.10000000149011612, 0.20000000298023224, 0.30000001192092896))
- H22.0.368: polybevel SOP parms for HIGH branch (group, offset/distance, divisions, limit) — confirmed OK (type=polybevel::3.0 parms=['group', 'grouptype', 'ignoregroup', 'ignoreflatedges', 'ignoreflatpoints', 'flatangle', 'offsettingsection', 'offset', 'useoffsetscale', 'collisionsandlimits', 'limit', 'pinchangle', 'filletsection', 'filletshape', 'divisions', 'profilesampling', 'profilesource', 'profile)
- H22.0.368: SOP types used by LOW/HIGH (normal, clean, fuse, groupcreate/groupexpression, measure) — confirmed OK ({'normal': True, 'clean': True, 'fuse::2.0': True, 'fuse': True, 'groupexpression': True, 'groupcreate': True, 'measure': True, 'uvlayout': True, 'boolean::2.0': True, 'polyextrude::2.0': True, 'copytopoints::2.0': True, 'attribwrangle': True, 'switch': True, 'output': True, 'polyreduce::2.0': True})
- H22.0.368: hou.hda.installFile / reloadFile / uninstallFile exist — confirmed OK (confirmed)

## Environment findings

- H22.0.368: `hython tools/hb.py env` reports `22.0.368`, Python 3.13.10.
- H22.0.368: `hb.py selftest --project . --no-render` builds `hda/bldg_building_generator.hda` and `hip/_selftest.hip`; cook OK, 0 errors/warnings, bbox 10.8 × 13.5 × 8.8 m.
- H22.0.368: Houdini's bundled `python313/python.exe` cannot import matplotlib (PIL `_imaging` DLL fails to load). `houdini_env.json` therefore has `python: ""`; contact sheets and network diagrams are made outside Houdini's Python.
- H22.0.368: OpenGL ROP exists but is NOT used in this project (user rule: previews are GUI viewport captures, no ROP/Karma/Mantra).
- H22.0.368: `polybevel::3.0` is the default PolyBevel; parms used for HIGH: `group`, `grouptype`, `ignoreflatedges`, `offset`, `divisions`, `limit` (see `_apicheck.json`).
- H22.0.368: loading the two original hips prints COP handle-binding warnings (`sidefx_transform2d/3d`, `sidefx_hud_*`) from stock COP HDAs; harmless for SOP work.
- git: the repo is owned by another Windows SID, so git refuses it ("dubious ownership"). Jobs call `git -c safe.directory=E:/Repos/mythic-forge ...` per command instead of changing the global config.
- Houdini GUI shows a startup dialog "SHFS not properly setup" (third-party package); dismiss it before typing into the Python Shell.
