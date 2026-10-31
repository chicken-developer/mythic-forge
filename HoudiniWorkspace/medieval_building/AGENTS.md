# Codex handoff — Storybook Building v5.1

## Goal and baseline
Create and validate ONE reusable Houdini SOP HDA definition with 24 parameter presets, not 24 unrelated HDA files. Source is restored v5 plus basic interiors v5.1. User rejected multi-segment bevel v6 for quality/performance: do not reintroduce it or globally increase tessellation. Preserve architectural identities and existing facade/roof/blockout parameters.

## Read first
Read START_HERE_VI.md, README_VI.md, parameter_schema.json and manifest.json. Current evidence: 24 offline-generated OBJ meshes, three actual CPU interior preview images, static tests. No native Houdini cook/HDA/HIP has been run in the authoring environment. Native material shading/Unreal assignment is not implemented by this handoff launcher.

## Workflow
1. Locate the installed Houdini hython executable and license; record actual version. Use installed HOM docs/capability checks; do not assume latest online docs match runtime.
2. Run `python create_hda.py --check-only`. Then use a separate hython process to run `create_hda.py` with one preset and a new output directory. Fix observed API/cook failures in a copy; preserve errors and source diff. Do not fabricate success.
3. Inspect real native geometry: floor holes, winding/normals, UV/material_family, parameter recook and roof joins. Check a rectangle, L wing, round building and hybrid tower. Test min/small/large cases, stairs/headroom/door access and intersections. Existing counters omit wing interior aggregation; do not confuse those counters with complete scene counts.
4. Export one definition. Run verify_hda.py in a second hython process. For relocation verification, copy only HDA plus verifier to a separate directory and run there, without build_hda.py or original package on Python path. Compare geometry_sha256 for same preset with build_report.json.
5. Once sample passes, run create_hda.py --all-presets in a NEW output directory. Keep failures per record. Validate live blockout input separately (contract in BLOCKOUT.md); the initial launcher packages parameter mode only.
6. Assign native materials from material_family/groups using assets/textures, validate UV scale against offline previews. Current HOM export emits geometry attributes and Cd only, not native shader networks; avoid claiming identical renders by importing OBJ/HDA alone. Expose material controls if requested; maintain relative resource paths or embed assets deliberately.
7. Preserve license suffix .hda/.hdalc/.hdanc and .hip/.hiplc/.hipnc. Do not rename noncommercial files as commercial.
8. Use a single 960×600 render test first; do not automatically rerender all 24. Log elapsed time and actual runtime, inspect pixels, then increase resolution. If render-test skill is unavailable, follow RENDER_TEST_WORKFLOW.md.
9. Deliver native HDA, demo HIP, runtime/cook reports and updated source once verified. Keep original offline evidence clearly labelled and separate from native results.

## Safety and scope
Do not delete /obj or unrelated scene nodes. Launchers refuse existing output folders. Preserve checkpoints and source seeds. No need to ask again for routine reversible fixes required to cook/export this requested HDA. Do not claim gameplay readiness until collision, NavMesh and character clearance are tested.
