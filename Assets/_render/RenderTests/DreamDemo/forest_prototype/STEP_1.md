# Bước 1: Kiểm tra project và kế hoạch

Ngày chạy: 2026-11-15. Bước này chỉ đọc, không sửa project.

**Lệch so với prompt:** prompt yêu cầu dừng ở đây chờ duyệt kế hoạch. Yêu cầu của phiên này là "demo shader theo hướng dẫn" trên map `Demonstration`, nên các bước 2 đến 8 đã được làm tiếp luôn. Những quyết định đã tự chọn được liệt kê ở cuối file để bạn duyệt lại.

## Môi trường

| Mục | Giá trị |
|---|---|
| Project | `TestShader/TestShader.uproject` (template Third Person, có module C++ `TestShader`) |
| Unreal | 5.8.4, bản cài từ Launcher (`C:\Program Files\Epic Games\UE_5.8`) |
| Houdini | 22.0.368, `E:\SideFX\Houdini 22.0.368\bin\hython.exe`, license Commercial |
| GPU | AMD Radeon RX 6900 XT, D3D12 SM6 |
| Gói asset | `/Game/Stylized_Forest` (36 static mesh, 34 texture, 7 material, 19 material instance, 2 map) |
| Git | `TestShader/` chưa được track trong repo `mythic-forge` (`?? TestShader/`) |

Plugin: Python Script Plugin và Editor Scripting chạy được trong commandlet; Niagara có sẵn; Houdini Engine 3.0 (H22.0.466) và HoudiniNiagara nằm trong `Plugins/` của project.

Render: Lumen GI và reflection bật (`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`), Virtual Shadow Maps bật, Substrate bật, static lighting tắt. `r.CustomDepth` là 1 (chưa có stencil) trước khi Bước 3 đổi thành 3. Không mesh nào của gói bật Nanite.

hython chạy được; Copernicus có trong bản Houdini này (383 node COP) nhưng không dùng (xem Bước 2).

## Gói asset thật khác với giả định của prompt

Prompt mô tả một quảng trường có nhà, cầu, đài phun, sạp, đèn đường, cờ. Gói `Stylized_Forest` là **rừng**: cây, đá, gốc cây, khúc gỗ, cành, cỏ, hoa, landscape. Không có:

- nhà, tường, cửa sổ, material emissive cho cửa sổ hoặc đèn;
- cầu, đài phun, mặt nước;
- sạp, thùng, giỏ, xe đẩy, ghế, đèn đường, cờ.

| Nhóm trong prompt | Có trong gói | Dùng làm gì |
|---|---|---|
| Mặt đất | Landscape 500 × 500 m, có lối đi lát đá | Mặt sân |
| Cây | `SM_tree_01..07`, khoảng 1100 cây foliage | Nền và "công trình cao ở xa" |
| Đá | `SM_stone_01..06`, `SM_small_stone_01..04` | Vách đá cho CAM_Wall, đồ vật trôi, Xenon đại diện |
| Gỗ | `SM_log_01..02`, `SM_stump_01..03`, `SM_branch_*`, `SM_small_branch_*` | Đồ vật trôi |
| Nhân vật | `SKM_Manny_Simple`, `SKM_Quinn_Simple`, animation `MM_Idle` | Ba Golem đại diện |

## Map

`/Game/Stylized_Forest/Maps/Demonstration`: 105 actor, không dùng World Partition. Ánh sáng có sẵn: directional light cường độ 4.0, `BP_Sky_Sphere`, sky light kiểu captured scene, exponential height fog, một post-process volume unbound (exposure min 0,5, max 0,6, bias 1,26). Không có SkyAtmosphere.

Quyết định: **không sửa map gốc.** Lưu thành `/Game/DreamDemo/Maps/L_DreamDemo` rồi làm trên bản sao (quy tắc 1 của Prompt 0).

## Kế hoạch đã thực hiện

| Bước | Cách làm trên project này |
|---|---|
| 2 | Bake bằng VEX trong hython, mỗi script một tiến trình; không Copernicus, không OpenGL, không Karma |
| 3 | Unreal Python trong commandlet: import, `MPC_Dream`, `MF_DreamDarknessMask`, `M_PP_DreamCaustics`, `M_DreamSky`, `M_NightSky` |
| 4 | "Sân" là khoảng trống trong rừng bán kính 15 m, tâm (-100, 500, 111), cây gần nhất cách 22 m. Bộ ánh sáng của gói thành Day; dựng thêm Night và Dream; bộ chuyển trạng thái là lớp C++ có Blueprint con |
| 5 | Cá, bong bóng, bụi bằng instanced static mesh và WPO; đồ vật trôi là đá, cành, khúc gỗ |
| 6 | Không có đèn và cửa sổ trong gói, nên dựng đèn tạm bằng basic shape |
| 7 | Chụp và đo trong game (`-game`), do lớp C++ điều khiển |
| 8 | Post-process material và logic chuyển cảnh trong lớp C++ |

## Rủi ro đã ghi nhận trước khi làm

- Blueprint graph (phím bấm, chuyển trạng thái) không dựng được tin cậy bằng Python, nên logic nằm trong C++ (`Source/TestShader/DreamDemo/`). Việc này cần build module và sửa `TestShader.Build.cs` (thêm phụ thuộc `RHI`, `RenderCore`).
- Substrate đang bật: material tạo bằng script đi qua lớp chuyển đổi legacy của Substrate.
- Cỏ của gói cao tới đầu gối, che đồ vật trôi thấp và làm vân gợn khó đọc trên mặt đất.

## Những chỗ đã tự quyết định, cần bạn duyệt lại

1. Làm tiếp các bước 2 đến 8 mà không dừng chờ duyệt.
2. Làm trên bản sao `L_DreamDemo`, không phải trên `Demonstration.umap`.
3. Dùng C++ cho bộ điều khiển thay vì Blueprint thuần.
4. Không tạo nhánh `demo/dream-visuals` và không commit: `TestShader/` chưa được track trong repo cha, và thêm cả project (gói asset, plugin, binary) vào repo là quyết định của bạn.
5. Dựng đèn lồng tạm, vì gói asset không có đèn.

Dữ liệu thô: `step1_inspect.json`.
