# Modular SOP architecture — functional modules with explicit contracts

Read this reference before generating or refactoring any Houdini building HDA graph. A visual network box or color is **not** a module. The required result is a hierarchy of real SOP subnets/sub-HDAs with explicit data flow, scoped state, and inspectable Houdini geometry operations. Apply this contract to both new generators and migrations of existing HDA networks.

## 1. Design principles

Apply software engineering patterns to geometry networks without forcing OOP syntax into SOP cooks:

- **Single responsibility:** one subnet solves one architectural concern (e.g. wall shell, opening cuts, roof junctions), not a miscellaneous collection of SOP nodes.
- **Encapsulation:** a parent consumes the module's declared input/output connectors; do not wire into arbitrary internal nodes, use cross-subnet absolute `op:` references as hidden dependencies, or reach outside the generator-owned subtree.
- **Functional transformations:** think `output_geometry, attributes = module(input_geometry, parameters, semantic_ids)`; keep deterministic seeds and immutable upstream geometry. Geometry mutation belongs to native SOP/VEX cook, not HOM construction loops.
- **Dependency inversion:** connect reusable modules through published geometry streams, semantic IDs and parameter channels. Avoid hardcoded paths to one building or one dataset.
- **Composition over inheritance:** build one house by composing layout, shell, openings, roof, facade, surfacing and output modules. Only promote a subnet into a reusable sub-HDA if it genuinely has independent reuse value and its version/dependency can be packaged.
- **Open for extension:** introduce a new roof grammar or ornament module without rewriting unrelated facade/UV modules; maintain stable public parameter names, output attributes and IDs.
- **Observability:** each module exposes a debug endpoint and meaningful node comments; a downstream failure can be isolated to one stage.

## 2. Reference hierarchy and graph ownership

Adapt stage names to the actual asset, but preserve the same boundaries and data flow. Empty or one-node decoration-only subnets are not required.

```text
BUILDING_HDA (public UI/schema, OUT_BUILDING)
  LAYOUT                         # intent / geometry-independent constraints
    input_normalize             # JSON/preset/HDA parameters
    semantic_layout             # footprint, wall and roof patch IDs
    constraint_validation       # fail invalid relationships before build
  SHELL
    foundation_and_floors
    wall_shell
    structural_supports
  OPENINGS
    opening_host_resolver
    opening_cuts
    frame_and_sills
  ROOF
    roof_patch_surfaces
    ridge_valley_solver
    roof_trim_and_tiles
  DETAILS
    facade_course_intervals
    beam_and_decor_placement
    context_wear_masks
  UV_MATERIAL
    local_uv_by_family
    normals_and_bevel_groups
    material_assignments
  OUTPUT
    geometry_assembly
    instance_and_collision_classification
    game_export
```

- `LAYOUT` provides canonical geometry-intent attributes/guide streams. `SHELL`, `OPENINGS` and `ROOF` receive declared dependencies from those guides; `DETAILS` depends on resolved structural outputs. `UV_MATERIAL` and `OUTPUT` consume the assembled streams. Do not force all branches into one long serial wire if separate branches are more correct.
- At the HDA root expose **a small number of stage subnets** and the final output. Inside a stage, group related atomic SOPs into task-specific module subnets; use a third nested helper level only when it prevents substantial duplication. Avoid both a flat graph of hundreds of nodes and a needless subnet around each SOP node.
- Use a bounded module size as a review heuristic (roughly 5–25 meaningful SOP nodes per module). A larger roof solver is acceptable with documented child modules; split by responsibility, never only to satisfy a numeric limit.
- A stage is a real subnet with visible connections and editable internal SOP nodes, not a named Null, network box or a single Python SOP that hides all geometry work. Network boxes, colors, sticky notes and node positions are presentation aids *inside* the actual modular hierarchy.

## 3. Required module interface

Before constructing nodes, define each module in a machine-readable `module_graph.json` and a concise contract table. Use a JSON root `{"schema_version": 1, "asset_name": "building_name", "final_module": "output.game_export", "modules": [...]}` where `modules` contains the records below. Mark a deliberately unconnected debug-only module with `"diagnostic_only": true`; never mark a production stage diagnostic to bypass reachability. Maintain a single source of module IDs/ports in the HOM builder; the manifest and actual subnets must agree.

