# HDA Building Visual Designer

Đọc file này chỉ khi được yêu cầu concept, master UI hoặc image-only. Với code/geometry/dataset, dùng SKILL.md và references/implementation-contract.md làm nguồn quy tắc; các giới hạn concept bên dưới không áp dụng cho triển khai. Đường dẫn assets trong file này tính từ thư mục gốc skill. Không áp dụng số lượng bảy sheet ở đây cho bộ nghiệm thu 5–10 biến thể.

## Mục tiêu và đầu vào

Biến tham chiếu kiến trúc thành một hệ HDA có thể triển khai và bộ hình giúp người xem hiểu mỗi parameter thay đổi gì. Không phụ thuộc lịch sử hội thoại hoặc ảnh chưa được cung cấp.

Lấy **cách trình bày của bộ Gothic / Storybook Medieval** làm chuẩn: bộ module tách rời rõ ràng, một master có công trình và UI, các bảng so sánh thay đổi có kiểm soát, rồi nhiều công trình hoàn chỉnh từ cùng bộ kit. Hiểu đây là chuẩn bố cục và mức giải thích, không phải yêu cầu biến mọi reference thành kiến trúc Gothic hoặc Medieval. Bám kiến trúc, palette và công năng của đầu vào; dùng mục 7 làm ví dụ tự chứa. Không dùng IronForge làm baseline mặc định.

Nhận ảnh, URL bộ asset, tên phong cách, tên HDA và công năng nếu có. Nếu thiếu tên, đề xuất `<Theme>_Building.hda`. Sử dụng dữ liệu đã có; chỉ hỏi khi thông tin thiếu làm thay đổi đáng kể kết quả. Mặc định giải thích bằng tiếng Việt, dùng snake_case cho parameter và nhãn tiếng Anh ngắn trong hình để dễ đọc.

Mặc định thể hiện concept bằng **stylized PBR với cảm giác hand-painted** theo quy tắc vật liệu ở mục 4; chỉ chuyển sang photorealistic/semi-realistic khi người dùng yêu cầu. Tách tham chiếu kiến trúc (hình khối, motif) khỏi tham chiếu texture/render (cách xử lý bề mặt).

Chọn phạm vi theo yêu cầu:
- **Full demo:** báo cáo phong cách/module/params + **bảy bảng hình tổng cộng, đã gồm module kit và master**, theo mục 4. Có thể gộp thành sáu bảng khi hai nhóm nhỏ dùng chung một sheet mà vẫn dễ đọc; yêu cầu số lượng cụ thể của người dùng được ưu tiên.
- **Master only:** một hình tổng hợp; dùng cấu hình đã chốt, không tự tạo lại toàn bộ bộ hình.
- **Module/variation/interior only:** chỉ tạo phần được yêu cầu; nội thất dùng góc cutaway hoặc góc nhìn trong phòng theo template cung cấp.
- **Parameter/spec only:** đặc tả bằng bảng, không tự sinh ảnh.
- **Implementation:** chỉ triển khai khi người dùng yêu cầu và môi trường có công cụ phù hợp; không tuyên bố đã tạo file HDA từ việc tạo ảnh.

Khi người dùng chỉ yêu cầu sửa skill, cập nhật skill; không tự chạy full demo hoặc tạo lại ảnh.

## 1. Phân tích tham chiếu

Mở URL được cung cấp và xem ảnh nguồn khi có công cụ. Phân biệt dữ kiện nguồn, quan sát hình ảnh và quyết định thiết kế. Nếu không truy cập được, nói rõ giới hạn và dùng phần tham chiếu có sẵn; không giả vờ đã xem.

Rút ra: silhouette, tỷ lệ, footprint, nhịp kết cấu, kiểu mái, cửa, trụ, motif, vật liệu và tổ hợp công năng. Xác định 3–5 đặc điểm phải giữ xuyên suốt các biến thể. Với nhiều ảnh, kiểm tra chúng có thuộc cùng một họ kiến trúc hay không trước khi chia thành nhiều style/HDA. Tách reference kiến trúc, reference bố cục sheet/UI và reference vật liệu; không chuyển motif giữa các vai trò này.

