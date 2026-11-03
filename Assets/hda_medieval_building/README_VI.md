# Medieval — chỉnh HIP, dataset và bake 15 mẫu

Model làm việc: [medieval_building.hip](medieval_building.hip), node
`/obj/medieval_building`. Chọn `CTRL_BUILDING` để sửa thông số. Network giữ nguyên
18 function native SOP/VEX; các cửa cắt xuyên shell thật. Giằng cong đã đổi thành
giằng đầu cột ngắn, cột/đà bám nhịp, cửa sổ và cánh chớp chừa khoảng tới khung gỗ.
Guild hall có bốn trụ/tháp đá chóp vuông; forgehouse có lò vòm và ống khói rỗng;
storage/workshop có cần nâng được chống đỡ. Các module này dùng native SOP và
chọn bằng `facade_overrides`, không cần geometry hoặc texture của SkyTown.

[manifest.json](manifest.json) chứa đúng 15 preset và mapping ref. Mỗi JSON trong
`datasets/` chứa toàn bộ effective parameters và seed; `baseline.json` là mẫu
kiểm tra bổ sung. Hai ảnh NormalHouse C trùng dữ liệu nên preset 09/10 khác nhau
ở ban công và canopy. Xem [analysis.md](analysis.md) và ảnh `comparisons/` để thấy
những chi tiết riêng của concept còn được xấp xỉ.

Trong Houdini Python Shell:

```python
import json, runpy
from pathlib import Path
root = Path(r"E:/Repos/mythic-forge/Assets/hda_medieval_building")
api = runpy.run_path(str(root / "build_hda.py"))
cfg = json.loads((root / "datasets/06_tavern.json").read_text(encoding="utf-8"))
building = api["build"](cfg)
```

Nếu HIP đã mở, `build()` áp dụng cấu hình vào object hiện có. Nếu dùng file HDA
độc lập, API tạo một instance SOP từ
`asset/medieval_house_layout.hda` (`mythic::medieval_house::3.0`). Định nghĩa này
đã được cook và đổi tham số trong process mới, từ bản copy ở thư mục khác.

Chạy lại toàn bộ bằng PowerShell, từ repository:

```powershell
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' Assets/hda_medieval_building/scripts/check_configs.py
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' Assets/hda_medieval_building/scripts/bake_variants.py --no-render --resolution 4096
& 'E:/SideFX/Houdini 22.0.368/bin/hython.exe' Assets/hda_medieval_building/scripts/render_variant.py 06_tavern
```

`--ids <id...>` giới hạn mẫu cần bake; `--skip-existing` bỏ qua kết quả có đúng
HIP hash/config. Mỗi mẫu có low FBX, high FBX/BGEO và normal 4096×4096 từ geometry
high có bevel theo nhóm chi tiết. Low được triangulate trước bake; UV0 được pack
riêng, normal dùng DirectX/MikkT và linear. Shader dùng base color constant,
không dùng texture albedo. Khi bake, các bộ phận được tách vị trí tạm thời để
tránh chiếu nhầm; mesh export giữ vị trí lắp ráp.

Export nằm ở `../_render/SourceAssets/Medieval15/`. Công cụ Unreal nằm trong
`../_render/Tools/Medieval15/`; log kiểm thử và screenshot ở
`../_render/RenderTests/Medieval15/`. Map review:
`/Game/Medieval15/Maps/L_Medieval15_Review` trong project
`../_render/TDKProps.uproject`.

`import_and_test.py` đọc `RenderTests/Medieval15/import_request.json`:
`{"cleanup": true}` import 15 mẫu, dựng map, rồi xoá các package cũ
`/Game/Gothic`, `/Game/Medieval`, `/Game/SkyTown`. Chỉ chạy trong project review.
Script kiểm tra số tam giác, kích thước, material slots, normal compression và
sRGB. `capture_review.py` cần full Unreal Editor để xuất ảnh từng mẫu và nội thất.

Houdini cook/export/bake, HDA portability, Unreal import và render có báo cáo
riêng. Static path audit không thay thế các kiểm thử đó. Mantra thiếu license
render trên máy này; ảnh Houdini dùng OpenGL ở process riêng để tránh xung đột
driver khi dùng chung với Copernicus.