| Field | Contract |
| --- | --- |
| `module_id` | Stable, unique, e.g. `roof.junctions` or `openings.frame_trim` |
| `stage` / `path` | Owning functional subnet and deterministic internal node path |
| `owner` | Stable generator ownership tag for safe incremental rebuilds |
| `responsibility` | One sentence defining the transformation |
| `inputs` | Named upstream producer and real subnet connector/parameter source; data class `geometry`, `guide`, `semantic`, or `mask` |
| `outputs` | Named, routable geometry streams and semantic attributes; explicit debug output when useful |
| `params` | Relevant HDA/channel references, not copies of another module's hidden UI state |
| `attributes` | Consumed/produced fields such as `wall_id`, `opening_id`, `roof_patch_id`, `name`, `material_family` |
| `depends_on` | Module IDs, forming an acyclic dependency graph |
| `debug_endpoint` | Node path/flag yielding inspectable intermediate geometry |
| `owner` / `version` | Generator ownership and module contract version for safe reruns/migration |

Example (shape only; names and connectors must match actual Houdini graph):

```json
{
  "module_id": "roof.junctions",
  "stage": "ROOF",
  "path": "ROOF/ridge_valley_solver",
  "owner": "design-hda-building",
  "responsibility": "Resolve intersecting roof patches before tile placement",
  "inputs": {
    "roof_patches": "roof.patch_surfaces:geometry",
    "adjacency": "layout.semantic_layout:roof_adjacency"
  },
  "outputs": ["roof_resolved", "ridge_valley_guides"],
  "params": ["roof_type", "eave_depth_m"],
  "attributes": ["roof_patch_id", "roof_junction_id", "part_role"],
  "depends_on": ["roof.patch_surfaces", "layout.semantic_layout"],
  "debug_endpoint": "ROOF/ridge_valley_solver/OUT_DEBUG",
  "version": 1
}
```

- Give each real subnet a predictable input path and output routing. `IN_*` and `OUT_*` Nulls are useful **internal markers** but are not a substitute for a correctly wired Houdini subnet connector/output; verify the actual exported connector behavior for Houdini 22.
- Preserve the `name`/semantic attributes across module boundaries. Define how missing attributes and empty geometry are handled; fail on missing required IDs rather than silently placing generic boxes.
- Use geometry groups/attributes to multiplex semantic streams when SOP input-count limits require it, but document the schema; do not overload one unnamed geometry stream without a contract.
- Validate the declarative contract with the bundled `scripts/validate_module_graph.py`: from the skill directory use `python scripts/validate_module_graph.py /path/to/module_graph.json`, or copy the validator into the generated deliverable's `scripts/` before running `python scripts/validate_module_graph.py module_graph.json`. The validator checks IDs, port references, DAG/cycles, and reachability into `final_module`; this **does not inspect Houdini nodes or produce offline geometry previews**.

## 4. HOM builder organization (FP-style factories)

Keep `build_hda.py` as the runnable public entry point, but split its implementation into small reusable builders/functions or a bundled source package as appropriate:

```text
build_hda.py                  # build/apply_config/export + CLI
hda_builder/
  schema.py                  # config normalize/validate, immutable specs
  graph_core.py              # node creation, wiring, ownership, layout helpers
  modules/
    layout.py
    shell.py
    openings.py
    roof.py
    details.py
    uv_material.py
    output.py
  graph_audit.py              # connectivity/contract checks
vex/                         # small scoped wrangles/functions
```

This directory layout is illustrative. The deliverable may also use a single `build_hda.py` with clearly separated factories for small assets; do not introduce packaging overhead for a 10-node graph. When using helper files, embed/resolve every required module in the portable HDA definition or ensure the *cooked* HDA's SOP graph does not depend on local script paths.

Implement a reusable builder service for `create_node`, `connect_port`, `tag_owned_node`, `expose_parameter`, `layout_module` and `publish_output`. Use a context object to supply parent, schema, module registry, version and seed. Builders should **return named port handles** rather than reach directly into unrelated module internals. Topologically sort declared `depends_on` edges and fail on cycles, missing producers, missing mandatory ports or duplicate `module_id` *before* scene mutation. Build only one shared generator across all datasets.

## 5. Network layout, maintenance and updates

