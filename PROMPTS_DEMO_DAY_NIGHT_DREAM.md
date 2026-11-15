# Bộ prompt: demo shader và VFX ban ngày, ban đêm, giấc mơ

Cập nhật 2026-11-15. Bộ prompt để giao cho một agent chạy trên máy có Houdini và Unreal Engine (Codex hoặc Claude Code), dựng demo ba trạng thái hình ảnh của Three Dream Golems trong một project Unreal mặc định có sẵn asset stylized.

## Cách dùng

1. Đặt hai file tài liệu vào thư mục `Docs/` của project demo: `art_define_HSR_bong_bong_ky_uc.md` và `TECH_DREAM_VISUALS.md`.
2. Thay các chỗ `<...>` trong Prompt 0 bằng giá trị thật.
3. Mỗi phiên làm việc: dán Prompt 0 trước, rồi dán đúng một prompt bước. Chỉ sang bước sau khi bước trước đã có báo cáo và bạn đã xem kết quả.
4. Sau mỗi bước, agent ghi báo cáo vào `Docs/DemoReports/`. Đọc báo cáo trước khi chạy bước tiếp theo.

| Bước | Nội dung | Ai xem kết quả |
|---|---|---|
| 1 | Kiểm tra project, lập kế hoạch, chưa sửa gì | Bạn |
| 2 | Houdini bake texture và mesh cá | Bạn hoặc generalist |
| 3 | MPC, material bầu trời và vân gợn | Generalist |
| 4 | Ba bộ ánh sáng và bộ chuyển trạng thái | Generalist |
| 5 | VFX giấc mơ: cá, bong bóng, bụi, đồ vật trôi, mực | Generalist |
| 6 | Hoàn thiện ban đêm ngoài đời | Generalist |
| 7 | Chụp ảnh, đo hiệu năng, bài thử | Cả team |
| 8 | Tùy chọn: hiệu ứng cổng mơ | Generalist |

## Giới hạn cần biết trước

- **Niagara khó dựng bằng script.** Agent có thể tạo material, actor, Blueprint đơn giản và đặt tham số qua Unreal Python, nhưng tự vẽ graph Niagara thì không đáng tin. Vì vậy bản demo làm cá, bong bóng và bụi bằng instanced static mesh có world position offset trong material. Cách này chạy được, rẻ, và đủ để xem thẩm mỹ. Bản sản xuất vẫn dùng Niagara như `TECH_DREAM_VISUALS.md`.
- **Viền vàng và nhà mới tinh** cần sửa master material của gói asset. Bản demo để phần này ở dạng tùy chọn trong Bước 5, làm trên bản sao material, không đụng bản gốc.
- **Copernicus và OpenGL:** repo chính từng ghi nhận crash driver AMD khi chạy chung một tiến trình. Agent phải chạy mỗi lần bake Houdini trong một tiến trình hython riêng.

---

## Prompt 0. Bối cảnh và quy tắc (dán đầu mỗi phiên)