Chọn trọng tâm phù hợp: Gothic thường nhấn nhịp vòm, cửa, trụ chống, mái và tháp; Storybook Medieval thường nhấn footprint, tầng đua, khung gỗ, mái, ban công và thích ứng địa hình. Chỉ dùng lò, ống công nghiệp, máy và crest người lùn khi đầu vào thực sự yêu cầu. Với IronForge được yêu cầu rõ, giữ kiến trúc IronForge nhưng dùng cùng format trình bày ở mục 4.

Với thông tin tích hợp Houdini/Unreal cụ thể, kiểm tra tài liệu SideFX hiện hành. Không suy diễn quyền sử dụng asset từ việc trang sản phẩm hiển thị ảnh.

## 2. Chốt hệ sinh trước khi tạo hình

Thiết kế một HDA cấp người dùng gồm các nhánh dựng footprint, facade, roof, assemblies, decoration, interior và outputs cùng đọc dữ liệu bố cục. Chia thành:
- Hình học procedural: mặt bằng, bề mặt mái, nền, đoạn nối.
- Module thư viện: tường, trụ, khung cửa, gờ, mesh trang trí.
- Tổ hợp có quy tắc: tháp, ban công, hiên, cầu thang, sàn lửng; máy/lò chỉ khi phù hợp công năng.
- Phần Unreal: vật liệu, tương tác, Niagara, animation và gameplay.

Ưu tiên thêm nhịp/module khi tăng kích thước. Không kéo giãn cửa, phù điêu và toàn bộ nhà để giả lập procedural.

Lập bảng parameter với cột: `parameter | UI label | type | default/range | visible when | tác động đầu ra | constraint`. Ghi rõ range là đề xuất, không phải giới hạn mặc định của Houdini Engine. Phân biệt parameter custom với điều khiển sẵn của plugin như recook/rebuild/bake; chỉ mô tả hành vi và vị trí chính xác sau khi kiểm tra phiên bản.

Tổ chức parameter theo các nhóm dưới đây; **nhóm UI không tương ứng bắt buộc một-một với bảng hình**. Ưu tiên các biến đổi kiến trúc nhìn thấy rõ, đưa nhóm chuyên biệt vào phần mở rộng có điều kiện.

| Nhóm | Parameter gợi ý | Cách chứng minh bằng hình |
|---|---|---|
| Presets | building_preset, apply_preset, layout_seed, variation_amount, symmetry_mode, preview_quality | 6 công năng có bố cục khác nhau từ cùng bộ kit |
| Massing & footprint | bay_width_m, bay_count_x/y, floor_count, floor_height_m, footprint_type, wing_length_bays, courtyard_width_m, upper_overhang_m, tower_radius_m, foundation_height_m | thay nhịp, tầng, Rect/L/U/Courtyard hoặc Round/Hybrid nếu có |
| Facade & openings | portal_profile, entrance_side, entrance_bay, entrance_span_bays, door_clear_width_m, door_clear_height_m, window_type, window_spacing_bays, opening_fill_ratio, buttress_type/depth_m, timber_pattern, shutter_type, trim_profile | cùng thân nhà, đổi cửa/vòm, trụ chống hoặc khung gỗ |
| Roof & towers | roof_type, roof_pitch_deg, roof_overhang_m, roof_tier_count, dormer_count, ridge_cap_type, tower_count, tower_height_m, tower_roof_type, spire_height_m | cùng footprint, đổi silhouette mái/tháp |
| Attachments & terrain | balcony_enable/length_bays, porch_type, stair_type, terrain_fit_mode, stilt_height_m, attachment_type, attachment_side, attachment_bay | thêm hiên/ban công/cầu thang, thích ứng nền dốc; cối xay khi phù hợp |
| Materials & condition | material_palette, wall_material, roof_material, timber_tint, weathering_preset, moss_amount, surface_wear, damage_state/region, rubble_density | palette, weathering và geometry damage tách riêng |
| Decoration — mở rộng | decoration_mode, ornament_set/density, crest_type, custom_crest_asset, crest_slot_group, crest_scale/offset_m, banner_enable/color, decoration_seed | đổi mesh cùng slot; thường gộp vào facade hoặc master |
| Interior — mở rộng | interior_mode/preset, interior_column_spacing_bays, mezzanine_enable/coverage, stair_width_m, clear_path_width_m, prop_fill_ratio | cutaway nội thất khi được yêu cầu |
| Functional assemblies — mở rộng theo đề tài | assembly_type, placement_side/bay, assembly_size; furnace_count, chimney_height_m, pipe_detail_level, emit_fx_sockets chỉ với công trình công nghiệp | bảng thêm khi là đặc trưng trọng yếu; không ép mọi nhà có máy/lò |

