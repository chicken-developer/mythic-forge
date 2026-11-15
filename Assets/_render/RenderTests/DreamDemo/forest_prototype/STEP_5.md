# Bước 5: VFX giấc mơ

Ngày chạy: 2026-11-15. Không dùng Niagara: instanced static mesh và world position offset trong material. Mọi actor của bước này có tag `State_Dream` và chỉ hiện ở Dream.

## Lệnh

```
bash Scripts/DreamDemo/ue_run.sh cmd Scripts/DreamDemo/ue/step5_dream_vfx.py dd_step5
```

## Đã làm

| # | Hạng mục | Asset và actor | Ghi chú |
|---|---|---|---|
| 1 | Cá sao | `M_StarFish`, `MI_StarFish_0..2`; 3 actor `StarFishSchool_*`, mỗi đàn 8 con (tag `FX_Fish`) | Unlit, phát sáng theo vertex color R và fresnel. Uốn thân theo UV0.x; quỹ đạo là phép quay quanh actor tính trong WPO, nên cá luôn hướng theo tiếp tuyến. Cao 8 đến 15 m. Không Tick, không đổ bóng, không vào distance field và ray tracing |
| 2 | Bong bóng ký ức | `M_MemoryBubble`; actor `MemoryBubbles`, 10 instance (tag `FX_Bubbles`) | Translucent, Surface TranslucencyVolume. Viền fresnel mũ 3, màu tra `T_ThinFilmRamp` cộng nhiễu trôi, hình ký ức từ `T_MemoryArray` theo `PerInstanceRandom`, chiếu theo pháp tuyến trong không gian camera. Lõi 0,2, viền 0,6. Bay lên và lặp bằng `frac`, mờ dần ở hai đầu vòng lặp. Tắt trong vùng bóng đêm |
| 3 | Bụi vũ trụ | `M_CosmicDust`; actor `CosmicDust`, 420 instance (tag `FX_Dust`) | Additive, vàng trắng nhạt, bay lên và lặp, mờ dần trong 3 m quanh camera |
| 4 | Đồ vật trôi | `Materials/Float/M_log_stump_Float` và các `MI_*_Float`; 13 actor `Float_*` (tag `FX_Float`) | Bản sao material của gói, thêm node `DreamFloat`: nâng lên, nhấp nhô chu kỳ 4 đến 7 s, nghiêng tối đa 6°, pha theo `ObjectPosition`, nhân `WorldBlend`, `FloatAmount` và `(1 - mặt nạ bóng đêm)`. NoCollision. Material gốc của gói không đổi |
| 5 | Mực bóng đêm | `M_Decal_DreamInk`; actor `Decal_DreamInk` (tag `FX_Ink`) | Deferred decal, `#0A0B1A`, roughness 0,05, mặt nạ từ `MF_DreamDarknessMask`. `DarknessSource0` ở (642, 1242), bán kính 450 cm, `DarknessCount = 1` |
| 6 | Đèn trong giấc mơ | 7 cột `Lantern_*_Post` (dùng chung), 7 `Lantern_*_Star` (chỉ Dream) | Đèn tạm bằng basic shape: gói asset không có đèn đường |
| 7 | Viền vàng cho nhà | Không làm | Mục tùy chọn, cần bạn xác nhận. Gói asset không có nhà, và không có texture hay vertex color nào dùng làm mặt nạ viền |

Lớp C++ `ADreamInstancedField` đặt instance theo vành tròn (số lượng, seed, bán kính, cung, độ cao, tỉ lệ) và dựng lại trong `OnConstruction`.

## Lệch so với prompt

1. **Đồ vật trôi là đá, cành và khúc gỗ**, không phải thùng, giỏ, ghế, bánh xe.
2. **Độ nâng 70 đến 130 cm** (`FloatLiftMin`, `FloatLiftMax`) thay vì 15 đến 40 cm: cỏ cao che mất đồ vật thấp. Đổi hai tham số MPC để về giá trị của tài liệu.
3. **Bụi là khối 20 mặt nhỏ**, không phải quad quay theo camera.
4. **Bounds:** thay vì bounds cố định trên component, ba mesh có bounds extension (cá ±40 m ngang) để GPU instance culling không loại instance đang bị WPO đẩy đi xa.
5. **Bong bóng dùng `SM_Bubble`** thay cho cầu của engine, cùng lý do.
6. **Vũng mực đặt ở 0,7 bán kính** để nằm trong khung của CAM_Wide.

## Kiểm tra

| Việc | Kết quả |
|---|---|
| Script chạy không lỗi, shader biên dịch | Đạt |
| Ảnh Dream ở ba camera | `captures/CAM_{Wide,Wall,Sky}_Dream.png` (chụp ở Bước 7) |
| Bong bóng và đồ vật trôi nằm ngoài 70% bán kính | Đạt: đồ trôi gần nhất 1054 cm, bong bóng gần nhất 1194 cm, ngưỡng 1050 cm (`step7_level_checks.json`) |
| Cá, bong bóng có hình ký ức, bụi, đồ vật trôi hiện đúng | Thấy trong ảnh chụp |
| Vân gợn tắt và đồ vật hạ xuống trong vùng mực | Chỉ thấy gián tiếp: trong `CAM_Wide_Dream.png` cỏ trong vũng mực chuyển đen và các viên đá trong vùng đó nằm sát đất. Chưa chụp cận cảnh, chưa đo |
| `stat gpu` 10 giây ở CAM_Wide | Đo ở Bước 7: Dream trung bình 8,91 ms GPU |

## Cần con người làm

- Vũng mực trên cỏ chỉ hiện thành một dải cỏ đen mỏng ở góc nhìn thấp; mặt mực bóng không thấy được vì cỏ che đất. Cần quyết định có dọn cỏ trong vùng mực hay không.
- Thay ảnh ký ức tạm.
- Xem lúc chạy: nhịp bơi của cá, tốc độ bong bóng, mật độ bụi ở tầm mắt.
- Chưa kiểm tra tùy chọn giảm chuyển động (phím M) bằng tay.

Dữ liệu thô: `step5_build.json`.
