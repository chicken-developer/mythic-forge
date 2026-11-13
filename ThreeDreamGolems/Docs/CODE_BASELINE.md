# Mã nguồn hiện có

Cập nhật 2026-11-14, giữa mốc M0. Mọi con số dưới đây đo trực tiếp trên repo ở nhánh `feature/r1-disable-legacy`, không lấy từ báo cáo cũ. Lịch sử: [REFACTOR_REPORT.md](REFACTOR_REPORT.md) (R0), [R1_REPORT.md](R1_REPORT.md) (R1).

## Nền tảng

| Hạng mục | Giá trị |
|---|---|
| Engine | Unreal Engine 5.8.4 (CL 58902052), bản cài tại `C:\Program Files\Epic Games\UE_5.8` |
| Công nghệ | C++, Gameplay Ability System, StateTree, Enhanced Input, CommonUI |
| Online | Plugin OnlineSubsystemNull, OnlineSubsystemSteam và SteamSockets đã bật; chưa có code phiên chơi |
| Quy mô | Khoảng 41.900 dòng C++ trong 263 file, 14 module (sau R1: 36.200 dòng; trước R1: 91.500 dòng) |
| Test | 56 test tự động, 56 PASS (lượt `R1b-test-3`, 2026-11-14); 18 trong số đó là nhóm `Golem.Color`, gồm một phiên chơi thật trên map M0 và một phiên ba người qua listen server |
| Nội dung | 69 asset: 5 map, 3 hero, 1 party, 2 Xenon, định nghĩa AI và combat, 4 StateTree chính và 3 StateTree test, 1 EQS, animation, Mannequin, 5 asset âm thanh |

## Module

| Module | Trách nhiệm hiện tại | File / dòng |
|---|---|---|
| `GolemCore` | Stable ID, kết quả và lỗi, log, settings, tag (gồm `Element.*`, `Class.*`), nguyên tố và màu hiển thị, sự kiện có kiểu, AssetManager | 28 / 1.700 |
| `GolemAbilitySystem` | ASC, bộ 4 chỉ số, ability set, sát thương, gục và hồi sinh, cue | 29 / 3.000 |
| `GolemCombat` | Đòn nhẹ và nặng theo pha, né, động tác class; khoảng trống màu, đạn đặt màu, Xenon và bộ não của nó | 42 / 6.400 |
| `GolemFramework` | Đội ba Golem, hero definition (chỉ số, lấy đà, kiểu đòn nặng) và party definition, quyền điều khiển và slot, input | 25 / 4.300 |
| `GolemAI` | StateTree task, action set, AI đồng đội và phần bồi màu của nó, quái và boss hai pha, threat, Combat Director | 42 / 8.400 |
| `ThreeDreamGolems` | Game mode gốc, ba sân thử cũ (Training, AbilityYard, PartyArena) và sân M0 `ColorArena` với HUD chỉ báo khoảng trống | 37 / 4.200 |
| `GolemDeveloper` (Editor) | Commandlet Bootstrap và authoring cho AI, combat; test tích hợp | 23 / 11.300 |
| `GolemWorld` | Khung tương tác, trạng thái thế giới, bản đồ vùng và ping của campaign cũ | 12 / 1.200 |
| `GolemQuest` | Khung quest của campaign cũ | 5 / 600 |
| `GolemDialogue` | Khung hội thoại | 5 / 250 |
| `GolemUI` | Chỉ còn `GolemGameUserSettings` | 3 / 430 |
| `GolemItems`, `GolemSave`, `GolemOnline` | Trống, chờ hệ thống mới | 1 / 3 mỗi module |

Module runtime không phụ thuộc vòng; `Scripts/AuditModules.ps1` kiểm tra điều đó.

## Đang chạy được