Giới hạn menu vào các tổ hợp thực sự hỗ trợ. Ẩn/disable parameter không áp dụng. Tách layout_seed, decoration_seed và weathering_seed khi cần. Định nghĩa rõ symmetry theo trục, floor index và bay index; không dùng từ Front/Full mơ hồ mà thiếu quy ước.

Lập bảng `module family | chức năng | kích thước/pivot/quy tắc ghép | params sử dụng | output` để nối module kit với đặc tả. Đặt một tên parameter chuẩn cho mỗi khái niệm và dùng thống nhất trong báo cáo, nhãn hình và master. Ký hiệu `x/y` hoặc `/` ở bảng gợi ý chỉ là cách viết gọn; tách thành tên snake_case cụ thể trong đặc tả cuối.

## 3. Áp dụng ràng buộc kiến trúc

- Đặt cửa, vùng đi và vùng thao tác trước props, cửa sổ và trụ phụ.
- Bỏ tường nội bộ ở giao khối; giải mái giao, valley, flashing và lỗ xuyên ống khói.
- Phân biệt U có một cạnh mở với Courtyard bốn cạnh khép kín. Chọn góc máy nhìn thấy cạnh mở.
- Chỉ đặt mái vòm trên khối đỡ hợp lệ, ví dụ sảnh vuông trung tâm.
- Dành đủ nhịp cho cổng lớn. Gatehouse phải có lối xuyên thật.
- Nối máy/lò với đường cấp, thao tác và thoát khí hợp lý; giữ vùng an toàn và lối tiếp cận.
- Ghép ban công/thang với cửa, sàn đúng cao độ và kết cấu đỡ.
- Dùng tỷ lệ cửa sổ/trang trí trên số slot hợp lệ sau khi loại trừ chướng ngại; không hiểu là thay kích thước cửa.
- Chuẩn hóa pivot, hướng, kích thước và giới hạn scale của mesh trang trí. Cho phép offset cục bộ theo slot.
- Random chỉ chọn module tương thích; giữ ID ổn định để đổi trang trí không xáo trộn layout.
- Tách weathering vật liệu với damage hình học; không hứa thay parameter vật liệu trong HDA sẽ tự tránh cook. Muốn chỉnh không cook, thiết kế đường chỉnh Material Instance trực tiếp ở Unreal.
- Xác định đơn vị mét cho đặc tả và kiểm tra chuyển đổi đơn vị khi tích hợp. Kiểm tra lối đi bằng capsule, camera và gameplay thực tế.

## 4. Tạo bộ hình biến thể

Đọc skill imagegen hiện có khi thực hiện tạo/chỉnh ảnh và dùng công cụ sinh ảnh. Không thay yêu cầu tạo ảnh bằng prompt thuần nếu có công cụ. Nếu thiếu công cụ, nói rõ và cung cấp prompt sẵn dùng; không báo đã tạo ảnh.

