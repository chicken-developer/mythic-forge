# Bước 4: Ba bộ ánh sáng và bộ chuyển trạng thái

Ngày chạy: 2026-11-15.

## Lệnh

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" TestShaderEditor Win64 Development -Project=<project> -WaitMutex
bash Scripts/DreamDemo/ue_run.sh cmd Scripts/DreamDemo/ue/step4_lighting_states.py dd_step4
```

## Đã làm

**Map.** `Demonstration` được lưu thành `/Game/DreamDemo/Maps/L_DreamDemo`. Map gốc không đổi.

**Sân.** `dd_layout.py` chọn khoảng trống rộng nhất gần điểm xuất phát từ vị trí 1126 cây foliage: tâm (-100, 500, 111), bán kính 1500 cm, cây gần nhất 22,2 m, không cây nào trong sân, mặt đất chênh khoảng 1,5 m. Kết quả lưu trong `Scripts/DreamDemo/ue/layout.json`; xóa file để chọn lại, sửa tay để dời sân.

**Ba nhóm ánh sáng** (folder Outliner `DreamDemo/Lighting_*`, vì map không dùng World Partition). Actor tham gia trạng thái bằng tag `State_Day`, `State_Night`, `State_Dream`; actor không có tag là dùng chung.

| Nhóm | Thành phần |
|---|---|
| Day | Ánh sáng có sẵn của gói, giữ nguyên: sun 4.0, `BP_Sky_Sphere`, sky light, fog, post-process volume (exposure tự động trong khoảng 0,5 đến 0,6) |
| Night | `DL_Moon` 0,45, trắng xanh, cao 42°; `Sky_Night` dùng `MI_NightSky` (hướng trăng khớp với đèn); `SL_Night` real-time capture; `Fog_Night`; `PPV_Night` exposure khóa (manual, bias 1,6) |
| Dream | `DL_Dream` pitch -80, `#7FD6D0`, 1,2 (30% của sun 4.0), source angle 8; `Sky_Dream` dùng `MI_DreamSky`; `SL_Dream` dùng cubemap `T_DreamNebula`; `Fog_Dream` `#2E5F7A`; `PPV_Dream` exposure khóa (manual, bias 1,0), bloom 0,9, vignette 0,35, saturation 0,9, blendable `M_PP_DreamCaustics` |

**Bộ điều khiển.** `BP_WorldStateController` là Blueprint con của lớp C++ `ADreamWorldStateController` (`Source/TestShader/DreamDemo/`). Khi đổi trạng thái: ẩn và hiện actor theo tag, bật tắt component ánh sáng và post-process volume, đặt `WorldBlend` và `NightBlend`, recapture sky light kiểu captured scene. Biến `InitialState` là trạng thái khởi đầu. Trong editor có ba nút `Apply Day`, `Apply Night`, `Apply Dream` trên panel Details.

| Phím | Việc |
|---|---|
| 1, 2, 3 | Day, Night, Dream (đổi tức thì) |
| F1, F2, F3 (hoặc 7, 8, 9) | CAM_Wide, CAM_Wall, CAM_Sky |
| F4 (hoặc 0) | Về camera người chơi |
| G, H | Cổng mơ vào Dream; thức dậy về Day (Bước 8) |
| M | Giảm chuyển động: `CausticsSpeed = 0`, `FloatAmount = 0`, ẩn cá, bụi chậm còn 30% |

**Camera.** `CAM_Wide` sau lưng ba nhân vật 4 m, cao 2 m, nhìn về Xenon. `CAM_Wall` nhìn cận một vách đá và cỏ trước nó, FOV 60, không thấy trời. `CAM_Sky` nhìn lên tán cây và bầu trời, trăng nằm trong khung ở Night.

**Đại diện.** Ba Mannequin đứng yên chạy `MM_Idle`, custom stencil 1. Xenon là `SM_stone_05` phóng 1,6 lần (cao khoảng 5,3 m), material xám đen, stencil 2. Điểm yếu là một đĩa phát sáng cam, stencil 3, đặt lên bề mặt đá bằng line trace. Hai `SM_stone_06` ở rìa sân làm vách đá cho `CAM_Wall`.

## Lệch so với prompt

1. **Logic nằm trong C++, không phải Blueprint thuần.** Graph Blueprint không dựng được tin cậy bằng Python. Hệ quả: project cần build, và `TestShader.Build.cs` có thêm phụ thuộc `RHI`, `RenderCore`.
2. **Day dùng sky sphere của gói, không dùng SkyAtmosphere.** Giữ nguyên hình ảnh ban ngày của gói.
3. **Sky light ban đêm dùng real-time capture.** Lượt chụp đầu cho thấy capture kiểu captured scene giữ lại ảnh của cảnh ban ngày, làm bóng tối ban đêm ám vàng.
4. **CAM_Wall nhìn vách đá, không phải tường có cửa sổ.** Gói asset không có nhà.

## Kiểm tra

| Việc | Kết quả |
|---|---|
| Module C++ build | Đạt |
| Ba trạng thái khác nhau trên cùng bối cảnh | Đạt: `captures/CAM_Wide_{Day,Night,Dream}.png`, tổng hợp trong `compare_full.png` |
| Đổi trạng thái qua `SetWorldState`, đổi camera qua `SetViewCamera` | Đạt (chuỗi chụp tự động dùng hai hàm này) |
| Bấm phím 1/2/3, F1 đến F4, M bằng tay | Chưa kiểm tra |
| Ba nút `Apply ...` trong editor | Chưa kiểm tra bằng tay; hàm `ApplyDay` được script gọi trong editor world và không lỗi |
| Về Day sau khi qua Dream | Thấy đúng trong các khung hình `wake_*` của Bước 8; chưa so pixel với ảnh Day gốc |

Dữ liệu thô: `step4_build.json`.

## Phụ: việc UnrealBuildTool đã làm

Lần build đầu, UBT build lại cả plugin `HoudiniEngine` của project (272 action, khoảng 13 phút), tức ghi đè `Plugins/HoudiniEngine/Binaries` và `Intermediate`. Không file nguồn nào của plugin bị sửa.

## Cần con người làm

- Bấm thử các phím trong PIE hoặc `-game`.
- Duyệt ba mức exposure. Night hiện khá sáng dưới trăng; Dream có cỏ và tán cây xanh lục rất đậm.
