# DreamDemo: demo hình ảnh Day, Night, Dream trên L_Kingdom_Main

Demo cho art define H "Bong bóng ký ức" (`art_define_HSR_bong_bong_ky_uc.md` và `TECH_DREAM_VISUALS.md` ở gốc repo). Ba trạng thái hình ảnh trên cùng một quảng trường của `/Game/Kingdom/Maps/L_Kingdom_Main`:

- **Day:** ánh sáng có sẵn của map (`KD_Sun`, `KD_SkyLight`, `KD_SkyAtmosphere`, `KD_Fog`, `KD_Clouds`).
- **Night:** đêm lễ hội ngoài đời: trăng, đèn lửa ấm, dây bóng đèn.
- **Dream:** giấc mơ: ánh sáng xanh mòng két từ trên, vân gợn, bong bóng ký ức, cá sao, bụi vũ trụ, đồ vật trôi, mực bóng đêm, không có lửa.

Toàn bộ logic nằm trong Blueprint. Project không cần build thêm C++ nào cho demo.

## Xem demo (team art)

1. Mở `Assets/_render/TDKProps.uproject`, mở map `Content/Kingdom/Maps/L_Kingdom_Main`.
2. Bấm **Play**. Camera tự chuyển về góc demo (`CAM_Wide`).
3. Phím:

| Phím | Việc |
|---|---|
| 1, 2, 3 | Day, Night, Dream (đổi tức thì) |
| G | Cổng mơ: vào Dream có chuyển cảnh |
| H | Thức dậy: về Day có chuyển cảnh |
| F1, F2, F3 (hoặc 7, 8, 9) | `CAM_Wide`, `CAM_Wall`, `CAM_Sky` |
| F4 (hoặc 0) | Về camera tự do của người chơi |
| M | Bật tắt giảm chuyển động (vân gợn đứng yên, đồ vật hạ xuống, ẩn cá, bụi chậm) |

**Không cần Play:** chọn actor `BP_DreamWorldState` trong Outliner (folder `DreamDemo`), trong Details bấm **Apply Day**, **Apply Night** hoặc **Apply Dream**. Map được lưu ở trạng thái Day; nếu lưu map khi đang ở trạng thái khác thì lần mở sau bấm lại nút tương ứng (giá trị `WorldBlend` không lưu theo map).

## Chỉnh ở đâu

| Muốn chỉnh | Chỗ chỉnh |
|---|---|
| Cường độ, tốc độ, tỉ lệ vân gợn; độ nâng đồ vật; cực quang; tâm và bán kính sân; nguồn bóng đêm | `Content/DreamDemo/MPC_Dream` (giá trị mặc định) |
| Bầu trời giấc mơ, trời đêm | `MI_DreamSky`, `MI_NightSky` |
| Bong bóng, cá, bụi | `M_MemoryBubble`, `MI_StarFish_0..2`, `M_CosmicDust` |
| Đèn, sương, exposure của Night và Dream | Actor trong folder `DreamDemo/Lighting_Night`, `DreamDemo/Lighting_Dream` |
| Màu và độ sáng đèn lồng, điểm yếu của Xenon | `MI_LanternFlame`, `MI_LanternStar`, `MI_WeakSpot_Orange` |
| Thời gian cổng mơ, trạng thái khởi đầu, có tự về `CAM_Wide` khi Play hay không | Thuộc tính của actor `BP_DreamWorldState` (nhóm DreamDemo) |
| Logic chuyển trạng thái, phím bấm | Graph của `Content/DreamDemo/Blueprints/BP_DreamWorldState` |
| Hình ký ức trong bong bóng | `T_MemoryPlaceholder_0..3` (ảnh tạm), gộp trong `T_MemoryArray` |

Thêm một actor vào một trạng thái: gắn tag `DreamState` cùng một hoặc nhiều tag `State_Day`, `State_Night`, `State_Dream`. Actor không có tag `DreamState` hiện ở mọi trạng thái.

Stencil theo bảng của tài liệu: nhân vật 1, Xenon 2, điểm yếu và hiệu ứng kỹ năng 3. Mesh có custom stencil khác 0 không nhận vân gợn.

## Bố cục

| Thư mục | Nội dung |
|---|---|
| `Content/DreamDemo/` | Asset: `MPC_Dream`, `Materials/`, `Textures/`, `Meshes/`, `Blueprints/` |
| `Tools/DreamDemo/houdini/` | Script bake texture và mesh (mỗi script một tiến trình hython) |
| `Tools/DreamDemo/ue/` | Script Unreal Python dựng asset và đặt actor lên map; `layout.json` là vị trí sân |
| `Tools/DreamDemo/bp_generator/` | Công cụ một lần đã sinh hai Blueprint (xem README trong đó) |
| `SourceAssets/DreamDemo/` | Kết quả bake (PNG, EXR, HDR, FBX) |
| `RenderTests/DreamDemo/` | Ảnh chụp, ảnh so sánh, kết quả kiểm tra; `forest_prototype/` là báo cáo của bản thử đầu tiên trên map rừng |

Mọi actor của demo nằm trong folder Outliner `DreamDemo` và có tag `DreamDemo`. Ngoài ra demo chỉ gắn tag lên năm actor ánh sáng `KD_*`.

## Dựng lại

```
bash Assets/_render/Tools/DreamDemo/build_all.sh
"E:\SideFX\Houdini 22.0.368\bin\hython.exe" Assets/_render/Tools/DreamDemo/capture_all.py
"E:\SideFX\Houdini 22.0.368\python313\python.exe" -X utf8 Assets/_render/Tools/DreamDemo/houdini/run_all_bakes.py
```

- `build_all.sh` chạy các bước 3, 4, 5, 6, 8 và kiểm tra cấp level (bước 7) trong commandlet. Chạy lại được nhiều lần: mỗi bước xoá actor của chính nó rồi tạo lại. **Chỉnh sửa tay trên các actor đó sẽ mất**; chỉnh sửa trong asset (MPC, material instance, Blueprint) được giữ, trừ các material mà script dựng lại graph.
- `capture_all.py` mở editor, chạy PIE, gọi các event của `BP_DreamWorldState` và chụp ảnh vào `RenderTests/DreamDemo/captures/`. Đóng editor của `_render` trước khi chạy.
- **Sau khi chạy `Tools/Kingdom/build_kingdom_map.py` phải chạy lại `build_all.sh`:** công cụ đó tạo lại các actor `KD_*` (mất tag Day) nhưng không đụng tới folder `DreamDemo`.
- Xoá `Tools/DreamDemo/ue/layout.json` để script chọn lại vị trí sân; sửa tay file đó để dời sân.

## Giới hạn hiện tại

- Nhân vật là Mannequin, Xenon là một tảng đá tối, đèn lồng là khối cơ bản của engine: đều là đồ tạm.
- Cửa sổ nhà chưa sáng đèn ban đêm và nhà chưa có viền vàng trong giấc mơ: cần mặt nạ `trim_mask`, `age_mask` từ HDA (TECH_DREAM_VISUALS mục 5.3).
- Không dùng Niagara: cá, bong bóng, bụi là instanced static mesh di chuyển bằng world position offset.
- Chưa có âm thanh.
- Chưa đo GPU trên map này.