Chốt baseline và danh sách ảnh trước; thực hiện luôn trong phạm vi được yêu cầu, không biến bước này thành vòng xin duyệt bắt buộc. Gắn mỗi phép so sánh với baseline và các giá trị thay đổi. Giữ nguyên những yếu tố không nằm trong phép so sánh: camera, ánh sáng, vật liệu, seed và tỷ lệ hình. Khi so sánh vật liệu thì giữ hình học; khi so sánh seed thì giữ các controls khác. Mỗi hàng chỉ đổi một parameter hoặc một nhóm liên quan; preset được đổi nhiều giá trị nhưng phải ghi là preset.

### Bộ hình chuẩn Gothic / Storybook Medieval

| Sheet | Bố cục và nội dung bắt buộc |
|---|---|
| 01 — Modular Kit | Bảng module tách rời, nhóm rõ Foundation / Wall / Opening / Roof / Tower hoặc Attachment / Trim. Khoảng 12–20 module đủ lớn để đọc, nhãn ngắn; có một inset lắp ghép hoặc công trình nhỏ để thấy quan hệ module → building. Không thay sheet này bằng toàn nhà hoàn chỉnh. |
| 02 — Master Parameters | Một công trình hoàn chỉnh bên trái và UI bên phải, theo mục 5; đây chính là master của bộ, không tạo thêm bản trùng ở cuối. |
| 03 — Massing & Footprint | 6–9 mẫu theo các hàng so sánh nhịp, số tầng, mặt bằng. Giữ camera/zoom để nhà cao hoặc dài hơn thực sự nhìn lớn hơn; có inset top view nếu cần phân biệt Rect/L/U/Courtyard. |
| 04 — Facade & Openings | 6–9 mẫu cùng khối nền, làm rõ cửa, vòm, trụ, tường, khung gỗ hoặc shutters. Có thể dùng crop mặt đứng/module lớn để tránh chi tiết quá nhỏ. Gộp thử decoration slot ở đây nếu phù hợp. |
| 05 — Roof, Towers & Attachments | 6–9 mẫu, mỗi hàng đổi mái, tháp hoặc phần gắn. Với Medieval ưu tiên mái/ngói, tầng đua, ban công, hiên và nền dốc; với Gothic ưu tiên mái dốc, tháp chuông, chóp và trụ chống. |
| 06 — Materials & Condition | Các hàng riêng cho palette, weathering và damage, khoảng 6 mẫu. Hàng vật liệu giữ nguyên hình học; hàng damage thể hiện phần vỡ thật. Không đồng thời đổi trời, camera và cây để giả lập hiệu ứng parameter. |
| 07 — Building Presets | Lưới 2×3 gồm 6 công trình hoàn chỉnh khác công năng, tỷ lệ và tổ hợp; cùng ngôn ngữ kiến trúc và thư viện module. Ghi tên preset và 2–3 giá trị phân biệt, không chỉ đổi màu cùng một nhà. |

Ưu tiên nội dung kiến trúc của reference hơn việc lấp đủ ô. Có thể gộp sheet 04 và 05 thành một sheet khi rõ ràng, cho bộ sáu ảnh; không gộp module kit, master hoặc presets. Nếu attachments/terrain là trọng tâm, thay một phần nội dung sheet 05 hoặc thêm sheet khi người dùng cần chi tiết. Không mặc định tách riêng Industrial Assembly, Decoration Slots và Interior thành ba bảng cho mọi phong cách. Nội thất chi tiết là phần mở rộng theo yêu cầu, không tự thay thế exterior demo.

### Ngôn ngữ trình bày