```
Bạn là technical artist làm việc trực tiếp trên máy này, với Unreal Engine và Houdini.

BỐI CẢNH
- Game: Three Dream Golems, co-op hành động, góc nhìn thứ ba, đồ họa 3D stylized kiểu anime.
- Mục tiêu của project này: một DEMO hình ảnh, không phải game. Dựng một góc phố/quảng trường bằng asset stylized có sẵn, và cho xem ba trạng thái trên CÙNG MỘT bối cảnh:
  1. DAY: thế giới thật ban ngày, nắng tự nhiên.
  2. NIGHT: thế giới thật ban đêm trong một sự kiện lễ hội: trăng, đèn lửa ấm, cửa sổ sáng vàng.
  3. DREAM: giấc mơ theo art define "Bong bóng ký ức": thành phố thời hoàng kim chìm trong biển bột vũ trụ, ánh sáng xanh mòng két từ trên cao, vân gợn như đáy hồ, bong bóng ký ức, cá sao trên trời, đồ vật trôi ở rìa, không có lửa, không có mặt trời và mặt trăng, mực bóng đêm ở một góc.
- Yêu cầu quan trọng nhất: NIGHT và DREAM phải phân biệt được ngay, kể cả khi thu ảnh còn 320 px và chuyển sang thang xám.
- Tài liệu bắt buộc đọc trước khi làm: Docs/art_define_HSR_bong_bong_ky_uc.md và Docs/TECH_DREAM_VISUALS.md. Khi hai tài liệu khác với prompt này, theo prompt này cho phần demo.

MÔI TRƯỜNG
- Project Unreal: <ĐƯỜNG_DẪN_FILE_UPROJECT>
- Phiên bản Unreal: <PHIÊN_BẢN_UE>
- Houdini: <ĐƯỜNG_DẪN_HYTHON>, phiên bản <PHIÊN_BẢN_HOUDINI>
- Gói asset stylized: <TÊN_GÓI_VÀ_THƯ_MỤC_TRONG_CONTENT>
- GPU: <TÊN_GPU>

QUY TẮC
1. Không sửa asset gốc của gói asset. Cần sửa thì nhân bản vào /Game/DreamDemo/ rồi sửa bản sao.
2. Mọi thứ mới đặt trong /Game/DreamDemo/ (asset Unreal) và RawContent/DreamDemo/ (file nguồn từ Houdini).
3. Tự động hóa Unreal bằng Unreal Python (PythonScriptPlugin, Editor Scripting Utilities) chạy qua UnrealEditor-Cmd hoặc trong editor. Lưu mọi script vào Scripts/DreamDemo/ để chạy lại được.
4. Mỗi lần bake Houdini chạy trong một tiến trình hython riêng. Không chạy Copernicus và OpenGL trong cùng một tiến trình.
5. Mọi giá trị chỉnh được (màu, cường độ, tốc độ) phải là tham số trong Material Parameter Collection MPC_Dream hoặc material instance, không viết cứng.
6. Không tạo file .uasset hoặc .umap bằng cách ghi text. Chỉ tạo qua API của Unreal.
7. Nếu project là git repo: làm trên nhánh demo/dream-visuals, commit sau mỗi bước, không push. Nếu không phải git repo: báo cho tôi trước khi sửa.
8. Không ghi PASS cho thứ chưa chạy thật. Thứ gì chưa kiểm tra được thì ghi rõ là chưa kiểm tra.
9. Cuối mỗi bước, ghi báo cáo vào Docs/DemoReports/STEP_<số>.md gồm: đã làm gì, file và asset đã tạo, lệnh đã chạy, kết quả kiểm tra, việc còn lại, và những chỗ cần con người làm tay.
10. Nếu gặp việc cần quyết định về thẩm mỹ hoặc phải đổi kế hoạch, dừng lại và hỏi tôi.
```

---

## Prompt 1. Kiểm tra project và lập kế hoạch

```
BƯỚC 1: KIỂM TRA VÀ LẬP KẾ HOẠCH. Không sửa project trong bước này.

1. Xác nhận phiên bản Unreal, Houdini, các plugin đang bật (Python, Editor Scripting, Niagara, Houdini Engine nếu có), và thiết lập render: Lumen, Virtual Shadow Maps, Nanite, custom depth-stencil (r.CustomDepth).
2. Liệt kê asset trong gói stylized theo nhóm: nhà cửa và tường, đường lát và mặt đất, cầu, đài phun hoặc vật trang trí trung tâm, sạp hàng, thùng, giỏ, xe đẩy, ghế, đèn đường, cây và hoa, cờ, nhân vật. Ghi rõ asset nào có sẵn material emissive cho cửa sổ hoặc đèn.
3. Liệt kê các map có sẵn. Đề xuất: dùng lại một map có sẵn hay dựng map mới /Game/DreamDemo/Maps/L_DreamDemo. Bối cảnh mục tiêu: một quảng trường nhỏ khoảng 40 × 40 m, có một mặt nước (kênh hoặc đài phun), một cây lớn, nhà hai bên, sạp hàng, và một công trình cao ở xa làm điểm nhấn.
4. Xem nhân vật có sẵn (Mannequin hoặc nhân vật của gói). Đề xuất dùng ba nhân vật đứng yên làm đại diện cho ba Golem, và một mesh lớn màu xám đen làm đại diện cho Xenon.
5. Kiểm tra hython chạy được, và Copernicus có sẵn trong phiên bản Houdini này không.
6. Viết kế hoạch chi tiết cho các bước 2 đến 8 dựa trên asset thật có trong project: tên asset sẽ dùng, vị trí đặt, những rủi ro.

Báo cáo: Docs/DemoReports/STEP_1.md. Dừng lại chờ tôi duyệt kế hoạch.
```

