# Art define H: "Bong bóng ký ức" (tham chiếu Honkai: Star Rail, Penacony)

Cập nhật 2026-11-13. Một trong hai phương án theo game tham chiếu. Phương án còn lại theo Genshin Impact nằm ở file riêng.

**Trạng thái:** phương án để dựng preview, chưa được duyệt. Mã màu và con số là điểm khởi đầu.

**Một câu:** giấc mơ là thành phố thời hoàng kim ngàn năm trước, chìm trong một biển vũ trụ: ánh sáng gợn như dưới nước, đồ vật lơ lửng trôi chậm, bong bóng ký ức phát sáng khắp nơi.

## 1. Tham chiếu

### Penacony làm gì

- Người chơi vào Dreamscape qua một bể mơ (Dreampool) trong khách sạn The Reverie.
- Dreamscape có kiến trúc theo chủ đề xa hoa và ánh đèn rực rỡ; tông chung là "giấc mơ dát vàng".
- Bản giấc mơ của khách sạn The Reverie mang cảm giác như đang ở dưới nước: đồ đạc trôi lơ lửng, đi được trên tường và mái, cá ảo bơi quanh, và ánh xanh của các bong bóng ký ức có mặt khắp nơi. Âm thanh bị kéo giãn và nghẹt như nghe qua nước.

### Lấy gì, bỏ gì

| Lấy | Bỏ |
|---|---|
| Cảm giác dưới nước: ánh sáng gợn, đồ vật trôi, âm thanh nghẹt | Đi trên tường và trần: phá luật combat và camera |
| Bong bóng ký ức phát sáng là dấu hiệu chính | Tông khách sạn xa hoa kiểu Mỹ thập niên 20 |
| Thành phố ở thời hoàng kim, dát vàng, mới tinh | Đèn neon và ánh đèn sân khấu |
| Cá ảo bơi trong không khí | |

Đổi cho hợp Three Dream Golems: biển ở đây là **biển bột vũ trụ**, cá là **cá sao**, bong bóng chứa **ký ức về trận chiến ngàn năm trước**.

## 2. Vì sao không lẫn với đêm trăng ngoài đời

| | Đêm trăng ngoài đời (nếu có sự kiện) | Giấc mơ |
|---|---|---|
| Nguồn sáng | Trăng từ trên cao, đèn lửa ấm | Ánh sáng xanh mòng két khuếch tán từ trên, gợn sóng trên mọi bề mặt |
| Trên mặt đất | Bóng đổ tĩnh | Vân sáng gợn như đáy hồ, chuyển động liên tục |
| Trong không khí | Trong trẻo | Bong bóng phát sáng, bột vũ trụ, cá sao |
| Đồ vật | Đứng yên | Đồ nhẹ trôi lơ lửng chậm |
| Màu chủ đạo | Xanh đêm và vàng cam của đèn | Xanh mòng két, chàm, vàng kim |

Ba dấu hiệu đầu đều chuyển động, nên ngay cả ảnh tĩnh thu nhỏ cũng khó nhầm, và khi chơi thì không thể nhầm.

## 3. Quy tắc hình ảnh của giấc mơ

### 3.1 Ánh sáng

- Không có mặt trời và mặt trăng. Nguồn sáng chính là ánh sáng khuếch tán từ trên cao, màu xanh mòng két nhạt `#7FD6D0`, như ánh sáng xuyên qua mặt nước.
- **Vân gợn (caustics):** một mẫu sáng gợn sóng chiếu lên mặt đất, tường và nhân vật, trôi chậm. Đây là dấu hiệu chính.
- Vùng sáng nhất của khung hình là các bong bóng và khoảng trống trên thân quái, không phải bầu trời.
- Bóng đổ mềm, màu chàm `#1F2A5A`, không bao giờ đen.

### 3.2 Bảng màu

| Vai trò | Màu | Mã |
|---|---|---|
| Ánh sáng chính | Xanh mòng két nhạt | `#7FD6D0` |
| Bóng | Chàm | `#1F2A5A` |
| Nền xa, sương | Xanh lục lam mờ | `#2E5F7A` |
| Trang trí thời hoàng kim | Vàng kim | `#D9B25F` |
| Đá và gỗ | Giữ màu gốc, nghiêng nhẹ về lạnh | |