- Dùng sheet ngang, nền trắng ngà/xám sáng, chữ tối đơn giản, khoảng trắng rộng và đường phân chia nhẹ. Dùng 3/4 orthographic-like cho module/biến thể, tránh phối cảnh mạnh che mất sự khác nhau.
- Ưu tiên công trình và module; để ảnh chiếm khoảng 70–80% diện tích các sheet so sánh. Nhãn mỗi ô chỉ gồm tên ngắn và giá trị như `floor_count = 1 / 2 / 3`, bảng văn bản giữ giải thích dài.
- Cho thấy khác biệt hình học đủ lớn; đừng dùng mũi tên hoặc chữ để thay cho khác biệt không xuất hiện trong hình. Không trình bày một bộ ảnh đẹp nhưng thiếu quan hệ parameter → kết quả.
- Với parameter sweep, giữ scale và vật liệu nhất quán; với preset, có thể đổi framing để đủ khung nhưng phải ghi công năng và có dấu hiệu tỷ lệ. Tránh địa hình, cây hoặc props lấn át asset.
- Dùng stylized PBR / hand-painted theo quy tắc bên dưới cho toàn bộ kit, master và biến thể. Không lấy nền tối, lava, khói, kim loại dày và máy móc làm nhận diện mặc định.
- Chỉ dùng panel UI đầy đủ trên master. Các sheet còn lại dùng lưới ảnh và nhãn; không nhồi bảng điều khiển vào từng ô.
- Khi có ảnh baseline đã được chấp nhận, dùng làm reference để giữ kiến trúc giữa các sheet. Phân định rõ ảnh tham chiếu hình khối với ảnh chỉ tham chiếu texture. Không hứa hình sinh là phép đo hình học chính xác.

Khung prompt cho sheet so sánh; thay hết biến trước khi gọi công cụ:

> Create an architectural HDA parameter comparison sheet for [HDA_NAME], titled [SHEET_TITLE]. Use the architecture and palette of [STYLE_GRAMMAR]. Arrange [ROW_PLAN] in a spacious landscape grid on a light neutral background. Baseline: [BASELINE]. Each row changes only [PARAMETER_AND_VALUES]; hold [INVARIANTS] fixed. Show these visible changes clearly: [OUTPUT_DIFFERENCES]. Consistent three-quarter orthographic-like camera, comparison scale and soft lighting. Large readable architectural renders, short exact English parameter labels, thin dividers, minimal props. Stylized 3D game materials with a hand-painted finish and restrained microdetail. No repeated software panels, no unrelated industrial machinery. Footer: CONCEPT • Proposed parameter effects.

Tạo ảnh thật bằng công cụ khi được yêu cầu, rồi xem và sửa những lỗi làm sai phép so sánh. Không dùng một ảnh overview duy nhất thay cả bộ full demo. Khói/lửa chỉ minh họa trạng thái hoàn thiện, không coi là geometry HDA.

### Texture và cách thể hiện stylized mặc định

- Dùng mảng màu lớn rõ ràng, gradient nhẹ có chọn lọc và biến thiên màu kiểu vẽ tay. Giảm micro-detail, hạt nhiễu và vân scan dày; giữ hình khối dễ đọc ở góc camera gameplay.
- Giữ cảm giác asset game 3D có chiều sâu: bevel/edge highlight mềm, chuyển sáng tối rộng, contact shadow/AO vừa phải. Không tự biến thành cel-shading, tranh phẳng hoặc low-poly faceted quá mức.
- Đá/tường: màu nền sạch, vài mảng sắc độ, đường ghép và vết sứt có chủ ý; tránh phủ vân marble photorealistic, lỗ rỗ hoặc grunge li ti lên toàn bộ bề mặt. Gỗ: chỉ giữ vài đường thớ lớn. Ngói: nhóm sắc độ ấm/lạnh và highlight cạnh, không nhiễu từng viên.
- Kim loại: giữ nhận diện vật liệu bằng vùng sáng rộng có kiểm soát; giảm phản xạ sắc như gương và độ bóng kiểu trang sức chụp sản phẩm. Không ép mọi vật liệu về cùng roughness; vàng vẫn đọc là kim loại, đá/gỗ/vữa thiên mờ. Ngọc/kính giữ màu và highlight đơn giản, hạn chế sparkle/caustics.
- Weathering dùng các mảng lớn đặt có chủ ý ở chân tường, khe và cạnh; không rải noise khắp mặt. Rêu/lá ưu tiên cụm màu dễ đọc nếu đã có trong cấu hình, không tự thêm cây hay geometry mới khi chỉ đổi texture.
- Giữ palette và motif của bộ kit; ví dụ Elysium vẫn ivory–gold–emerald. Không sao chép nhà gỗ, mái ngói đỏ, địa hình, viền chọn màu cam hay UI của ảnh style sang một đề tài khác.
- Khi người dùng chỉ yêu cầu đổi texture, giữ nguyên silhouette, topology, số tầng/nhịp, module, seed, camera, bố cục bảng, chữ và giá trị panel. Đổi chất liệu/shading; không âm thầm redesign hình học.
- Chỉ dùng `assets/stylized-material-reference.png` làm **tham chiếu xử lý bề mặt** khi cần đối chiếu trực quan; mở ảnh trước khi dùng. Các quy tắc văn bản ở đây phải đủ để thực hiện nếu ảnh không khả dụng. Nếu người dùng cung cấp style mới, ưu tiên ảnh/chỉ dẫn mới và không tuyên bố đã xem ảnh không đọc được.

