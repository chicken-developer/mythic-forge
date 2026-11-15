# DreamDemo trên L_Kingdom_Main: kết quả kiểm tra

Ngày chạy: 2026-11-16. UE 5.8.4, AMD Radeon RX 6900 XT. Cách xem và chỉnh demo: `Tools/DreamDemo/README.md`.

## Đã chuyển gì từ project TestShader

| Trước (TestShader) | Bây giờ (`Assets/_render`) |
|---|---|
| Lớp C++ `ADreamWorldStateController` | Blueprint thuần `/Game/DreamDemo/Blueprints/BP_DreamWorldState` |
| Lớp C++ `ADreamInstancedField` | Blueprint `/Game/DreamDemo/Blueprints/BP_DreamInstancedField` (một component, không graph); instance do script đặt và lưu trong map |
| Chuỗi chụp và đo trong C++ (`-DreamCapture`) | `Tools/DreamDemo/ue/pie_tour.py`: chạy PIE và gọi event của Blueprint |
| Map rừng `L_DreamDemo` | `/Game/Kingdom/Maps/L_Kingdom_Main`, sân ở quảng trường |
| `Scripts/DreamDemo`, `RawContent/DreamDemo`, `Docs/DemoReports` | `Tools/DreamDemo`, `SourceAssets/DreamDemo`, `RenderTests/DreamDemo` |

Asset trong `/Game/DreamDemo` được dựng lại từ đầu bằng script ngay trong project này, không chép `.uasset` từ TestShader (trừ hai Blueprint, sinh bằng `Tools/DreamDemo/bp_generator`).

Thay đổi so với bản rừng:

- **Cổng mơ không còn Tick.** Blueprint đặt `GateMode`, `GateStartTime`, `GateDuration` trong `MPC_Dream`; material tự tính độ phủ theo thời gian game. `WorldBlend` đổi thẳng lúc màn hình đang phủ kín thay vì tăng dần 0 → 0,5 → 1.
- **Giảm chuyển động ẩn cá bằng material** (`MotionReduced` cắt opacity mask), nên Blueprint không cần duyệt actor.
- **Trạng thái áp bằng component visibility** cho mọi actor có tag `DreamState`, cộng `bEnabled` cho post-process volume. Cách này chạy được cả trong editor (ba nút Apply) lẫn khi Play.
- **Đồ vật trôi là chum, thùng gỗ, thùng rượu** của gói Stylized_Village, nâng 15 đến 40 cm như tài liệu.
- **CAM_Wall nhìn vào mặt tiền một ngôi nhà có cửa sổ.**
- **Sân nhỏ hơn:** tâm (3450, 0, 390), bán kính 1350 cm thay vì 1500, vì khoảng trống lớn nhất gần đài phun chỉ rộng chừng đó.

## Thay đổi ngoài thư mục DreamDemo

| File | Thay đổi |
|---|---|
| `Config/DefaultEngine.ini` | Thêm `r.CustomDepth=3` (custom depth có stencil) |
| `Content/Kingdom/Maps/L_Kingdom_Main.umap` | Thêm actor trong folder `DreamDemo`; gắn tag `DreamState`, `State_Day` lên `KD_Sun`, `KD_SkyLight`, `KD_SkyAtmosphere`, `KD_Fog`, `KD_Clouds`; đặt 27 actor `KD_Label_*` thành hidden in game (khi Play chúng hiện chữ lơ lửng trên trời) |
| `Assets/README.md`, `MEMORY.md` | Thêm dòng chỉ tới demo |

Source C++ của `TDKProps` không đổi.

## Kiểm tra đã chạy

