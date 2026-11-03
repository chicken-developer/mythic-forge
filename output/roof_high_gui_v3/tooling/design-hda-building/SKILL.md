---
name: design-hda-building
description: "Dựng/sửa HDA building Gothic, Medieval hoặc phong cách khác trong Houdini 22 theo Hybrid SOP và context-aware architecture. Python HOM tạo mạng SOP module hóa như OOP/FP (subnet trách nhiệm đơn, input/output contracts, dependency graph, graph audit), VEX xử lý semantic rules, native SOP tạo geometry/UV. Case A: Houdini Editor cook rồi bake Unreal-native; không full Python SOP, không offline Python preview mặc định, không Unreal runtime cook. Dùng khi dựng HDA từ ảnh + 5–10 biến thể/JSON hoặc refactor network node rời rạc thành module dễ debug."
---

# HDA Building — Reference to Houdini Geometry

## Chọn chế độ và đọc tài nguyên

- **Mặc định: implementation + evaluation.** Khi nhận ảnh sample cùng các biến thể, hoặc yêu cầu Python/geometry/HDA/dataset, phân tích rồi triển khai đầy đủ. Không dừng ở đề xuất params, prompt tạo ảnh hay nhà dạng box blockout.
- **Case A — Houdini Editor → Unreal Bake (mặc định với Unreal):** Houdini/Houdini Engine cook HDA trong Editor, QA, rồi bake thành Unreal-native Static Mesh/instances/collision/material. **Không coi Houdini Engine là bộ sinh mesh trong packaged gameplay runtime.** Nếu người dùng yêu cầu xây nhà realtime trong game, tách thành một dự án Unreal C++ runtime riêng; không thay thế Case A một cách âm thầm.
- **Concept/master/image-only:** chỉ khi người dùng yêu cầu đầu ra này; đọc [concept-workflow.md](references/concept-workflow.md). Giữ format Gothic/Storybook Medieval, stylized hand-painted và khả năng dùng reference bất kỳ. Không ép mọi kiến trúc thành Gothic/Medieval/IronForge.
- **Update skill:** sửa skill, kiểm tra và lưu; **mặc định bỏ qua toàn bộ Python offline geometry/semantic preview** theo workflow Houdini thực. Không tự chạy `scripts/preflight_case_a.py` hoặc render proxy khi không được yêu cầu rõ ràng. Static schema/syntax validation vẫn được phép nhưng không thay thế cook.
- Với implementation hoặc refactor HDA node graph, **đọc cả năm**: [modular-sop-architecture.md](references/modular-sop-architecture.md), [hybrid-sop-workflow.md](references/hybrid-sop-workflow.md), [context-aware-editor-pipeline.md](references/context-aware-editor-pipeline.md), [implementation-contract.md](references/implementation-contract.md), [dataset-contract.md](references/dataset-contract.md) **trước khi viết code**. Dùng `scripts/validate_dataset.py` kiểm tra dataset; đây chỉ là kiểm tra cấu trúc dữ liệu, không phải kiểm thử Houdini.

Giải thích tiếng Việt, tên parameter snake_case, ID/file ổn định. Kế thừa chỉ dẫn và reference đã chốt trong phiên làm việc, không bắt người dùng duyệt lại các bước thông thường.

## 1. Nhận và kiểm tra input

Input chính: **một ảnh sample/baseline + 5–10 ảnh biến thể mục tiêu** từ đơn giản đến trung bình và phức tạp. Biến thể là reference geometry, không mặc định là HDA đã chạy. Nhận thêm `.hip/.hda`, mesh module, params gốc, kích thước chuẩn, Houdini version, mục tiêu Unreal nếu có.

Nếu yêu cầu đánh giá/nâng cấp HDA hiện có, đọc asset/schema hiện tại trước; giữ tên parameter, semantics và output tương thích khi có thể, thêm migration/version rõ ràng khi cần đổi. Không coi ảnh screenshot là bằng chứng đã đọc network hoặc tự thay asset cũ bằng một generator mới không tương thích.

Nếu nhiệm vụ là **gom nhóm/refactor node graph hiện có**, phải inventory actual node paths, wiring, HDA parms và user-owned edits trong Houdini, lập `module_graph.json` rồi tổ chức lại bằng subnet có contract; sau đó cook/so sánh before-after cùng dataset/seed. Không coi việc vẽ thêm network box hoặc rename nodes là hoàn thành refactor. Đọc mục refactor trong [modular-sop-architecture.md](references/modular-sop-architecture.md).

