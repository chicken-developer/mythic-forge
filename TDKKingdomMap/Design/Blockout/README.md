# Thành A blockout

Environment final-image camera poses live in `../Environment/capture_views.json`; changing that camera-only file does not require regeneration or another trial. `Generated/camera_capture_report.json` records the poses actually rendered. `Tools/Environment/check_guards.ps1` verifies expected legacy-generator and stale-snapshot rejection without editing the primary map.

The primary map now includes the Stylized Forest environment. Use `Tools/Environment/run.ps1` for environment work; the original blockout `Generate` command deliberately stops before replacement when it detects the environment ownership tag. See `../Environment/execution_report.txt` for the inspected package/sample, assets, completed work, render evidence and remaining limits.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Prepare
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Trial -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode TrialCapture -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Generate -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Verify -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Capture -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Environment/run.ps1 -Mode Benchmark -SkipBuild
```

Edit `../Environment/environment.json` for stable tree IDs/transforms, scatter zones, seed, density, exclusion margins, culling and cameras. `Prepare` derives masks from the existing design and protected actor footprints. It does not run the blockout generator or replace terrain. `Trial` saves a separate environment test level. After inspecting its actual UE images, record the current compiled fingerprint and `visually_reviewed: true` in `../Environment/Generated/trial_review.json`; `Generate` requires that receipt and a successful trial. This is a visual validation gate, not a request for external approval.

The supported environment runner checks saved instancing, the primary map file and owned material/mesh files against `Generated/saved_state.json` before generation or capture. If a saved manual edit is detected, it stops before replacing anything. Back up and reconcile the edit into the design before deliberately establishing a new snapshot; do not remove the guard receipt to silence a discrepancy. Save or close any interactive Editor before using the external runner: it can only detect edits saved on disk. Own package directories are versioned and retained to protect references.

Edit `layout.json`, then from the project root run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Validate
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Generate
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Verify -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Capture -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Playtest
```

`-Engine` overrides the installed UE path. Validate compiles design artifacts, builds the C++ modules, and runs a read-only Unreal commandlet; it does not change a level. `Generate` loads or creates only `/Game/Maps/Blockout/L_ThanhA_Blockout`, builds and saves assets, replaces actors carrying `ThanhA.Blockout.v1`, imports Landscape, saves the map and tests actual collision. `Verify` reloads the saved map and reruns collision queries. `Capture` uses Unreal SceneCapture in the Editor world, not a fabricated screenshot. `Playtest` uses the supplied CharacterMovement in a standalone Unreal game world. See the execution report for what actually ran.

Generation uses NullRHI; capture runs separately with DX12 and up to 16 shader workers. The commandlet rejects renderer-enabled generation to avoid Landscape resource teardown during rendering. Save manual edits in the Editor before running the external commandlet: it reads the saved level from disk. Input/layout/compiler/heightmap hashes are checked before any replacement.

The compiler `Tools/Blockout/prepare.py` uses Python 3.11 standard library only (bundled with UE). Its sole normal design input is `layout.json`; `seed_layout.py` is the original manual tracing transcription, **not** the rerun entry point. Do not run seed_layout after editing the design.

Runtime module `TDKKingdomMap` owns gameplay classes, a native mesh actor with a stable design root, and passive serialized measurement metadata without ticking or drawing. Generation, Landscape import, mesh creation, grid material construction, measurement visualization and validation are in the Editor module `ThanhABlockoutEditor`.

Open the saved map after restarting the previously opened Blank project Editor so the new native modules are loaded. In PIE, WASD moves, mouse looks, Space jumps. Start at the north gate, follow market/plaza/council forecourt, then east to the port switchback; also walk the west residential and east craft branches. A closed Council Hall is checked by reaching its forecourt, not by entering an interior. Automated traces and sweeps cannot replace visual/player review of corners, silhouettes and subjective ease of movement.

Outliner groups: Terrain, Roads, Walls, Medieval, Gothic, Hybrid, Landmarks and Debug. Every generated actor has an ownership tag and stable ID tag. Shared Engine primitives are not modified. Generated mesh/material assets are owned through package metadata and placed in an input-fingerprint directory, so old assets are retained on design edits rather than deleting anything manually referenced. Unchanged input reuses assets. Untagged actors survive reruns. Treat manually modified tagged actors as generator-owned; remove their ownership tag to retain them.

`Generated/layout_preview.svg` and `.png` are the top-down interpretation, with architecture colors independent of elevation outlines, IDs, roads, openings and landmark massing. `buildings.json` records stable building IDs/locations; Unreal actor labels expose the full component IDs. `reference_01_crop.png` preserves the measured source crop. Engine top-down capture exports rows North-up/East-right; raw heightmap row zero remains South.

