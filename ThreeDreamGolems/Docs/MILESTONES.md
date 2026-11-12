# Milestones

Cập nhật 2026-11-13. Tài liệu này thay toàn bộ kế hoạch cũ (các file `imp_plan_m00` tới `imp_plan_m12`). Đặc tả của từng hệ thống nằm ở [DEV_SPEC.md](DEV_SPEC.md); ở đây chỉ ghi thứ tự, phạm vi và điều kiện đạt.

## Bảng tổng

| Mốc | Tên | Mục đích | Trạng thái |
|---|---|---|---|
| R0 | Dọn repo và đổi tên | Xoá tài liệu cũ, đổi Knight thành Golem, build và test lại | Đạt |
| R1 | Tắt hệ thống cũ | Đưa những hệ thống thiết kế mới không dùng ra khỏi build | Đạt |
| M0 | Luật trộn màu | Biết luật trộn màu có vui hay không | Đang làm |
| M1 | Core loop | Biết vòng ngày-đêm có khiến người chơi quay lại hay không | Chưa bắt đầu |
| M2 | Một khu hoàn chỉnh | Khu chợ với art thật, chơi được qua Steam | Chưa bắt đầu |
| M3 | Đủ nội dung | Chơi được từ mở đầu tới kết bản Early Access | Chưa bắt đầu |
| M4 | Phát hành Early Access | Cân bằng, ngôn ngữ, tối ưu, sửa lỗi | Chưa bắt đầu |

Trạng thái dùng một trong bốn giá trị: Chưa bắt đầu, Đang làm, Đạt, Bị chặn. Chỉ ghi Đạt khi mọi điều kiện đạt đã được kiểm tra thật, có log hoặc kết quả playtest kèm theo.

Hai quy tắc về thứ tự:

- **Không làm M2 khi M0 và M1 chưa đạt.** Nếu luật trộn màu không vui ở M0 thì sửa luật trước.
- **M0 và M1 không cần art.** Art làm song song và ghép vào từ M2.

## R0. Dọn repo và đổi tên

Đã xong ngày 2026-11-12. Kết quả, lệnh đã chạy và những gì chưa chạy ghi ở [REFACTOR_REPORT.md](REFACTOR_REPORT.md). Còn một việc cho chủ dự án: mở editor có giao diện và chơi thử bản đồ mặc định.

| Việc | Xong khi |
|---|---|
| Commit nguyên trạng trước khi sửa | Có một commit checkpoint trên nhánh riêng |
| Xoá tài liệu và nội dung cũ | Thư mục project chỉ còn `README.md`, `CODING_RULES.md` và `Docs/` là tài liệu |
| Mở `.gitignore` cho tài liệu | `git status` thấy `README.md`, `CODING_RULES.md`, `Docs/**` |
| Đổi tên project, module, class, file | Không còn chuỗi `Knight` trong `Source/`, `Config/`, `Scripts/`, trừ redirect |
| Đổi thư mục nội dung `/Game/Knight` | Editor mở được bản đồ mặc định, không có lỗi thiếu asset |
| Build và test | Editor build thành công; bộ test tự động chạy lại, kết quả ghi trong báo cáo |

## R1. Tắt hệ thống cũ

Đã xong ngày 2026-11-13, xem [R1_REPORT.md](R1_REPORT.md). Theo chỉ dẫn của chủ dự án, hệ thống cũ được xoá hẳn thay vì chỉ đưa ra khỏi build; bản cũ còn trong lịch sử git.

Mục đích là để mã nguồn khớp với thiết kế mới trước khi viết thêm.

| Việc | Module | Xong khi |
|---|---|---|
| Bỏ 9 spec và tài nguyên kiểu mana; mỗi nhân vật còn một class cố định | `GolemAbilitySystem`, `GolemItems` | Ba nhân vật vào trận chỉ với bộ động tác có sẵn |
| Gán ba nhân vật chuẩn thành Mars, Jupiter, Neptune với nguyên tố Earth, Sky, Sea | `GolemCore`, `GolemFramework` | Definition và stable ID dùng tên mới |
| Bỏ 12 ô trang bị, loot và các chỉ số Crit, Haste, Mastery, Versatility | `GolemItems`, `GolemAbilitySystem` | Còn 4 chỉ số: `MaxHealth`, `Power`, `Agility`, `ColorHold` |
| Bỏ campaign cũ: hai nhiệm vụ demo, di chuyển nhanh giữa vùng, minimap | `GolemQuest`, `GolemWorld`, `GolemUI` | Menu chính không còn đường vào campaign cũ |
| Gỡ các màn hình RPG cũ khỏi HUD | `GolemUI` | Không còn Specialization, Talents, Spellbook, Runes, Inventory |
| Tắt test của hệ thống đã tắt | `GolemDeveloper` | Bộ test còn lại chạy PASS; số test trước và sau được ghi lại |
| Cập nhật `CODING_RULES.md` nếu có quy tắc gắn với hệ thống đã bỏ | — | Không còn quy tắc nhắc tới spec, loot, campaign cũ |