- **Đội ba Golem:** Mars (Earth, Warrior, đỏ), Jupiter (Sky, Hunter, vàng), Neptune (Sea, Mage, xanh dương) trong party `GolemParty:ThreeGolems`. Mỗi Golem có ASC riêng; người chơi nhận quyền điều khiển, AI nhận lại khi người chơi rời.
- **Chỉ số:** `MaxHealth`/`Health`, `Power`, `Agility`, `ColorHold`, lấy từ hero definition của từng Golem. `Power` nhân vào hệ số của đòn đánh, `Agility` nhân vào tốc độ đi và thời gian hồi né, `ColorHold` cộng vào cửa sổ trộn màu.
- **Combat:** đòn có pha lấy đà, hiệu lực, hồi; động tác class trên nút đỡ cũ (Warrior đỡ, Hunter lộn lùi bắn, Mage lá chắn); né có thời gian hồi. Golem không còn bị choáng; chỉ Xenon choáng khi một khoảng trống được lấp. Sát thương bằng `Power` nhân hệ số của đòn (nhẹ 1,0, nặng 2,5). Đòn nặng của Mars là cú vung búa, của Jupiter và Neptune là đạn bay thẳng 25 m.
- **AI:** StateTree quyết định, GAS thực thi; đồng đội theo đội hình và cứu người gục; ngân sách tấn công cho hai phe; boss hai pha.
- **Gục và hồi sinh:** `GolemLifeComponent`, ability Revive giữ nút.
- **Settings:** hiển thị, 4 bus âm thanh, đổi phím, cỡ chữ, giảm rung.
- **Luật trộn màu (M0, đang làm):** khoảng trống 5 trạng thái và dấu ra hiệu trong `GolemCombat/Voids`; đạn đặt màu trong `GolemCombat/Projectiles`; Xenon, hai loại Lính và Khiên cùng bộ não đuổi, báo trước, ra đòn trong `GolemCombat/Xenon`; AI đồng đội bồi màu trong `GolemAI/ColorAssist`; HUD chỉ báo trong `ThreeDreamGolems/ColorArena`. Tiến độ chi tiết ở [MILESTONES.md](MILESTONES.md).

## Điểm vào

| Mục | Giá trị |
|---|---|
| Bản đồ mặc định | `/Game/Golem/Maps/Playables/L_M0_ColorArena` (sân M0: ba Golem, một Lính, một Khiên) |
| Các map khác | `L_M00_Boot`, `L_M01_TrainingYard`, `L_M02_AbilityYard`, `L_M04_PartyArena` |
| Dựng lại nội dung | `Scripts\ContinuousRun.ps1 -Action Bootstrap -Milestone 0`, rồi `1`, `2`, `4`, `10` (sân M0 và Xenon) |
| Build, test | `Scripts\ContinuousRun.ps1 -Action BuildEditor`, `-Action Test -TestFilter 'Golem.'` |

Tên map `L_M0x_...` theo số mốc của kế hoạch cũ; sân đấu của M0 là `L_M0_ColorArena`. Bootstrap ghi lại số thiết kế (chỉ số Golem, hệ số đòn đánh, Xenon) vào asset mỗi lần chạy, nên đổi bản nháp là sửa số trong commandlet rồi chạy lại mốc 10.

### Phím trên sân M0

| Việc | Bàn phím và chuột | Tay cầm |
|---|---|---|
| Di chuyển, nhìn | WASD, chuột | Cần trái, cần phải |
| Đòn nhẹ (không đặt màu) | Chuột trái | RT |
| Đòn nặng (đặt màu của Golem lên khoảng trống) | Chuột phải | LT |
| Né | Shift trái | B |
| Động tác class (đỡ, lộn lùi bắn, lá chắn) | Q | LB |
| Ra hiệu khoảng trống đang nhìn | X | D-pad lên |
| Đổi Golem (chơi đơn) | Tab hoặc 1, 2, 3 | RB |
| Cứu đồng đội gục (giữ) | E | X |
| Host đặt lại sân với Xenon mới | R | Y |

Phím của hệ thống cũ còn lại: N/Z/B/K (lệnh cho đồng đội trên sân PartyArena cũ). B (Focus) nay chỉ vào kẻ địch gần nhất trong tầm nhìn, vì không còn khoá mục tiêu. C (parry), chuột giữa (khoá mục tiêu), H (finisher) và V (traversal) đã bỏ ngày 2026-11-14.

## Chưa có

