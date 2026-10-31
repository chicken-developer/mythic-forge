# Storybook v5.1 — Interior update

Nâng cấp từ v5 đã được khôi phục, không dùng bevel/smooth v6. Đây là geometry procedural cho ngoại thất + nội thất cơ bản; nội thất được thiết kế bổ sung, không suy là nội thất có thật trong ảnh reference ngoại thất.

## Đã có
- 24 cấu hình / 6 building, mỗi family base, small, compact, large; tất cả đã xuất OBJ/MTL mới.
- Sàn ván có độ dày, dầm sàn và cột; sàn trên mở lỗ thực cho cầu thang.
- Khối chữ nhật dùng cầu thang U hai vế, chiếu nghỉ, stringer, tay vịn; khối tròn dùng cầu thang xoắn và sàn vành khuyên.
- Lan can quanh khoảng mở; khung mái chữ nhật (tie/rafter/kingpost), rafter mái nón.
- Vách phòng trên tầng với cửa mở; giường, chăn/gối, bàn, ghế băng, kệ và bình. Preset inn thêm quầy; mill thêm cối đá trong khối chữ nhật; round dùng workbench/kệ quanh lõi thang.
- Sáu parameters mới: interior_enable, interior_furniture, interior_partitions, interior_joist_spacing_m, interior_stair_width_m, interior_style. Schema là nguồn UI dùng chung trong builder.
- Giữ nguyên kiểu topology bevel nhẹ sẵn có của v5. Không áp dụng multi-segment bevel v6.

## Preview
Ba ảnh của Townhouse base, mỗi ảnh 1920×1200: ground (tầng trệt cắt lớp), cutaway (ba tầng), room (camera trong nhà). Tường và mái bị ẩn/cắt CHỈ trong view chẩn đoán; OBJ chính giữ công trình đầy đủ. Preview là render CPU từ chính OBJ mới, không phải ảnh AI hoặc render native Houdini. Góc room có near-plane clipping và ambient fill ấm để nhìn rõ; renderer không có GI đầy đủ. JSON từng ảnh chứa camera, renderer/source hash và thời gian đo.

Lệnh render lại một ảnh, từ thư mục package:

```bash
OPENBLAS_NUM_THREADS=1 python render_interior.py room 960
```

Trên Windows có thể bỏ tiền tố OPENBLAS_NUM_THREADS=1. Python ngoài Houdini cần numpy, scipy, Pillow. Chạy `python update_package.py --sample` để dựng lại một mẫu; `python update_package.py` dựng lại 24 OBJ, không tự render. `python test_interior.py` chạy các kiểm tra geometry chính.

## Houdini
`build_hda.py` có pure Python core và cài Python SOP/UI. Đã compile source/core, chưa có Houdini/hython ở môi trường này để cook hoặc xuất asset native. Không có file HIP/HDA đã được kiểm thử trong gói này.

Trong Houdini Python Shell (đổi đường dẫn theo máy):

```python
import runpy, json
builder = runpy.run_path(r"D:/Storybook_Interior_v5_1/build_hda.py")
config = json.load(open(r"D:/Storybook_Interior_v5_1/assets/datasets/01_townhouse_base.json"))
node = builder['build'](config, asset_name='storybook_interior_v51')
```

Các tham số mới được nhúng cùng generator khi dùng export_asset. Chưa kiểm thử cook/portability của asset export; không coi static check là chứng nhận Houdini.

## Giới hạn
- Đây là nội thất cơ bản phục vụ layout và preview; chưa phải kit nội thất sculpt/production đầy đủ.
- Chưa bake collision, NavMesh, lightmap UV hoặc chạy kiểm tra nhân vật đi xuyên toàn bộ các tầng. Chưa chứng nhận headroom/circulation cho mọi tổ hợp, đặc biệt tháp nhỏ, khối giao nhau và preset nhỏ.
- Cửa ngoại thất hiện là mesh đóng, chưa có bản lề/logic mở cửa hoặc collision gameplay.
- Thang xoắn tháp nhỏ là giải pháp kiến trúc gọn; cần kiểm tra với capsule nhân vật mục tiêu. Các giằng/khớp gỗ giao nhau có chủ đích, không phải kết cấu kỹ thuật được tính tải.
- Phòng chia chủ yếu trong khối chữ nhật tầng trên. Round dùng không gian mở; không dựng phòng ngủ trong tháp phụ nhỏ. Không có cầu thang lên attic.
- Các khoảng trống mái dùng module rafters cơ bản; cupola không có đường tiếp cận riêng.
- Không render lại 24 ngoại thất trong lần cập nhật này; chỉ render 3 view nội thất của mẫu đại diện theo render-test.
