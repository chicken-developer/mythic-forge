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

- Parry, finisher, traversal, khoá mục tiêu và poise còn trong lõi combat. Thiết kế mới không nhắc tới chúng; M0 viết bộ động tác mới sẽ quyết định. **Đã xoá ngày 2026-11-14, xem phần bổ sung cuối báo cáo.**
- `GolemWorld`, `GolemQuest`, `GolemDialogue` còn khung campaign cũ và 8 test của chúng. Không còn code nào gọi tới; M1 xử lý cùng lúc viết khu, manh mối và cờ cốt truyện.
- Hai sân thử Training và AbilityYard cùng map của chúng còn lại vì test Framework và GAS chạy trên đó.

## Chưa chạy

- Mở editor có giao diện và chơi thử bằng tay (cũng còn nợ từ R0).
- Test ở chế độ có render, cook, đóng gói, cấu hình DebugGame.

## Bổ sung 2026-11-14: bỏ parry, finisher, traversal, khoá mục tiêu và poise

Việc này để lại từ R1 và nằm trong bảng tiến độ M0. Bộ động tác của M0 (đòn nhẹ, đòn nặng, né, động tác class) đã xong, nên năm hệ thống cũ được xoá hẳn. Chưa commit.

### Đã xoá

- **`GolemCombat`:** `UGolemParryAbility`, `UGolemLockTargetAbility`, `UGolemFinisherAbility`, `UGolemTraversalAbility`, `UGolemFinisherComponent`, `UGolemTraversalComponent` (12 file). Trong `UGolemCombatComponent`: poise, stagger của Golem, cửa sổ parry, khoá mục tiêu và camera khoá. Trong combat profile: `MaxPoise`, `StaggerDuration`, `ParryWindow`, `PoiseDamage`, bốn montage stagger, traversal, finisher.
- **`GolemAbilitySystem`:** tham số `PoiseDamage` của `ApplyCombatDamage` và `IGolemDamageResponse::ResolveIncomingDamage`; cue Parry và Stagger.
- **`GolemCore`:** 14 tag: `Input.Combat.Parry`, `.LockOn`, `.Finisher`, `Input.World.Traverse`, bốn tag `Ability.*` tương ứng, `State.Combat.Parrying`, `.Finishing`, `.Traversing`, `Data.Damage.Poise`, `GameplayCue.Golem.Parry`, `.Stagger`.
- **`GolemFramework`, `GolemUI`:** phím C, chuột giữa, H, V và phần tay cầm của chúng; tuỳ chọn `HoldLock`; bốn dòng đổi phím (còn 21).
- **Nội dung:** `AM_Golem_Stagger`, `AM_Golem_Traversal`, `AS_Golem_Traversal`, `AM_Golem_Finisher`, `AM_Golem_FinisherVictim` và ba animation Mannequin chỉ chúng dùng (`MM_Jump`, `MM_HitReact_Front_Hvy_01`, `MM_Death_Front_01`).
- **Plugin MotionWarping** khỏi `.uproject`, ba file `Build.cs` và `AuditModules.ps1`.

### Giữ lại và đổi tên

- Tag `State.Combat.Staggered` còn, vì Xenon choáng khi bị lấp khoảng trống. Nó vẫn chặn ability combat và AI.
- `LockedTarget` thành `AttackTarget`, `LockRange` thành `TargetRange`: mục tiêu mà AI quay về khi ra đòn. Golem của người chơi không còn mục tiêu ngầm.
- `UGolemCameraModeComponent` còn nhưng hiện không ai dùng.

### Thay đổi hành vi

- **Golem không còn bị choáng** và không còn giật người khi trúng đòn.
- **Golem có 6 ability** từ hai ability set (trước là 10).
- **Lệnh Focus (phím B) của sân PartyArena cũ** chỉ vào kẻ địch còn sống gần nhất trong tầm nhìn, trong 22 m, thay cho mục tiêu đang khoá. Host vẫn kiểm lại ID, phe, tầm và tầm nhìn.
- **Boss của sân PartyArena cũ có `Power` 4** (trước là 10). Trước đây nó bị choáng gần như suốt trận vì poise; không còn poise thì nó hạ cả đội trong 27 giây ở test `Golem.AI.Arena.Playthrough` (lượt `R1b-test-2`). Đây là số của sân thử cũ, không phải số thiết kế.

### Dựng lại nội dung

Ability set `Combat`, combat profile, bốn action set và archetype boss của sân cũ được xoá rồi tạo lại bằng `-Action Bootstrap -Milestone 10`. Bước tạo `AS_Golem_Traversal` từng gặp ensure trên engine 5.8.4 không còn, nên mọi asset của game (trừ âm thanh) nay dựng lại được bằng Bootstrap.

### Kiểm tra đã chạy

| Việc | Kết quả |
|---|---|
| Build `ThreeDreamGolemsEditor` Win64 Development | Qua (`R1b-build-3`) |
| Build với `-DisableAdaptiveUnity` | Qua |
| `AuditModules.ps1` | Qua, 14 module |
| Bootstrap mốc 10 | Qua (`R1b-bootstrap-10b`) |
| Test `Golem.` | 56 test, 56 PASS (`R1b-test-3`). Hai lượt trước lỗi 2 rồi 1 test, đều ở sân PartyArena cũ: số ability 10 thành 6, mã lỗi của Focus vào đồng đội, và trận boss nói trên |

Số liệu: 263 file nguồn, khoảng 41.900 dòng (trước là 275 file, 43.600 dòng); 69 asset (trước là 77).

### Chưa chạy

- Mở editor có giao diện và chơi thử bằng tay; test ở chế độ có render; cook và đóng gói.
- Chơi thử Focus trên sân cũ bằng tay và trên nhiều máy.
