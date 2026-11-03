# Case A — Context-aware architecture, Houdini Editor cook → Unreal bake

Use this contract for Gothic/Medieval/Storybook procedural buildings in Houdini 22 where level artists modify HDA parameters and **bake** assets for Unreal. This is an engineering adaptation of context-aware procedural modeling, **not** a claim to reproduce Tiny Glade's unpublished algorithms.

## 1. Data and semantic graph

Split data into two layers:

1. **Authoring intent** (JSON/datasets/HDA UI): footprint, module dimensions, wall paths, roof type, openings, material family, seeds and functionality.
2. **Semantic graph** (generated/validated before any mesh): stable part IDs and typed edges, for example `opening_id → wall_id`, `roof_patch_A ↔ roof_patch_B : valley`, `support_id → attachment_id`.

Keep old flat `parameter_schema.json` and `datasets/*.json` compatible. A versioned `semantic_graph.json` is an optional sidecar for complex/topological inputs; it must not duplicate or contradict parameter defaults. If migrating datasets, provide explicit conversion and old/new comparisons.

Use Houdini's Y-up convention, metres, wall-local `u` measured from its stable start point, and `x,z` for the plan. Typical IDs: `building_id`, `wall_id`, `opening_id`, `roof_patch_id`, `roof_junction_id`, `module_id`, `part_role`, `material_family`. Disallow dangling references, duplicate IDs, zero-length walls, window outside host wall, unsupported roof adjacency and structurally impossible spans. Validate **before** mutating the scene.

Sample input: `assets/case_a_semantic_demo.json`. Preflight utility: `scripts/preflight_case_a.py`; it validates one intentionally simplified rectangle/L-shaped roof-patch schema and draws a diagnostic preview. Do not present the preflight as a full Houdini parser, exact roof-intersection solver or geometry renderer.

## 2. Solve constraints before geometry

Required order:

1. Compute usable footprint, facade modules, access, floor dimensions and roof envelopes.
2. Resolve wall segment adjacencies/corners, openings, load/support, height and clearance.
3. Resolve roof patches and **actual** ridge/valley/hip/dormer intersections before shingle/tile placement.
4. Construct thick shell, open holes, roof geometry and main frame/trim with native SOPs.
5. Subtract opening intervals from each repeating masonry/plank course, then instance/trim pieces only on valid intervals. Cap ends, avoid paper-thin pieces, and use custom border rules for arches.
6. Generate architectural detail and wear masks from distance, surface orientation, water paths and neighbouring geometry.
7. Generate UV, material groups, collision and Unreal output classifications.

Example wall-course algorithm:

```text
for each stable wall_id:
    for each course_height:
        blockers = opening intervals intersecting the course
        usable_intervals = subtract([0, wall_length], union(blockers))
        for each usable_interval:
            distribute bricks/planks with stable bond/seed
            clip or cap terminal pieces using native SOP
    connect each frame to its opening_id, not to anonymous polygon indices
```

The outer algorithm describes **rules only**. Implement bulk geometry with Curve/Sweep/PolyExtrude/Boolean/Copy to Points/instancing and VEX attributes. Do not implement a monolithic Python SOP that creates all polygons. Avoid hundreds of Boolean operations where interval subtraction or a single surface-level cut is sufficient.

Roof junctions are separate topology problems. Build a roof surface per patch, compute line/plane or boundary intersections under a documented grammar, select retained faces, stitch/generate flashing/valley trim and clip the tile layout to the final roof domain. Declare which combinations are implemented (`gable`, `hip`, `L`, `T`, dormer, cone/spire etc.). A named graph edge `valley` alone **does not mean** its geometry has been solved.

## 3. Recommended HDA network

```text
INPUT_INTENT / HDA PARAMS / OPTIONAL SEMANTIC JSON
  └─ VALIDATION / SEMANTIC IDS / CONSTRAINTS
      ├─ WALL_LAYOUT → WALL_SHELL → OPENING_CUTS → FRAME/TRIM
      ├─ ROOF_PATCHES → RIDGE_VALLEY_SOLVER → TILE/FLASHING
      ├─ STRUCTURAL_SUPPORTS / CHIMNEY / ATTACHMENTS
      └─ COURSE_SPLIT → BRICK/PLANK INSTANCERS → WEAR MASKS
                  ↘ MERGE → UV_MATERIAL → LOD/COLLISION → OUTPUT
```

