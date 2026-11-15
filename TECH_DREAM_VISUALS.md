# Kỹ thuật hình ảnh giấc mơ: "Bong bóng ký ức"

Cập nhật 2026-11-13. Tài liệu kỹ thuật để triển khai art define H ("Bong bóng ký ức") trên project hiện tại: Unreal Engine 5.8, C++ và GAS, co-op listen server. Đọc kèm `art_define_HSR_bong_bong_ky_uc.md` cho phần quy tắc hình ảnh.

## 0. Giả định và những điều cần kiểm tra trước

| # | Giả định | Kiểm tra |
|---|---|---|
| 1 | Bản đồ Thành Golem dùng World Partition | Nếu không, thay Data Layer bằng level streaming ở mục 3 |
| 2 | Lumen bật cho cả hai thế giới | Config hiện có `r.DynamicGlobalIlluminationMethod=1` và `r.ReflectionMethod=1`, tức Lumen; cần xác nhận trên map Thành Golem |
| 3 | Nhà cửa sinh từ HDA Houdini, xuất sang Unreal | Mục 5.3 cần thêm hai thuộc tính vào HDA |
| 4 | Đổi tên Knight thành Golem (mốc R0) có thể chưa xong | Tên module và đường dẫn dưới đây dùng tên mới; trước R0 đọc `Golem` là `Knight` |
| 5 | Cấu hình máy mục tiêu chưa chốt | Mục 9 dùng ngân sách tạm cho GPU tầm RTX 3060 ở 1080p |

Tên asset trong tài liệu theo quy ước phổ biến của Unreal (`M_`, `MI_`, `MF_`, `MPC_`, `NS_`, `T_`, `DA_`). Đối chiếu lại với `CODING_RULES.md` của repo trước khi tạo asset.

## 1. Kiến trúc tổng thể

Giấc mơ dùng lại hình khối của khu đó ngoài đời. Không dựng map riêng.

```
EGamePhase (host, replicate)
        │  đổi pha
        ▼
UDreamWorldSubsystem (mỗi máy, chỉ hình ảnh)
  ├─ MPC_Dream.WorldBlend        0 = thật, 1 = mơ
  ├─ Data Layer: DL_Real / DL_Dream
  ├─ MPC_Dream.Darkness*          nguồn bóng đêm
  └─ Settings giảm chuyển động
        │
        ├─ M_PP_DreamCaustics     vân gợn (post-process)
        ├─ MF_DreamSurface        viền vàng, xóa dấu thời gian, đồ vật trôi
        ├─ NS_MemoryBubble        bong bóng
        ├─ NS_CosmicDust          bột vũ trụ
        ├─ NS_StarFish            cá sao
        ├─ M_Decal_DreamInk       mực bóng đêm
        └─ Audio: lọc thông thấp + reverb
```

Nguyên tắc:

- **Mọi thứ trong tài liệu này là hình ảnh và chạy cục bộ trên từng máy.** Host chỉ replicate thứ đã có: `EGamePhase`, thời điểm đổi pha, trạng thái khoảng trống, mức bóng đêm của sân. Không replicate bong bóng, cá hay đồ vật trôi.
- **Một tham số điều khiển tất cả:** `MPC_Dream.WorldBlend`. Vật liệu, post-process và âm thanh đều đọc tham số này, nên cảnh chuyển chỉ có một đường đi.
- **Không chạm gameplay:** đồ vật trôi không có va chạm; mặt sân là geometry gốc, không bao giờ di chuyển.

## 2. Material Parameter Collection `MPC_Dream`

| Tham số | Kiểu | Mặc định | Ai ghi |
|---|---|---|---|
| `WorldBlend` | scalar | 0 | `UDreamWorldSubsystem` |
| `CausticsIntensity` | scalar | 0,6 | Settings |
| `CausticsSpeed` | scalar | 0,15 | Settings; 0 khi giảm chuyển động |
| `CausticsScale` | scalar | 350 (cm mỗi ô) | Settings |
| `ArenaCenterRadius` | vector | xyz tâm sân, w bán kính | Subsystem, theo sân hiện tại |
| `ArenaCausticsFloor` | scalar | 0,4 | Mức vân gợn ở tâm sân so với rìa |
| `FloatAmount` | scalar | 1 | Settings; 0 khi giảm chuyển động |
| `GildAmount` | scalar | 1 | Settings |
| `DarknessSource0..7` | vector | xyz vị trí, w bán kính | Subsystem |
| `DarknessCount` | scalar | 0 | Subsystem |
| `DarknessEdgeNoise` | scalar | 0,3 | Settings |