Bảy màu gameplay (đỏ, vàng, xanh dương và ba màu trộn, trắng) giữ nguyên và phát sáng. Môi trường không dùng chúng ở bão hòa cao.

### 3.3 Thành phố thời hoàng kim

- Cùng hình khối với thành phố hiện tại, nhưng mới: không rêu, không vết nứt, không mái vá.
- Có thêm viền vàng kim ở mái, khung cửa, thành cầu và đài phun nước. Cờ và băng rôn mới, màu sâu.
- Không có dân. Sạp hàng có hàng nhưng không có người bán.

### 3.4 Những thứ chuyển động (dấu hiệu chính)

| Thành phần | Mô tả | Giới hạn |
|---|---|---|
| Bong bóng ký ức | Bong bóng trong suốt cỡ 0,5–2 m, ánh cầu vồng trượt trên bề mặt, trôi lên rất chậm. Bên trong mờ mờ một hình ảnh của trận chiến xưa | Tối đa 8–12 cái trong khung hình; không trôi vào giữa sân đấu |
| Bột vũ trụ | Hạt sáng nhỏ trôi lên như bọt khí | Mật độ thấp ở tầm mắt nhân vật |
| Cá sao | Đàn cá phát sáng nhạt bơi chậm trên cao, giữa các mái nhà | Không bay xuống dưới 4 m |
| Đồ vật trôi | Thùng, giỏ, bánh xe, ghế đẩu ở rìa sân lơ lửng và xoay chậm | Chỉ đồ không có va chạm với gameplay; mặt sân luôn đứng yên |
| Nước | Nước kênh và đài phun chảy chậm như trong mơ, phát sáng nhẹ | |

### 3.5 Rìa sân đấu

Ngoài sân, trọng lực yếu dần: đồ vật trôi nhiều hơn, các mảng mái nhà tách ra lơ lửng. Giới hạn sân được vẽ bằng hình ảnh, không cần tường vô hình lộ liễu. Mặt sân và mọi thứ người chơi đứng lên được không bao giờ trôi.

### 3.6 Bóng đêm của Xeros

Chất đen như mực, loang trên mặt đất từ rìa sân. Nơi mực lan tới, vân gợn tắt, bong bóng vỡ, đồ vật rơi xuống đất. Mất chuyển động là dấu hiệu bóng đêm đang thắng.

### 3.7 Xenon lấy lại màu

Màu lan ra toàn thân Xenon trong khoảng 1,5 giây, và các bong bóng quanh nó sáng lên. Thắng một sân thì bong bóng trong sân vỡ thành mưa hạt vàng kim.

### 3.8 Bên trong công trình

Giữ nguyên vân gợn và đồ vật trôi. Lò và đèn không có lửa, chỉ chứa một đốm sao. Cửa sổ nhìn ra biển bột vũ trụ.

## 4. Thế giới thật

Ánh sáng mặt trời tự nhiên, không có vân gợn, không có đồ vật trôi, có dân. Khu đang thua thì phai dần về xám nhạt và ấm. Nếu có sự kiện đêm trăng: đèn lửa ấm, trăng, bóng tối thật.

## 5. Cách làm trong Unreal

| Lớp | Cách làm | Chi phí |
|---|---|---|
| Vân gợn | Light function trên directional light, hoặc một decal phủ sân đấu với texture caustics trôi | Thấp |
| Ánh sáng | Sky light màu xanh mòng két, directional light yếu, tắt mặt trời | Thấp |
| Sương | Exponential height fog màu xanh lục lam, mật độ vừa | Thấp |
| Bong bóng | Niagara, vật liệu trong suốt có fresnel và màu cầu vồng | Vừa; giới hạn số lượng |
| Cá sao, bột vũ trụ | Niagara, sprite hoặc mesh nhỏ | Thấp |
| Đồ vật trôi | Bản sao không va chạm của đồ trang trí, chuyển động bằng world position offset | Thấp |
| Viền vàng kim | Biến thể vật liệu cho bản mơ của nhà cửa | Phải làm vật liệu riêng |
| Âm thanh | Bộ lọc thông thấp và reverb dài trên bus môi trường | Thấp |