---

## Prompt 2. Houdini bake

```
BƯỚC 2: HOUDINI BAKE TEXTURE VÀ MESH.

Viết script hython trong Scripts/DreamDemo/houdini/, mỗi asset một script, mỗi script chạy trong một tiến trình riêng. Xuất vào RawContent/DreamDemo/. Mỗi script có tham số ở đầu file (độ phân giải, seed, màu) để chạy lại với giá trị khác.

Cần xuất:

1. T_Caustics: texture vân gợn liền mạch theo cả hai chiều, 1024 × 1024, một kênh, PNG 8-bit. Vân sáng mảnh, mạng lưới kiểu đáy hồ. Kiểm tra tính liền mạch bằng cách ghép 2 × 2 và lưu ảnh kiểm tra.
2. T_DreamNebula: ảnh toàn cảnh 360 độ kiểu equirectangular, 4096 × 2048, EXR 16-bit half. Tinh vân xanh mòng két (#2E5F7A, #7FD6D0) đến chàm (#1F2A5A, #0E1230), có nhiều tầng mây khí ở các khoảng cách khác nhau, có dải bụi tối, sợi mảnh ở mép. Nửa dưới của ảnh (dưới đường chân trời) tối dần về chàm đặc. Không có mặt trời hay mặt trăng. Ưu tiên volume nhiễu render bằng Karma; nếu không khả thi, dựng 2D bằng Copernicus.
3. T_AuroraStrip: dải cực quang 2048 × 256, một kênh, liền mạch theo chiều ngang, có sọc dọc mảnh và nếp gấp, mép trên mềm, mép dưới sáng hơn.
4. T_AuroraFlow: flow map 512 × 512, hai kênh RG, PNG 8-bit, hướng trôi chủ đạo theo chiều ngang có xoáy nhẹ. Ghi chú: phải nhập vào Unreal với sRGB tắt.
5. T_StarField: 1024 × 1024, một kênh, liền mạch, sao nhiều kích cỡ, đa số rất nhỏ.
6. T_ThinFilmRamp: 256 × 8, màu cầu vồng nhạt kiểu màng xà phòng, dùng cho viền bong bóng.
7. T_MemoryPlaceholder_0..3: 4 ảnh 512 × 512 tạm cho hình ký ức trong bong bóng: bóng đen của vài nhân vật cầm cờ và vũ khí trên nền vàng nâu, mép mờ tròn. Đây là ảnh tạm, 2D artist sẽ thay sau.
8. SM_StarFish: mesh cá đơn giản 150 đến 400 tam giác, dài khoảng 60 cm, trục dài theo X, đầu hướng +X. UV0.x chạy từ 0 ở đầu đến 1 ở đuôi (dùng làm mặt nạ uốn thân). Vertex color R = 1 ở viền vây và đuôi (vùng phát sáng), 0 ở thân. Xuất FBX.

Sau khi xuất: tạo một trang Docs/DemoReports/STEP_2_preview.png ghép hình thu nhỏ của mọi texture để tôi xem nhanh.

Báo cáo: Docs/DemoReports/STEP_2.md, kèm thời gian bake của từng asset và lỗi nếu có.
```

---

## Prompt 3. MPC, material bầu trời và vân gợn