Thêm vào prompt tạo/chỉnh ảnh: `Stylized 3D game materials with a hand-painted finish, broad clean color masses, subtle painterly gradients, simplified intentional texture marks, restrained microdetail, soft edge highlights, mostly matte stone/wood/plaster, controlled broad metallic highlights, coherent material separation and soft lighting. Preserve the source architecture and palette. Avoid photorealistic scanned textures, dense marble veining, noisy grunge, mirror-like gold and excessive sparkle.` Khi chỉnh ảnh, ghi rõ ảnh nào là edit target, ảnh nào chỉ là material-style reference.

## 5. Tạo hình master

Mặc định bố cục ngang: công trình khoảng 65–70% bên trái, panel UI khoảng 30–35% bên phải. Ưu tiên bố cục ảnh mẫu nếu người dùng cung cấp; không tuyên bố tái tạo chính xác mẫu chưa xem.

- Đặt tên `<Theme>_Building.hda` rõ ở đầu.
- Hiển thị công trình hoàn chỉnh góc 3/4, không crop mái, tháp hoặc nền. Giữ nền sáng trung tính, công trình có silhouette rõ như các bảng Gothic/Medieval.
- Dùng panel tối gợi Details Panel trong Unreal, folder, dropdown, checkbox và numeric field dễ đọc.
- Hiển thị khoảng 14–20 giá trị quan trọng trong các nhóm Preset / Massing / Facade / Roof / Attachments / Materials; nhóm phụ có thể thu gọn. Chỉ tăng số hàng khi vẫn dễ đọc. Để thông số đầy đủ trong bảng văn bản, không nhồi tất cả lên master.
- Chốt một configuration duy nhất. Số tầng/nhịp/tháp, kiểu mái, cửa và attachment trên ảnh phải khớp panel. Chỉ hiện số lò/máy nếu công trình thực sự có các phần đó.
- Chỉ dùng callout mảnh tại vị trí hữu ích; tránh phủ lên công trình.
- Phân biệt vùng custom parameters và vùng thao tác plugin. Có thể ghi Recook / Rebuild / Bake như concept thao tác, không khẳng định là screenshot UI thật.
- Ghi nhỏ: `CONCEPT • Proposed HDA output & UI`.

Dùng khung prompt sau, thay đầy đủ các biến trước khi gửi:

> Create one polished architectural HDA master concept sheet for [HDA_NAME]. Landscape composition, hero building occupying the left 68%, readable dark Unreal-inspired parameter panel on the right 32%. Architecture: [STYLE_GRAMMAR]. Show exactly this configuration: [CONFIGURATION]. Display only these exact parameter labels and values: [UI_ROWS]. Keep [INVARIANTS]. Make [VISIBLE_FEATURES] unmistakable and match all visible counts with the panel. Three-quarter view, full building visible, neutral background, soft lighting, stylized PBR game materials with a hand-painted finish, clean broad color masses, subtle painterly gradients, restrained texture noise and controlled soft highlights; preserve the theme palette. Hierarchical folder headings and restrained callouts. No unrelated extra buildings, no contradictory thumbnails, no photorealistic scanned surfaces, no fictitious claim of a working software screenshot. Small footer: CONCEPT • Proposed HDA output & UI.

