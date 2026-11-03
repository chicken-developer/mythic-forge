# Houdini implementation contract

## Deliverable layout

```text
<asset_name>/
  build_hda.py
  evaluate_hda.py
  parameter_schema.json
  manifest.json
  analysis.md
  evaluation.md
  graph_audit.json
  datasets/variant_01.json ... variant_NN.json
  references/<source images or source mapping>
  previews/variant_01_hero.png ... variant_NN_hero.png
  comparisons/variant_01.png ... variant_NN.png
  geometry/<variant_id>.bgeo.sc                 # only after actual export
  asset/<asset_name>.<license-correct suffix>   # only after actual packaging
  results/<variant_id>.json                    # measured results or not_run
```

Preview/comparison outputs are mandatory completion targets, but missing-runtime status must be explicit rather than fabricated. Do not fill the folders with unrelated generic renders. Preserve user filenames in manifest; use safe output IDs in paths.

## Python entry points

Deliver a self-contained main Python file (stdlib + `hou`; no pip installation for its default path) that **constructs a SOP graph**, not one that directly constructs every polygon. Embed baseline configuration so it runs even without external datasets. Keep optional library meshes separate and identify their paths. Avoid executable `eval` for JSON settings.

Expose equivalent APIs, with signatures shown in delivery instructions:

```python
build(config=None, parent_path="/obj", asset_name="reference_building")
# return the owned object/HDA node; config=None uses embedded baseline
apply_config(asset_node, config)
# validate everything first, then set real parms outside SOP cook
export_asset(asset_node, output_path)
# optional; create real definition, portable code/defaults, license-aware
```

A runnable Python Shell example must use the actual deliverable and real signature:

```python
import runpy
builder = runpy.run_path(r"D:/HDA/reference_building/build_hda.py")
asset_node = builder["build"]()
```

Use main guard deliberately: importing/run_path must not create duplicate nodes; CLI execution may call `main()`. Deliver a command such as:

```text
hython build_hda.py --dataset datasets/variant_01.json --output-dir output
hython evaluate_hda.py --manifest manifest.json --output-dir output --render
```

Implement these arguments if documented; don't provide hypothetical commands. Give paths relative to deliverable root and resolve from `__file__` or explicit root, not cwd. Batch evaluator must loop all records, log each failure, return nonzero on failure and produce a summary. Persist effective config and generator SHA256 with every result.

## Procedural architecture — required Hybrid SOP

Build a SOP subnet/HDA with exposed controls and channel references into **native SOPs**. Use Python HOM for installation, node wiring, UI/schema, runner and export **outside cook**. Use VEX for per-element attributes/rules, not for a monolithic opaque geometry generator. Preserve a shared layout data model; do not scale a whole frozen house for every setting.

**Disallow Python SOP as the primary geometry generator.** A Python SOP may serve only as a documented, bounded data adapter without an equivalent native operation, and must not construct the bulk of points, polygons, roofs, walls or UV islands. Do not replace a missing native operation by quietly writing Python polygon loops. If a feature needs a complex geometry algorithm, construct a legible native SOP subnet or declare and test a narrowly scoped VEX/native alternative.

Organize actual graph dependencies in labeled stages such as `LAYOUT → SHELL → OPENINGS → ROOF → DETAILS → UV_MATERIAL → OUTPUT`, with branches/merges when suitable. Stages with no geometry need not be empty placeholder subnets. Make one usable debug display endpoint per major stage. Store stable semantic primitive groups (`wall`, `roof`, `frame`, `trim`, etc.) and `name`/material attributes for downstream operations.

Houdini-specific rules:
- Create network/UI/set parms outside Python SOP cook. SideFX explicitly cautions that setting parms inside a Python SOP causes broken dependencies.
- If a narrow Python SOP adapter is exceptionally necessary, only write `hou.pwd().geometry()` inside that cook. Geometry retrieved from another cooked SOP is not writable. Never iterate over all architecture polygons in Python as a substitute for native modeling.
- Check actual node types and `parm`/`parmTuple` existence before setting version-dependent controls. No guessed Boolean/UV/PolyBevel parm names hidden by `except: pass`.
- Expose a single schema source for defaults/types/menu tokens/ranges. Verify `apply_config` uses correct parm encoding; menu token versus index and checkbox int versus JSON bool need explicit conversion.
- Prevent cyclic dependencies. Do not edit the locked asset definition from within its own geometry cook. Installation and export run outside cook.
- Own a named subtree via user data/namespace; on rerun update only owned nodes. Do not save over an existing HIP unless explicitly targeted.
- If using Python SOP code strings, compile both outer script and embedded strings. If using HDA PythonModule, qualify calls with the proper asset node; embed needed modules/resources or package with documented search paths.
- For portability, create a second instance from the packaged definition in a fresh scene/process, move the deliverable, cook without the authoring script/scratch path. An external script path in a locked HDA is not a self-contained asset.