Tham số đề xuất: `WorldBlend`, `CausticsIntensity`, `CausticsSpeed`, `CausticsScale`, `DreamFogColor`, `BubbleCount`, `FloatAmplitude`, `GildTrimAmount`, `DarknessAmount`.

## 6. Bài thử

| # | Bài thử | Đạt khi |
|---|---|---|
| T1 | Giấc mơ và đêm trăng ngoài đời, 320 px, thang xám | Nói đúng thế giới trong một giây |
| T2 | Cắt bỏ trời, quái và nhân vật | Vẫn nhận ra giấc mơ nhờ vân gợn và đồ vật trôi |
| T3 | Bảy màu khoảng trống trên Xenon, cách 25 m | Đọc được cả bảy; khoảng trống nổi hơn bong bóng |
| T4 | Vân gợn chạy trên thân ba Golem khi đánh nhau | Không làm rối hình dáng nhân vật; cân nhắc tắt vân gợn trên nhân vật |
| T5 | Chơi thử 20 phút | Không ai báo chóng mặt vì vân gợn hay đồ vật trôi |
| T6 | Chi phí GPU của bong bóng và vân gợn | Ghi lại để chỉnh số lượng |

Rủi ro chính là T5: chuyển động liên tục khắp màn hình có thể gây mệt. Cần một tùy chọn giảm chuyển động tắt đồ vật trôi và làm vân gợn đứng yên.

## 7. Prompt cho công cụ tạo ảnh

Viết bằng tiếng Anh vì công cụ tạo ảnh thường hiểu tiếng Anh tốt nhất. Cách dùng:

1. Tạo ảnh thế giới thật trước bằng prompt 7.1.
2. Đính kèm ảnh đó khi chạy prompt 7.2, 7.3, 7.5, để giữ nguyên góc máy và bố cục.
3. Nếu kết quả vẫn ra kiểu tranh vẽ, thêm câu: "This must look like a screenshot from a real-time 3D game engine, not an illustration."

### 7.1 Thế giới thật, ban ngày (ảnh gốc)

```
A real-time 3D in-engine game screenshot. Scene: a cozy medieval fantasy town square on a canal. On the left a stone arch bridge over a canal and half-timbered houses with flower boxes and ivy, in the middle-back a fairytale palace on a hill, on the right a three-tier stone fountain under a big leafy tree, market stalls with fruit, a wooden cart with barrels in the left foreground. Navy blue banners with a white anvil and star emblem. Clear morning sunlight, soft blue sky with a few clouds, warm natural colors, a few townsfolk stone golems walking in the background.

Characters: three small chibi stone golems seen from behind, about 1 m tall, blocky grey stone bodies with castle-rook shaped heads: on the left a golem in a yellow cape with a yellow circle emblem holding a bow; in the middle a golem in a red tabard with a red triangle emblem holding a stone hammer; on the right a golem in a blue cape with a wave emblem holding a crystal staff. A tiny fairy princess with lavender hair and a small gold crown floats beside them, leaving a faint sparkle trail.

Rendering: stylized anime cel-shaded characters with soft two-tone shading and a thin rim light, painterly but clean environment textures, physically plausible lighting, third-person camera about 4 m behind and 2 m above the characters, wide angle, 16:9.

Avoid: comic book or manga panel look, ink outlines on the environment, flat illustration, watercolor or paper texture, text, UI, logos, real-world flags, photorealism. Do not copy any character or location from an existing game.
```

### 7.2 Giấc mơ, quảng trường