| Kiểm tra | Kết quả |
|---|---|
| Hai Blueprint biên dịch | Đạt, công cụ sinh báo 0 lỗi nối dây |
| `build_all.sh` (bước 3, 4, 5, 6, 8, 7) | Đạt cả 6 bước |
| Shader biên dịch khi chạy PIE | Không có dòng `Failed to compile` trong log |
| Lỗi Blueprint lúc chạy | Không có `Blueprint Runtime Error` trong log |
| Event `ApplyState` cho cả ba trạng thái | Đạt: 9 ảnh `captures/{CAM_Wide,CAM_Wall,CAM_Sky}_{Day,Night,Dream}.png`, tổng hợp trong `compare_full.png` |
| Event `ViewWide`, `ViewWall`, `ViewSky` | Đạt (chính các ảnh trên) |
| Event `Gate` (vào Dream rồi thức dậy) | Đạt: `gate_01`, `gate_02` và `wake_01` phủ kín; trước đó là cảnh cũ, sau đó là cảnh mới. Ảnh ghép: `STEP_8_gate_frames.png` |
| Event `ToggleReducedMotion` | Đạt theo ảnh `captures/check_reduced_motion.png`: cá biến mất, thùng gỗ nằm xuống đất |
| Stencil: nhân vật không nhận vân gợn | Đạt: chênh 0,34/255 trên pixel có stencil, 6,53/255 trên mặt đất |
| Dream không có nguồn sáng hay emissive màu ấm | Đạt |
| Không bong bóng hay đồ vật trôi trong 70% bán kính sân | Đạt: ngưỡng 945 cm, đồ trôi gần nhất 1116 cm, bong bóng gần nhất 1000 cm |
| Stencil đúng bảng (Golem 1, Xenon 2, điểm yếu 3) | Đạt |
| Instance của cá, bong bóng, bụi còn nguyên sau khi mở lại map | Đạt (bước 7 đếm lại sau khi nạp map từ đĩa) |

Chi tiết: `step7_results.json`, `step7_level_checks.json`, `captures/tour_log.json`.

Thư mục `captures/` (ảnh gốc 1920 × 1080) không đưa vào git; chạy `Tools/DreamDemo/capture_all.py` để tạo lại. Các ảnh ghép `compare_full.png`, `compare_320_gray.png`, `STEP_8_gate_frames.png` có trong repo.

## Chưa kiểm tra

- **Phím bấm.** Các event mà phím gọi đã được chạy qua script, nhưng chưa ai bấm phím thật. Node phím nằm trong `BP_DreamWorldState`; actor đặt `Auto Receive Input = Player 0`.
- **Ba nút Apply trong editor** bằng tay. Script gọi `ApplyDay` trong editor world không lỗi.
- **Việc tự chuyển về `CAM_Wide` khi Play** bằng tay (nhánh này gọi cùng event `ViewWide`).
- **GPU trên map này.** Bộ đo cũ nằm trong C++ đã bỏ. Số của bản rừng: `forest_prototype/STEP_7.md`.
- **Khung hình nhòe do Lumen khi qua cổng mơ:** cần người soi `captures/gate_03.png` trở đi.
- Thời lượng cổng mơ trong chuỗi ảnh không phản ánh 1,2 giây thật: mỗi lần chụp làm game khựng lại.

## Cần con người đánh giá

- `compare_320_gray.png`: Night và Dream ở 320 px thang xám (đáp án trong `compare_320_gray_key.txt`).
- Mức vân gợn: trên tường trắng của nhà và trên nền lát đá sáng, vân khá đậm. Chỉnh `CausticsIntensity`, `CausticsBrightness` trong `MPC_Dream`.
- Cả cảnh Dream nghiêng hẳn về xanh mòng két vì nền quảng trường sáng màu.
- Vũng mực hiện rõ trên nền lát đá (khác bản rừng, nơi cỏ che mất).
- Chơi thử 20 phút xem có chóng mặt không.

## Lỗi đã gặp khi chuyển

- Terrain `KD_Terrain` không trả lời line trace trong commandlet; cao độ mặt đất lấy từ pivot của cây hoặc nhà `KD_*` gần nhất (quảng trường phẳng theo bậc).
- Chụp trong PIE: editor chạy nền bị giảm tốc và lệnh chụp làm script bị gọi lồng, khiến ảnh ghi đè hoặc bị bỏ qua. `pie_tour.py` giờ tắt giảm tốc, chờ file ghi xong và chặn gọi lồng.
