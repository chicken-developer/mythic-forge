# Medieval modular rebuild v2 — đánh giá 2026-11-03

**15 biến thể + baseline: cook 16/16, geometry gate 16/16, hero render 16/16.** Houdini 22.0.368, Commercial. HDA `mythic::medieval_house::4.0`.

Giữ nguyên byte của 15 dataset và baseline; giữ nguyên seed, reference mapping, expected features và public parameter names. Không cần migration dataset; mapping node paths nằm trong `results/migration.json`. Hash của 29 file nguồn được kiểm tra, số file gốc đổi: 0.

## Inventory và phạm vi

| Family | Nguồn thật | Xử lý |
| --- | --- | --- |
| Medieval current | Authoring HIP + HDA 3.0; 15 JSON + baseline | refactor_existing → HDA 4.0 |
| Gothic current | HIP có thật; không có dataset/builder/HDA ở authoring folder hiện tại | Inventory; ngoài HDA target đã chỉ định |
| Legacy Medieval review | HDA/Args ở `_render/SourceAssets/Houdini/Medieval`; 25 argument JSON | Retired review; repository cấm khôi phục full-Python generator này |
| House/window/stair reference kits | HIPLC/HDALC trong hda_samples | Reference, không tính là dataset target |

Không tìm thấy family có tên file Gothic_Building_Final, Golem_Hearth_Forge hay Storybook. Danh sách filesystem/HDA definitions thật: `results/workspace_inventory.json`.

## Kiến trúc và lỗi đã sửa

- Root từ 18 F_* function và các caller rời thành 7 stage thật, 19 module có contracts, debug outputs và connectors. Các function native/typed local args được giữ qua mapping đường dẫn. Xem `module_contracts.md`.
- Thay 5 Object Merge nội bộ bằng helper connectors; chỉ còn optional external blockout adapter đã khai báo. Python SOP: 0.
- Stage sub-HDA có input/output count thật, HDA chính mang đủ nested definitions trong một file. Fresh process di chuyển HDA và đổi width đã qua.
- Producer module_id/part_role được gắn ở stage ports; wall/opening/roof IDs ổn định, cửa attic có host ID.
- Thêm optional window_move_id/window_offset_u_m và UV controls; dataset gốc không phải sửa.
- Cleanup dùng VEX mask 64-bit + native Blast với group type primitive tường minh, giữ bề mặt hợp lệ và không weld các part.
- Tổng mặt trùng đo trước/sau: 633 → 0; mặt suy biến: 41 → 0.
- UV trên mặt nghiêng dùng basis local trực chuẩn theo mét; các mặt axis-aligned giữ orientation cũ. UV tiling recook không đổi topology.

## Gate và bằng chứng