Giữ nguyên: lõi combat, AI đồng đội, gục và hồi sinh, khung boss hai pha, khung cửa hàng, khung lưu game, settings.

## M0. Luật trộn màu

Đặc tả: DEV_SPEC mục 4, 5, 6 và 12.

| Việc | Module |
|---|---|
| Thêm `EElement` và GameplayTag `Element.Earth`, `Element.Sky`, `Element.Sea`; gán cho ba Golem | `GolemCore` |
| Bộ động tác có sẵn cho ba class: đòn nhẹ, đòn nặng có cờ `bAppliesColor`, né, động tác class | `GolemAbilitySystem`, `GolemCombat` |
| Đòn nặng tầm xa của Jupiter và Neptune: đạn bay thẳng 25 m, phải trúng đúng vùng khoảng trống. Code hiện chưa có đạn bay | `GolemCombat` |
| `UColorVoidComponent` với 5 trạng thái và luật chuyển trạng thái; replicate `State`, `AppliedElements`, `TimerEndServerTime` | `GolemCombat` |
| Thân xám nhận 10% sát thương; `VoidsToFill` và `MaxOpenVoids`: khoảng trống lộ lần lượt, mỗi lần lấp trừ `MaxHealth / VoidsToFill` và gây choáng; trạng thái `Restored` | `GolemCombat` |
| Hai loại Xenon dùng Mannequin phóng to: Lính (lấp 3, luôn `Open`) và Khiên (lấp 4, xen kẽ khiên và lưng) | `GolemAI` |
| Nút ra hiệu và dấu hiệu trên khoảng trống | `GolemCombat`, `GolemUI` |
| AI đồng đội: đáp lại dấu ra hiệu, tự bồi màu vào khoảng trống đang `Primed`, không bao giờ đánh sai màu, không tự mở đầu khoảng trống nào | `GolemAI` |
| Chỉ báo khoảng trống trong thế giới 3D: màu, ký hiệu nguyên tố, vòng đếm giờ | `GolemUI` |
| Một sân đấu hình khối xám; chơi 3 người qua listen server bằng OnlineSubsystemNull | `GolemWorld`, `ThreeDreamGolems` |
| Test tự động cho từng luật ở DEV_SPEC mục 5 | `GolemDeveloper` |

**Đạt khi:** 5 nhóm ba người chơi 15 phút. Ít nhất 4 nhóm tự hiểu luật trong 5 phút đầu mà không cần giải thích, và ít nhất 4 nhóm muốn chơi tiếp.

## M1. Core loop

Đặc tả: DEV_SPEC mục 2, 7, 8, 9, 10, 11 và 14.

| Việc | Module |
|---|---|
| `EGamePhase` với 8 trạng thái, host giữ và báo cho client | `GolemFramework` |
| Một khu dạng khối xám với 5 `ArenaVolume`, mỗi đêm chọn 3 theo seed | `GolemWorld` |
| Một loại khung kẻ canh giữ; bộ sinh ghép bộ màu, 2 trong 4 chiêu, 1 trong 3 vị trí điểm yếu; lõi trắng ở pha 2 | `GolemAI`, `GolemWorld` |
| 3 giờ tự do; ba hành động ban ngày; manh mối lộ dần ba phần của kẻ canh giữ | `GolemWorld`, `GolemQuest` |
| Trạng thái khu 4 mức và độ bão hoà màu theo khu | `GolemWorld` |
| DreamCoin: nguồn, chỗ tiêu, ví theo người chơi | `GolemItems` |
| Cửa hàng của Princess phù thủy với 11 sách bậc 1; túi 4 sách mỗi Golem; sách miễn phí mỗi đêm | `GolemItems`, `GolemUI` |
| 6 lá tarot: rơi, giữ 3, bật 30 giây | `GolemItems`, `GolemAbilitySystem` |
| Vũ khí nâng tới bậc 3 ở lò rèn | `GolemItems`, `GolemUI` |
| Little Princess: gợi ý khu, mở cổng mơ, mang bài, pha màu, kéo đội dậy | `GolemAI`, `GolemFramework` |
| Gục và hồi sinh theo DEV_SPEC mục 10 | `GolemCombat` |
| `PlayerSave` và `WorldSave`; cờ `bNightInProgress` | `GolemSave` |
| Màn kết quả đêm, sổ tay, ô giờ tự do | `GolemUI` |