Giới hạn 8 nguồn bóng đêm là đủ: mỗi sân có tối đa 3 Xenon cộng vài điểm loang ở rìa.

## 3. Bật tắt hai thế giới

### 3.1 Data Layer

| Data Layer | Chứa |
|---|---|
| `DL_Real` | Dân, người bán ở sạp, đèn lửa, khói bếp, bộ ánh sáng ban ngày (directional light, sky light, sky atmosphere, height fog, post-process volume) |
| `DL_Dream` | Bộ ánh sáng giấc mơ, Niagara bong bóng, bụi, cá sao, đồ vật trôi, đèn có đốm sao, các mảnh mái lơ lửng ở rìa sân |
| Không thuộc layer nào | Nhà cửa, đường, cầu, đài phun, cây, sạp hàng: dùng chung, đổi hình bằng `WorldBlend` |

Data Layer ở dạng runtime. `UDreamWorldSubsystem` đặt trạng thái của chúng trên mỗi máy dựa trên `EGamePhase` đã replicate. Actor gameplay (Xenon, rào sân, điểm hồi sinh) do host spawn như hiện nay, không đặt trong Data Layer.

Cần kiểm tra: trạng thái runtime Data Layer có được replicate từ server không, và đặt cục bộ trên client có gây xung đột không. Nếu có, chỉ đặt trên server và để engine replicate.

Nếu map không dùng World Partition: thay bằng ba sublevel `Lighting_Real`, `Lighting_Dream`, `Props_Dream`, bật tắt bằng level streaming.

### 3.2 Chuyển cảnh

Không trộn hai bộ ánh sáng cùng lúc; Lumen sẽ cần thời gian hội tụ và cảnh sẽ nhòe. Thay vào đó:

1. `DreamGate`: hiệu ứng cổng mơ phủ dần màn hình trong 1,2 giây (Niagara hoặc post-process tỏa từ Little Princess). `WorldBlend` tăng 0 → 0,5.
2. Khi màn hình phủ kín: đổi Data Layer.
3. Chờ 2–3 khung hình để Lumen ổn định, rồi mở dần trong 1,2 giây. `WorldBlend` tăng 0,5 → 1.
4. `Wake` làm ngược lại, nhanh hơn (0,6 giây mỗi nửa).

Trong lúc phủ kín, tắt input của người chơi. Tổng thời gian khoảng 3 giây, nằm trong 20 giây của pha `DreamGate`.

## 4. Vân gợn (dấu hiệu chính)

### 4.1 Phương án chọn: post-process

`M_PP_DreamCaustics`, blendable location `Before Tonemapping`, gắn vào post-process volume trong `DL_Dream`.

1. Dựng lại vị trí thế giới từ `SceneDepth`.
2. Lấy mẫu `T_Caustics` (texture lát liền, kênh đơn) chiếu từ trên xuống theo XY thế giới, hai lớp trôi ngược hướng nhau với tốc độ `CausticsSpeed`, lấy `min` hai lớp để ra vân sắc.
3. Hệ số theo pháp tuyến: lấy `WorldNormal` từ GBuffer, `saturate(dot(N, up))` cho mặt đất; tường nhận vân yếu hơn (0,35).
4. Hệ số theo tâm sân: `lerp(ArenaCausticsFloor, 1, smoothstep(0,4R, R, dist))`. Tâm sân yên hơn rìa.
5. Hệ số bóng đêm: 0 bên trong vùng bóng đêm (mục 7).
6. Loại trừ bằng custom stencil: nhân vật, Xenon, khoảng trống, hiệu ứng kỹ năng không nhận vân.
7. Cộng vào SceneColor: `SceneColor + Caustics * Intensity * DiffuseColor * TealTint`, nhân với `DiffuseColor` để vân trông như ánh sáng chứ không như lớp sơn.