Landscape fallback/manual import: use `Generated/terrain.png` (16-bit grayscale) or little-endian `terrain.r16`, resolution 1009×1009, 63×63 quads, one section/component, 16×16 components. Scale X/Y=100, Z=12.5; actor location X/Y=-50400, Z=0 cm, rotation 0. Row zero is South: **do not flip Y**. Disable edit-layer assumptions; import the base height data. The exact equation is `Z_cm = actorZ + (encoded-32768)*ZScale/128`. Read `heightmap_metadata.json` if plan scale or encoding changes. Sample elevations must be queried after import. Source: [Epic Landscape technical guide](https://dev.epicgames.com/documentation/unreal-engine/landscape-technical-guide-in-unreal-engine).

`design_validation.json` is Python design validation. `unreal_collision_report.json` records actual ground traces and capsule sweeps. `unreal_execution_report.json` records map save/engine/owned actor count, actual Landscape components and Sky Atmosphere presence. The playtest report, when present, is separate evidence of CharacterMovement execution. `Saved/Logs/ThanhA_*.log` contains actual build and Unreal command logs.

`unreal_capture_report.json` stores image capture results separately; `unreal_verification_report.json` records read-only reload checks. `rerun_before.json`, `rerun_after.json` and `rerun_validation.json` are actual same-input rerun receipts, including an untagged manual sentinel and sorted stable-ID/transform hashes. At paved sample points, `scene.json` includes `landscape_ground_m` for the substrate (design datum minus 0.25 m), while surface traces test the actual walkable slab at design datum +0.04 m.

The ground is an imported, editable Unreal Landscape, not an assembly of boxes. Terrain mesh actors are reserved for retaining walls and the noncolliding water slab. Daytime sun, Sky Atmosphere and realtime SkyLight are saved in the map; brightness/rotation/capture exposure live in `layout.json.lighting`. Native modules must be loaded by reopening the project after a C++ build.

The LevelPrototyping update uses the actual pack Cube and Cylinder, with measured bounds and pivot compensation. See `mesh_mapping.md` and `prototyping_mesh_mapping.json` for paths, collisions, slots and exceptions. `Audit` queries the live Asset Registry and saved level without changing assets. Original pack materials use a different minor spacing and axis projection, so the generator duplicates the pack grid material into its own generated folder and replaces BaseColor with a metric surface shader. Own `MI_Metric_*` instances provide light style tints. Grid major/minor defaults are 100/10 cm. A surface frame with inverse-transpose normal scaling measures distance on slopes after nonuniform scaling; Landscape keeps its own world normal. Round walls use circumference/ellipse arc distance. Minor lines fade at distant views to avoid aliasing. Pack assets are never edited.

Editor Preferences → Level Editor → Thanh A Blockout contains ShowDimensions, SelectedOnly (default true), ShowElevation, ShowGrid, GridSizeCm, MinorGridSizeCm and MaxLabels. Select any generated mesh actor to draw live dimensions. In its BlockoutDimensions component, turn off MeasureWholeBuilding to measure the selected mesh part; whole-building mode includes foundations, roofs and spires. L/W follow the body actor's design axes. Base/Top are actual geometric min/max Z in the map datum, so Base can be below the ground when a foundation is included. Labels compare actual geometry sizes with generated design sizes and mark changes. They exclude debug objects. Ramp/stair detail uses the stored route profile transformed with the current actor, while L/W/H come from mesh vertices; editing mesh topology independently requires regeneration of route metadata. Interactive Editor preferences override data defaults for the preview, without rewriting saved material parameters.

Run the separate calibration map and render checks:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Audit -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode Calibrate -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Blockout/run.ps1 -Mode CalibrateCapture -SkipBuild
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Blockout/verify_grid_render.py
```

`/Game/Maps/Blockout/L_ThanhA_GridCalibration` contains actual pack fixtures: 1×1×1 m, 2×4×3 m, rotated/nonuniform boxes, pack ramp, cylinder and a composite with a generated roof. `calibration.json` controls these fixtures. The Unreal report measures actual vertices, rotates/scales a fixture temporarily and tests composite scope, then restores its transform. Face-aligned BaseColor captures and the Python reader measure physical pixel periods including ramp/roof slopes. Capture labels use the same measurement function as the viewport visualizer. These checks do not claim that interactive mouse selection was tested.

To repeat stair movement checks, launch UnrealEditor-Cmd with the project and blockout map, `-game -ThanhAAutoWalk -ThanhAStairWalk=plaza_west_stair -nullrhi -unattended -nosound`, or `plaza_east_stair`. Each test spawns once on the lower landing, then walks up and down with CharacterMovement, without route teleports/jumps. Results are separate `unreal_stair_playtest_*.json` files.

Geometry fixes are parameterized in `layout.json.geometry`: foundation and retaining caps extend 0.12 m above adjoining ground, road endpoints extend 0.03 m, exact coplanarity tolerance is 0.00001 m. Building body/roof and walkable road elevations are preserved. Roads use connected miter geometry; stairs use a single closed extrusion. Coplanar polygon subtraction removes overlapping exposed road/plaza faces, with an original closed mesh retained for collision. Deliberate interior building intersections remain. See `unreal_geometry_report.json` for clipped areas and closed collision checks. Change `prototyping.asset_revision` when modifying mesh/material generation algorithms so unchanged-input asset reuse cannot hide a code change.

`Capture -SkipBuild -DetailedViews` reads camera-only `detail_capture_views.json` and writes `unreal_detail_capture_report.json`, including actual camera settings. These views inspect the retaining cap without building occlusion, Unlit geometry at foundations/junctions and the west stair. Camera-only data does not change generation fingerprints or map geometry. Captures draw a transient Canvas label using an offline font in commandlets; no label actor is saved to the map.
