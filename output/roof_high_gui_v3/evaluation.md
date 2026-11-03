# Medieval roof / HIGH / GUI — đánh giá 2026-11-03

15 biến thể gốc + baseline đã cook, xuất riêng LOW/HIGH và chụp HIGH trực tiếp từ GUI Houdini 22.0.368 Commercial. HDA 4.1; không ghi đè nguồn authoring.

**16 hero + 24 ảnh bổ sung**, mỗi ảnh có bản toàn cửa sổ và crop viewport. `review.html` là trang xem cục bộ. `results/gui_capture_all.json` / `gui_capture_extra.json` chứa provenance và hash ảnh.

| ID | LOW faces | HIGH faces | Ratio | Roof / canopy / finite UV,N / exact face QA |
|---|---:|---:|---:|---|
| baseline | 15,224 | 125,720 | 8.26 | pass |
| 01_food_shop | 9,601 | 91,229 | 9.50 | pass |
| 02_general_store | 11,156 | 115,968 | 10.40 | pass |
| 03_herbal_store | 10,206 | 92,110 | 9.03 | pass |
| 04_merchant_house | 14,458 | 129,530 | 8.96 | pass |
| 05_mixed_shop | 16,537 | 160,749 | 9.72 | pass |
| 06_tavern | 21,161 | 165,625 | 7.83 | pass |
| 07_weapon_guild | 23,957 | 183,325 | 7.65 | pass |
| 08_normal_house_b | 15,224 | 125,720 | 8.26 | pass |
| 09_normal_house_c | 16,336 | 146,884 | 8.99 | pass |
| 10_normal_house_c_canopy | 16,902 | 155,086 | 9.18 | pass |
| 11_artisan_house | 16,709 | 133,321 | 7.98 | pass |
| 12_craft_guild_hall | 21,272 | 202,308 | 9.51 | pass |
| 13_material_storage | 8,610 | 102,062 | 11.85 | pass |
| 14_toolmaker_workshop | 16,603 | 131,419 | 7.92 | pass |
| 15_forgehouse | 22,157 | 190,357 | 8.59 | pass |

## Nguyên nhân và kết quả

Mái chính/cánh trước đây dùng chung Skin cho nhiều role, tạo mặt nối không thuộc mái nào. Bản mới tách profile theo role, dựng closed volume, Boolean union, lấy miền cao nhất khớp mặt mái nguồn rồi tái dựng shell thickness. Các vùng ngói/trim bị chôn được loại theo miền mái này. Sai số bề mặt nguồn tối đa được lưu từng record, ngưỡng 0.1 mm.

Dormer trước đây đặt theo cao độ giữa khối và cutter dạng hộp cao vượt mái dormer. Bản mới đặt chân trước lún 8 cm vào mặt mái, giải chiều sâu tới mái chủ, kiểm tra ridge clearance, dùng cutter có tiết diện khớp mái và thêm ngói. Không nới cutter tới đỉnh mái chính.

Canopy/lean-to lấy datum tường, Boolean cắt phần chôn. Ngói rải từ quad nguồn trước Boolean; polygon ngon sau Boolean không còn làm mất toàn bộ điểm rải. Mái vải vẫn dùng hình học vải và các thanh đỡ.

HIGH là mesh khác LOW. PolyBevel chỉ nhận các thanh/ngói có góc vuông, feature size được giới hạn theo cạnh ngắn. Các mặt Boolean hoặc chi tiết mỏng được bảo toàn. Cả LOW/HIGH có vị trí, UV, normal hữu hạn và không còn mặt suy biến/trùng theo mask hình học 64-bit. Bounds LOW/HIGH giữ nguyên trong sai số 1 mm.

## Kiểm tra thực hiện

- Graph: 22 module / 7 stage thật; Python SOP = 0; connector/ownership audit pass.
- 17 hồi quy: cửa/host/cutter/frame/dressing, UV tiling, determinism, width/wing recook, invalid inputs, safe rerun, min/max và hipped/conical/hybrid grammar.
- Fresh process chỉ cài HDA đã di chuyển: LOW/HIGH khác nhau, width thay đổi geometry, không nhập script authoring hoặc file mesh ngoài.
- Awning lấy mép balcony thực qua connector overhead_access; vươn thêm ít nhất 45 cm. Kiểm tra cả 8 cấu hình bật awning và thay balcony_depth_m thêm 35 cm: mái vải và cột/tay đỡ dịch đúng 35 cm. Xem results/canopy_clearance.json.
- 29 file nguồn giữ nguyên hash; 15 dataset + baseline giữ nguyên byte. Các đường dẫn output gốc được phục vụ bằng alias tới geometry HIGH và crop screenshot GUI.
- Xem `results/variants.json`, `regressions.json`, `portability.json`, `static_checks.json`, `visual_review.json` và `graph_audit.json`.

## Phạm vi và giới hạn

Đã xem toàn bộ hero, các góc cận trước/sau mái 6, 7, 8, 11, canopy vải và wire LOW/HIGH. Đây là đánh giá các vùng nhìn thấy, không chứng minh mọi tổ hợp parameter hoặc mọi bề mặt khuất. HIGH chưa có sculpt gỗ/đá, texture final hoặc normal bake mới. Bespoke props/emblems/organic dressing vẫn theo mức đơn giản hóa của nguồn.

Không chạy bake/reimport Unreal, collision/LOD hoặc kiểm tra đi lại nội thất. T/courtyard và đồ thị mái tùy ý tiếp tục bị từ chối. Không coi các trường expected_features của dataset là bằng chứng tính năng đã bake.

Ở common hero mẫu 10, awning vải chưa đọc rõ. Kiểm tra projection và support recook là số liệu geometry; không coi đó là chứng nhận visual fidelity hoàn chỉnh cho canopy vải.

Repository path audit hiện báo 47 resource thiếu trong các manifest review cũ đã retired; không sửa lịch sử đó. Static package, cook và HDA portability được báo riêng, đều pass.

Preview được chụp bằng QScreen.grabWindow trên native HWND của chính cửa sổ Houdini đang chạy. Crop chỉ dùng pixel từ cùng screenshot; không chụp desktop hoặc ứng dụng khác, không ROP render, không imagegen. HUD là widget trong GUI trước lúc chụp.