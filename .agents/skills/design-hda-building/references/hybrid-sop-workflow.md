# Hybrid Houdini SOP implementation rules

Use this reference before implementing or converting any building HDA. The target is an **inspectable, procedural SOP network** that an artist can edit manually after Codex generates it. Python is the graph builder/orchestrator, not the primary mesh cook engine. Follow the user's Houdini version; check actual operator names/parameters with that installation.

## 1. Choose the execution owner

| Task | Preferred owner | Avoid |
| --- | --- | --- |
| Create HDA, node graph, parameter UI, datasets, batch runner | Python HOM, executed once/outside cook | Scene/node mutations inside Python SOP cook |
| Primitive shapes, shell thickness, extrusions, curves, profiles, booleans | Native SOP | Python `createPoint`/`createPolygon` loops for whole buildings |
| Roof intersections, openings, support members, bevel, normal corrections | Native SOP subnets with intermediate display endpoints | Ad hoc mesh concatenation and blanket bevel |
| Parameter rules, per-piece attributes, masks, groups, placement, deterministic noise | VEX Wrangle, SOP parameters | One giant VEX Detail Wrangle covering all modeling |
| Material groups and simple module-relative planar UV/tiling | SOP groups + VEX or UV Texture/Project | Arbitrary world-space `@P` mapping on rotated/local parts |
| Complex UV seams, unwrap, island packing | UV Auto Seam/UV Flatten/UV Layout, or Labs Auto UV | Homegrown Python unwrapping/packing |
| Screenshot, geometry export, performance report | Houdini evaluator with actual runtime | Claiming static validation proves cook/render |

Prefer native SOP for well-supported topology-changing work. Python may create any of these native nodes and configure connections/channel expressions; that is still the required Hybrid approach.

## 2. Graph requirements

- Build one shared generator/HDA for all variants, with at least one representative *golden sample* configured before batch generation. Preserve existing HDA parameter names and datasets when updating an asset; document a migration if changes are necessary.
- Provide meaningful stage labels: `LAYOUT`, `SHELL`, `OPENINGS`, `ROOF`, `DETAILS`, `UV_MATERIAL`, `OUTPUT`; omit empty stages, branch/merge when required, and keep visible intermediate debug outputs.
- Expose real HDA parameters and bind them through channels or stable SOP inputs; changing a parameter must recook an affected stage. Do not produce an opaque frozen mesh or script-only shape pretending to be procedural.
- Assign stable `name`/`module_id`, `material_family`, `part_role` and relevant dimension attributes so VEX, UV nodes, renderers and QC can target individual parts.
- Distinguish `construction` (structural walls, roof, openings) from `decoration` (timbers, tiles, ornaments, props). Keep feature changes localized: roof changes should not rebuild unrelated interior/landscape assets without necessity.
- Preserve existing user nodes and HIP content. Regenerate only the subtree marked as generator-owned, using a deterministic naming/versioning strategy. Test idempotence.

## 3. Architectural constraints before modeling

Create a feature map `reference_feature -> module/subnet -> exposed parameter -> expected validation`. For house/workshop/blacksmith modules:

1. Derive massing/footprint, functional volume, floor height and entrances; never blindly scale one complete house.
2. Define wall continuity and actual openings. Doors/windows must cut the shell (unless explicitly a decorative faux opening).
3. Generate roof structure from supported footprint and roof grammar, not by placing disconnected roof prisms. Handle ridge, valley, eave, gable/hip/cone/dormer junctions explicitly.
4. Place architectural attachments only when their supports, clearance, access and roof/window compatibility pass.
5. Apply trims, tiles and decorations after the underlying structure is valid. Use instances/packed pieces for repeated parts where possible.

For unsupported combinations, reject with a clear error. Do not silently replace unsupported plus-shaped, courtyard, cone or mixed roof with a simple gable or box.

## 4. Bevel, normals and level of detail