Mở và xem từng ảnh; không suy đoán nội dung từ tên file. Nếu ảnh nằm trong Library, dùng skill Library để lấy đúng file. Với sheet chứa nhiều ô, đánh ID từng ô và giữ mapping tới ảnh nguồn/crop. Không đọc các ảnh không liên quan trong tài khoản.

- Giữ đúng số biến thể người dùng cung cấp trong khoảng 5–10; sample là baseline riêng, không tự tính thành một biến thể thứ sáu.
- Nếu chỉ có sample nhưng vẫn yêu cầu triển khai: mặc định đề xuất **8 biến thể** cùng họ kiến trúc; ghi rõ `designed`, không báo là input đã có. Nếu có ít hơn 5 hoặc hơn 10 ảnh biến thể, giữ ảnh nguồn và hỏi phạm vi nếu cần; có thể bổ sung biến thể thiết kế để đạt 5, nhưng phân biệt nguồn.
- Nếu không có ảnh sample hay ảnh kiến trúc đọc được: yêu cầu đính kèm đúng ảnh; không dùng ảnh khác hoặc dựng một họ kiến trúc tùy ý để thay input.
- Nếu thiếu kích thước: dùng cửa cao 2.1 m làm giả định tỉ lệ khi phù hợp, đánh dấu giả định và expose scale; không trình bày như số đo từ ảnh. Bề mặt khuất, nội thất và topology không thể xác định từ một góc nhìn phải có ghi chú.
- Nếu thiếu Houdini version: kiểm tra runtime nếu có, nếu không ghi target tương thích dự kiến và version chưa kiểm thử; dùng capability checks. Không suy từ Unreal version ra Houdini version.

## 2. Phân tích và đánh giá trước khi viết code

Lập bảng từng input: `reference_id | complexity | footprint | floors/bays | roof | openings | attachments | motifs | observed/assumed | procedural strategy`.

Đánh giá **độ phức tạp theo cấu trúc**, không chỉ số polygon: simple = một khối/mái ít giao; medium = nhiều tầng/nhịp, ban công/dormer; complex = khối giao, mái valley, tháp/courtyard, tổ hợp bất đối xứng. Dùng đặc điểm thật của ảnh, không thêm tháp chỉ để tăng hạng.

Chốt:
1. 3–5 đặc điểm nhận dạng bất biến, tỷ lệ chính và hướng front/up; đơn vị mét, Houdini Y-up mặc định.
2. Phần chung có thể parameter hóa, phần cần module riêng, phần organic/sculpt cần input mesh. Ưu tiên procedural native cho shell/mái/khung và instance cho chi tiết lặp.
3. Mapping mỗi khác biệt nhìn thấy → parameter hoặc quy tắc ghép → module/output thực sự. Không expose menu chưa triển khai hoặc parameter không tác động geometry.
4. Footprint, layout, opening và roof grammar; ràng buộc tương thích, giới hạn min/max, ngân sách polygon/cook theo cấu hình.
5. Các giả định, giới hạn tái dựng từ ảnh và lý do cần custom mesh. Không ngầm yêu cầu asset thương mại để script mặc định chạy.

Với building HDA hướng Case A, xác định **semantic architecture graph** trước geometry: footprint, wall IDs, roof patch IDs, openings (gắn vào wall theo khoảng cách local), support/attachment IDs, quan hệ kề/giao, mức ưu tiên và tính hợp lệ. Chỉ expose footprint/roof/junction grammar đã có solver và test. Không coi `JSON` là mesh generator; JSON chỉ ghi ý đồ/params/IDs/presets.

Tạo `analysis.md` ngắn và `parameter_schema.json`; tiến hành dựng luôn trong phạm vi đã yêu cầu. Chỉ hỏi khi thiếu input làm thay đổi căn bản giải pháp; vẫn hoàn thành phần không phụ thuộc câu trả lời.

## 3. Dựng một generator dùng chung — Hybrid SOP bắt buộc