- Place top-level stages in readable processing order, branches with minimal wire crossings, primary data flow consistently left-to-right or top-to-bottom. Use `layoutChildren`/network boxes/comments/colors via verified HOM APIs as needed, but never confuse visual arrangement with dependency structure.
- Label nodes by action and semantic role (`cut_wall_openings`, `solve_valley_01`, `assign_roof_uv`) rather than default `boolean23`, `attribwrangle54`. Avoid duplicate ambiguous names; stable names enable script reruns and QA.
- Clearly annotate critical decisions: coordinate system, expected attributes, supported roof grammar, opening assumptions and upstream dependencies. Keep comments brief and on relevant module or SOP node.
- Preserve user-authored subnets/nodes. Track generator-owned modules with node userdata and rebuild/update only those modules. Maintain `schema_version`/contract migration if outputs or public parm semantics change.
- For each parameter change, compare affected module outputs/IDs and verify unaffected branches retain stable IDs/seeds. A parameter with no visible downstream influence is a defect.
- Never over-abstract a single primitive or add excessive helper HDAs. The goal is maintainability and topology correctness, not maximum hierarchy depth.

## 6. Refactor an existing scattered Houdini graph

For an input `.hip`/`.hda`, **reorganize the existing node graph instead of replacing it with a new building generator**:

1. In Houdini, inventory the current HDA parameter interface, all SOP paths/types, wire edges, expression/path references, user-owned nodes, output flags, displayed intermediate results and critical geometry hashes. Save a before-state `graph_audit.json`/manifest and backup safely without overwriting the original file.
2. Classify functional subgraphs by **semantic responsibility and data dependencies**, not by node type or screen position. Identify the cut edges where module connectors will be introduced; keep roof and opening solvers separate from repeated trim/tile instancers.
3. Plan each module's inputs/outputs/attributes first in `module_graph.json`; run the static validator. Detect any cycles, absolute path references, HDA interface assumptions and incompatible subnet connector limits before modifying the actual nodes.
4. Encapsulate or reconstruct generator-owned nodes inside real functional subnets, reroute **through declared input/output connectors**, and fix relative paths/channel references. Preserve public HDA parameters, stable `name`/part IDs, manual edits and user-authored nodes. Only migrate/change public APIs with explicit version mapping.
5. Cook **before and after** under matched datasets, seeds, Houdini versions and operator settings. Compare bounds, geometry/attribute hashes, UV/material groups, opening/roof joins and a structural parameter sweep. Record intentional changes separately; a successful recook with changed or disconnected output is a regression.
6. Reorganize node positions/comments after wiring passes. Keep a diagnostic copy of the old graph until artist QA confirms parity. Never delete user nodes or overwrite the original HDA/scene during an automatic refactor.

If Houdini is missing, deliver only a refactor migration plan/script and mark node-by-node migration and geometry parity `pending_houdini`; do not claim the original HDA has been reorganized.

## 7. Mandatory modularity audit in Houdini

After HOM creates the graph, inspect actual nodes in a Houdini/hython session; `module_graph.json` alone is a **design contract**, not audit evidence. Augment `graph_audit.json` with:

1. Each declared module's real path, type, parent, owner tag, input/output connectors and resolved upstream dependencies.
2. One upstream-to-final-output reachability check for every non-diagnostic module; no orphaned critical stages, broken connector outputs, dangling path references or dependency cycles.
3. Root/stage/module hierarchy and node counts per module. Flag an excessively flat root, anonymous ungrouped SOP chains, large multi-purpose modules, trivial one-node subnet proliferation or cross-module internal-node dependencies.
4. Real named parameters and `apply_config` propagation: moving one window updates `OPENINGS`/facade dependencies, while unrelated roof layout IDs remain stable; changing roof type updates `ROOF` and its downstream detail/surfacing branches.
5. Debug endpoints for `LAYOUT`, `SHELL`, `OPENINGS`, `ROOF`, `DETAILS`, `UV_MATERIAL` where populated, plus a proven final `OUTPUT` SOP.
6. A small network screenshot *only when captured inside Houdini*, plus the module contract table and actual `pass`/`fail`/`not_run` results.

Graph-level acceptance is **failed** when a module exists only as a color/network box, wiring bypasses a declared subnet, or the geometry engine is a monolithic Python SOP/giant VEX wrangle. Fail with path-specific guidance, not just a score. If Houdini is unavailable, report `graph_modularity=not_run` and prepare the audit for the user to run in Houdini; never fabricate inspected nodes, screenshots or recook metrics.

## 8. Source references

- SideFX: https://www.sidefx.com/docs/houdini/network/organize.html
- SideFX: https://www.sidefx.com/docs/houdini/hom/hou/OpNode.html
- SideFX: https://www.sidefx.com/docs/houdini/hom/nodeuserdata.html
