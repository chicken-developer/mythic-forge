# Current Medieval authoring workflow

Read `../../MEMORY.md`, `analysis.md`, and `README_VI.md` before editing this
asset. Apply `.agents/skills/design-hda-building/SKILL.md` from the repository.

The editable source is `medieval_building.hip`, `/obj/medieval_building`. Retain its
18 function subnets, typed local arguments, public output ordering, native SOP
topology and VEX rules. Only caller bindings read `CTRL_BUILDING`. Preserve
existing parameter names; new roof-axis, window-shape, awning-span and palette
choices have implemented geometry/material effects. Do not restore the obsolete
full-Python generator from old Unreal review copies.

`scripts/update_model.py` updates only the named native nodes it owns. Its VEX
sources are in `sources/vex/`. `build_hda.py` loads/applies full configs and
packages `asset/medieval_house_layout.hda` (`mythic::medieval_house::3.0`). A fresh
HDA process must pass `scripts/check_portable.py` after graph/interface changes.

The 15 requested configs are in `datasets/`; `baseline.json` is additional
validation evidence. Keep source reference IDs stable. `manifest.json` is the
full batch; three group manifests allow the installed 5–10-record validator to
check the explicitly requested 15 records without editing the skill.

Use `scripts/check_configs.py`, then `scripts/bake_variants.py --no-render`.
Render via separate `scripts/render_variant.py <id>` processes. Do not mix GPU
Copernicus baking and OpenGL rendering in one process on this machine.
Unreal review tools live in `../_render/Tools/Medieval15/`; new source exports
are in `../_render/SourceAssets/Medieval15/`. Record cook, export, normal bake,
render, HDA portability and Unreal checks independently.
