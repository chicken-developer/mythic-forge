# Medieval — sửa mái phụ, HIGH và screenshot Houdini GUI

Bản review riêng cho `Assets/hda_medieval_building/medieval_building.hip`.
Houdini **22.0.368 Commercial**, HDA `mythic::medieval_house::4.1`.
Giữ nguyên 15 dataset + baseline, parameter/seed/reference gốc và 18 function
native; không ghi đè asset authoring hoặc xuất sang Unreal.

Mở `scenes/medieval_building_roof_high_v3.hip`, chọn `BUILDING`. Điều khiển
**Preview Quality = HIGH/LOW** nằm đầu giao diện. Output SOP 0 là nhánh đang
chọn, output 1 là LOW, output 2 là HIGH. Các cảnh theo ID ở `scenes/` mở
sẵn HIGH. `geometry/*_low.bgeo.sc` và `*_high.bgeo.sc` là hai mesh riêng.

HIGH dùng PolyBevel native trên thanh/ngói/khối có góc vuông phù hợp; giới hạn
offset theo cạnh ngắn nhất. Không bevel bừa các polygon Boolean hoặc chi tiết
mỏng. Đây là high geometry với chi tiết kiến trúc và bevel, chưa phải sculpt
gỗ/đá hoặc vật liệu hand-painted hoàn chỉnh.

Các sửa mái:

- Skin riêng từng role mái chính/cánh; không nối chéo profile giữa hai khối.
- Boolean union các thể tích mái, lấy miền mái nhìn từ trên; giữ thickness,
  ID mái và bỏ các phần trim/ngói bị chôn trong mái chủ.
- Dormer đặt theo cao độ chân trước, tính chiều sâu về mái chủ, dùng cutter
  khớp tiết diện mái dormer; thêm ngói thực. Giới hạn không vừa được báo lỗi.
- Canopy/lean-to áp datum tường và cắt phần chôn; rải ngói trên quad nguồn
  trước khi Boolean làm mặt thành ngon. Canopy vải giữ mặt vải.
- Awning lấy độ vươn theo mép balcony thực, nhô thêm 45 cm; cột và tay đỡ
  thay đổi cùng mái khi chỉnh balcony depth.

`screenshots/final/*_gui.png` là **screenshot cửa sổ Houdini đang chạy**.
`*_gui_viewport.png` chỉ crop từ chính screenshot cửa sổ đó. HUD được đặt
trong GUI trước khi chụp. Không dùng OpenGL ROP, ảnh offline hoặc imagegen
cho bộ preview này. `results/gui_capture_*.json` ghi camera, scene, geometry
counts, window title, `hou.isUIAvailable()` và hash ảnh.
Runner reload definition khi hash HDA thay đổi và so hash toàn bộ vị trí point
trong GUI với mesh LOW/HIGH đã QA; số mặt giống nhau không đủ để chấp nhận
preview nếu HDA cũ vẫn còn trong cache GUI.

`previews/` chứa bản crop viewport theo đường dẫn output gốc của dataset;
`comparisons/` ghép reference với screenshot, có nhãn nguồn. Ảnh toàn cửa sổ
vẫn được giữ để đối chiếu nguồn gốc. `review.html` xem toàn bộ bộ ảnh và cận mái.

Chạy trong Python Shell Houdini:

```python
import sys
sys.path.insert(0, r"E:/Repos/mythic-forge/output/roof_high_gui_v3")
import build_hda
building = build_hda.build()
building.parm('preview_quality').set('high')
```

Đường dẫn trên là ví dụ vị trí hiện tại; thay bằng thư mục package sau khi
giải nén. Builder tự resolve tài nguyên theo `__file__`.

Build/QA bằng hython chỉ dùng để tạo graph, cook và xuất geometry:

```powershell
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' build_hda.py --rebuild --package
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' evaluate_roof_high.py
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' check_regressions.py
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' check_portable.py
```

Chụp lại bằng GUI: `launch_gui.ps1` mở một phiên review riêng, rồi chạy
`capture_gui_batch.py --mode all` bằng Python thường; `--mode extra --start 200`
chụp cận mái/LOW/HIGH. Bộ extra có ảnh BEFORE từ package v2 đặt cạnh package
này; không có v2 thì bỏ các job BEFORE. Giữ launcher chạy trong lúc chụp.

Normal bake, texture final, Unreal bake/reimport, collision/LOD và cấu trúc mái
T/courtyard hoặc đồ thị giao mái tùy ý chưa được kiểm tra trong bản này.
Các grammar được kiểm tra ngoài tập 15: hipped, round/conical, hybrid tower.
Xem `evaluation.md` và `results/final_summary.json` để biết phạm vi QA thực tế.
