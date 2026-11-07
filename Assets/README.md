# Assets

| Directory | Purpose |
| --- | --- |
| `hda_gothic_building/` | Gothic Houdini source, datasets, scenes and HDA |
| `hda_medieval_building/` | Medieval Houdini source; read [AGENTS.md](hda_medieval_building/AGENTS.md) |
| `hda_samples/` | Reference HDA and HIP files |
| `skills/` | Original skill ZIP packages |
| `texture_gothic/` | Substance graphs, texture exports and material manifest |
| `3d_central_fountain/` | Fountain reference assets |
| `_render/` | [TDKProps Unreal project](_render/TDKProps.uproject), source exports, tools and review results |
| `_composition/` | [TDKKingdomMap Unreal project](_composition/TDKKingdomMap.uproject) |
| `tools/`, `hda/`, `presets/`, `specs/`, `hip/`, `renders/`, `docs/` | hbuild building generator (medieval + gothic, one HDA); start at [docs/building_generator.md](docs/building_generator.md) |

Current Medieval source is the editable
[HIP](hda_medieval_building/medieval_building.hip), with its native graph updater
in [update_model.py](hda_medieval_building/scripts/update_model.py) and the public
config/HDA API in [build_hda.py](hda_medieval_building/build_hda.py). Read the
[current review guide](hda_medieval_building/README_VI.md) before editing it.
The earlier function-refactor source directory was absent when the current
layout review began; preserve the 18 native function subnets in the HIP.
The new 15-house dataset and bake outputs use `Medieval15` in `_render`.

Filesystem folders changed on 2026-11-02. Unreal project/module names and
`/Game/...` package paths remain the same. See [project memory](../MEMORY.md)
for the old-to-new mapping and audit command.
