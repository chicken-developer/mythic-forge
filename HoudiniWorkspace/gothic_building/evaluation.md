# Trạng thái bàn giao 2.1.0

32 cấu hình: 10 dataset + baseline + 21 sweep. Tất cả được dựng từ cùng geometry core, xuất OBJ, đọc lại và render offline. Các kiểm tra cùng config/seed, tọa độ hữu hạn, mặt suy biến, mặt trùng và exterior-envelope đều đạt trong bộ cấu hình này.

| Record | Primitives | Geometry/render |
|---|---:|---|
| baseline | 21233 | OBJ / offline |
| variant_01 | 5118 | OBJ / offline |
| variant_02 | 14559 | OBJ / offline |
| variant_03 | 10421 | OBJ / offline |
| variant_04 | 25092 | OBJ / offline |
| variant_05 | 5661 | OBJ / offline |
| variant_06 | 42901 | OBJ / offline |
| variant_07 | 4986 | OBJ / offline |
| variant_08 | 22187 | OBJ / offline |
| variant_09 | 5673 | OBJ / offline |
| variant_10 | 53217 | OBJ / offline |
| massing_1_1 | 8709 | OBJ / offline |
| massing_1_2 | 10309 | OBJ / offline |
| massing_1_3 | 11908 | OBJ / offline |
| massing_2_1 | 6534 | OBJ / offline |
| massing_2_2 | 10309 | OBJ / offline |
| massing_2_3 | 10309 | OBJ / offline |
| massing_3_1 | 10309 | OBJ / offline |
| massing_3_2 | 11526 | OBJ / offline |
| massing_3_3 | 18037 | OBJ / offline |
| roof_1_1 | 10309 | OBJ / offline |
| roof_1_2 | 10309 | OBJ / offline |
| roof_1_3 | 10309 | OBJ / offline |
| roof_2_1 | 10309 | OBJ / offline |
| roof_2_2 | 12191 | OBJ / offline |
| roof_2_3 | 14605 | OBJ / offline |
| roof_3_1 | 2836 | OBJ / offline |
| roof_3_2 | 3554 | OBJ / offline |
| roof_3_3 | 4986 | OBJ / offline |
| roof_4_1 | 3554 | OBJ / offline |
| roof_4_2 | 3554 | OBJ / offline |
| roof_4_3 | 3554 | OBJ / offline |

Mái chóp: sáu kiểm tra vỏ kín trên đường tựa và envelope — `results/spire_validation.json`. Ba công trình được yêu cầu có ảnh riêng và `details/spire_overview.png`.
Kiểm tra control: numeric min/max, cấu hình không hợp lệ, nhận dataset 1.2 và tính độc lập của 21 sweep — `results/control_validation.json`. Những phép kiểm này không xác nhận mọi tổ hợp parameter tùy ý hoặc tính đúng kết cấu xây dựng.

Hero toàn batch có cùng camera. Năm góc nhìn gần được render từ OBJ đã xuất, có hash nguồn trong `details/render_manifest.json`; chúng đóng khung riêng. Các image trong gói là geometry thật, không phải AI concept.

**Stage:** code_generated=true; static_checked=true; geometry_exported=true; rendered=true; houdini_cooked=not_run; hda_packaged=not_run. Target runtime chưa được kiểm chứng vì không có Houdini/hython ở môi trường bàn giao.

UV planar và normals được tạo khi `fill_hou` ghi Python SOP; OBJ offline không mang đầy đủ dữ liệu UV/material Houdini. Các màu preview là material-family palette, chưa phải texture PBR cuối.

Phạm vi: ngoại thất và nội thất cơ bản. Không có sculpt tượng/hoa văn độc bản, cầu thang đầy đủ, tính tải trọng hoặc topology destruction-ready. Dataset 10 là diễn giải reference fantasy bằng các module hiện có, không phải bản sao toàn bộ công trình mở trong ảnh.

Generator SHA256: cb20149f7ee8c761a6d821b7b879d871a106042aed8584bdd88087584db34de3