Ưu điểm: một material, không sửa asset nào, tắt hẳn khi `WorldBlend` bằng 0. Chi phí cố định theo độ phân giải.

### 4.2 Phương án thay thế

- **Light function trên directional light của giấc mơ:** đúng vật lý hơn (vân chỉ ở chỗ có ánh sáng, bị bóng đổ che), nhưng phủ cả nhân vật và khó giảm ở tâm sân. Thử nếu post-process trông "dán" lên cảnh.
- **Deferred decal phủ sân:** dễ giới hạn theo sân, nhưng chồng với decal mực bóng đêm và tốn hơn khi sân lớn.

### 4.3 Stencil

Dùng chung một bảng giá trị custom stencil cho cả dự án:

| Giá trị | Đối tượng |
|---|---|
| 0 | Môi trường |
| 1 | Ba Golem, Little Princess, Princess phù thủy |
| 2 | Xenon |
| 3 | Khoảng trống, dấu ra hiệu, hiệu ứng kỹ năng |

Bật `r.CustomDepth=3` (có stencil) trong `DefaultEngine.ini` nếu chưa có.

## 5. Vật liệu bề mặt: `MF_DreamSurface`

Material function gắn vào master material của nhà cửa và đạo cụ. Đọc `WorldBlend` để chuyển.

### 5.1 Thành phố thời hoàng kim

| Kênh | Thế giới thật | Giấc mơ |
|---|---|---|
| Base color | Gốc, có rêu, bụi, vết nứt theo `AgeMask` | Bỏ phần `AgeMask`, nghiêng lạnh nhẹ (nhân với `#E6F2F2`) |
| Viền vàng | Không | Theo `TrimMask`: base color `#D9B25F`, metallic 1, roughness 0,3 |
| Emissive | 0 | 0, trừ cửa sổ (mục 5.4) |

### 5.2 Đồ vật trôi

Cho actor đồ trang trí có cờ `bDreamFloat` (bật qua material instance `MI_*_Float`):

- World position offset: nâng lên `FloatAmount * (15 + rand*25) cm`, nhấp nhô sin chu kỳ 4–7 giây, xoay quanh trục ngang tối đa 6 độ. Pha ngẫu nhiên lấy từ `ObjectPosition` để mỗi vật khác nhau.
- Nhân với hệ số bóng đêm tại `ObjectPosition`: khi mực loang tới, vật hạ về vị trí gốc, tức "rơi xuống".
- Đặt vật ở vị trí nghỉ trên mặt đất trong editor. WPO chỉ nâng lên.
- Collision `NoCollision`, `Can Ever Affect Navigation` tắt.
- Với mesh Nanite: bật `Evaluate World Position Offset` trên mesh đó. Chỉ khoảng 20–40 vật mỗi sân, nên chi phí nhỏ.

Đồ vật trôi chỉ đặt ở rìa sân. Validator ở mục 10 kiểm tra không có vật trôi nào trong `ArenaCenterRadius * 0,7`.

### 5.3 Mặt nạ từ Houdini

HDA nhà cửa hiện xuất màu phẳng theo preset. Thêm hai thuộc tính và xuất ra vertex color:

| Thuộc tính | Kênh | Giá trị |
|---|---|---|
| `trim_mask` | Vertex color R | 1 ở mép mái, khung cửa, góc tường, lan can |
| `age_mask` | Vertex color G | 1 ở chỗ có rêu, vết bẩn, mái vá |

Cả hai đều suy được từ các nhóm (group) mà HDA đã có cho mái, khung cửa, góc. Đây là thay đổi ở bước xuất, không phải làm lại hình khối. Asset làm tay (cầu, đài phun, sạp) vẽ hai kênh này trong Substance hoặc gán bằng vertex paint.

### 5.4 Cửa sổ

