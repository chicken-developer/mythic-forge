# Bước 8 (tùy chọn): Chuyển cảnh cổng mơ

Ngày chạy: 2026-11-15.

## Lệnh

```
bash Scripts/DreamDemo/ue_run.sh cmd Scripts/DreamDemo/ue/step8_dream_gate.py dd_step8
```

Chạy sau Bước 4. Khi chạy lại Bước 4, bộ điều khiển tự nối lại với `PPV_DreamGate` nếu volume đã có.

## Đã làm

- `M_PP_DreamGate`: post-process (`Scene Color After DOF`, priority 10). Vòng sáng xanh mòng két lan từ `GateCenter` ra phủ kín màn hình theo `GateProgress`, mép méo nhẹ và có hạt sáng lấp lánh.
- `PPV_DreamGate`: volume unbound, tắt sẵn; bộ điều khiển chỉ bật trong lúc chuyển cảnh, nên lúc thường không tốn pass nào.
- Logic trong `ADreamWorldStateController::TickGate`, theo TECH_DREAM_VISUALS mục 3.2:
  1. Phủ: `GateProgress` 0 → 1 trong 1,2 giây, `WorldBlend` đi tới 0,5.
  2. Khi phủ kín: đổi nhóm ánh sáng và actor; tắt input di chuyển và nhìn.
  3. Giữ 3 khung hình, bật lại input.
  4. Mở: `GateProgress` 1 → 0 trong 1,2 giây, `WorldBlend` 0,5 → 1.
  5. Thức dậy (phím H) dùng 0,6 giây mỗi nửa.
- Phím G vào Dream từ Day hoặc Night; phím H về Day. Thời gian và số khung giữ là thuộc tính của bộ điều khiển (`GateHalfSeconds`, `WakeHalfSeconds`, `GateHoldFrames`).

## Kiểm tra

Chuỗi chụp gọi `StartGateTransition(Dream)` từ Day rồi `StartGateTransition(Day)`, mỗi bước chụp một khung: 30 khung `gate_*`, 16 khung `wake_*`, ghép trong `STEP_8_gate_frames.png`.

| Việc | Kết quả |
|---|---|
| Màn hình phủ kín trước khi đổi bộ ánh sáng | Đạt: `gate_03` phủ 100%; khung trước đó vẫn là cảnh Day, khung mở ra đầu tiên đã là cảnh Dream |
| Thức dậy về Day | Đạt: `wake_02` đến `wake_05` phủ 100%, sau đó là cảnh Day |
| Không khung hình nào nhòe do Lumen chưa ổn định | **Chưa kết luận.** Ở cỡ ảnh thu nhỏ không thấy khung nào nhòe, nhưng cần người soi ảnh gốc `captures/gate_04.png` đến `gate_12.png` |
| Bấm G và H bằng tay | Chưa kiểm tra |
| Input bị tắt lúc phủ kín | Chưa kiểm tra (chuỗi chụp không có người chơi điều khiển) |

**Giới hạn của chuỗi khung hình:** mỗi lần chụp làm game khựng lại, nên các khung cách nhau nhiều hơn 0,1 giây thời gian game. Trong chuỗi, pha phủ chỉ chiếm 4 khung thay vì 12. Chuỗi này cho thấy đúng thứ tự phủ, đổi, mở; nó không đo được thời lượng 1,2 giây. Muốn xem thời lượng thật thì bấm G trong game hoặc quay video.

## Cần con người làm

- Duyệt hình thức của cổng mơ: hiện là một màn sáng trắng xanh lan theo vòng tròn từ giữa màn hình. Tài liệu muốn hiệu ứng tỏa từ Little Princess; đổi `GateCenter` trong MPC (tọa độ màn hình) hoặc chiếu vị trí nhân vật vào đó.
- Soi khung hình gốc để tìm nhòe Lumen.

Dữ liệu thô: `step8_build.json`, mục `gate` trong `step7_results.json`.