```
BƯỚC 3: NHẬP ASSET, MPC, MATERIAL BẦU TRỜI VÀ VÂN GỢN.

1. Nhập các file ở Bước 2 vào /Game/DreamDemo/Textures và /Game/DreamDemo/Meshes với thiết lập đúng: T_DreamNebula thành TextureCube (HDR, BC6H); T_AuroraFlow tắt sRGB; texture một kênh dùng nén Grayscale hoặc BC4; T_MemoryPlaceholder gộp thành Texture2DArray T_MemoryArray.

2. Tạo /Game/DreamDemo/MPC_Dream với các tham số (tên, mặc định):
   WorldBlend 0; NightBlend 0; CausticsIntensity 0.6; CausticsSpeed 0.15; CausticsScale 350; ArenaCenterRadius (x,y,z,1500); ArenaCausticsFloor 0.4; FloatAmount 1; AuroraIntensity 0.6; AuroraSpeed 0.05; AuroraHueShift 0; DarknessSource0..3 (xyz, w = bán kính); DarknessCount 0; DarknessEdgeNoise 0.3.
   ArenaCenterRadius đặt đúng tâm quảng trường demo.

3. Tạo M_DreamSky (unlit, Is Sky bật) gồm ba lớp: cubemap T_DreamNebula; T_AuroraStrip ánh xạ theo góc phương vị và độ cao, trôi bằng T_AuroraFlow với tốc độ AuroraSpeed, nhân AuroraIntensity; T_StarField lát lặp, lấp lánh bằng nhiễu theo ô. Có static switch hoặc tham số chất lượng để tắt lớp cực quang và lấp lánh ở mức thấp. Độ sáng tổng của bầu trời giữ thấp: không vượt độ sáng của một ngọn đèn đường.

4. Tạo M_PP_DreamCaustics (post-process, Before Tonemapping) theo TECH_DREAM_VISUALS.md mục 4.1:
   dựng lại vị trí thế giới từ SceneDepth; chiếu T_Caustics từ trên xuống theo XY, hai lớp trôi ngược hướng, lấy min; nhân hệ số pháp tuyến (mặt đất 1, tường 0.35); nhân hệ số tâm sân lerp(ArenaCausticsFloor, 1, smoothstep(0.4R, R, khoảng cách)); nhân 0 trong vùng bóng đêm; loại các pixel có custom stencil khác 0; cộng vào SceneColor dưới dạng SceneColor + Caustics * CausticsIntensity * DiffuseColor * màu #7FD6D0; nhân toàn bộ với WorldBlend.
   Viết hàm mặt nạ bóng đêm thành material function MF_DreamDarknessMask (hợp các vòng tròn từ DarknessSource, mép biến dạng bằng nhiễu) để Bước 5 dùng lại.

5. Bật custom depth-stencil trong cấu hình project nếu chưa bật. Đặt custom stencil 1 cho ba nhân vật đại diện Golem, 2 cho mesh đại diện Xenon.

6. Kiểm tra: mở map demo, đặt tạm WorldBlend = 1, chụp một ảnh màn hình có bầu trời và một ảnh nhìn xuống mặt đất. Lưu vào Docs/DemoReports/STEP_3_*.png.

Báo cáo: Docs/DemoReports/STEP_3.md.
```

---

## Prompt 4. Ba bộ ánh sáng và bộ chuyển trạng thái