Vật liệu kính cửa sổ: thế giới thật dùng ánh đèn ấm hoặc tối; giấc mơ dùng cubemap `T_DreamOcean_Cube` (biển bột vũ trụ có cá sao) với parallax nhẹ. Chuyển theo `WorldBlend`.

### 5.5 Đèn

Đèn đường và lò có hai thành phần: ngọn lửa (trong `DL_Real`) và đốm sao (trong `DL_Dream`). Không dùng chung một actor để tránh logic điều kiện.

## 6. Niagara

| System | Kiểu | Số lượng | Ghi chú |
|---|---|---|---|
| `NS_MemoryBubble` | Mesh renderer (sphere), CPU | 8–12 mỗi sân | Spawn ở vành sân, bay lên 5–15 cm/s, lắc ngang bằng curl noise nhẹ. Kill khi ra khỏi bán kính rìa hoặc khi chạm bóng đêm |
| `NS_BubblePop` | Sprite, GPU | 30 hạt mỗi lần | Mưa hạt vàng kim khi thắng sân hoặc khi bong bóng chạm bóng đêm |
| `NS_CosmicDust` | Sprite, GPU | 300–600 trong thể tích sân | Bay lên chậm; tắt mờ trong 3 m quanh camera |
| `NS_StarFish` | Mesh renderer, GPU | 2–3 đàn, mỗi đàn 6–10 con | Bơi theo spline hoặc vòng tròn ở độ cao trên 4 m; xoay theo vận tốc; vây lắc bằng WPO trong material |
| `NS_DreamGate` | Sprite và ribbon | 1 | Hiệu ứng cổng mơ ở mục 3.2 |

### 6.1 Vật liệu bong bóng `M_MemoryBubble`

- Translucent, lighting mode `Surface TranslucencyVolume`, render sau depth of field.
- Viền: fresnel với số mũ khoảng 3.
- Ánh cầu vồng: lấy fresnel làm tọa độ tra vào `T_ThinFilmRamp` (dải màu cầu vồng nhạt), cộng thêm nhiễu trôi chậm.
- Hình ký ức bên trong: `T_MemoryArray` (Texture 2D Array, 8–12 lớp, 512×512, tông vàng nâu, hình bóng trận chiến xưa). Chỉ số lớp lấy từ dynamic parameter của Niagara, ngẫu nhiên mỗi hạt. Chiếu theo UV hướng camera, có parallax nhẹ, che bằng mặt nạ tròn mềm.
- Độ trong suốt lõi 0,15–0,25; viền 0,6.
- Emissive tối đa bằng 30% emissive của khoảng trống trên thân Xenon.

Translucent không vào phản xạ Lumen; chấp nhận được với kích thước này.

### 6.2 Hình ký ức

8–12 hình, mỗi hình một khoảnh khắc của trận chiến ngàn năm trước: ba Golem đầu tiên đứng trước cổng thành, Little Princess thả nguyên tố vào Xenon, Xeros bị giam vào giấc ngủ. Đây là việc của 2D artist. Có thể mỗi khu một bộ riêng, để bong bóng cũng kể chuyện.

## 7. Bóng đêm của Xeros

| Thành phần | Cách làm |
|---|---|
| Hình | `M_Decal_DreamInk`, deferred decal (DBuffer) phủ sân. Mặt nạ tính từ `DarknessSource0..7`: hợp các vòng tròn, mép biến dạng bằng nhiễu trôi chậm theo `DarknessEdgeNoise` |
| Chất liệu | Base color gần đen `#0A0B1A`, roughness 0,05, có phản chiếu tinh vân yếu |
| Ảnh hưởng | Cùng một hàm mặt nạ dùng trong vân gợn (tắt vân), đồ vật trôi (hạ xuống) và bong bóng (vỡ) |
| Nguồn | Host replicate mức bóng đêm của sân (số thực 0–1) và vị trí Xenon. Mỗi máy tự suy ra vị trí, bán kính của các nguồn và ghi vào MPC |

Hàm mặt nạ viết một lần trong `MF_DreamDarknessMask`, để vật liệu, post-process và decal dùng chung và luôn khớp nhau.

