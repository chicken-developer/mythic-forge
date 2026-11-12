# R1. Báo cáo tắt hệ thống cũ

Ngày thực hiện: 2026-11-13 (Asia/Saigon). Nhánh `feature/r1-disable-legacy`, tách từ `refactor/golem-rename`. Chỉ commit trên máy, chưa push.

Theo chỉ dẫn của chủ dự án trong lúc làm, hệ thống cũ được **xoá hẳn**, không chuyển sang thư mục lưu. Bản cũ chỉ còn trong lịch sử git (từ commit `e6af5732` trở về trước).

## Kết quả

| Tiêu chí của R1 | Trạng thái |
|---|---|
| Bỏ 9 spec và tài nguyên kiểu mana | Xong. Skill runtime, projection, RuneEnergy, Stamina và ability Rune đã xoá |
| Ba nhân vật là Mars, Jupiter, Neptune với Earth, Sky, Sea | Xong. Hero definition có `ElementTag` và `ClassTag`; party là `GolemParty:ThreeGolems` |
| Còn 4 chỉ số | Xong: `MaxHealth`, `Power`, `Agility`, `ColorHold` (kèm `Health` và meta `IncomingDamage`) |
| Bỏ 12 ô trang bị, loot, chỉ số kiểu MMO | Xong. `GolemItems` chỉ còn module rỗng |
| Bỏ campaign cũ khỏi đường chơi | Xong. Không còn game instance, game mode, map hay menu của campaign |
| Gỡ màn hình RPG cũ | Xong. `GolemUI` chỉ còn settings |
| Test còn lại PASS, ghi số trước và sau | Xong: 134, rồi 74, rồi 38; lượt nào cũng PASS toàn bộ |
| Cập nhật `CODING_RULES.md` | Xong, hai câu nhắc inventory và respec |

Build `ThreeDreamGolemsEditor` Win64 Development qua, kể cả khi tắt adaptive unity. `AuditModules.ps1` qua với 14 module.

## Commit

| Commit | Nội dung |
|---|---|
| `561538fc` | Xoá lớp RPG và campaign: 162 file nguồn, 87 asset |
| `7457b198` | Lõi: 4 chỉ số, bỏ spec, đổi tên ba Golem, dựng lại nội dung |
| (commit này) | Báo cáo và tài liệu |

## Số liệu

| | Trước R1 | Sau R1 |
|---|---|---|
| Dòng C++ | 91.500 | 36.200 |
| File nguồn | 636 | 230 |
| Asset | 163 | 74 |
| Map | 17 | 4 |
| Test | 134 | 38 |

Test còn lại theo nhóm: AI 15, Core 5, GAS 5, Framework 4, M6 4, M7 4, Party 1.

## Đã xoá

- **Module `GolemItems`:** inventory, loot, catalog 41 món, rune, skill tree, tiến trình RPG.
- **`ThreeDreamGolems`:** RPGLoop, GearLab, SkillLab, Story, Campaign, CombatChallenge.
- **`GolemAbilitySystem`:** `Skills/`, `Projection/`, `Equipment/`, `GolemSpecializationAbility`, các effect consumable, power crystal, source projection, arena resource recovery, `GolemArmorResponse`.
- **`GolemCombat`:** ability Rune và effect làm chậm; chi phí stamina của đòn đánh, đỡ, parry, né.
- **`GolemUI`:** RPG panel (Specialization, Talents, Spellbook, Runes, Inventory, Gear, Campaign).
- **`GolemSave`:** checkpoint của campaign.
- **`GolemDeveloper`:** commandlet authoring cho RPG, gear, story, campaign, encounter quality; 21 file test; hai fixture lưu `.golem`.
- **Nội dung:** 13 map (`L_M03`, `L_M05*`, `L_M06*`, `L_M07*`) và 75 definition của item, story, campaign, encounter.
- **Tag:** `Equipment.Slot.*`, các tag Rune.

## Dựng lại nội dung

Sau khi đổi code, toàn bộ definition, StateTree, montage và 4 map được xoá rồi tạo lại bằng `-run=GolemBootstrap` với mốc 0, 1, 2, 4. Hai ngoại lệ:

- `AS_Golem_Traversal.uasset` lấy lại từ git, vì bước tạo chuỗi animation này gặp ensure ở `AnimSequencerController` trên engine 5.8.4.
- Asset `Vendor/` và `Audio/` giữ nguyên, không dựng lại.

## Thay đổi hành vi cần biết

- **Đòn đánh không còn tốn stamina.** Đỡ luôn giảm sát thương theo `BlockDamageFraction`; trước đây hết stamina thì vỡ thế đỡ.
- **Không còn Defense.** Sát thương tới thẳng lớp phản ứng đỡ, parry, né rồi trừ máu.
- **`Power` mặc định là 10** (trước là `AttackPower` 25). Ability test gây sát thương bằng `Power`, nên các test liên quan lấy kỳ vọng từ giá trị mặc định.
- **Golem có 10 ability** từ hai ability set (trước là 11, có Rune).

## Chưa làm, để lại có chủ ý

- Parry, finisher, traversal, khoá mục tiêu và poise còn trong lõi combat. Thiết kế mới không nhắc tới chúng; M0 viết bộ động tác mới sẽ quyết định.
- `GolemWorld`, `GolemQuest`, `GolemDialogue` còn khung campaign cũ và 8 test của chúng. Không còn code nào gọi tới; M1 xử lý cùng lúc viết khu, manh mối và cờ cốt truyện.
- Hai sân thử Training và AbilityYard cùng map của chúng còn lại vì test Framework và GAS chạy trên đó.

## Chưa chạy

- Mở editor có giao diện và chơi thử bằng tay (cũng còn nợ từ R0).
- Test ở chế độ có render, cook, đóng gói, cấu hình DebugGame.
