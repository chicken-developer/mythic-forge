# Bước 3: Nhập asset, MPC, material bầu trời và vân gợn

Ngày chạy: 2026-11-15.

## Lệnh

```
bash Scripts/DreamDemo/ue_run.sh cmd Scripts/DreamDemo/ue/step3_assets_materials.py dd_step3
```

Chạy trong commandlet (`-run=pythonscript`, không render). Sau khi chạy lại Bước 3 **phải chạy lại Bước 5**: Bước 3 dựng lại `MF_DreamDarknessMask`, các material của Bước 5 gọi function này và sẽ mất kết nối. `Scripts/DreamDemo/build_all.sh` chạy đúng thứ tự.

## Asset đã tạo (trong `/Game/DreamDemo`)

| Asset | Ghi chú |
|---|---|
| `Textures/T_Caustics`, `T_AuroraStrip`, `T_StarField`, `T_CloudNoise` | Grayscale, sRGB tắt |
| `Textures/T_AuroraFlow` | BC7, sRGB tắt |
| `Textures/T_ThinFilmRamp` | Không mip, clamp |
| `Textures/T_MemoryPlaceholder_0..3`, `T_MemoryArray` | Texture2DArray 4 lớp |
| `Textures/T_DreamNebula` | TextureCube, HDR nén (BC6H), nhập từ `.hdr` |
| `Meshes/SM_StarFish`, `SM_DustMote`, `SM_Bubble` | Có bounds extension để instance không bị cull khi WPO đẩy đi xa |
| `MPC_Dream` | Bảng dưới |
| `Materials/MF_DreamDarknessMask` | Hợp tối đa 4 vòng tròn `DarknessSource`, mép méo bằng ba sóng theo góc trôi chậm |
| `Materials/M_PP_DreamCaustics` | Post-process, vị trí `Scene Color After DOF` |
| `Materials/M_DreamSky`, `MI_DreamSky`, `MI_DreamSky_Low` | Unlit, Is Sky; static switch `HighQuality` tắt cực quang và lấp lánh |
| `Materials/M_NightSky`, `MI_NightSky` | Trời đêm riêng: gradient, sao, trăng, mây |

Cấu hình: thêm `r.CustomDepth=3` vào `[/Script/Engine.RendererSettings]` trong `Config/DefaultEngine.ini`.

## MPC_Dream

Theo prompt: `WorldBlend 0`, `NightBlend 0`, `CausticsIntensity 0.6`, `CausticsSpeed 0.15`, `CausticsScale 350`, `ArenaCenterRadius (-100, 500, 110.9, 1500)`, `ArenaCausticsFloor 0.4`, `FloatAmount 1`, `AuroraIntensity 0.6`, `AuroraSpeed 0.05`, `AuroraHueShift 0`, `DarknessSource0..3`, `DarknessCount`, `DarknessEdgeNoise 0.3`.

Thêm so với prompt:

| Tham số | Mặc định | Lý do |
|---|---|---|
| `CausticsBrightness` | 1,2 | Mức sáng của vân so với đèn chính của giấc mơ |
| `CausticsSharpness` | 2,2 | Số mũ áp lên `min` hai lớp |
| `CausticsMaxDistance` | 9000 | Vân tắt dần trước khoảng cách này; cũng loại pixel bầu trời |
| `CausticsFoliageBoost` | 2,5 | Cỏ dùng shading model subsurface nên sáng hơn nhiều so với diffuse của nó; không có hệ số này vân chìm hẳn trên cỏ |
| `CausticsTint` | `#7FD6D0` | Màu vân, để chỉnh được |
| `FloatLiftMin`, `FloatLiftMax` | 70, 130 cm | Tài liệu ghi 15 đến 40 cm cho sân lát đá; cỏ ở đây che mất đồ vật thấp |
| `DustSpeed` | 1 | Cho tùy chọn giảm chuyển động |
| `GateProgress`, `GateCenter` | 0; (0,5, 0,52) | Cổng mơ (Bước 8) |
| `DebugStencilView` | 0 | 1 = mặt nạ stencil; 2 đến 5 = shading model, diffuse, base color, pháp tuyến |

## M_PP_DreamCaustics so với TECH_DREAM_VISUALS mục 4.1

Làm đúng: vị trí thế giới từ depth, hai lớp trôi ngược hướng lấy `min`, mặt đất 1 và tường 0,35, hệ số tâm sân `lerp(Floor, 1, smoothstep(0.4R, R, d))`, bằng 0 trong vùng bóng đêm, loại pixel có custom stencil khác 0, cộng `Caustics * Intensity * DiffuseColor * Tint`, nhân `WorldBlend`.

Khác:

1. **Vị trí blendable là `Scene Color After DOF`.** Từ UE 5.4 đây là tên mới của `Before Tonemapping` (cùng giá trị enum 1).
2. **Chiếu ba hướng trên mặt dốc.** Chiếu từ trên xuống kéo vân thành sọc dọc trên vách đá (thấy rõ ở lượt chụp đầu). Mặt đất vẫn chiếu XY như tài liệu; mặt dốc trộn thêm hai phép chiếu ngang theo pháp tuyến. Tốn 6 lần lấy mẫu thay vì 2.
3. **Nhân `View.PreExposure`** vì scene color ở vị trí này đã nhân pre-exposure.
4. **Tăng vân trên cỏ** bằng `CausticsFoliageBoost`, nhận biết cỏ qua shading model ID trong GBuffer (2 hoặc 6) và pháp tuyến hướng lên.

## Kiểm tra

| Việc | Kết quả |
|---|---|
| Script chạy không lỗi | Đạt |
| Shader biên dịch khi chạy game | Đạt ở lượt chụp cuối: log không còn dòng `Failed to compile` nào |
| `T_DreamNebula` là TextureCube | Đạt |
| `T_MemoryArray` có 4 lớp | Đạt |
| Cá: đầu +X, UV0.x 0 ở đầu và 1 ở đuôi | Đạt (đọc từ mesh description) |
| Ảnh có bầu trời và ảnh nhìn xuống đất với `WorldBlend = 1` | Không chụp riêng cho bước này; dùng `captures/CAM_Sky_Dream.png` và `captures/CAM_Wide_Dream.png` của Bước 7 |
| Stencil loại nhân vật khỏi vân gợn | Đạt, đo ở Bước 7 |
| Độ sáng bầu trời không vượt một ngọn đèn đường | Chưa đo |

Dữ liệu thô: `step3_build.json`.

## Cần con người làm

- Duyệt việc chiếu ba hướng trên tường (khác tài liệu) và mức `CausticsFoliageBoost`.
- Thân cây ở gần nhận vân khá đậm, trông như vằn; cân nhắc giảm hệ số tường 0,35 hoặc `CausticsBrightness`.
