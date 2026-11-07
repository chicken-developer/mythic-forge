# Project memory

Updated: 2026-11-03 (Asia/Saigon).

## Current layout

Repository: `E:/Repos/mythic-forge`. Prefer paths relative to the repository or
the script's `__file__`; do not rely on the shell's working directory.

| Purpose | Current location |
| --- | --- |
| Main game project and C++ code | `ThreeDreamingKnights/ThreeDreamingKnights.uproject`, `ThreeDreamingKnights/Source/` |
| Gothic HDA authoring | `Assets/hda_gothic_building/` |
| Medieval HDA authoring | `Assets/hda_medieval_building/` |
| Houdini reference assets | `Assets/hda_samples/AllHDAs/`, `Assets/hda_samples/AllHipFIles/` |
| Source skill ZIP | `Assets/skills/design-hda-building.zip` |
| Modular Houdini 22 skill ZIP | `Assets/skills/design-hda-building-modular-houdini22.zip` |
| Installed building skill | `.agents/skills/design-hda-building/SKILL.md` |
| Gothic Substance sources and exports | `Assets/texture_gothic/`, `Assets/texture_gothic/exports/` |
| Fountain reference | `Assets/3d_central_fountain/` |
| Unreal asset review project | `Assets/_render/TDKProps.uproject` |
| Review scripts, imported sources and results | `Assets/_render/Tools/`, `SourceAssets/`, `RenderTests/` |
| Unreal composition project | `Assets/_composition/TDKKingdomMap.uproject` |

`HoudiniWorkspace/gothic_building` and `HoudiniWorkspace/medieval_building` moved
to the two `Assets/hda_*` folders. `HoudiniWorkspace/samples` moved to
`Assets/hda_samples`. `SubstanceWorkspace/src` moved to `Assets/texture_gothic`.
The old root `TDKProps/` and `TDKKingdomMap/` directories moved to `_render` and
`_composition` under `Assets`. Those old filesystem paths are superseded.

The `.uproject` filenames, Unreal C++ module names and `/Game/...` package paths
remain valid. A source authoring HDA and a copy in
`Assets/_render/SourceAssets/Houdini/<family>/` have different roles; confirm the
intended workflow before publishing one over the other.

## Active Medieval workflow

Read `Assets/hda_medieval_building/AGENTS.md`, `README_VI.md` and `analysis.md`.
At the start of the current 2026-11-02 modeling task that source directory
contained only `medieval_building.hip`; the previously documented
`house_rebuild/function_refactor/` tree, asset definitions and preset files were
absent. Do not treat the earlier memory entry as proof those files still exist.
The intact HIP contained the 18 native `F_*` functions, typed local arguments,
declared geometry inputs and shared aperture/cutter streams. Their public
outputs and input ordering remain intact. Only caller bindings read
`CTRL_BUILDING`; do not add hidden controller dependencies inside functions.

The working scene remains `medieval_building.hip`, object
`/obj/medieval_building`. `scripts/update_model.py` updates named native nodes
and uses `sources/vex/`; it does not generate the mesh in Python.
`build_hda.py` is the public config/load/package API. The new portable asset is
`asset/medieval_house_layout.hda`, type `mythic::medieval_house::3.0`.
The starting HIP is retained in `backup/before_layout_20261102/`.

`manifest.json` maps the 15 supplied concept sheets to full JSON configs in
`datasets/`; a separate baseline and three validation-group manifests are also
provided. Two NormalHouse C sheets have identical bytes and are distinguished
by canopy/balcony preset choices. Native reference kits add four square guild
turrets, a Boolean furnace aperture, hollow industrial flue and supported hoists.
They are selected through explicit facade override slots. Bespoke emblems,
vegetation, fire simulation, the forge bell and unique props remain simplified.

The graph now aligns facade bays across floors, reserves frame/shutter
clearances, uses deterministic facade-module placement, short knee braces,
gable trusses and corresponding interior wall frames. Roof ridge axis and
rectangular/arched window controls are implemented. Controller and local
function ranges agree for the larger referenced buildings.

Use `scripts/check_configs.py`, `scripts/check_portable.py`, and
`scripts/bake_variants.py --no-render --resolution 4096`. Native UV Layout,
triangulation and semantic bevel produce low/high geometry; Copernicus bakes
actual DirectX/MikkT normals using temporarily separated connected parts.
Run `scripts/render_variant.py <id>` in a separate process. Mantra has no render
license, and mixing Copernicus with OpenGL in one process triggered an AMD
driver crash. Do not infer render or Unreal status from a successful cook.

New review exports: `Assets/_render/SourceAssets/Medieval15/`; Unreal tools:
`Assets/_render/Tools/Medieval15/`; reports/screenshots:
`Assets/_render/RenderTests/Medieval15/`. Target map:
`/Game/Medieval15/Maps/L_Medieval15_Review`. Materials use constant base colors
and high-to-low normal maps, with no albedo textures or SkyTown dependency.
Keep historical run reports unchanged; current results have independent hashes.

