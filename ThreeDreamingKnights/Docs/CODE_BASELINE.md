# Mã nguồn hiện có

Cập nhật 2026-11-12. Tài liệu này tóm tắt trạng thái mã nguồn tại thời điểm chuyển sang thiết kế Three Dream Golems. Nó được rút từ `FINAL_REPORT.md` (2026-10-18) và README cũ trước khi hai file đó bị xoá.

**Giới hạn:** nội dung dưới đây lấy từ báo cáo, chưa được đối chiếu lại với từng file nguồn. Trước khi dựa vào một API hay một hành vi, hãy đọc header thật.

Tên trong tài liệu này dùng tiền tố `Golem`, tức tên sau khi xong mốc R0. Trước R0, các tên đó mang tiền tố `Knight` và project tên `ThreeDreamingKnights`.

## Nền tảng

| Hạng mục | Giá trị |
|---|---|
| Engine | Unreal Engine 5.8.3 (CL 58210709) |
| Toolchain | MSVC 14.44.35227, Windows SDK 10.0.22621.0 |
| Công nghệ | C++, Gameplay Ability System, StateTree, Enhanced Input, CommonUI, MotionWarping |
| Online | Plugin OnlineSubsystemNull, OnlineSubsystemSteam và SteamSockets đã bật; chưa có code phiên chơi Steam |
| Quy mô | Khoảng 91 nghìn dòng C++, 636 file nguồn, 14 module |
| Test | 134/134 test tự động PASS ở hai lượt chạy (build 20 và 21, 2026-10-17) |
| Đóng gói | Win64 Development PASS: 652 package, 0 lỗi, 0 cảnh báo |
| Git | Baseline `master@d875829`; công việc từ mốc M6 cũ trở đi chưa được commit tại thời điểm báo cáo |

## Module

| Module | Trách nhiệm | Số file |
|---|---|---|
| `ThreeDreamGolems` | Target, boot, GameInstance, GameMode, PlayerController, HUD, ghép các module | 126 |
| `GolemAbilitySystem` | ASC, ability, effect, attribute, ability set, cue | 53 |
| `GolemDeveloper` (Editor) | Validator, chẩn đoán, harness test tự động, audit | 51 |
| `GolemCombat` | Trạng thái combat, trace, combo, phòng thủ, khoá mục tiêu, stagger, finisher | 44 |
| `GolemAI` | StateTree task, quyết định của đồng đội, quái và boss, threat, Combat Director | 37 |
| `GolemItems` | Định nghĩa vật phẩm, túi đồ, loot, trang bị, rune, tiến trình kỹ năng | 36 |
| `GolemCore` | Stable ID, kết quả và lỗi, log, settings, tag, sự kiện có kiểu, AssetManager | 28 |
| `GolemFramework` | Trạng thái đội, ba nhân vật, quyền điều khiển và slot, vòng đời | 25 |
| `GolemUI` | CommonUI, lớp input, ViewModel, HUD, các màn hình, GameUserSettings | 14 |
| `GolemWorld` | Tương tác, trạng thái thế giới, encounter, checkpoint, bản đồ, cổng, di chuyển | 13 |
| `GolemQuest` | Định nghĩa nhiệm vụ, trạng thái, mục tiêu, phần thưởng | 6 |
| `GolemDialogue` | NPC, hội thoại, vòng đời hội thoại khi co-op | 6 |
| `GolemSave` | Mô hình lưu có phiên bản, snapshot, ghi bất đồng bộ, phục hồi, migration | 6 |
| `GolemOnline` | Gần như trống (2 file) | 2 |

Module runtime không được phụ thuộc vòng. Audit đồ thị module nằm ở `Scripts/AuditModules.ps1`.

## Những gì đã chạy được

- **Đội ba nhân vật:** luôn có đúng ba nhân vật chuẩn; mỗi nhân vật có ASC riêng; người chơi nhận quyền điều khiển, AI nhận lại khi người chơi rời; có stable ID.
- **Combat:** đòn đánh có pha lấy đà, hiệu lực, hồi; đỡ, parry, né, finisher; khoá mục tiêu; phản hồi khi trúng đòn.
- **AI:** StateTree quyết định, GAS thực thi; đồng đội theo đội hình, qua cổng và cầu hẹp, quay lại cứu người gục; ngân sách tấn công riêng cho hai phe.
- **Boss:** hai pha, có giáp; thử lại dọn sạch trạng thái; người chơi vào giữa trận được dựng lại đúng trạng thái.
- **Kỹ năng và trang bị kiểu cũ:** 9 spec (Jupiter, Mars, Mercury, Neptune, Pluto, Saturn, Terra, Uranus, Venus), tài nguyên kiểu mana, Runestone, 12 ô trang bị, loot.
- **Nhiệm vụ và hội thoại:** khung quest, dialogue, world state; demo hai nhiệm vụ đầu.
- **Giao diện:** menu campaign, journal, bản đồ, minimap, ping, di chuyển nhanh, settings (hiển thị, 4 bus âm thanh, đổi phím, cỡ chữ, giảm rung), HUD dùng snapshot và subscription.
- **Lưu game:** có phiên bản schema, tiếp tục từ tiến trình lạnh đã được kiểm tra.