Sinh `build_hda.py` theo implementation contract để **tạo và cấu hình một node graph Houdini có thể inspect/debug**, không dùng Python làm geometry cook engine. Một generator dùng chung cho mọi record; không viết 10 công trình hardcoded không có params. Tách normalize/validate config, layout, module construction, facade, roof intersections, attachments, UV/material groups, output và đánh giá.

**Kiến trúc node graph như một dự án phần mềm (bắt buộc):** thay các node phân mảnh bằng **subnet/module thật có trách nhiệm đơn**, giới hạn rõ input/output và dependencies, giữ graph root chỉ gồm các stage chức năng (`LAYOUT`, `SHELL`, `OPENINGS`, `ROOF`, `DETAILS`, `UV_MATERIAL`, `OUTPUT` khi có nội dung). Mỗi stage có module con và debug endpoint; dữ liệu đi qua connector công khai, không nối xuyên vào node nội bộ hoặc giấu dependency bằng absolute path. Python HOM dùng các *module builder/factory* nhỏ trả về named ports; tái sử dụng logic bằng composition và data contracts, không tạo một script/bộ node monolithic. Tuân theo [modular-sop-architecture.md](references/modular-sop-architecture.md).

**Phân vai bắt buộc**:

- **Python HOM (ngoài SOP cook):** tạo/khai báo SOP nodes, subnet/module, HDA interface, channel refs, defaults, dataset loader, testing/batch runner và export. Python chỉ dựng graph; không lặp `createPoint()`/`createPolygon()` để xây toàn bộ nhà trên mỗi lần cook.
- **Native SOP:** thực hiện geometry/topology, Boolean, Sweep, PolyExtrude, PolyBevel, Fuse, normals và UV operators. Chỉ nhóm SOP thành subnet theo chức năng/semantic dataflow; màu hoặc network box không đủ để coi là module. Giữ node graph đọc được và có thể mở/inspect bên trong.
- **VEX Wrangle:** tạo attributes, mask, transforms, semantic groups, procedural rules, scatter và UV projection đơn giản. Dùng VEX nơi có lợi hơn native SOP; tránh nhồi cả công trình vào một Detail Wrangle khó debug.
- **UV:** ưu tiên UV Texture/Project hoặc UV Auto Seam → UV Flatten → UV Layout theo từng nhóm hình học; VEX chỉ gán/điều chỉnh UV khi có quy tắc local mapping rõ ràng. Kiểm tra distortion, texel density và material/trim alignment.
- **Semantic/constraint stage:** trước mọi branch tạo mesh, kiểm tra cửa nằm trên tường hợp lệ, hỗ trợ kết cấu, clearance, roof patches trùng/giao và junction được khai báo. Gắn `wall_id`, `opening_id`, `roof_patch_id`, `module_id`, `part_role` và `material_family` ổn định. Không để generator chọn geometry thay thế chỉ vì gặp trường hợp khó.
- **Context-aware details:** sinh gạch/ván theo *các đoạn còn lại sau khi trừ khoảng cửa*, tạo frame/caps/trim theo local coordinates, quy tắc roof valley/ridge trước khi rải ngói. Chỉ thêm rêu/ivy/wear masks sau khi structural shell hợp lệ; tránh Boolean cho từng viên gạch nếu phép chia interval/clip rẻ hơn.

**Không chấp nhận** một script full-Python dựng các vertex/faces làm output duy nhất, một Python SOP tạo toàn bộ geometry, một node chứa mọi logic, hoặc HDA chỉ bọc static mesh mà không recook theo params. Chỉ cho phép ngoại lệ Python SOP rất nhỏ cho adapter dữ liệu không có native equivalent, phải đo performance, ghi rõ lý do và giữ xử lý topology chính ở native SOP. Nếu người dùng yêu cầu đích danh full-Python, giải thích tradeoff rồi thực hiện theo yêu cầu đó.