| Gate | Status | Evidence |
| --- | --- | --- |
| A syntax/dataset/module DAG + image signatures | pass | results/static_checks.json; 3 batch × 5 records |
| B actual graph/connectors/ownership/Python SOP | pass | graph_audit.json |
| C baseline + all variants | 16/16 pass | results/after/*.json, geometry/after/*.bgeo.sc |
| D interaction/determinism/invalid/ranges | pass, 17 tests | results/regressions.json |
| E relocated HDA, clean process | pass | results/portability.json |
| F real Houdini hero images | 16/16 | renders/after/, comparisons/, results/visual_review.json |
| UV checker/density | measured + rendered | renders/checker/, results/uv_density.json |
| Unreal editor bake | not_run theo yêu cầu | Chưa sửa/bake Unreal |

Repository path audit: **failed**, 47 thiếu resource ở các review manifest cũ trong Assets/_render. Các file này ngoài HDA target, có sẵn trước lần refactor; giữ nguyên thay vì sửa chứng cứ cũ. Báo cáo mới: `results/repo_path_audit.json`. Static checks riêng của package đã qua.

Refactor thuần trước sửa geometry có 16/16 hash parity vị trí/topology/name/UV/material (`results/pure_refactor/records`). Sau sửa, raw topology/UV hash đổi có chủ đích. Tập bề mặt hợp lệ và kích thước phải giữ nguyên; final `geometry_acceptance` là gate dùng để đếm pass.

Cook time đo trong process này: min 0.771s, median 1.109s, max 2.988s. Không coi đây là benchmark chung trên máy khác.

## Giới hạn còn lại

- Giữ roof/body-trim grammar native của nguồn cho các preset hiện có; chưa dựng solver ridge/valley tổng quát cho arbitrary roof graphs. T/courtyard/custom semantic topology bị từ chối rõ ràng. Stress tests legacy grammar chứng minh cook, không chứng minh fidelity của mọi tổ hợp mái.
- Kiểm tra automated gồm finite geometry/normals/UV, exact duplicate/degenerate faces, aperture rays, host IDs, bounds, surface preservation và recook. Giao cắt tùy ý, watertight roof seams và support/load không có chứng minh tổng quát; hero view QA không thay thế kiểm tra kết cấu mọi góc.
- Baseline và 15 mẫu giữ artistic identity của geometry nguồn; bespoke emblems, vegetation, fire, forge bell, hàng hóa và props đặc thù vẫn giản lược như nguồn.
- Các expected_features cũ được giữ byte-identical, kể cả normal_bake_from_high. Lần này không bake lại normal maps; cần bake mới theo UV đã sửa trước handoff Unreal. Collision/LOD/Nanite và walkability nội thất chưa nghiệm thu.
- Không chụp Houdini network UI trong headless run. Diagram là contract diagram; ảnh hero/checker là OpenGL ROP từ geometry cook thật.
- OpenGL/Vulkan crash khi đổi nhiều model trong một process; runner dùng mỗi dataset một hython riêng. Logs lỗi/probes được giữ trong results để audit.

## Mapping từng variant

Tất cả dùng HDA `asset/medieval_building_modular.hda`, type `mythic::medieval_house::4.0`. Manifest chi tiết: `variant_mapping.json`.

| variant_id | Dataset / seed decoration | Cook / geometry | Hero / comparison |
| --- | --- | --- | --- |
| 01_food_shop | [JSON](datasets/01_food_shop.json) / 17 | pass / pass; [HIP](scenes/variants/01_food_shop.hip), [BGEO](geometry/01_food_shop.bgeo.sc) | [hero](previews/01_food_shop_hero.png), [comparison](comparisons/01_food_shop.png) |
| 02_general_store | [JSON](datasets/02_general_store.json) / 17 | pass / pass; [HIP](scenes/variants/02_general_store.hip), [BGEO](geometry/02_general_store.bgeo.sc) | [hero](previews/02_general_store_hero.png), [comparison](comparisons/02_general_store.png) |
| 03_herbal_store | [JSON](datasets/03_herbal_store.json) / 17 | pass / pass; [HIP](scenes/variants/03_herbal_store.hip), [BGEO](geometry/03_herbal_store.bgeo.sc) | [hero](previews/03_herbal_store_hero.png), [comparison](comparisons/03_herbal_store.png) |
| 04_merchant_house | [JSON](datasets/04_merchant_house.json) / 17 | pass / pass; [HIP](scenes/variants/04_merchant_house.hip), [BGEO](geometry/04_merchant_house.bgeo.sc) | [hero](previews/04_merchant_house_hero.png), [comparison](comparisons/04_merchant_house.png) |
| 05_mixed_shop | [JSON](datasets/05_mixed_shop.json) / 17 | pass / pass; [HIP](scenes/variants/05_mixed_shop.hip), [BGEO](geometry/05_mixed_shop.bgeo.sc) | [hero](previews/05_mixed_shop_hero.png), [comparison](comparisons/05_mixed_shop.png) |
| 06_tavern | [JSON](datasets/06_tavern.json) / 17 | pass / pass; [HIP](scenes/variants/06_tavern.hip), [BGEO](geometry/06_tavern.bgeo.sc) | [hero](previews/06_tavern_hero.png), [comparison](comparisons/06_tavern.png) |
| 07_weapon_guild | [JSON](datasets/07_weapon_guild.json) / 17 | pass / pass; [HIP](scenes/variants/07_weapon_guild.hip), [BGEO](geometry/07_weapon_guild.bgeo.sc) | [hero](previews/07_weapon_guild_hero.png), [comparison](comparisons/07_weapon_guild.png) |
| 08_normal_house_b | [JSON](datasets/08_normal_house_b.json) / 17 | pass / pass; [HIP](scenes/variants/08_normal_house_b.hip), [BGEO](geometry/08_normal_house_b.bgeo.sc) | [hero](previews/08_normal_house_b_hero.png), [comparison](comparisons/08_normal_house_b.png) |
| 09_normal_house_c | [JSON](datasets/09_normal_house_c.json) / 17 | pass / pass; [HIP](scenes/variants/09_normal_house_c.hip), [BGEO](geometry/09_normal_house_c.bgeo.sc) | [hero](previews/09_normal_house_c_hero.png), [comparison](comparisons/09_normal_house_c.png) |
| 10_normal_house_c_canopy | [JSON](datasets/10_normal_house_c_canopy.json) / 17 | pass / pass; [HIP](scenes/variants/10_normal_house_c_canopy.hip), [BGEO](geometry/10_normal_house_c_canopy.bgeo.sc) | [hero](previews/10_normal_house_c_canopy_hero.png), [comparison](comparisons/10_normal_house_c_canopy.png) |
| 11_artisan_house | [JSON](datasets/11_artisan_house.json) / 17 | pass / pass; [HIP](scenes/variants/11_artisan_house.hip), [BGEO](geometry/11_artisan_house.bgeo.sc) | [hero](previews/11_artisan_house_hero.png), [comparison](comparisons/11_artisan_house.png) |
| 12_craft_guild_hall | [JSON](datasets/12_craft_guild_hall.json) / 17 | pass / pass; [HIP](scenes/variants/12_craft_guild_hall.hip), [BGEO](geometry/12_craft_guild_hall.bgeo.sc) | [hero](previews/12_craft_guild_hall_hero.png), [comparison](comparisons/12_craft_guild_hall.png) |
| 13_material_storage | [JSON](datasets/13_material_storage.json) / 17 | pass / pass; [HIP](scenes/variants/13_material_storage.hip), [BGEO](geometry/13_material_storage.bgeo.sc) | [hero](previews/13_material_storage_hero.png), [comparison](comparisons/13_material_storage.png) |
| 14_toolmaker_workshop | [JSON](datasets/14_toolmaker_workshop.json) / 17 | pass / pass; [HIP](scenes/variants/14_toolmaker_workshop.hip), [BGEO](geometry/14_toolmaker_workshop.bgeo.sc) | [hero](previews/14_toolmaker_workshop_hero.png), [comparison](comparisons/14_toolmaker_workshop.png) |
| 15_forgehouse | [JSON](datasets/15_forgehouse.json) / 17 | pass / pass; [HIP](scenes/variants/15_forgehouse.hip), [BGEO](geometry/15_forgehouse.bgeo.sc) | [hero](previews/15_forgehouse_hero.png), [comparison](comparisons/15_forgehouse.png) |

## Lệnh tái tạo

Từ repository root trong PowerShell:

```powershell
$hython = 'E:/SideFX/Houdini 22.0.368/bin/hython.exe'
& $hython output/modular_rebuild_v2/build_hda.py --rebuild --package
& $hython output/modular_rebuild_v2/evaluate_hda.py --render
& $hython output/modular_rebuild_v2/check_regressions.py
& $hython output/modular_rebuild_v2/check_portable.py
& $hython output/modular_rebuild_v2/audit_uv.py
& $hython output/modular_rebuild_v2/static_checks.py --require-images
```

Python Shell:

```python
import runpy
api = runpy.run_path(r"E:/Repos/mythic-forge/output/modular_rebuild_v2/build_hda.py")
building = api["build"]()
```

Deps: Houdini/hou 22.0.368; NumPy cho QA; Pillow cho compositing, có trong runtime đã dùng. Builder không pip-install, không Python SOP geometry. Cần mang toàn bộ package khi rebuild từ snapshot; HDA riêng đã kiểm tra di chuyển. Khi di chuyển HIP, install HDA trong package trước khi mở scene.

API/connector capabilities đã probe trên Houdini 22.0.368; online docs là tham chiếu, không thay kết quả runtime: [OpNode](https://www.sidefx.com/docs/houdini/hom/hou/OpNode.html), [HDADefinition](https://www.sidefx.com/docs/houdini/hom/hou/HDADefinition.html).