Xenon loại Ném và Húc, tinh anh, kẻ canh giữ, `EGamePhase`, khu và trạng thái khu, sách kỹ năng, tarot, DreamCoin, vũ khí và dream stone, Little Princess, lưu game, giao diện CommonUI trong trận, phiên chơi Steam. Chơi ba người mới được kiểm trong một tiến trình editor (listen server, hai client, trễ giả lập), chưa kiểm trên ba máy thật.

## Cần biết khi làm tiếp

- **Động tác class dùng chung một ability** (`UGolemClassMoveAbility`) gắn vào input `Input.Combat.Block` và tag `Ability.Combat.Block`; hero definition quyết định nó là đỡ, lộn lùi bắn hay lá chắn. Actor không có hero definition (quái của sân cũ) nhận thế đỡ không giới hạn. AI đồng đội chưa tự dùng động tác class.
- **Bộ động tác khớp thiết kế:** ability set `Combat` chỉ còn đòn nhẹ, đòn nặng, né và động tác class. Parry, finisher, traversal, khoá mục tiêu và poise đã xoá ngày 2026-11-14, xem phần bổ sung trong [R1_REPORT.md](R1_REPORT.md).
- **`AttackTarget` thay cho mục tiêu bị khoá.** Chỉ AI đặt nó (qua `SetValidatedTarget`) để quay về phía mục tiêu khi ra đòn; Golem của người chơi đánh theo hướng đang quay mặt, đạn bay theo tâm màn hình.
- **`UGolemCameraModeComponent`** (chồng chế độ camera) còn trên Golem nhưng không còn ai đẩy chế độ nào vào: ba hệ thống dùng nó đều đã xoá.
- **Sân PartyArena cũ** (đợt quái và boss hai pha) vẫn còn và là lớp cha của sân M0. Boss của nó trước đây bị choáng gần như liên tục vì poise; bỏ poise thì nó đánh không nghỉ, nên `Power` của boss cũ giảm từ 10 xuống 4 để trận đấu trong test còn thắng được. Test `Golem.AI.Arena.Playthrough` của nó được viết cho thế đỡ giữ suốt trận, nên trong test đó thế đỡ của người chơi được đặt lại thành không giới hạn; giới hạn 3 giây của thiết kế được kiểm ở `Golem.Color.Arena.Playthrough`. Khi M0 ổn, nên tách sân M0 ra khỏi nó rồi xoá sân cũ cùng `GolemArenaEnemy`.
- **AI đồng đội chỉ đặt màu vào đúng khoảng trống được giao.** `UGolemCombatComponent::SetColorIntent` giữ khoảng trống đó; đòn nặng của AI trúng khoảng trống khác thì tính như trúng thân xám. Người chơi không bị giới hạn này.
- **Xenon không có AIController.** `UGolemXenonBrainComponent` tự xoay, tự đi thẳng và tự ra đòn; nó chưa dùng navmesh nên có thể kẹt sau vật cản.
- **Choáng chỉ đến từ khoảng trống.** `UGolemVoidHostComponent` gắn tag `State.Combat.Staggered` cho Xenon trong `StaggerSeconds` sau mỗi lần lấp. Tag này vẫn chặn ability combat và AI của bất kỳ actor nào mang nó, nên sách kỹ năng gây choáng ở M1 dùng lại được.
- **`GolemWorld`, `GolemQuest`, `GolemDialogue`** còn nguyên khung của campaign cũ (bản đồ vùng, ping, quest) và 8 test `Golem.M6.*`, `Golem.M7.*`. Chúng không còn ai gọi; M1 sẽ thu gọn hoặc viết lại theo DEV_SPEC mục 11 và 14.
- **Asset âm thanh** (`SC_Golem_*`, `SMix_Golem_User`) do commandlet campaign cũ tạo. Commandlet đó đã bị xoá, nên không dựng lại được nếu mất.
- **Lần khởi động đầu chậm vì PSO** (việc tồn P-04) và **ca timeout ngẫu nhiên trong ma trận encounter** (P-05) thuộc về nội dung đã xoá hoặc để M4 xử lý.
