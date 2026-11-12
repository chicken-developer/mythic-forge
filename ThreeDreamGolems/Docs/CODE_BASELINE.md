# Mã nguồn hiện có

Cập nhật 2026-11-13, sau mốc R1. Mọi con số dưới đây đo trực tiếp trên repo ở nhánh `feature/r1-disable-legacy`, không lấy từ báo cáo cũ. Lịch sử: [REFACTOR_REPORT.md](REFACTOR_REPORT.md) (R0), [R1_REPORT.md](R1_REPORT.md) (R1).

## Nền tảng

| Hạng mục | Giá trị |
|---|---|
| Engine | Unreal Engine 5.8.4 (CL 58902052), bản cài tại `C:\Program Files\Epic Games\UE_5.8` |
| Công nghệ | C++, Gameplay Ability System, StateTree, Enhanced Input, CommonUI, MotionWarping |
| Online | Plugin OnlineSubsystemNull, OnlineSubsystemSteam và SteamSockets đã bật; chưa có code phiên chơi |
| Quy mô | Khoảng 36.200 dòng C++ trong 230 file, 14 module (trước R1: 91.500 dòng, 636 file) |
| Test | 38 test tự động, 38 PASS (lượt `R1b-test-3`, 2026-11-13) |
| Nội dung | 74 asset: 4 map, 3 hero, 1 party, định nghĩa AI và combat, 4 StateTree chính và 3 StateTree test, 1 EQS, animation, Mannequin, 5 asset âm thanh |

## Module

| Module | Trách nhiệm hiện tại | File / dòng |
|---|---|---|
| `GolemCore` | Stable ID, kết quả và lỗi, log, settings, tag (gồm `Element.*`, `Class.*`), sự kiện có kiểu, AssetManager | 25 / 1.600 |
| `GolemAbilitySystem` | ASC, bộ 4 chỉ số, ability set, sát thương, gục và hồi sinh, cue | 28 / 3.000 |
| `GolemCombat` | Đòn nhẹ và nặng theo pha, đỡ, parry, né, khoá mục tiêu, stagger, finisher, traversal | 38 / 4.000 |
| `GolemFramework` | Đội ba Golem, hero và party definition, quyền điều khiển và slot, input | 24 / 4.200 |
| `GolemAI` | StateTree task, action set, AI đồng đội, quái và boss hai pha, threat, Combat Director | 36 / 7.600 |
| `ThreeDreamGolems` | Game mode gốc và ba sân thử: Training, AbilityYard, PartyArena | 32 / 3.600 |
| `GolemDeveloper` (Editor) | Commandlet Bootstrap và authoring cho AI, combat; test tích hợp | 19 / 9.600 |
| `GolemWorld` | Khung tương tác, trạng thái thế giới, bản đồ vùng và ping của campaign cũ | 12 / 1.200 |
| `GolemQuest` | Khung quest của campaign cũ | 5 / 600 |
| `GolemDialogue` | Khung hội thoại | 5 / 250 |
| `GolemUI` | Chỉ còn `GolemGameUserSettings` | 3 / 430 |
| `GolemItems`, `GolemSave`, `GolemOnline` | Trống, chờ hệ thống mới | 1 / 3 mỗi module |

Module runtime không phụ thuộc vòng; `Scripts/AuditModules.ps1` kiểm tra điều đó.

## Đang chạy được

- **Đội ba Golem:** Mars (Earth, Warrior, đỏ), Jupiter (Sky, Hunter, vàng), Neptune (Sea, Mage, xanh dương) trong party `GolemParty:ThreeGolems`. Mỗi Golem có ASC riêng; người chơi nhận quyền điều khiển, AI nhận lại khi người chơi rời.
- **Chỉ số:** `MaxHealth`/`Health`, `Power`, `Agility`, `ColorHold`. `Agility` và `ColorHold` đã có trong bộ thuộc tính nhưng chưa hệ thống nào dùng.
- **Combat:** đòn có pha lấy đà, hiệu lực, hồi; đỡ, parry, né; khoá mục tiêu; stagger theo poise; finisher; traversal. Sát thương là số ghi trong combat profile, cộng chênh lệch `Power` so với lúc khởi tạo.
- **AI:** StateTree quyết định, GAS thực thi; đồng đội theo đội hình và cứu người gục; ngân sách tấn công cho hai phe; boss hai pha.
- **Gục và hồi sinh:** `GolemLifeComponent`, ability Revive giữ nút.
- **Settings:** hiển thị, 4 bus âm thanh, đổi phím, cỡ chữ, giảm rung.

## Điểm vào

| Mục | Giá trị |
|---|---|
| Bản đồ mặc định | `/Game/Golem/Maps/Playables/L_M04_PartyArena` |
| Các map khác | `L_M00_Boot`, `L_M01_TrainingYard`, `L_M02_AbilityYard` |
| Dựng lại nội dung | `Scripts\ContinuousRun.ps1 -Action Bootstrap -Milestone 0`, rồi `1`, `2`, `4` |
| Build, test | `Scripts\ContinuousRun.ps1 -Action BuildEditor`, `-Action Test -TestFilter 'Golem.'` |

Tên map `L_M0x_...` theo số mốc của kế hoạch cũ. Sân đấu của M0 sẽ là map mới.

## Chưa có

Luật trộn màu và khoảng trống, Xenon, kẻ canh giữ, `EGamePhase`, khu và trạng thái khu, sách kỹ năng, tarot, DreamCoin, vũ khí và dream stone, Little Princess, nút ra hiệu, lưu game, giao diện trong trận, phiên chơi Steam. Đòn tầm xa cũng chưa có: mọi đòn hiện là cận chiến.

## Cần biết khi làm tiếp

- **Động tác thừa so với thiết kế:** parry, finisher, traversal và khoá mục tiêu vẫn nằm trong ability set `Combat`. Thiết kế mới chỉ có đòn nhẹ, đòn nặng, né và một động tác class; M0 quyết định giữ hay bỏ từng cái.
- **Poise và stagger** vẫn chạy bằng số trong combat profile. Thiết kế mới cho Xenon choáng khi bị lấp khoảng trống.
- **`GolemWorld`, `GolemQuest`, `GolemDialogue`** còn nguyên khung của campaign cũ (bản đồ vùng, ping, quest) và 8 test `Golem.M6.*`, `Golem.M7.*`. Chúng không còn ai gọi; M1 sẽ thu gọn hoặc viết lại theo DEV_SPEC mục 11 và 14.
- **`AS_Golem_Traversal`** là asset duy nhất không dựng lại được bằng Bootstrap trên engine 5.8.4 (ensure ở `AnimSequencerController`); file hiện có lấy từ lần dựng trước.
- **Asset âm thanh** (`SC_Golem_*`, `SMix_Golem_User`) do commandlet campaign cũ tạo. Commandlet đó đã bị xoá, nên không dựng lại được nếu mất.
- **Lần khởi động đầu chậm vì PSO** (việc tồn P-04) và **ca timeout ngẫu nhiên trong ma trận encounter** (P-05) thuộc về nội dung đã xoá hoặc để M4 xử lý.
