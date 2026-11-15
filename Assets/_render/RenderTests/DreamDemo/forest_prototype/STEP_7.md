# Bước 7: Chụp ảnh so sánh, đo hiệu năng, bài thử

Ngày chạy: 2026-11-15. GPU AMD Radeon RX 6900 XT, D3D12, cửa sổ 1920 × 1080, chạy `-game` (không cook).

## Lệnh

```
"E:\SideFX\Houdini 22.0.368\bin\hython.exe" Scripts/DreamDemo/capture_all.py
bash Scripts/DreamDemo/ue_run.sh cmd Scripts/DreamDemo/ue/step7_checks.py dd_step7
```

`capture_all.py` mở game với `-DreamCapture=<thư mục>`. `ADreamWorldStateController` đi qua từng trạng thái và camera, chờ 3 giây sau mỗi lần đổi (đồng hồ chờ chạy lại từ đầu chừng nào shader hay asset còn đang biên dịch), rồi chụp high-res screenshot 1920 × 1080. Sau đó script ghép ảnh và phân tích. Chạy bằng hython vì cần Pillow và numpy. Tùy chọn: `--quick` (chỉ chụp 9 ảnh), `--no-gate`, `--skip-capture`.

## Ảnh

| File | Nội dung |
|---|---|
| `captures/{CAM_Wide,CAM_Wall,CAM_Sky}_{Day,Night,Dream}.png` | 9 ảnh, đủ cả 9 |
| `compare_full.png` | Lưới 3 × 3: hàng là camera, cột là Day, Night, Dream |
| `compare_320_gray.png` | CAM_Wide và CAM_Wall của Night và Dream, 320 px, thang xám, không nhãn. Đáp án trong `compare_320_gray_key.txt` |
| `captures/check_*.png` | Ba ảnh của kiểm tra stencil |
| `captures/gate_*.png`, `wake_*.png`, `STEP_8_gate_frames.png` | Chuỗi khung hình cổng mơ |

## Hiệu năng ở CAM_Wide

Thời gian GPU trung bình trong 10 giây (`RHIGetGPUFrameCycles`), VSync tắt, không giới hạn FPS.

| Trạng thái | GPU (ms) | Khung hình (ms) | Draw call | Số khung đo |
|---|---|---|---|---|
| Day | 10,11 | 12,18 | 729 | 822 |
| Night | 9,57 | 11,57 | 782 | 865 |
| Dream | 8,91 | 10,94 | 718 | 915 |

**Dream thấp hơn Day 1,19 ms.** Mục tiêu "Dream tốn thêm tối đa 1,5 ms so với Day" đạt trên máy này. Lưu ý: máy đo là RX 6900 XT, không phải RTX 3060 của tài liệu; và Day đắt hơn vì bóng đổ mặt trời góc thấp trên cả rừng cây, không phải vì Dream rẻ.

Chi phí từng hiệu ứng: tắt hiệu ứng, đo 6 giây, lấy Dream (8,91 ms, đo lại lần hai) trừ đi.

| Hiệu ứng tắt | GPU còn lại (ms) | Chi phí (ms) | Ngân sách trong tài liệu (ms) |
|---|---|---|---|
| Vân gợn (post-process) | 8,84 | 0,07 | 0,25 |
| Bầu trời giấc mơ | 8,82 | 0,09 | không có |
| Cá sao | 8,91 | 0,00 | 0,25 cho bụi, cá, hạt vỡ |
| Bong bóng | 8,90 | 0,01 | 0,3 |
| Bụi | 9,00 | -0,09 | (gộp với cá) |
| Đồ vật trôi | 8,85 | 0,06 | 0,1 |
| Mực (decal) | 8,79 | 0,12 | 0,2 |

Hai lần đo đầy đủ cho từng hiệu ứng lệch nhau tới khoảng 0,1 ms (ví dụ bụi: +0,03 rồi -0,09; bong bóng: 0,10 rồi 0,01). **Sai số của phép đo vì vậy cỡ ±0,1 ms, ngang với chính các con số trong bảng.** Điều chắc chắn rút ra được: không hiệu ứng nào vượt ngân sách của nó, và tổng nằm dưới 0,6 ms. Muốn số chính xác hơn cần đo lâu hơn hoặc dùng Unreal Insights.

## Kiểm tra tự động

| Kiểm tra | Kết quả | Số liệu |
|---|---|---|
| Dream không có point light hay emissive màu ấm | Đạt | 1 đèn và 8 material emissive được xét, 0 vi phạm. Xét theo tag trạng thái. Điểm yếu cam của Xenon được loại trừ |
| Không bong bóng hay đồ vật trôi trong 70% bán kính | Đạt | Ngưỡng 1050 cm; đồ trôi gần nhất 1054 cm, bong bóng gần nhất 1194 cm (13 đồ trôi, 10 bong bóng) |
| Mesh có stencil khác 0 không nhận vân gợn | Đạt | Chênh lệch trung bình giữa `WorldBlend` 0 và 1: 1,41/255 trên pixel có stencil, 13,72/255 trên mặt đất. Ngưỡng: dưới 2/255 và mặt đất lớn hơn ít nhất 3 lần |
| Stencil đúng bảng (Golem 1, Xenon 2, điểm yếu 3) | Đạt | 5 component |
| Lửa đèn chỉ Day và Night, đốm sao chỉ Dream | Đạt | 47 actor ấm, 7 đốm sao |

Về kiểm tra stencil: lần chạy đầu trượt (4,13/255) vì ba nhân vật đang chạy animation idle giữa hai ảnh. Chuỗi chụp giờ dừng animation trong lúc so. Phần 1,41/255 còn lại là nhiễu theo thời gian của TSR và Lumen; phép đo này không tách được nó khỏi một lượng vân gợn rất nhỏ, nếu có.

## Cần con người đánh giá

| Bài thử | Tài liệu |
|---|---|
| Night và Dream ở 320 px thang xám: nói đúng trong một giây | `compare_320_gray.png`. Số tham khảo: ở CAM_Wall, năng lượng cạnh của Dream gấp đôi Night (0,061 so với 0,030) nhờ vân gợn; ở CAM_Wide hai ảnh gần nhau hơn (0,065 so với 0,056) |
| CAM_Wall ở Dream vẫn nhận ra giấc mơ | `captures/CAM_Wall_Dream.png`: vân gợn trên vách đá và đá trôi |
| Tâm sân yên hơn rìa | Chưa đánh giá. Tâm sân phủ cỏ, vân gợn trên cỏ vốn đã khó thấy |
| Nhân vật nổi hơn nền, điểm yếu sáng nhất | `captures/CAM_Wide_Dream.png` |
| Chóng mặt khi xem 20 phút (T5) | Chưa làm; cần người chơi thử |
| Độ đẹp | Cả team |

## Chưa làm

- Bảy màu khoảng trống ở 25 m (T3): demo chỉ có một điểm yếu màu cam.
- Đo trên GPU tầm RTX 3060.
- Âm thanh (lọc thông thấp, reverb): không nằm trong bộ prompt này.

Dữ liệu thô: `step7_results.json`, `step7_level_checks.json`, `captures/capture_results.json`.