- Tạo node UI cùng params thật; thay params phải recook và thay geometry đúng dự kiến. Yêu cầu đầu ra vẫn là Python dùng được ngay trong Houdini/hython để **tạo graph**, không phải Python SOP mesh builder.
- Không chỉ tạo frozen mesh ở lần chạy đầu rồi gọi nó là HDA procedural. Nếu có cache, cache theo config/version/seed và invalidation rõ ràng.
- Dựng shell có độ dày, lỗ cửa thật, khung/trim, mái có thickness/eaves/ridge, foundation, motif và attachments nhìn thấy ở input. Giải giao khối/mái, bỏ mặt coplanar trùng, tránh phần nổi không có support. Không dùng texture vẽ cửa để giả mở geometry.
- Định nghĩa normals, UV theo module/material family, tên/group hoặc `name` ổn định cho parts, material IDs; có vật liệu preview tự chứa. Bevel phải dựa trên nhóm edge và kích thước feature; không dùng mức bevel/subdivision toàn cục khiến góc/mái bị bóp. Hoàn thiện geometry không đồng nghĩa sculpt tuyệt đối, texture final, nội thất hoặc runtime destruction nếu chưa thuộc scope.
- Mọi topology/roof/footprint được expose phải có code, constraints và ít nhất một dataset kiểm chứng. Không âm thầm thay roof/footprint không hỗ trợ bằng box/gable.
- Với custom ornament chưa có mesh, dựng approximation có hình khối phù hợp và đánh dấu; expose input thay thế. Thiếu mesh tùy chọn không làm mặc định thất bại; thiếu mesh thiết yếu phải báo đúng dependency.
- Random tái lập bằng seed và stable part IDs; tách layout/decoration khi phù hợp, tránh Python `hash()` cho seed giữa process.
- Rerun chỉ cập nhật nhánh node do script sở hữu; không xóa `/obj`, node người dùng, không ghi đè scene/file không liên quan.
- **Case A outputs:** giữ `name`/semantic IDs từ Houdini qua Houdini Engine khi có thể, xuất instancing/packed primitives cho chi tiết lặp, nhóm material, collision và LOD theo yêu cầu game. Đính kèm bake/replace instructions cho Unreal Editor; giữ source HDA + JSON không chỉnh sửa mesh baked bằng cách cố recook trong game.

Xuất digital asset thật nếu runtime/license cho phép; nhúng PythonModule hoặc code SOP cùng defaults/schema để HDA hoạt động sau khi di chuyển, không lệ thuộc đường dẫn scratch. Dùng suffix asset theo license thực tế; không đổi tên `.hdalc/.hdanc` thành `.hda` để giả commercial. Tách mục tiêu editor cook/bake khỏi runtime gameplay Unreal.

## 4. Tạo 5–10 dataset tái lập

Hiểu **dataset = một cấu hình đầy đủ dùng để dựng một biến thể**, không phải tập huấn luyện AI. Dùng [dataset-contract.md](references/dataset-contract.md).

- Tạo `datasets/variant_01.json` … `variant_NN.json` với toàn bộ effective params, seeds, module dependencies, expected features và reference mapping. Dùng cùng schema và generator.
- Có ít nhất simple/medium/complex khi input hỗ trợ; không thay biến thể người dùng bằng danh sách preset Gothic mặc định. Nếu input không có đủ hạng, báo thiếu coverage, chỉ thêm stress tests ngoài bộ input có ghi rõ.
- Thứ tự tăng độ phức tạp, mapping ID tới ảnh gốc không đổi. Complexity không phải detail/LOD; phân biệt rõ.
- Các sweep kiểm chứng đổi một parameter/nhóm liên quan, giữ camera/material/light/seed; preset có thể đổi nhiều params và phải ghi `preset`. Ít nhất một sweep phải đổi tham số hình học (ví dụ width/roof type), một sweep đổi UV tiling/material và xác nhận thay đổi được truyền qua node graph.
- Bổ sung test min/max, tổ hợp không hợp lệ, cùng config/seed hai lần và rerun an toàn. Các test này nằm ngoài N dataset nếu không tương ứng input.

## 5. Tạo ảnh sample tương ứng và kiểm chứng

Mỗi dataset cần **ít nhất một ảnh hero từ geometry thực do chính generator tạo**, ghép cạnh input để so. Xuất thêm front/side/top và clay/wireframe khi giúp kiểm tra; contact sheet tổng không thay N ảnh riêng.

Dựng và đối chiếu baseline riêng bằng cấu hình baseline nhúng trong builder; lưu ảnh/result baseline ngoài N dataset biến thể. Không bỏ kiểm tra ảnh sample chính chỉ vì đủ số biến thể.