## SOP, VEX and UV allocation

- Construct module shape with native SOPs, including curve/sweep/polyextrude/boolean/polybevel as applicable. Prefer instancing over duplicated geometry for repeated tiles/ornament. Use per-module roof/building rules and volume/junction checks before detailing.
- Use VEX Wrangles for feature assignments, layout constraints, metadata, point/primitive masks and local UV transforms; prefer native SOP for topology-changing work and complex arbitrary UV unwrap.
- Apply bevel to explicitly selected edges and cap width relative to adjacent feature dimensions; validate concave/convex corners, thin elements and intersection topology. Do not equate `HIGH` detail with blanket subdivision or bevel of every edge.
- For tileable walls/roofs use consistent module-relative UV projection/scale; for trim-sheet geometry retain expected orientation/UV bands; for irregular pieces use UV Auto Seam/UV Flatten/UV Layout or Labs Auto UV as supported by the installed version.
- Validate every material family with UV checker/grid, seam/stretch checks, tiling direction and comparable texel density. Keep editable Unreal material tiling separate from baked mesh UV assumptions.

## Hybrid graph audit

Produce `graph_audit.json` after actual Houdini execution with `houdini_version`, counts by SOP type, Python SOP node list, Wrangle node list, active stage paths, final output node path, parameter-to-node connections, and a `not_run` state when inspection was impossible. Flag a full-Python geometry generator as a failure. A static script inspection is not a cooked network audit.

For a geometry-affecting parameter and an independent UV/material parameter, record before/after observations or hashes (geometry positions/counts and UV/material attributes as appropriate). Verify no orphaned output nodes, accidental file-backed frozen result, cyclic dependencies or silently disconnected HDA parameters. Include a representative graph screenshot only when actually captured from Houdini, never synthesize one and call it an application capture.

## Geometry acceptance

Set an explicit finish scope before modeling: exterior with all visible structural details by default; playable interior, sculpted ornament, destruction and final textures only if requested. Do not call a blockout completed geometry.

Required checks for each supported configuration:
- Nonempty output, finite coordinates, expected dimensions and floor/bay/opening/attachment counts.
- Ground contact, realistic thickness, usable entrance clearance; openings cut the shell, not a panel layered over a solid wall.
- Roofs cover bodies and solve junctions/valleys; compatible dome/cone supports. No floating dormer, stair or balcony without intentional structural logic.
- Avoid coplanar duplicates/z-fighting. Separate intentionally overlapping structural members from accidental clipping. Weld only appropriate boundaries; do not force all modular assemblies into one watertight manifold.
- Inspect degenerate faces, unwinding/inverted normals, UV coverage/seams, tile scale/orientation, visible stretching and per-family material assignment. Treat intentional open borders with documented per-part rules.
- Finite reasonable polygon/cook budget declared for each detail mode; no unbounded per-tile Python mesh loop at large counts.
- Same seed/config produces stable part IDs and topology/positions; compare canonical geometry hashes excluding timestamps and render metadata.
- Invalid combinations produce actionable errors before scene mutation. Never silently downgrade footprint/roof.

Run baseline, all N records, min/max supported ranges and incompatible configurations. Compare a repeated record and a safely rerun owned subtree. These extra tests do not replace input-matched records. Use measured results; AST compilation is not HOM/cook validation.

## Image and evaluation pipeline

Implement `evaluate_hda.py` to build/apply configs, force cook, collect errors/warnings, export geometry, measure bounds/counts/timing, and render or capture. Select render backend by available version/license capabilities. Record backend, camera transforms, projection, resolution and light setup in manifest/results. Headless code must not assume a Desktop/SceneViewer exists.

Per variant: hero image plus comparison with its exact input; orthographic front/top/wireframe as diagnostic views when useful. Compose labels from JSON, not AI-generated text. Matching input view uses its own camera when known; fixed evaluation view remains comparable across the batch. For source collage crops, label crop ID.

Offline previews are allowed only when generated from the shared geometry data used by the Houdini builder. Exported OBJ rendered outside Houdini is also real geometry evidence but doesn't prove HDA cook unless export came from a measured Houdini run. Keep AI concepts in a separate clearly labeled category.

Report expected versus observed features, geometry validity, visual mismatch, code fixes and remaining limitations. Do not give a numeric accuracy percentage from an arbitrary aesthetic impression. If features cannot be checked automatically, mark manual visual QA pending.