```
BƯỚC 4: BA BỘ ÁNH SÁNG VÀ BỘ CHUYỂN TRẠNG THÁI.

1. Dựng bối cảnh quảng trường theo kế hoạch đã duyệt ở Bước 1, nếu chưa có.

2. Tạo ba nhóm actor ánh sáng, mỗi nhóm trong một folder Outliner riêng (hoặc sublevel riêng nếu map không dùng World Partition; dùng Data Layer nếu có World Partition): Lighting_Day, Lighting_Night, Lighting_Dream. Mỗi nhóm có directional light, sky light, sky sphere hoặc sky atmosphere, exponential height fog, post-process volume.

   Lighting_Day: nắng buổi sáng ấm, trời xanh có mây, SkyAtmosphere, bóng đổ rõ, exposure tự động trong khoảng hẹp.

   Lighting_Night: trăng tròn là directional light màu trắng bạc cường độ thấp, trời đêm xanh đậm có sao và trăng (dùng sky sphere riêng, KHÔNG dùng M_DreamSky), đèn đường và cửa sổ phát ánh vàng cam ấm (point light và emissive), bóng tối thật, sương xanh đen nhẹ. Không có vân gợn, không bong bóng, không cá.

   Lighting_Dream: theo TECH_DREAM_VISUALS.md mục 8.1: directional light pitch khoảng -80, màu #7FD6D0, cường độ khoảng 30% ban ngày, bóng mềm; sky sphere dùng M_DreamSky; sky light dùng cubemap T_DreamNebula làm nguồn chỉ định, không chụp lúc chạy; height fog màu #2E5F7A; post-process volume có M_PP_DreamCaustics, exposure khóa (min = max), bloom vừa phải. Tắt SkyAtmosphere.

3. Tạo Blueprint BP_WorldStateController đặt trong map:
   - Phím 1 = Day, 2 = Night, 3 = Dream.
   - Khi đổi trạng thái: ẩn và bật đúng nhóm ánh sáng và nhóm actor của trạng thái đó; đặt WorldBlend (Dream = 1, còn lại 0) và NightBlend (Night = 1, còn lại 0) trong MPC_Dream; bật tắt ngọn lửa và đốm sao của đèn (xem Bước 6); recapture sky light nếu cần.
   - Đổi tức thì trong bước này. Hiệu ứng chuyển cảnh làm ở Bước 8.
   - Có biến cho trạng thái khởi đầu.

4. Tạo ba camera cố định để chụp ảnh so sánh: CAM_Wide (góc người chơi, sau lưng ba nhân vật khoảng 4 m, cao 2 m, thấy cả trời), CAM_Wall (cận cảnh một bức tường có cửa sổ và mặt đất, không thấy trời), CAM_Sky (nhìn lên công trình cao và bầu trời). Phím F1, F2, F3 chuyển camera.

5. Kiểm tra: chụp CAM_Wide ở cả ba trạng thái, lưu Docs/DemoReports/STEP_4_{day,night,dream}.png.

Báo cáo: Docs/DemoReports/STEP_4.md.
```

---

## Prompt 5. VFX giấc mơ