- Camera, projection, ánh sáng, scale so sánh phải đồng nhất. Tính frame theo bounding box lớn nhất toàn bộ bộ biến thể, không tự zoom từng nhà khiến mất khác biệt kích thước. Thêm view khớp góc nguồn riêng nếu reference có nhiều góc.
- Đặt `variant_id`, thông số phân biệt và trạng thái kiểm thử bằng compositor/text thật; không nhờ imagegen vẽ chữ/số nghiệm thu.
- **Ảnh imagegen chỉ là concept bổ sung**, đọc skill imagegen nếu sử dụng; không dùng ảnh AI để chứng minh code/HDA đạt hình hay số lượng. Không gọi AI reference image là Houdini render.
- Nếu có Houdini/hython: chạy batch build, cook, export, render/capture; kiểm tra hình, sửa code rồi chạy lại records bị ảnh hưởng. Render headless không dùng `hou.ui`; screenshot viewport chỉ dùng trong phiên GUI.
- Nếu không có Houdini: giao code builder, datasets và lệnh batch đánh giá/render trong Houdini cho người dùng; ghi ảnh thực `pending_houdini`. **Bỏ offline Python proxy/preflight/2D preview theo mặc định**; không tạo ảnh giả và không claim đã có Houdini cook. Chỉ tạo offline preview khi người dùng đổi yêu cầu rõ ràng.

**Graph modularity gate (Houdini):** tạo `module_graph.json` từ module contracts trước khi dựng, chạy `python scripts/validate_module_graph.py <path-to-module_graph.json>` để kiểm tra DAG/ports; sau khi build, dùng HOM trong Houdini kiểm tra actual stage/subnet paths, connectors, dependency DAG, reachability tới `OUTPUT`, orphan nodes, version/ownership, debug endpoints và parameter recook. Ghi kết quả vào `graph_audit.json`; **fail nếu chỉ dùng network box để gom nhóm, root quá phẳng hoặc wiring vượt qua interface module**. Không có Houdini thì `graph_modularity=not_run`.

Đánh giá từng record: silhouette/tỷ lệ, counts, openings, roof joins, attachments/support, UV/normals, lỗi/trùng mặt, bounds, point/primitive counts, cook time, determinism và dependency portability. Kiểm tra riêng **module graph contracts, graph ownership, subnet structure, real module I/O, node types, Python SOP usage, output dependency và recook propagation**. Đo thật hoặc ghi `not_run`; không tự điền số benchmark, pass hoặc visual score khi chưa quan sát/đo. Tách visual fidelity với technical validity; ghi intentional intersections/seams khác với lỗi geometry.

## 6. Giao đầu ra và báo mức hoàn thành

Giao theo [implementation-contract.md](references/implementation-contract.md): Python **SOP network builder** + runner đánh giá + schema + 5–10 JSON + `module_graph.json` + manifest + ảnh input/output đối chiếu + báo cáo ngắn, geometry/HDA khi đã chạy. Giao kèm sơ đồ module dependencies, module I/O contract và báo cáo audit graph modularity/Python SOP vs native SOP/VEX. File thực phải được lưu theo skill Library, trừ repo dự án đã được git quản lý. Khi cập nhật chính skill này, lưu skill bằng git theo skill-creator.

Trả tên và link file deliverable, câu lệnh chạy trong Houdini Python Shell/hython, version/dependencies, số dataset và số ảnh thực đã có. Nêu riêng các trạng thái `code_generated`, `static_checked`, `houdini_cooked`, `geometry_exported`, `rendered`, `hda_packaged`; không suy trạng thái sau từ trạng thái trước. Nếu không có runtime, báo ngắn phần chưa chạy, không tuyên bố production-ready.

## Nguồn kỹ thuật

Kiểm tra tài liệu chính thức đúng Houdini/Houdini Engine version khi dùng HOM/operator parms; doc online latest không xác nhận khả năng bản cũ. Ghi version cùng API/operator capability vào báo cáo.
- https://www.sidefx.com/docs/houdini/hom/pythonsop.html
- https://www.sidefx.com/docs/houdini/hom/hou/Geometry.html
- https://www.sidefx.com/docs/houdini/hom/hou/OpNode.html
- https://www.sidefx.com/docs/houdini/unreal/parameters.html
- https://www.sidefx.com/docs/houdini/unreal/instancing.html
- https://www.sidefx.com/docs/houdini/unreal/materials.html