Kiểm tra ảnh sau khi sinh nếu công cụ cho phép. Sửa lỗi số tầng/nhịp/tháp, mặt bằng U, vật thể trôi, mái không nối, chữ sai hoặc panel mâu thuẫn. Không tuyên bố đã kiểm tra ảnh nếu chưa xem được. Nếu chữ ảnh vẫn khó đọc, dùng bảng văn bản làm nguồn giá trị chuẩn và nêu giới hạn ngắn gọn.

## 6. Mô tả tích hợp và đầu ra

Đề xuất outputs theo nhu cầu: Structure, Roof, ModuleInstances, DecorationInstances, FunctionalAssemblies, Collision, GameplaySockets. Một HDA có thể tạo nhiều mesh và instances; không bắt buộc một mesh gộp.

Với custom asset picker, xác minh cách dùng string parameter tag `unreal_ref` và attribute `unreal_instance` theo tài liệu hiện hành. Phân biệt socket mesh với point data/actor placement; nêu cách tiêu thụ data thực tế thay vì hứa mọi điểm tự thành gameplay socket.

Để Niagara/Blueprint xử lý khói, lửa, chuyển động và tương tác. Phân biệt mesh hư hỏng lúc cook với destruction runtime. Mô tả authoring trong Unreal Editor → chỉnh parameters → cook → bake; giữ bản HDA authoring để tiếp tục sửa. Không hứa Houdini Engine cook trong bản game đóng gói nếu chưa có giải pháp được xác minh.

Nguồn ưu tiên khi cần kiểm chứng:
- https://www.sidefx.com/docs/houdini/unreal/parameters.html
- https://www.sidefx.com/docs/houdini/unreal/instancing.html
- https://www.sidefx.com/docs/houdini/unreal/materials.html
- https://www.sidefx.com/docs/houdini/unreal/intro.html
- Trang bộ KitBash3D hoặc nguồn kiến trúc người dùng đưa.

## 7. Hai mẫu chuẩn tự chứa

Dùng hai ví dụ này để hiểu mức độ module hóa, so sánh và đa dạng công năng. Chỉ dùng kiến trúc của ví dụ khi đúng reference; với đề tài mới hãy thay module/menu/baseline, giữ format mục 4. Các giá trị là đề xuất thiết kế, chưa được kiểm thử bằng HDA.

### Gothic_Building.hda

- Nhận diện: đá sáng, vòm/cửa nhọn, nhịp trụ đứng, mái đá phiến dốc và tháp/chóp theo công năng. Không mặc định mọi preset đều là nhà thờ.
- Module: stone foundation, wall bay, corner pier, pointed portal, lancet window, traceried window, buttress, gable, roof bay, tower floor, belfry, spire, cornice và ornament.
- Presets: Chapel, Townhouse, Library, GuildHall, Gatehouse, BellTower. Gatehouse dùng cổng xuyên; townhouse/library vẫn đọc là công trình dân dụng.
- Phép so sánh mẫu: `bay_count_x = 3/5/7`; `floor_count = 1/2/3` trên preset dân dụng; `window_type = Lancet/PairedLancet/Traceried`; `tower_count = 0/1/2` trên layout hỗ trợ; `roof_pitch_deg = 40/50/60`.
- Palette/condition: LimestoneSlate / WarmSandstone / DarkStone; Fresh / Aged / Mossy và damage trên hàng riêng.

```yaml
hda_name: Gothic_Building.hda
building_preset: GuildHall
layout_seed: 42
footprint_type: Rect
bay_width_m: 3.0
bay_count_x: 4
bay_count_y: 3
floor_count: 2
floor_height_m: 3.5
portal_profile: Pointed
entrance_span_bays: 1
window_type: PairedLancet
buttress_type: Engaged
roof_type: SteepGable
roof_pitch_deg: 50
tower_count: 1
tower_placement: RearRight
tower_height_m: 10.0
tower_roof_type: Spire
spire_height_m: 3.0
material_palette: LimestoneSlate
weathering_preset: Aged
damage_state: Intact
```

