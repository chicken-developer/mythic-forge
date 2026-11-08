# Assets

| Directory | Purpose |
| --- | --- |
| `hda_gothic_building/` | Original gothic building HIP (kept as reference, not edited) |
| `hda_medieval_building/` | Original medieval building HIP (kept as reference, not edited) |
| `img_refs/` | Concept refs (`medievals_refs`) and moodboards (`medievals_moodboard`, `gothics_moodboard`) |
| `3d_central_fountain/` | Fountain reference assets |
| `_render/` | [TDKProps Unreal project](_render/TDKProps.uproject); hbuild import tooling in `Tools/HBuild`, exports in `SourceAssets/HBuild`, checks in `RenderTests/HBuild`; level `/Game/Levels/NewMap` |
| `_composition/` | [TDKKingdomMap Unreal project](_composition/TDKKingdomMap.uproject) |
| `tools/`, `hda/`, `presets/`, `specs/`, `hip/`, `renders/`, `docs/` | hbuild building generator (medieval + gothic, one HDA); start at [docs/building_generator.md](docs/building_generator.md) |

`temp/` (job logs, scratch) is git-ignored and safe to delete.
