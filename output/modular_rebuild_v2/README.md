# Medieval modular rebuild v2

Open [evaluation.md](evaluation.md) for measured results, source inventory,
limitations, every variant mapping and reproduction commands.

- Scene: [medieval_building_modular_v2.hip](scenes/medieval_building_modular_v2.hip).
- HDA: [medieval_building_modular.hda](asset/medieval_building_modular.hda),
  `mythic::medieval_house::4.0`, Houdini 22.0.368 Commercial.
- [All 16 hero images](renders/after_contact_sheet.png), including separate baseline.
- [Real graph audit](graph_audit.json), [module contracts](module_contracts.md),
  [variant mapping](variant_mapping.json), [final summary](results/final_summary.json).

The package preserves the original 15 variant JSON files, baseline, seeds and
reference mapping. Original authoring sources are unchanged. Install the HDA
from this package before opening a HIP after moving the package.

From the repository root in PowerShell:

```powershell
$hython = 'E:/SideFX/Houdini 22.0.368/bin/hython.exe'
& $hython output/modular_rebuild_v2/build_hda.py --rebuild --package
& $hython output/modular_rebuild_v2/evaluate_hda.py --render
& $hython output/modular_rebuild_v2/check_regressions.py
& $hython output/modular_rebuild_v2/check_portable.py
& $hython output/modular_rebuild_v2/static_checks.py --require-images
```

The existing native roof/body-trim grammar is retained. Arbitrary valley graphs
are not implemented. Normal maps require a new bake for the corrected UVs;
Unreal has not been modified.