```
BƯỚC 5: VFX GIẤC MƠ. Làm bằng instanced static mesh và world position offset trong material, không dựng graph Niagara. Mọi actor của bước này thuộc nhóm Dream và chỉ hiện ở trạng thái Dream.

1. Cá sao:
   - M_StarFish: opaque, unlit, emissive màu #7FD6D0 nhân vertex color R cộng fresnel nhẹ; độ sáng thấp hơn khoảng trống trên Xenon.
   - Bơi tại chỗ: WPO sóng sin dọc thân theo UV0.x, biên độ tăng dần về đuôi, pha ngẫu nhiên theo PerInstanceRandom.
   - Bay theo đàn: BP_StarFishSchool dùng InstancedStaticMeshComponent, 8 con một đàn. Chuyển động quỹ đạo tính trong WPO của material (vòng tròn quanh tâm quảng trường, bán kính và độ cao theo tham số, mỗi con lệch vị trí cố định cộng nhiễu nhẹ), hướng mũi cá theo tiếp tuyến quỹ đạo. Không Tick.
   - Đặt 3 đàn ở độ cao 8 đến 15 m. Tắt cast shadow, tắt Affect Distance Field Lighting, tắt hiển thị trong ray tracing. Bounds cố định đủ bao quỹ đạo.

2. Bong bóng ký ức:
   - M_MemoryBubble: translucent, lighting mode Surface TranslucencyVolume, render after DOF. Viền fresnel mũ khoảng 3; ánh cầu vồng tra T_ThinFilmRamp theo fresnel cộng nhiễu trôi; hình ký ức lấy từ T_MemoryArray với chỉ số theo PerInstanceRandom, chiếu theo hướng camera, mặt nạ tròn mềm. Lõi trong suốt 0.15 đến 0.25, viền 0.6. Emissive thấp.
   - BP_MemoryBubbles: ISM của mesh cầu, 10 bong bóng kích thước 0.5 đến 2 m, đặt ở vành quảng trường, KHÔNG đặt trong 70% bán kính tâm. Bay lên chậm và lắc ngang bằng WPO, lặp lại từ dưới khi lên quá độ cao tối đa (dùng frac theo thời gian cộng pha ngẫu nhiên).

3. Bụi vũ trụ: ISM của quad nhỏ quay theo camera, 300 đến 500 hạt trong thể tích quảng trường, material additive màu vàng trắng nhạt, bay lên chậm bằng WPO và lặp lại, mờ dần trong 3 m quanh camera.

4. Đồ vật trôi:
   - Nhân bản material của 4 đến 6 đồ vật nhỏ (thùng, giỏ, ghế, bánh xe) thành MI_*_Float, thêm WPO: nâng lên FloatAmount * (15 đến 40 cm), nhấp nhô chu kỳ 4 đến 7 giây, nghiêng tối đa 6 độ, pha theo ObjectPosition, nhân WorldBlend, và nhân (1 - MF_DreamDarknessMask tại ObjectPosition).
   - Đặt bản sao các đồ vật này ở rìa quảng trường, collision NoCollision, chỉ thuộc nhóm Dream. Đặt chúng ở vị trí nghỉ trên mặt đất; WPO chỉ nâng lên.

5. Mực bóng đêm:
   - M_Decal_DreamInk: deferred decal, base color #0A0B1A, roughness 0.05, mặt nạ từ MF_DreamDarknessMask, mép trôi chậm.
   - Đặt một decal phủ quảng trường. Đặt DarknessSource0 ở một góc quảng trường, bán kính khoảng 4 m, DarknessCount = 1.
   - Kiểm tra vân gợn tắt và đồ vật trôi hạ xuống bên trong vùng mực.

6. Đèn trong giấc mơ: cho mỗi đèn đường, thêm một mesh đốm sao nhỏ phát sáng trắng xanh, chỉ hiện ở Dream; ngọn lửa và point light ấm của đèn chỉ hiện ở Day và Night.

7. TÙY CHỌN, chỉ làm nếu tôi xác nhận: viền vàng cho nhà cửa. Nhân bản master material của nhà trong gói asset, thêm nhánh theo WorldBlend: tô vàng kim #D9B25F (metallic 1, roughness 0.3) ở các mép lồi dựa trên bản đồ độ cong hoặc vertex color, và giảm bẩn, rêu. Báo cáo trước khi làm: gói asset có texture hoặc kênh nào dùng được làm mặt nạ không.

8. Kiểm tra: chụp CAM_Wide, CAM_Wall, CAM_Sky ở trạng thái Dream; lưu Docs/DemoReports/STEP_5_*.png. Đo stat gpu ở CAM_Wide trong 10 giây, ghi số trung bình.

Báo cáo: Docs/DemoReports/STEP_5.md.
```

---

## Prompt 6. Hoàn thiện ban đêm ngoài đời

```
BƯỚC 6: HOÀN THIỆN TRẠNG THÁI NIGHT.

Mục tiêu: NIGHT đẹp và đúng không khí lễ hội, nhưng khác DREAM ở mọi điểm sau:
- Ánh sáng đến từ trên cao (trăng) và từ các nguồn ấm (đèn, cửa sổ). DREAM không có nguồn sáng ấm nào.
- Bóng tối thật, không có sao trong bóng, không có vân gợn.
- Trời có trăng và mây. DREAM không có trăng.
- Không có vật gì trôi, không bong bóng, không cá.

Việc cần làm:
1. Cửa sổ: dùng material emissive sẵn có của gói asset nếu có; nếu không, nhân bản material kính và thêm emissive vàng cam theo NightBlend.
2. Đèn đường: point light ấm cường độ vừa, bán kính giới hạn, có lập lòe rất nhẹ.
3. Thêm vài dây đèn lễ hội hoặc đèn lồng nếu gói asset có.
4. Kiểm tra số lượng point light có bóng đổ; giới hạn để không vượt ngân sách.

Kiểm tra: chụp CAM_Wide, CAM_Wall, CAM_Sky ở NIGHT; lưu Docs/DemoReports/STEP_6_*.png.

Báo cáo: Docs/DemoReports/STEP_6.md.
```

---

## Prompt 7. Chụp ảnh, đo hiệu năng, bài thử