## 8. Ánh sáng, sương, phơi sáng, âm thanh

### 8.1 Bộ ánh sáng `DL_Dream`

| Actor | Thiết lập đề xuất |
|---|---|
| Directional light | Hướng gần thẳng đứng (pitch −80), màu `#7FD6D0`, cường độ khoảng 30% ban ngày, bóng đổ mềm (source angle lớn) |
| Sky light | Captured từ sky sphere giấc mơ, cường độ vừa, tông xanh mòng két |
| Sky sphere | Material tinh vân xanh lục lam đến chàm, tự phát sáng, không mặt trời |
| Exponential height fog | Màu `#2E5F7A`, mật độ vừa; bật volumetric fog nếu ngân sách cho phép |
| Post-process volume | Manual exposure hoặc khóa min = max của eye adaptation; bloom vừa; vignette nhẹ |

### 8.2 Phơi sáng

Khóa phơi sáng trong giấc mơ. Nếu để tự động, khi camera nhìn vào bong bóng hay khoảng trống sáng, cả cảnh sẽ tối đi và quy tắc "khoảng trống sáng nhất" bị phá.

### 8.3 Âm thanh

- Bus môi trường qua submix có bộ lọc thông thấp (khoảng 2–4 kHz) và reverb đuôi dài, trộn theo `WorldBlend`.
- Âm thanh gameplay (đòn đánh, khoảng trống, ra hiệu) đi bus riêng, không qua bộ lọc, để luôn rõ.

## 9. Ngân sách hiệu năng (tạm)

Mục tiêu: giấc mơ tốn thêm tối đa 1,5 ms GPU so với ban ngày, ở 1080p trên GPU tầm RTX 3060.

| Hạng mục | Ngân sách |
|---|---|
| Vân gợn post-process | 0,25 ms |
| Bong bóng (translucent) | 0,3 ms |
| Bụi, cá sao, hạt vỡ | 0,25 ms |
| Decal mực | 0,2 ms |
| WPO đồ vật trôi | 0,1 ms |
| Height fog, volumetric fog | 0,3 ms |
| Dự phòng | 0,1 ms |

Đo bằng `stat gpu` và Unreal Insights trên sân có 3 Golem và 3 Xenon. Mỗi mức scalability có giá trị riêng cho số bong bóng, mật độ bụi, volumetric fog bật hay tắt.

## 10. Code trong repo

### 10.1 Lớp mới

| Lớp | Module | Vai trò |
|---|---|---|
| `UDreamWorldSubsystem` (`UWorldSubsystem`, có tick chỉ khi đang chuyển cảnh) | `GolemWorld` | Nghe `EGamePhase`, chạy chuyển cảnh, ghi `MPC_Dream`, đổi Data Layer, cập nhật nguồn bóng đêm |
| `UDreamVisualSettings` (`UDataAsset`, instance `DA_DreamVisualSettings`) | `GolemWorld` | Mọi giá trị mặc định ở mục 2, thời gian chuyển cảnh, tham chiếu Data Layer |
| `ADreamArenaVisualVolume` | `GolemWorld` | Đặt trên mỗi `ArenaVolume`; cung cấp tâm, bán kính, bộ hình ký ức cho bong bóng |
| `UDreamDarknessSourceComponent` | `GolemWorld` | Gắn lên Xenon và điểm loang ở rìa; đăng ký với subsystem |

Không thêm phụ thuộc mới giữa module: subsystem nghe sự kiện đổi pha qua hệ thống sự kiện có kiểu trong `GolemCore`, không gọi trực tiếp `GolemFramework`.

### 10.2 Liên kết với cái đã có

- **Giảm chuyển động:** `UGolemGameUserSettings` (trước R0 là `UKnightGameUserSettings`) đã có tùy chọn giảm chuyển động và rung. Subsystem đọc tùy chọn này: `CausticsSpeed = 0`, `FloatAmount = 0`, ẩn cá sao, giảm tốc độ bụi.
- **Scalability:** đăng ký giá trị riêng cho số bong bóng và volumetric fog theo nhóm scalability hiện có.
- **Phase:** dùng `EGamePhase` và thời điểm đổi pha theo đồng hồ host như DEV_SPEC mục 2.