```
Use the attached image as the layout reference. Keep the exact same camera angle, buildings, bridge, fountain, tree, market stalls and the three small golems and the fairy princess. Transform the scene into its dream version: the same town 1000 years ago in its golden age, submerged in a sea of cosmic dust, like being underwater in space.

Changes:
- No sun and no moon. The sky is a deep teal-to-indigo cosmic ocean with soft nebula light far above.
- Soft teal light comes from above as if through a water surface. Animated-looking water caustics ripple across the cobblestones, walls and fountain. Shadows are soft indigo, never black.
- Buildings look brand new and noble: no moss, no cracks, thin gold trims on roofs, window frames, the bridge railing and the fountain. Banners are new and deep navy with gold edges.
- Glowing translucent memory bubbles, 0.5 to 2 m wide, with iridescent rainbow sheen, drift slowly upward around the edges of the square; inside some bubbles faint silhouettes of an ancient battle. About ten bubbles, none in the middle of the square.
- A small school of softly glowing star-fish swims slowly between the rooftops.
- Tiny glowing cosmic dust motes rise like air bubbles.
- At the edges of the square, barrels, baskets, a cart wheel and a stool float a little above the ground and tilt slowly. The cobblestone floor where the characters stand stays solid and still.
- Canal and fountain water glows faintly and flows in slow motion.
- No townsfolk, no fire. Lanterns hold a small white-blue star instead of a flame.
- In the bottom right corner, a patch of glossy black ink-like darkness spreads over the ground; where it touches, the caustics and bubbles disappear.

Add: facing the three small golems, a hostile dark charcoal boulder golem about 3 times their height, with a glowing ring-shaped weak spot on its chest: the ring glows orange and contains a red triangle and a yellow circle. Further back, a second big golem that has been restored: pale warm stone with soft green and coral patches and small flowers, standing peacefully, with bubbles glowing brighter around it.

Rendering: real-time 3D in-engine game screenshot, stylized anime cel-shaded characters, clean environment, 16:9. The orange weak spot must be the brightest element in the image.

Avoid: comic or manga look, outlines on the environment, flat illustration, paper texture, purple-only color grading, horror, text, UI.
```

### 7.3 Giấc mơ, bên trong lò rèn

```
A real-time 3D in-engine game screenshot of the dream version of a medieval blacksmith workshop with a high wooden roof truss, a stone forge on the right, an anvil, a workbench with tongs and two mugs on the left, tools on the wall, an arched window on the left, a navy banner with a white anvil and star.

Dream rules: the workshop is submerged in a sea of cosmic dust. Teal light enters from above as if through a water surface and casts rippling caustics on the floor, walls and anvil. Part of the roof has drifted apart into floating tiles, showing a deep teal cosmic ocean outside. The window shows that same ocean with a school of glowing star-fish passing by. The forge holds no fire, only a cluster of small white-blue stars. Mugs, tongs and a stool float slightly and tilt slowly. Three to five glowing iridescent memory bubbles drift near the roof. The floor in the middle stays solid and still. Everything is new and clean with thin gold trims. A patch of black ink-like darkness spreads from the bottom right corner.

Characters: the same three small chibi stone golems seen from behind (yellow cape with circle emblem and bow; red tabard with triangle emblem and hammer; blue cape with wave emblem and crystal staff), the tiny fairy princess with lavender hair and a small gold crown, facing a hostile dark charcoal boulder golem about 3 times their height with an orange glowing ring weak spot containing a red triangle and a yellow circle. On the left, a restored pale stone golem with green and coral patches.

Rendering: stylized anime cel-shaded characters, soft two-tone shading, thin rim light, 16:9, third-person camera behind the golems. Avoid: comic look, outlines on the environment, flat illustration, horror, text, UI.
```

### 7.4 Ảnh đối chứng: đêm trăng ngoài đời

Dùng cho bài thử T1. Đính kèm ảnh 7.1.

```
Use the attached image as the layout reference with the exact same camera and buildings. Show the same real-world town square at night during a festival: a full moon and stars in a dark blue sky, warm orange lantern light and lit windows, real dark shadows, townsfolk golems walking. Nothing floats, no water caustics, no bubbles, no glowing fish. Real-time 3D in-engine game screenshot, stylized anime cel-shaded characters, 16:9. Avoid comic look, outlines, flat illustration, text, UI.
```

### 7.5 Ảnh cắt: không có trời và nhân vật

Dùng cho bài thử T2.

```
Use the attached dream image as the reference. Make a close-up crop of only a stone wall with a window and the cobblestone ground in front of it, in the same dream style: rippling teal caustics on the wall and ground, a barrel floating slightly above the ground, two small glowing dust motes, the window showing the teal cosmic ocean. No sky, no characters, no monsters. Real-time 3D in-engine game screenshot.
```