The Gothic authoring folder also contains only `gothic_building.hip` in the
current workspace. Its former Python builder is absent; the path audit now
requires the actual retained HIP source instead of that unavailable entry point.
Legacy generators/export copies in the review project are retired with the old
Gothic/Medieval/SkyTown review assets. This does not regenerate Gothic geometry.

## Versioned Medieval modular review — 2026-11-03

`output/modular_rebuild_v2/` contains a separate review package for the selected
Medieval authoring HIP. The original HIP/HDA, builder, schema, 16 JSON files and
VEX sources retain their recorded hashes. This output has not been promoted over
the authoring source or published into Unreal.

The new SOP HDA is `asset/medieval_building_modular.hda`, type
`mythic::medieval_house::4.0`. It retains the 18 typed native functions through
an explicit path migration, composed as 19 modules in 7 real stages. The public
entry is `build_hda.py`; factories, registry and audits are in `hda_builder/`.
`module_graph.json` comes from the same registry used by the builder. Nested
stage/module definitions are bundled into the main HDA; a relocated copy cooks
and responds to width changes without importing authoring scripts.

All 15 original variants plus a separate baseline have fresh cook, BGEO, HIP,
hero renders and reference/before/after comparisons. Graph audit and 17
regressions cover real connectors, window/host/frame/dressing recook, UV tiling,
determinism, invalid inputs, ranges and safe reruns. Pure-refactor parity was
recorded before deliberate duplicate/degenerate face cleanup and sloped UV fixes.
The cleanup Blast must use `grouptype=prims`; automatic group guessing removed
adjacent valid roof faces in a diagnostic run. Final results require preserved
valid surfaces and dimensions, zero exact duplicate/degenerate polygons, finite
UV/normals and unobstructed aperture rays. Preview/checker rendering uses one
hython process per image because changing models in one Vulkan context crashed.

Optional added UI controls are documented in `extension_schema.json`; original
datasets remain byte-identical. Arbitrary roof-junction graphs and T/courtyard
footprints are unsupported; the original body-trim roof grammar is retained.
Normal maps need rebaking for the corrected UVs. General all-angle roof/support,
collision/LOD, interior walkability and Unreal bake checks remain separate from
the measured geometry gates. See `evaluation.md`, `variant_mapping.json`,
`graph_audit.json` and `results/final_summary.json` in the package.

The 2026-11-03 repository static path audit reports 47 missing resources in two
pre-existing retired review manifests (Gothic FacadeReview and Medieval material
exports). The new audit is stored in the modular package's results; the older
`tools/path_audit_report.json` is retained as historical evidence. Package-local
syntax, dataset, module DAG, image and original-source integrity checks pass.

## Medieval roof / HIGH / GUI review — 2026-11-03

The follow-up review is `output/roof_high_gui_v3/`, with HDA
`mythic::medieval_house::4.1` in `asset/medieval_building_modular.hda` and scene
`scenes/medieval_building_roof_high_v3.hip`. It supersedes v2 for roof/HIGH
review only; the authoring HIP/HDA and all 16 source datasets remain unchanged.

For this workflow the user requires published previews to be screenshots from
the actual Houdini GUI, including HIGH geometry. Do not substitute hython +
OpenGL ROP images. Hython remains suitable for graph construction, geometry
cook/export and tests. `gui_bootstrap.py`, `gui_review.py` and
`capture_gui_batch.py` use a separate visible Houdini session and native window
capture. Viewport crops come from the same captured Houdini window pixels;
never capture the desktop or other applications. Restore a minimized window
before applying a review configuration to avoid a stale captured frame.
Explicitly reload/prefer the current bundled HDA after it changes, and compare
the GUI geometry position hash to the validated export; matching primitive
counts alone cannot detect stale cached helper definitions.

The v3 graph has 22 real modules in 7 stages and no Python SOP. Roof profiles
are skinned separately per role, then native Boolean roof volumes establish the
visible top domain. Dormer height/depth/cutter follow the host roof; dormer and
solid canopy roofs have tiles. Canopy tiles must be placed on source quads
before Boolean clipping changes them to ngons. Fabric awnings keep fabric.
The site module consumes actual balcony geometry through its declared
`overhead_access` input. Fabric awnings project at least 45 cm beyond its front
edge, and their posts/arms recook with balcony depth. The dedicated clearance
check covers all 8 awning-enabled configs and a 35 cm balcony-depth change.

`preview_quality` is visible at the top of the public UI and defaults to HIGH.
Public SOP outputs: 0 selected, 1 LOW, 2 HIGH. HIGH uses native PolyBevel on
appropriate orthogonal members with limits based on local edge lengths; avoid
beveling arbitrary Boolean polygons (a diagnostic version produced NaNs).
Both branches have separate BGEO exports and exact-face cleanup.

All 15 variants plus baseline passed the current roof/canopy, finite P/UV/N,
duplicate/degenerate and LOW/HIGH geometry gates. The 17 regression checks,
actual graph audit and relocated HDA-only portability test passed. Original
29-file hashes and byte-identical datasets are checked separately. Review
images and limits are in `review.html`, `evaluation.md` and `results/`.
HIGH is bevel geometry, not sculpted surface detail or final textures. Normal
rebake, Unreal bake/reimport, collision/LOD, interior walkability and arbitrary
T/courtyard roof graphs remain outside the completed checks.