### 10.3 Validator trong `GolemDeveloper`

- Không actor nào có `bDreamFloat` nằm trong 70% bán kính tâm sân.
- Mọi đèn có lửa thuộc `DL_Real`; mọi đèn có đốm sao thuộc `DL_Dream`.
- Mọi actor nhân vật và Xenon có custom stencil đúng bảng ở mục 4.3.
- Mọi mesh nhà cửa trong khu có vertex color cho `trim_mask` và `age_mask` (cảnh báo, không chặn).

### 10.4 Test tự động

- Đổi pha `Day → DreamGate → DreamHall` thì `WorldBlend` đi 0 → 1 trong đúng thời gian cấu hình, Data Layer `DL_Dream` bật.
- Bật giảm chuyển động thì `CausticsSpeed` và `FloatAmount` về 0.
- Thêm và bớt nguồn bóng đêm thì `DarknessCount` đúng, không vượt 8.
- Chụp ảnh render ở hai pha để so thủ công (theo kiểu rendered test đã có trong repo).

## 11. Thứ tự làm

Hình ảnh giấc mơ thuộc mốc M2 trong `MILESTONES.md`. Phần lớn có thể làm song song từ bây giờ, vì M0 và M1 không cần art.

| Bước | Việc | Ai | Ở đâu | Xong khi |
|---|---|---|---|---|
| A1 | Dựng một góc quảng trường bằng asset hiện có, đặt hai bộ ánh sáng | Generalist | Project review `Assets/_render` | Chụp được cùng góc ở hai thế giới |
| A2 | `M_PP_DreamCaustics` và stencil | Generalist | Như trên | Qua bài thử T2 trong art define |
| A3 | `NS_MemoryBubble`, `M_MemoryBubble`, 4 hình ký ức đầu | Generalist, 2D artist | Như trên | Bong bóng đọc được ở 25 m, không lấn khoảng trống |
| A4 | Bụi, cá sao, đồ vật trôi bằng WPO | Generalist, animator cho cá | Như trên | Chơi thử 20 phút không chóng mặt |
| A5 | Xuất `trim_mask`, `age_mask` từ HDA; `MF_DreamSurface` | Generalist | Houdini, project review | Một dãy nhà có viền vàng và sạch trong mơ |
| A6 | Mực bóng đêm và hàm mặt nạ chung | Generalist | Project review | Vân, bong bóng, đồ trôi phản ứng đúng mép mực |
| C1 | `UDreamWorldSubsystem`, settings, MPC, Data Layer | Dev (Claude, Codex) | Project game | Test 10.4 PASS |
| C2 | Chuyển cảnh cổng mơ | Dev | Project game | Không có khung hình nhòe khi đổi bộ ánh sáng |
| C3 | Port A1–A6 sang khu chợ của Thành Golem | Generalist, dev | Project game | Bài thử T1–T6 trong art define |

## 12. Từ hai bản preview vừa rồi

Giữ:

- Khoảng trống trên thân Xenon nổi nhất khung hình, cả trong nhà lẫn ngoài trời.
- Bong bóng chứa hình trận chiến xưa, cá sao trên cao, đồ vật trôi ở rìa: đọc ngay là giấc mơ.
- Viền vàng nhẹ trên nhà và đồ đạc.

Chỉnh khi làm thật:

- **Vân gợn trên mặt sân quá gắt.** Trong ảnh, tâm sân là vùng rối nhất. Đây là lý do có `ArenaCausticsFloor` = 0,4.
- **Cả cảnh một tông xanh mòng két.** Đá và gỗ cần giữ màu gốc nhiều hơn, chỉ nghiêng lạnh nhẹ.
- **Vân gợn chạy lên nhân vật** trong vài vùng của ảnh; stencil ở mục 4.3 loại bỏ điều này.
- **Ký hiệu trong khoảng trống:** một ảnh vẽ hình tròn đặc, ảnh kia vẽ vòng tròn rỗng. Cần chốt một kiểu cho ký hiệu Sky.