- Tag bevel candidates by semantic edge group before `PolyBevel`; avoid beveling every edge after arbitrary Boolean operations.
- Clamp bevel width relative to the local minimum feature thickness and neighboring edges. Test corners, narrow window frames, tile ends, tower tips and roof intersections.
- Inspect silhouettes and shading at fixed gameplay camera distance. A higher polygon count is not equivalent to better stylized quality.
- Keep `LOW`, `MID` and `HIGH` clearly distinguished by geometric detail, but preserve massing, extents and attachment locations unless a deliberate LOD decision says otherwise.
- Add normals/fuse cleanup after topology operations, with intentional hard/soft boundaries and no uncontrolled smoothing over crisp silhouette transitions.

## 5. UV/material strategy by part

| Part family | Default UV strategy | Validation |
| --- | --- | --- |
| Flat/regular walls, floor, stone blocks | Local UV projection, world-unit-consistent tiling | Same apparent texel scale at varied dimensions |
| Gable/hip roofs, repeatable tiles | Roof-local axes, tile instances/UV pattern | Texture grain aligns with slope; no rotation/flip |
| Timber beams, pillars | Along-length mapping or trim-sheet region | Grain direction and trim bands correct |
| Decorative trim and modular ornaments | Trim sheet/known UV bands | UV stays within assigned bands |
| Irregular chimney/cap, sculpted unique mesh | Native seams + flatten/layout, manual refinement if needed | No unintended overlap/stretch, correct padding |

Never run one global projection across all rotated faces. On native unwrap, use explicit seam/group constraints where known; automatic seams are only a starting point. Decide whether UV0 is tileable/overlapping by design or uniquely packed. If a non-overlapping secondary UV channel is needed for a project's bake/lightmap pipeline, generate/validate it separately; do not impose it on every asset by default.

Substance/Unreal controls may vary tile scale, offset and material palette; preserve their semantic mapping to HDA material groups. Add a checker/grid render for every material family in the representative sample and at least one stressed variant.

## 6. Verification gates and reporting

**Gate A — static:** Validate syntax for outer Python and embedded VEX snippet storage where feasible, dataset schema and expected node type/parameter checks. Never claim a Houdini result from static checks.

**Gate B — graph:** In a Houdini process, build the graph; inspect stage paths, final output dependency, node types, Python SOP usage and warnings. Fail if the HDA is a wrapper around one Python mesh-building SOP. Save `graph_audit.json` with actual evidence or `not_run` when no runtime exists.

**Gate C — geometry:** Cook baseline plus all 5–10 datasets and additional min/max/invalid parameter tests. Check bounds, nonempty output, openings, roof junctions, supports, self-intersections/z-fighting, degenerate faces, normals, UV and stable part IDs.

**Gate D — interaction:** Change one structural parameter (for example footprint width or roof grammar) and one independent UV/material parameter; verify the correct geometry/UV/material property changes and unrelated features remain stable. Reset and repeat with the same seed to verify deterministic output.

**Gate E — portability:** If packaging is supported, instantiate the saved HDA in a fresh scene/process without the authoring script path; verify a parameter change still recooks. Check Houdini Engine/Unreal material grouping and instance output when available.

**Gate F — visuals:** Render actual cooked geometry per dataset plus comparable fixed-camera sheet; inspect roof, bevel, silhouette, scale and UV checker. Keep AI concept images separate. Do not fabricate missing Houdini render/cook evidence.

Record `pass`, `fail`, `not_run` or `pending_houdini` per gate with evidence paths and exact limitations. If no Houdini runtime is available, deliver the generated network builder and batch evaluator but clearly mark geometry/UV/render/HDA gates unverified.

## 7. Deliverable standard

Maintain the existing `implementation-contract.md` and `dataset-contract.md` structure. Include `build_hda.py` as a **native SOP graph builder**, `evaluate_hda.py`, parameter schema, 5–10 complete JSON variants, manifest, code/graph audit, geometry exports and per-variant images when actually produced, and an evidence-based evaluation. Optionally store VEX snippets as named text modules, but embed/resolve them portably when packaging an HDA.

Never substitute fake preview art for executed Houdini output. Source code, datasets and assertions are not proof that a complex Gothic or Medieval building cooks and renders correctly.