```
BƯỚC 7: CHỤP ẢNH SO SÁNH, ĐO HIỆU NĂNG, CHẠY BÀI THỬ.

1. Viết Scripts/DreamDemo/capture_all.py: với mỗi camera (CAM_Wide, CAM_Wall, CAM_Sky) và mỗi trạng thái (Day, Night, Dream), chụp ảnh 1920 × 1080 bằng high-res screenshot sau khi chờ 3 giây cho Lumen ổn định. Lưu Docs/DemoReports/captures/{camera}_{state}.png.

2. Tạo các trang ghép để so sánh:
   - compare_full.png: lưới 3 × 3 (hàng là camera, cột là trạng thái).
   - compare_320_gray.png: CAM_Wide và CAM_Wall của NIGHT và DREAM, thu còn 320 px chiều ngang, chuyển thang xám, đặt cạnh nhau, không có nhãn. Đây là ảnh cho bài thử "nói đúng thế giới trong một giây".

3. Đo hiệu năng ở CAM_Wide cho cả ba trạng thái: thời gian GPU trung bình trong 10 giây (stat gpu hoặc ProfileGPU), số draw call, và chi phí riêng của từng hiệu ứng giấc mơ bằng cách tắt lần lượt: vân gợn, bầu trời, cá, bong bóng, bụi, đồ vật trôi, mực. Ghi vào bảng.

4. Kiểm tra tự động những gì đo được bằng script:
   - Ở DREAM không có point light hay emissive màu ấm nào đang bật (hue trong khoảng 15 đến 60 độ và bão hòa trên 0.4).
   - Không có bong bóng hay đồ vật trôi nào trong 70% bán kính tâm quảng trường.
   - Các mesh có custom stencil khác 0 không nhận vân gợn (so pixel của nhân vật đại diện giữa WorldBlend = 0 và 1 khi chỉ bật vân gợn).

5. Ghi lại những gì cần con người đánh giá: độ đẹp, chóng mặt khi xem lâu, độ nổi của nhân vật và khoảng trống.

Báo cáo: Docs/DemoReports/STEP_7.md với bảng hiệu năng, kết quả từng kiểm tra, và đường dẫn các ảnh.
```

---

## Prompt 8. Tùy chọn: hiệu ứng cổng mơ

```
BƯỚC 8 (TÙY CHỌN): CHUYỂN CẢNH CỔNG MƠ.

Theo TECH_DREAM_VISUALS.md mục 3.2:
1. Phím G: chuyển từ Day hoặc Night sang Dream bằng hiệu ứng cổng mơ.
2. Một post-process material M_PP_DreamGate: vòng sáng xanh mòng két lan từ một điểm trên màn hình ra phủ kín trong 1.2 giây, mép có hạt bột sáng. Trong lúc này WorldBlend tăng 0 đến 0.5.
3. Khi phủ kín: đổi nhóm ánh sáng và nhóm actor sang Dream.
4. Chờ 3 khung hình, rồi mở dần trong 1.2 giây, WorldBlend tăng 0.5 đến 1.
5. Phím H: chuyển ngược về Day, nhanh hơn (0.6 giây mỗi nửa).
6. Kiểm tra: quay video ngắn bằng công cụ có sẵn hoặc chụp chuỗi ảnh mỗi 0.1 giây, xem có khung hình nào bị nhòe do Lumen chưa ổn định không.

Báo cáo: Docs/DemoReports/STEP_8.md.
```

---

## Khi xem kết quả, soi những điểm này

| Điểm | Đạt khi |
|---|---|
| Night và Dream ở 320 px thang xám | Nói đúng ngay, không do dự |
| CAM_Wall ở Dream (không có trời) | Vẫn nhận ra giấc mơ nhờ vân gợn và đồ vật trôi |
| Tâm quảng trường ở Dream | Yên hơn rìa; vân gợn không làm rối mặt đất |
| Nhân vật đại diện | Không có vân gợn chạy trên người; nổi hơn nền |
| Dream không có nguồn sáng ấm | Không một ngọn lửa hay cửa sổ vàng nào |
| Hiệu năng | Dream tốn thêm tối đa khoảng 1,5 ms GPU so với Day |