**Đạt khi:** 5 người, mỗi người chơi 5 ngày-đêm. Ít nhất 3 người có dò manh mối từ ngày thứ 3 trở đi mà không bị nhắc, và ít nhất 3 người muốn chơi ngày thứ 6.

## M2. Một khu hoàn chỉnh

| Việc | Module |
|---|---|
| Phiên chơi qua Steam: tạo phòng, mời bạn, chọn Golem, AI nhận Golem khi người chơi rời | `GolemOnline`, `GolemFramework` |
| Khu chợ với art thật, bản đời thực và bản giấc mơ dùng chung hình khối | Nội dung |
| Model và animation cho ba Golem, Little Princess, hai loại Xenon, một kẻ canh giữ | Nội dung |
| Giao diện đầy đủ theo DEV_SPEC mục 15; hỗ trợ tay cầm và chuột-phím | `GolemUI` |
| Màn mở đầu trong mơ và ngày đầu tiên có hướng dẫn | `ThreeDreamGolems`, `GolemQuest` |
| Trang Steam và trailer | Ngoài code |

**Đạt khi:** ba người ở ba nơi khác nhau chơi trọn 3 ngày-đêm qua Steam không gặp lỗi chặn. Bản build dùng được để quay trailer.

## M3. Đủ nội dung

| Việc | Số lượng |
|---|---|
| Khu | 5: khu chợ, quảng trường, khu lò rèn, bến cảng, cung điện |
| Loại Xenon bóng đêm | 4, kèm bản tinh anh |
| Loại khung kẻ canh giữ | 3 |
| Trận Xeros ở cung điện và đoạn kết bản Early Access | 1 |
| Sách kỹ năng | 22 |
| Dream stone | 15 |
| Lá tarot | 22 |
| Độ khó mỗi khu | 3 |
| Bậc vũ khí | 5 |
| Đoạn cảnh kể nguồn gốc Lithea | 4, sau lần thắng đầu ở 4 khu đầu |

**Đạt khi:** một người chơi đi được từ màn mở đầu tới đoạn kết bản Early Access.

## M4. Phát hành Early Access

| Việc | Ghi chú |
|---|---|
| Cân bằng kinh tế và độ khó | Dựa trên số liệu playtest, chỉ sửa dữ liệu |
| Ba ngôn ngữ: Anh, Việt, Trung giản thể | Kiểm tra font và tràn chữ |
| Steam Cloud, thành tựu | |
| Tối ưu khởi động và PSO | Việc tồn từ P-04 trong [CODE_BASELINE.md](CODE_BASELINE.md) |
| Build Shipping, chạy thử dài, sửa lỗi | |

**Đạt khi:** không còn lỗi chặn, và một người chơi mới đi hết bản Early Access mà không cần hỗ trợ.

## Kế hoạch cũ đi đâu

| Mốc cũ | Nội dung | Số phận |
|---|---|---|
| M0–M4 | Nền tảng, đội ba nhân vật, GAS, combat, AI | Đã làm, giữ làm nền |
| M5, M5.5, M5.6 | RPG loop, 9 spec, trang bị 12 ô | Đã làm, bị thay ở R1 |
| M6 | Khung quest, dialogue, world, save | Đã làm, thu gọn ở R1 và M1 |
| M6.5 | XP, mở khoá, kinh tế campaign | Chưa làm, bỏ |
| M7, M7.5 | Giao diện campaign, bản đồ, chất lượng encounter | Làm một phần; giữ settings và HUD, bỏ phần campaign |
| M8 | Steam co-op, gia cố lưu game | Chưa làm, chuyển sang M2 và M4 |
| M9–M12 | Cổng sản xuất, nội dung cốt truyện, beta, phát hành | Chưa làm, thay bằng M3 và M4 |