Quy ước ví dụ: `tower_height_m` là chiều cao thân tháp tính từ nền công trình, chưa gồm chóp; footprint 12 × 9 m là khối chính, chưa gồm tháp gắn thêm. Giữ cách đo này trong spec và hình.

### Storybook_Building.hda

- Nhận diện: chân đá, vữa kem/ochre, khung gỗ nâu, mái ngói đỏ hoặc màu theo reference, tỷ lệ ấm áp, tầng đua, tháp tròn hoặc khối lai. Không hiểu Medieval chỉ là nhà gỗ chữ nhật giống nhau.
- Module: stone base, plaster wall, timber wall bay, corner post, brace, door, window/shutter, roof strip, gable, curved tower wall, conical roof, dormer, balcony, porch, stairs và stilts.
- Presets: Cottage, Townhouse, Inn, ArtisanHouse, Watchtower, MillHouse. Chỉ dùng cối xay nước/gió nếu có module và vị trí hợp lệ.
- Phép so sánh mẫu: `footprint_type = Rect/L/Round/Hybrid`; `floor_count = 1/2/3`; `upper_overhang_m = 0/0.3/0.6`; `timber_pattern = Simple/Cross/Diagonal`; `roof_type = Gable/Hip` trên cùng thân nhà; Conical chỉ trên khối tròn/đa giác phù hợp.
- Dùng sheet 05 để so mái, ban công và địa hình Flat/Slope/Stilts theo từng hàng. Palette vữa/ngói và tuổi bề mặt nằm ở sheet 06.

```yaml
hda_name: Storybook_Building.hda
building_preset: Townhouse
layout_seed: 42
footprint_type: Rect
bay_width_m: 2.5
bay_count_x: 3
bay_count_y: 2
floor_count: 2
floor_height_m: 3.0
foundation_height_m: 0.6
upper_overhang_m: 0.3
timber_pattern: Cross
window_type: Shuttered
roof_type: Gable
roof_pitch_deg: 45
roof_overhang_m: 0.5
dormer_count: 1
balcony_enable: true
balcony_length_bays: 1
terrain_fit_mode: Flat
material_palette: CreamTimberTerracotta
weathering_preset: LightWear
damage_state: Intact
```

Giải thích footprint 7.5 × 5 m là khối nền chính, chưa gồm tầng đua, mái đua và ban công. Chọn 14–20 hàng dễ thấy nhất cho master; các giá trị còn lại nằm trong spec. Với MVP, ưu tiên 2–3 presets, các kiểu mái tương thích và module nối ổn định trước nội thất phức tạp/destruction.

## Kết quả và kiểm tra cuối

Trả đúng chế độ đã chọn, phân biệt rõ **concept đề xuất** với **kết quả HDA đã chạy**. Với full demo, trình bày: phong cách và đặc điểm nhận dạng → module kit/mapping → bảng params và constraints → bộ hình theo thứ tự mục 4 → sáu preset và đề xuất phạm vi MVP. Với master only, trả ảnh và chú thích ngắn, không lặp toàn bộ báo cáo. Không tạo file `.hda` giả hoặc báo đã triển khai từ ảnh concept.

Trước khi hoàn tất, kiểm tra: có module tách rời, master và presets đúng scope; từng hàng so sánh cho thấy tác động thật của parameter; không rò motif lò rèn hay Gothic/Medieval vào reference khác; ảnh đúng topology và số lượng; texture stylized/hand-painted ít nhiễu và giữ palette; các bảng/master cùng mức stylization; thông số có kiểu/default/range/đơn vị; điều kiện giữa các nhóm hợp lý; spec có quy tắc seed tái lập; panel khớp công trình; claims kỹ thuật được xác minh. Đánh giá ảnh chỉ là kiểm tra concept, không phải kiểm thử cook, tái lập seed hay HDA runtime.