Keep stages as readable subnets with inspection outputs; document their producer/consumer attributes. Python HOM creates/configures those subnets and applies datasets *outside* cook. Native SOPs own topology and UV. VEX evaluates spatial rules, semantic attributes, interval splitting and stable placement. Do not pack all features into one large Detail Wrangle or make a Python-generated frozen geometry cache the sole output.

For topology-changing JSON edits, use explicit Editor rebuild/update commands that preserve user-owned nodes and source IDs. For parameter edits on a baked graph, use channel references and native SOP cook dependencies. Cache per stable module/seed/config revision where practical and invalidate only affected semantic branches.

Give `WALL_LAYOUT`, `OPENING_CUTS`, `RIDGE_VALLEY_SOLVER` and other significant operations actual module boundaries with declared ports, stable IDs and debug outputs; follow [modular-sop-architecture.md](modular-sop-architecture.md). A drawing of disconnected subnets does not satisfy this requirement: actual connections must follow `module_graph.json`.

## 4. Unreal output and baking

Default target: Houdini Engine **inside Unreal Editor**. Follow:

```text
Versioned JSON/preset → Houdini/HDA cook → geometry/UV/collision QA
    → Unreal material/instance QA → Bake/Replace → reopen UE map/gameplay QA
```

Outputs:

- Exterior shell and trim as named geometry groups/parts; use deliberate material slot IDs and stable metadata.
- Repeated roof tiles, beams and decor as packed/instanced primitives where supported; check actual Unreal instancer outputs and grouping.
- Game collision (door clearance, walkable interior if in scope), optional LOD/Nanite policy and lightmap UV as specified. Do not generate a hidden interior merely because a building has a window.
- HDA/dataset/seed/generator-version/building-ID handoff for level artists. Retain source HDA for editing; published levels use baked Unreal-native assets.

**Do not describe Houdini Engine as a gameplay runtime geometry generator.** A Tiny-Glade-like player-facing builder requires a separate Unreal C++/geometry runtime effort; baking a static HDA is not that feature.

Official verification targets:
- SideFX Unreal introduction: https://www.sidefx.com/docs/houdini/unreal/intro.html
- SideFX Unreal packaging/bake: https://www.sidefx.com/docs/houdini/unreal/packaging.html
- SideFX meshes/instancing: https://www.sidefx.com/docs/houdini/unreal/meshes.html
- SideFX outputs: https://www.sidefx.com/docs/houdini/unreal/outputs.html

## 5. Required diagnostic matrix

| Case | Scope | Must demonstrate |
| --- | --- | --- |
| Golden baseline | One facade, door and window | True wall holes, caps, frame, roof eave, UV/material groups |
| Param sweep | Move one window by a known distance | Only its wall/opening/detail branch changes; other stable part IDs unaffected |
| UV sweep | Material tiling/offset | UV/material changes without topology drift |
| Medium roof | L-shaped massing + two intersecting gables | Real roof valley/ridge treatment; no exposed overlapping slabs |
| Complex roof | Multi-roof tower/dormer/attachment if supported | Supports, clearance, no missing roof panels |
| Invalid | Window outside wall/overlapping non-mergeable window; missing roof joint | Validation error before scene mutation; no silent fallback |
| Determinism | Same config/seed twice | Same stable part IDs and normalized geometry hashes |
| Editor bake | Bake and replace in Unreal Editor | Valid native assets after map reload; no live HDA cook in game |

## 6. Preview/evidence integrity

When Houdini is absent, **skip offline Python proxy render and semantic 2D previews by default**. Prepare the real graph builder, JSON datasets and `module_graph.json` and run syntax/schema/DAG checks only; report `houdini_cooked=not_run`, `rendered=not_run`, `unreal_baked=not_run` and `graph_modularity=not_run`. The existing `scripts/preflight_case_a.py` is a legacy optional diagnostic and must not be run unless the user explicitly requests it.

When Houdini and Unreal Editor are available, continue with real graph/cook/geometry/UV/render/bake gates from the other reference contracts, recording version, errors, timing and screenshot provenance. Do not fill missing measurements with estimates or claim game-ready status from preflight alone.
