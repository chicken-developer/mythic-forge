# Bước 2: Houdini bake texture và mesh

Ngày chạy: 2026-11-15. Houdini 22.0.368.

## Lệnh

```
"E:\SideFX\Houdini 22.0.368\python313\python.exe" -X utf8 Scripts/DreamDemo/houdini/run_all_bakes.py
```

`run_all_bakes.py` gọi từng script bằng một tiến trình hython riêng, rồi ghép ảnh xem nhanh trong thêm một tiến trình hython. Tham số (độ phân giải, seed, màu) nằm ở đầu mỗi script.

## Kết quả

| Asset | Script | Thời gian tiến trình | Ghi chú |
|---|---|---|---|
| `T_Caustics.png` 1024², 1 kênh | `bake_caustics.py` | 2,3 s | Vân rộng và mềm có chủ ý: material lấy `min` hai lớp trôi ngược nhau để ra vân sắc |
| `T_DreamNebula.exr` 4096 × 2048, half, và `.hdr` | `bake_nebula.py` | 3,5 s | Xem "Lệch so với prompt" |
| `T_AuroraStrip.png` 2048 × 256, `T_AuroraFlow.png` 512² RG | `bake_aurora.py` | 2,5 s | Flow map phải nhập với sRGB tắt |
| `T_StarField.png` 1024², `T_CloudNoise.png` 1024² | `bake_stars_noise.py` | 2,6 s | `T_CloudNoise` là asset thêm |
| `T_ThinFilmRamp.png` 256 × 8, `T_MemoryPlaceholder_0..3.png` 512² RGBA | `bake_ramp_memory.py` | 2,1 s | Ảnh ký ức là ảnh tạm, alpha là mặt nạ tròn mềm |
| `SM_StarFish.fbx`, `SM_DustMote.fbx`, `SM_Bubble.fbx` | `bake_meshes.py` | 2,0 s | Hai mesh sau là asset thêm |

Mọi file nằm trong `RawContent/DreamDemo/`; ảnh kiểm tra và số đo trong `RawContent/DreamDemo/_checks/` (`bake_summary.json`). Ảnh xem nhanh: `STEP_2_preview.png`.

## Kiểm tra đã chạy

- **Liền mạch:** so bước nhảy trung bình qua mép lặp với bước nhảy giữa hai pixel kề nhau. Các texture lặp đều cho hai số cùng cỡ (ví dụ `T_Caustics`: 0,015 trong ảnh, 0,013 đến 0,016 qua mép). Ảnh ghép 2 × 2 nằm trong `_checks/*_tile2x2.png`. Chưa soi bằng mắt từng ảnh ghép.
- **`T_DreamNebula`:** giá trị lớn nhất 0,68; nửa trên trung bình 0,073, nửa dưới 0,010 (tối về chàm đặc).
- **`SM_StarFish`:** 169 tam giác, 60 × 27,5 × 28,5 cm sau khi nhập vào Unreal. Đỉnh có X lớn nhất có UV0.x = 0, đỉnh có X nhỏ nhất có UV0.x = 1: đầu hướng +X (kiểm tra bằng script ở Bước 3).
- **Vertex color R trên vây:** đặt trong script, thấy vây sáng hơn thân trong ảnh chụp; chưa đọc lại giá trị từng đỉnh sau khi nhập.

## Lệch so với prompt

1. **Tinh vân không render bằng Karma.** Ảnh được tính bằng nhiễu 3D trên hướng nhìn ở ba "khoảng cách", cộng dải bụi tối và sợi mảnh. Cách này liền mạch theo kinh độ và bake trong vài giây. Nó cũng không phải Copernicus: mọi ảnh được tính bằng VEX trên một đám điểm (một điểm một pixel), nên không đụng tới lỗi driver AMD khi chạy Copernicus chung với OpenGL.
2. **Thêm `T_DreamNebula.hdr`.** Unreal chỉ biến ảnh long-lat thành TextureCube khi nhập file Radiance `.hdr`. Bản EXR vẫn được xuất theo prompt.
3. **Thêm ba asset:** `T_CloudNoise` (mây cho trời đêm và nhiễu cho màng bong bóng), `SM_DustMote` (khối 20 mặt cho bụi), `SM_Bubble` (cầu UV 624 tam giác). Bụi và bong bóng di chuyển bằng WPO nên cần mesh riêng để đặt bounds extension; cầu của engine không sửa được.
4. **Bụi là khối 20 mặt, không phải quad quay theo camera.**
5. **Ảnh ký ức và dải màu màng xà phòng vẽ bằng Pillow trong hython**, không qua node Houdini.

## Lỗi đã gặp và đã sửa

- `smooth(a, b, x)` của VEX không nhận mép đảo (`a > b`); ba chỗ dùng kiểu GLSL cho kết quả ngược. Đã đổi thành `1 - smooth(b, a, x)`.
- FBX xuất theo mét; Unreal nhập thành 0,6 cm. Đã bật `convertunits` trên ROP FBX.
- Pillow không nạp được trong `python313\python.exe` (thiếu DLL), chỉ chạy trong hython.

## Cần con người làm

- 2D artist thay `T_MemoryPlaceholder_0..3`.
- Xem `STEP_2_preview.png` và các ảnh ghép 2 × 2 để duyệt thẩm mỹ của vân gợn, tinh vân, cực quang.
