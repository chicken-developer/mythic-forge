# Bước 6: Hoàn thiện trạng thái Night

Ngày chạy: 2026-11-15.

## Lệnh

```
bash Scripts/DreamDemo/ue_run.sh cmd Scripts/DreamDemo/ue/step6_night.py dd_step6
```

Cần Bước 5 chạy trước (bước đó dựng cột đèn).

## Đã làm

| Việc trong prompt | Kết quả |
|---|---|
| 1. Cửa sổ sáng vàng | **Không làm được.** Gói `Stylized_Forest` không có nhà hay cửa sổ |
| 2. Đèn đường ấm, lập lòe nhẹ | 7 đèn tạm: `Lantern_*_Flame` (cầu phát sáng cam, `MI_LanternFlame`) và `Lantern_*_Light` (point light 30 cd, màu (255, 166, 84), bán kính 1200 cm). Light function `M_LF_LanternFlicker` cho lập lòe khoảng ±6% |
| 3. Dây đèn lễ hội hoặc đèn lồng | Gói asset không có. Dựng tạm 3 dây, 33 bóng phát sáng màu ấm (`MI_FestivalBulb_0..2`), không kèm đèn thật, chỉ hiện ở Night |
| 4. Giới hạn point light có bóng đổ | 3 trên 7 đèn đổ bóng (ngân sách đặt là 4): hai đèn gần CAM_Wide nhất và đèn cạnh vách đá |

Tag: ngọn lửa và point light có `State_Day` và `State_Night`; bóng lễ hội chỉ `State_Night`; không cái nào có `State_Dream`.

## Lệch so với prompt

- **Ban ngày đèn sáng 15%.** Prompt yêu cầu point light hiện ở cả Day và Night. Ở mức đầy đủ, đèn để lại một mảng cam rõ trên vách đá giữa ban ngày. Light function nhân thêm `lerp(DayScale, 1, NightBlend)` với `DayScale = 0,15`; đặt `DayScale = 1` trong material để về đúng prompt.
- **Toàn bộ đèn là đồ tạm** dựng từ cylinder và sphere của engine.

## Night khác Dream ở đâu (đối chiếu danh sách của prompt)

| Điểm | Trạng thái |
|---|---|
| Night có nguồn sáng ấm, Dream không có | Đạt theo kiểm tra tag: trong Dream không có point light hay emissive nào có hue 15° đến 60° và bão hòa trên 0,4 (`step7_level_checks.json`). Điểm yếu màu cam của Xenon được loại khỏi kiểm tra vì là màu gameplay |
| Bóng tối thật, không vân gợn | Vân gợn nhân `WorldBlend`, bằng 0 ở Night. Thấy trong `captures/CAM_Wall_Night.png` |
| Trời có trăng và mây; Dream không có trăng | Thấy trong `captures/CAM_Sky_Night.png` và `CAM_Sky_Dream.png` |
| Không vật trôi, không bong bóng, không cá | Các actor đó chỉ có tag `State_Dream` |

## Kiểm tra

- Ảnh Night ở ba camera: `captures/CAM_{Wide,Wall,Sky}_Night.png` (chụp ở Bước 7).
- GPU ở CAM_Wide: Night 9,57 ms, thấp hơn Day 0,54 ms (số của Bước 7).
- Lập lòe của đèn: chưa xem lúc chạy.

## Cần con người làm

- Thay đèn tạm bằng asset đèn thật khi port sang khu chợ.
- Duyệt không khí lễ hội: dây bóng nhỏ và thưa ở khoảng cách của CAM_Wide.

Dữ liệu thô: `step6_build.json`.
