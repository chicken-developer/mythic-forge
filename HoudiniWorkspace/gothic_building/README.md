# Gothic Building — Native SOP Graph 2.2

Generator procedural cho nhà nguyện, nhà thờ, townhouse, cổng thành và tháp. Bộ dữ liệu giữ nguyên geometry tham chiếu 2.1: 1 baseline, 10 preset và 21 cấu hình khảo sát.

## Scene và HDA chính

Mở **gothic_building.hip** ở gốc project. Asset nằm tại **asset/gothic_building.hda**; scene nhúng full definition của **quynh::gothic_building::2.2**, không phụ thuộc đường dẫn Downloads cũ.

Chọn **/obj/gothic_building_v2_1/generator** rồi đổi **Building Preset**. Preset và các tham số của scene được giữ khi sửa hoặc đóng gói lại. Các báo cáo hiện hành nằm trong **reports/**; dữ liệu chạy tạm và backup nằm trong **houdini_output/**, được bỏ qua bởi Git.

## Debug bằng node

Graph đầu ra không dùng Python SOP tạo mesh. Dive vào generator để xem:

~~~text
construction/generated
  nave, aisle, transept, tower...       module riêng theo cấu hình
    PROFILES                          profile phẳng được nhúng trong Stash
    edit_profiles                     chỉnh profile thủ công
    solid_profiles -> PolyExtrude     dựng tường, mái, trụ và chi tiết
    surface_patches                   các mặt mái/chóp đặc biệt
    CUT_*                             Clip SOP theo từng mặt phẳng giao khối
    MERGE_MODULES
MANUAL_EDIT
remove_duplicate_faces
remove_exact_duplicate_surfaces
color_stone, color_trim, color_roof...
stone_variation_and_gold
planar_uvs
outward_vertex_normals
global_scale
boundary_precision -> final_boundary_alignment
OUT_GOTHIC_BUILDING
~~~

Tắt **Auto Rebuild Generated Modules** trước khi sửa các node trong **construction/generated**. Bật display flag ở PROFILES, PolyExtrude hoặc từng nhánh CUT để kiểm tra profile, khối và mặt cắt riêng. HDA cho phép chỉnh nội dung construction và MANUAL_EDIT.

Nút **Rebuild Generated Modules** cập nhật nhánh generated. Đặt các node sửa riêng trong construction nhưng ngoài generated, rồi nối sau generated. Nhánh, dây nối và display flag của chúng được giữ khi rebuild. MANUAL_EDIT cũng được giữ; cần kiểm tra lại selection điểm nếu thay đổi topology.

Python còn dùng để tính profile Gothic, chọn bố cục và cập nhật graph khi đổi tham số hình học. Việc extrude, cắt giao, merge, loại mặt trùng, màu, UV, normal và scale được thực hiện bằng SOP/VEX có thể kiểm tra trực tiếp. Code tạo profile và callback được nhúng trong HDA; không cần import project khi mở asset ở nơi khác.

Global Scale, Stone Palette, Roof Material và Gold Accents là các điều khiển trực tiếp của SOP, không cần tạo lại các profile.

## Dựng và kiểm chứng trước khi commit

Chạy từ thư mục project bằng hython của Houdini 22.0.368:

~~~text
hython validate_native.py --all
hython deploy_houdini.py --prepare
hython verify_houdini.py
~~~

validate_native so sánh kích thước, diện tích bề mặt, normal và các giao khối với mesh tham chiếu, đồng thời xác nhận không có Python SOP ở graph đầu ra. Số point có thể giảm nhờ chia sẻ điểm giữa các mặt; không dùng số point cũ làm tiêu chí tương đương geometry. Ở giao khối, Clip SOP có thể giữ lại các mặt coplanar ở biên khác với solver tham chiếu; kiểm tra cho phép chênh lệch tổng diện tích tối đa 0,1%, đồng thời giữ kiểm tra kích thước, normal và mặt lọt khối.

deploy_houdini sao lưu scene hiện tại, giữ preset/tham số, xuất HDA đầy đủ rồi chỉ ghi lại gothic_building.hip sau khi graph mới vượt kiểm tra. File scene chính luôn ở gốc project.

verify_houdini kiểm tra instance HDA mới, callback preset, khả năng tắt auto rebuild để debug manual, giữ nhánh manual, các điều khiển scale/màu và mở lại scene đã copy sang thư mục khác sau khi gỡ thư viện HDA ngoài khỏi tiến trình kiểm tra.

Các file chính để review: gothic_building.hip, asset/gothic_building.hda, native_network.py, deploy_houdini.py, validate_native.py, verify_houdini.py và reports/. Không đưa backup/cache trong houdini_output/ vào commit.

## Bộ mesh tham chiếu và renderer offline

build_hda.py giữ bộ sinh mesh tham chiếu 2.1 và exporter tương thích cũ để đối chiếu. Scene và HDA chính sử dụng native_network.py. evaluate_hda.py, geometry/, previews/, comparisons/ và các kết quả gốc trong results/ phản ánh bộ geometry tham chiếu, không thay thế báo cáo native hiện hành.

## Các control tạo khác biệt kiến trúc

| Control | Giá trị | Tác động geometry |
|---|---|---|
| `facade_style` | `lancet`, `portal_gallery`, `tracery_front` | Mặt tiền đơn; nhóm ba cửa kính và dải vòm; hoặc cửa kính lớn phía trên một/ba portal, archivolt nhiều lớp và trụ ghép |
| `window_pattern` | `single`, `paired`, `rhythmic` | Nhịp cửa bên một lỗ, hai lỗ hoặc nhịp A–B–A; chia theo bề rộng facade còn lại sau khi xác định interface |
| `tower_crown` | `pyramid`, `canopied`, `openwork` | Chóp đặc; chóp có đầu hồi/canopy và crockets; hoặc chóp mái kín phân tầng với đầu hồi nhỏ và đường ghép mái |
| `gate_composition` | `fortified`, `urban_asymmetric` | Cổng có cánh/turret; hoặc vòm xuyên dưới tầng đồng hồ, tháp tròn lớn một bên và turret nhỏ bên kia |
| `porch_depth` | 0–2.4 m | Mái hiên lối vào có cột đỡ và vòm, thông thoáng hai bên; 0 tắt |
| `roof_material` | `slate`, `copper`, `oxblood` | Nhóm màu mái đá phiến, đồng xanh hoặc đỏ sẫm; không phải texture PBR hoàn chỉnh |
| `ridge_detail` | `sparse`, `crest` | Điểm nhấn thưa hoặc hàng crest dọc nóc |

`facade_style` phân tầng mặt tiền và porch áp dụng cho church khi `wall_height >= 6 m`; nhà thấp dùng mặt tiền gọn. Phong cách này cũng điều khiển tầng chuông, trụ và tracery ở các module có liên quan. `window_pattern` chia nhịp ở mặt bên church; nó không biến mọi cửa của mọi loại nhà thành cửa đôi. `gate_composition=urban_asymmetric` chỉ hợp lệ cho gatehouse. Chóp openwork ở church phải bắt đầu cao hơn mái tiếp giáp 0.4 m; nếu không đủ, tăng tầng/chiều cao tháp hoặc giảm pitch. Bộ tham số kích thước, footprint, layout tháp và seed có trong `parameter_schema.json`.

## Dataset

| ID | Công trình |
|---|---|
| variant_01 | Village Chapel — giản lược |
| variant_02 | Town Church — mặt tiền gallery, mái đỏ |
| variant_03 | Gothic Townhouse — đầu hồi bậc, mái đồng |
| variant_04 | Twin Tower Church — mặt tiền tracery và chóp canopied |
| variant_05 | Clock Gate — tổ hợp đô thị bất đối xứng |
| variant_06 | Dark Cathedral — chữ thập, cửa theo nhịp và chóp mái kín |
| variant_07 | Civic Watchtower — nghiên cứu từ reference đô thị |
| variant_08 | Layered Twin Basilica — nghiên cứu từ reference nhà thờ đôi |
| variant_09 | Astronomical City Gate — nghiên cứu từ ảnh cổng đồng hồ |
| variant_10 | Ceremonial High Cathedral — diễn giải reference fantasy theo khả năng generator |

`manifest.json` liên kết dataset với input, OBJ, ảnh và kết quả. `previews/all_variants.png` là tổng quan; `details/` có góc nhìn gần cho bốn reference mới. Hero toàn batch dùng cùng camera/scale; ảnh detail đóng khung riêng và không dùng so kích thước.

## Batch

```text
hython evaluate_hda.py --render
python evaluate_hda.py --offline --render --render-workers 4
python validate_controls.py
```

Render offline cần NumPy/Pillow. Chế độ nhiều worker chỉ dùng với `--offline`; khi chạy hython dùng mặc định một worker. `--output-dir` đặt thư mục xuất; `--manifest` chọn manifest.

## Phạm vi bàn giao

Ảnh và OBJ được tạo từ geometry thật, không dùng AI concept làm bằng chứng. Đây là hệ ngoại thất và tổ chức nội thất cơ bản; chưa có tượng sculpt, hoa văn điêu khắc riêng, vật liệu PBR hoàn chỉnh, cầu thang/nội thất tháp đầy đủ hoặc kiểm chứng kết cấu xây dựng. Reference fantasy thứ tư có công trình mở và trang trí độc bản: dataset 10 kế thừa phân cấp khối và tính chất nghi lễ, không tái dựng toàn bộ bố cục đó.

Gói gốc được tạo và kiểm tra offline; các báo cáo cũ trong `results/` vẫn phản ánh lần chạy đó. Bản native hiện hành dùng `gothic_building.hip` và `asset/gothic_building.hda`; các báo cáo kiểm chứng nằm trong `reports/`.

Quy tắc chóp 2.1: `openwork` giữ nguyên ID để nhận preset cũ, nay chỉ mô tả chi tiết Gothic ở chân chóp; vỏ mái luôn kín. Vai mái rộng chuyển sang thân chóp hẹp và cao, gable nhỏ tựa trên vai mái. Tháp tròn dùng chân mái 12 cạnh; tháp vuông dùng chân mái bát giác trên cornice trong phạm vi tường. Không có giằng X xuyên suốt chiều cao chóp. Envelope mái tham gia cắt giao khối. Đây là diễn giải hình học từ reference, không phải hồ sơ tính kết cấu.
