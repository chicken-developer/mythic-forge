# Dataset and evidence contract (schema_version 1)

## Files and paths

Deliver 5–10 **individual** JSON config records and a batch manifest. Keep `schema_version: 1` at each JSON root. Paths are POSIX-style, relative to the deliverable root; reject absolute paths and `..`. Copy input references when permitted, otherwise preserve source ID/URL and tell the user how to resolve them. Output IDs use letters, digits, underscore or hyphen only.

`parameter_schema.json` is a custom parameter registry, not the JSON Schema standard:

```json
{
  "schema_version": 1,
  "parameters": {
    "floor_count": {"type": "integer", "default": 1, "min": 1, "max": 3, "unit": "count"},
    "floor_height_m": {"type": "number", "default": 3.0, "min": 2.4, "max": 4.0, "unit": "m"},
    "roof_type": {"type": "string", "default": "gable", "enum": ["gable", "hip"]},
    "balcony_enable": {"type": "boolean", "default": false}
  },
  "constraints": ["balcony_enable requires floor_count >= 2"]
}
```

Use implemented params, not this illustrative list. Add `ui_label`, `visible_when`, effect and module mapping when useful. Constraints are descriptive; implement and test them in generator validation. Dataset validator checks primitive types/ranges/enums, not architectural expressions.

Each dataset record:

```json
{
  "schema_version": 1,
  "variant_id": "variant_01",
  "label": "Simple cottage",
  "complexity": "simple",
  "comparison_kind": "preset",
  "source_kind": "input",
  "reference_ids": ["ref_variant_01"],
  "parameters": {
    "floor_count": 1,
    "floor_height_m": 3.0,
    "roof_type": "gable",
    "balcony_enable": false
  },
  "seeds": {"layout": 42, "decoration": 101},
  "module_dependencies": [],
  "expected_features": {"floor_count": 1, "entrance_count": 1, "roof_type": "gable"},
  "assumptions": ["Hidden rear elevation follows the visible facade grammar"],
  "outputs": {
    "geometry": "geometry/variant_01.bgeo.sc",
    "hero": "previews/variant_01_hero.png",
    "comparison": "comparisons/variant_01.png",
    "result": "results/variant_01.json"
  }
}
```

All parameters must be fully expanded, including defaults; reject unknown/missing params. `seeds` is the canonical RNG seed source; expose matching UI controls and map them in code, do not maintain conflicting independent copies. `source_kind` is `input` for user variants or `designed` for additional planned variants; designed variants reference baseline but must never be counted as supplied input. Dependencies list asset ID/path, optional status and fallback. `comparison_kind` is `preset`, `sweep` or `baseline`; sweep additionally records changed parameter names and baseline ID. Stable source IDs remain unchanged when variants are reordered by complexity.

The `parameters` registry excludes RNG seed controls; define their UI-to-`seeds` mapping separately. Full effective config means `parameters` plus `seeds`; do not duplicate seeds in both. Baseline config, preview, comparison and result are separate from the N variant records and must be evaluated as well.

Manifest shape:

```json
{
  "schema_version": 1,
  "asset_name": "reference_building",
  "generator": "build_hda.py",
  "parameter_schema": "parameter_schema.json",
  "references": [
    {"reference_id": "ref_sample", "role": "baseline", "path": "references/sample.png"},
    {"reference_id": "ref_variant_01", "role": "variant", "path": "references/variant_01.png"}
  ],
  "datasets": ["datasets/variant_01.json"],
  "target_houdini_version": "unknown; capability checked at execution",
  "camera": {"projection": "orthographic", "framing": "common maximum bounds"},
  "render_backend": "pending_houdini"
}
```

This is a shape example, not a complete manifest; a real batch needs 5–10 records. References can have `source` instead of `path` when external/Library provenance is retained. Add image/crop index for sheet references. Use additional renderer settings as needed; no claim of exact camera calibration from unknown source.

## Evaluation result

`results/<variant_id>.json` includes variant ID, effective config, generator hash, runtime Houdini/Python version if executed, stage flags, metrics, observed features, failures and image provenance. Separate stages:

| Stage | Evidence |
|---|---|
| code_generated | Actual Python deliverable exists |
| static_checked | Outer/embedded Python compilation and dataset validation logged |
| houdini_cooked | Actual force cook, runtime version, node errors/warnings |
| geometry_exported | Exported file exists and opens with expected geometry |
| rendered | Actual geometry image file with backend/camera/config provenance |
| hda_packaged | Real digital asset definition, portable second-instance cook |

Use `not_run` or `pending_houdini` for unexecuted stages, null for unmeasured metrics. For image provenance use `houdini_geometry_render`, `houdini_viewport_capture`, `offline_geometry_preview` or `ai_concept`; AI concepts are never acceptance evidence. Source mapping, filenames and labels come from actual records.

## Validator

```text
python scripts/validate_dataset.py /path/to/deliverable/manifest.json
python scripts/validate_dataset.py /path/to/deliverable/manifest.json --require-images
```

The first invocation checks registry, 5–10 unique full configs, values, seed types, reference/variant mapping and output path safety. It reports incomplete complexity coverage. The second additionally checks every referenced hero/comparison file exists and has a PNG signature. This does **not** prove a PNG depicts its geometry; runtime evaluator and visual review must verify provenance and fidelity. Never interpret a validator pass as HDA pass.