## Runtime and checks

Installed Houdini observed during this review: `E:/SideFX/Houdini 22.0.368/`.
Its Python is `python313/python.exe`; Houdini CLI is `bin/hython.exe`.
The bare `python` command currently resolves to the Windows Store alias, so use
the installed executable explicitly if a normal Python environment is absent.
Both asset review `.uproject` files declare Unreal Engine `5.8`; the directory
refactor does not rename their projects or modules.

Run the static audit from any directory:

```powershell
& 'E:/SideFX/Houdini 22.0.368/python313/python.exe' -X utf8 E:/Repos/mythic-forge/tools/audit_paths.py
```

The audit checks source syntax, retired paths, resolved source roots and active
JSON/config files. Historical reports, checkpoints, archived authoring copies,
third-party plugins and generated Unreal files retain their original paths.
Do not rewrite their hashes or treat old successful runs as new validation.
Houdini HIP/HDA and Unreal binary assets need separate runtime checks for
embedded absolute paths; static validation cannot certify those dependencies.

## Path review completed 2026-11-02

`tools/audit_paths.py` passed for 272 Python files, 1,376 JSON files, 129 active
manifests, 183 resolved source roots and 290 manifest resource paths. Both
exporters and importers now treat mesh manifest `file` values as relative to
the repository root, including the `Assets/_render/` prefix.

`tools/audit_houdini_paths.py` passed in Houdini 22.0.368 for the two authoring
HIP scenes and five HDA instances. In `medieval_building.hip`, 48 broken texture
paths were changed to `$HIP/assets/textures/...`. The saved scene was reloaded;
all 424 node records, their connections and all other parameter values matched
the pre-edit snapshot. No geometry cook or render was forced.
The original scene is preserved in
`Assets/hda_medieval_building/backup/path_refactor_20261102/medieval_building.hip`.
The HDA definitions were inspected without saving changes to them.

Reports: `tools/path_audit_report.json`, `tools/houdini_path_audit_report.json`
and `tools/houdini_texture_path_repair.json`. Rerun the Houdini check in a separate
process, not inside the artist's GUI session:

```powershell
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' E:/Repos/mythic-forge/tools/audit_houdini_paths.py
```

The active Medieval manifest's 8 representative configs passed the skill's
structural validator; it still reports the documented absence of `simple`
complexity coverage. All 24 compatibility presets remain present. Unreal editor
and binary `.uasset` reimport metadata were not validated in this path review.
Export manifests retain the hashes/version of their original export runs:
the SkyTown export scene hashes already differed from the current authoring HIP
before the texture-path repair. Do not replace these hashes with the new HIP
hash as if a new export or geometry verification had occurred.

## hbuild building generator (houdini-procedural-builder flow) — 2026-11-07

Branch `hbuild/building-generator`. A new, separate rebuild: the two original authoring HIPs
(`Assets/hda_medieval_building/medieval_building.hip`, `Assets/hda_gothic_building/gothic_building.hip`) are
read-only inputs; their sha256 are in `Assets/docs/original_hip_hashes.json` and were re-checked after the work.
Project root of this flow is `Assets/`: `tools/` (hb.py + `hbuild/`, source of truth: Python + `.vfl`),
`hda/bldg_building_generator.hda` (type `bldg::building_generator::1.0`, one generator for medieval and gothic,
all presets embedded), `presets/*.json` (14 medieval + 3 gothic), `specs/*.md`, `hip/<asset>.hip`,
`renders/<asset>/` (GUI viewport screenshots + `compare_rN.png`), `docs/` (`building_generator.md`,
`hou_api_notes.md`, `source_analysis.md`, `review_log.md`). Outputs: 0 selected, 1 LOW, 2 HIGH (PolyBevel only on
hardsurface parts; walls/roof planes flat for Substance Designer).

Previews: no ROP/Karma/Mantra. `tools/hgui/capture_batch.py` starts one fresh Houdini GUI per asset
(`HOUDINI_PATH` hook `tools/hgui/houdini_path/scripts/456.py`, `HOUDINI_PACKAGE_SKIP=1`) and flipbooks the
SceneViewer; capturing many assets in one GUI session crashed, and `hou.exit()` from a deferred callback
segfaults, so the hook hard-exits. Git refuses this repo without `-c safe.directory=E:/Repos/mythic-forge`
(different owner SID); jobs pass it per command instead of changing global config. Houdini's own python has a
broken PIL, so compare sheets are built outside Houdini (`tools/compare_sheet.py`, Pillow).

References (`Assets/img_refs/`, reorganised by the user 2026-11-08): `medievals_refs/` = building concept sheets
(one preset each), `medievals_moodboard/` and `gothics_moodboard/` = mood/style only. Gothic has no building
concept sheets: the three gothic presets are style studies from moodboard images (`"ref_kind": "moodboard"`).