## Điểm vào hiện tại

| Mục | Giá trị |
|---|---|
| Bản đồ mặc định | `/Game/Golem/Maps/Playables/L_M07_Campaign` |
| Vùng thứ hai | `L_M07_Courtyard` |
| Sân thử encounter | `/Game/Golem/Maps/Playables/L_M07_5_ForgeTrial` (boss hai pha, 3 Runestone, cần gạt, cổng, cầu) |
| Phím | WASD và chuột, F tương tác, I màn hình nhân vật, Esc menu, M bản đồ, J journal, F1–F4 kỹ năng |
| Art trong game | Mannequin của UE, khối hộp thay trang bị, âm thanh tạo bằng thủ tục |

Tên bản đồ `L_M0x_...` theo số mốc của kế hoạch cũ, không liên quan tới các mốc mới trong [MILESTONES.md](MILESTONES.md).

## Trùng tên cần để ý

- **Mars, Jupiter, Neptune** hiện là tên của ba trong chín spec cũ. Trong thiết kế mới, đó là tên của ba Golem. Khi làm mốc R1, bỏ nghĩa cũ trước khi dùng nghĩa mới.
- **Ba nhân vật chuẩn** trong code cũ được gọi theo màu Red, Blue, Green và theo nguyên tố Sky, Sea, Earth (bí danh Storm, Tide, Forge). Cần đọc definition thật để gán đúng: Earth là Mars (đỏ), Sky là Jupiter (vàng), Sea là Neptune (xanh dương).

## Việc còn dang dở từ kế hoạch cũ

| Mã cũ | Nội dung | Xử lý trong thiết kế mới |
|---|---|---|
| P-01 | Chưa có XP, mở khoá, talent, kinh tế campaign | Bỏ. Thay bằng DreamCoin, vũ khí, dream stone theo DEV_SPEC mục 11 |
| P-02 | Chưa có kiểm thử với người thật, tay cầm vật lý, ngôn ngữ | Chuyển thành hai bước kiểm chứng M0 và M1 |
| P-03 | Thiếu animation, âm thanh, art sản xuất | Làm theo tài liệu art, ghép vào từ M2 |
| P-04 | Lần khởi động đầu chậm vì PSO (khoảng 77 giây tới UI); chưa có Shipping, Steam, soak | Giữ, xử lý ở M4 |
| P-05 | Một ca solo DPS bị timeout trong ma trận encounter 18 ca, không tái hiện được | Encounter cũ bị thay; vẫn theo dõi vì có thể là lỗi AI hoặc driver test |

## Phần nào giữ, phần nào thay

Bảng đầy đủ nằm ở mục 3 của [DEV_SPEC.md](DEV_SPEC.md). Tóm tắt theo module:

| Module | Hướng xử lý |
|---|---|
| `GolemCore`, `GolemFramework`, `GolemAbilitySystem`, `GolemCombat` | Giữ. Thêm nguyên tố cho đòn đánh và hệ số sát thương theo vùng trúng |
| `GolemAI` | Giữ. Thêm phản ứng với nút ra hiệu và tự bồi màu; viết AI cho 4 loại Xenon và kẻ canh giữ |
| `GolemItems` | Thay phần lớn: còn vũ khí 5 bậc, dream stone, sách kỹ năng, tarot, ví DreamCoin |
| `GolemQuest` | Thu gọn thành cờ cốt truyện và hành động ban ngày |
| `GolemDialogue` | Giữ cho trò chuyện ban ngày |
| `GolemWorld` | Đổi nội dung: khu và trạng thái khu, sân đấu, cổng mơ, manh mối. Bỏ di chuyển nhanh giữa vùng |
| `GolemUI` | Giữ khung và settings. Thay các màn hình RPG cũ bằng danh sách ở DEV_SPEC mục 15 |
| `GolemSave` | Giữ khung. Tách `PlayerSave` và `WorldSave` theo DEV_SPEC mục 14. Không cần đọc được file lưu cũ |
| `GolemOnline` | Viết mới: phiên chơi Steam, mời bạn |
| `GolemDeveloper` | Giữ. Test của hệ thống bị tắt thì tắt theo |
