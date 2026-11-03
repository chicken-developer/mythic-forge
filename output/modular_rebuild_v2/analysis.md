# Phân tích refactor Medieval

Target: scene native `/obj/medieval_building`; giữ 18 function và input/output ordering. 15 preset có sẵn là nguồn cấu hình; baseline riêng. Đơn vị mét, Y-up. Kích thước là modeling targets từ nguồn, không phải đo đạc lại concept.

Giữ tỷ lệ main/wing, nhịp cửa và timber, mái cong/roof axis, palette, foundation/canopy/balcony và các module guild/forge/hoist. Bề mặt khuất/nội thất dùng grammar gốc. 15 reference được xem theo từng sheet trong ba contact sheet `references/review_group_*.png`.

DAG: layout → shell/visible bays → opening cuts; roof và details là các nhánh riêng; UV/material lắp đúng ordering cũ rồi output. Bay-window attachments ở OPENINGS vì chúng tham gia replacement và cutter streams.

| ID | Complexity từ dataset | Footprint | Floors | Roof | Reference |
| --- | --- | --- | --- | --- | --- |
| 01_food_shop | simple | rectangular | 1 | gable | ref_01_food_shop |
| 02_general_store | simple | rectangular | 1 | gable | ref_02_general_store |
| 03_herbal_store | simple | rectangular | 1 | gable | ref_03_herbal_store |
| 04_merchant_house | medium | rectangular | 2 | gable | ref_04_merchant_house |
| 05_mixed_shop | medium | rectangular | 2 | gable | ref_05_mixed_shop |
| 06_tavern | complex | l_shape | 2 | gable | ref_06_tavern |
| 07_weapon_guild | complex | l_shape | 2 | gable | ref_07_weapon_guild |
| 08_normal_house_b | medium | rectangular | 2 | gable | ref_08_normal_house_b |
| 09_normal_house_c | medium | rectangular | 2 | gable | ref_09_normal_house_c |
| 10_normal_house_c_canopy | medium | rectangular | 2 | gable | ref_10_normal_house_c_canopy |
| 11_artisan_house | complex | l_shape | 2 | gable | ref_11_artisan_house |
| 12_craft_guild_hall | complex | rectangular | 2 | gable | ref_12_craft_guild_hall |
| 13_material_storage | simple | rectangular | 1 | gable | ref_13_material_storage |
| 14_toolmaker_workshop | complex | l_shape | 2 | gable | ref_14_toolmaker_workshop |
| 15_forgehouse | complex | l_shape | 2 | gable | ref_15_forgehouse |

Feature mapping: wall/opening IDs và window offset → SHELL/facade_layout → OPENINGS/windows + wall_cuts → DETAILS/facade dressing. UV tiling/local slope projection → UV_MATERIAL/surfacing. Exact duplicate/degenerate masks → native primitive Blast ở OUTPUT.

Phần exterior của 15 config có budget đo được trong results; sweep range/grammar là stress tests ngoài tập input. T/courtyard và custom semantic roof graphs được reject trước khi ghi parameters. Các façade plaster gốc không có generator masonry/plank courses; không tạo một menu courses giả.