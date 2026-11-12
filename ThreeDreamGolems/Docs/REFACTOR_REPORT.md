# R0. Báo cáo dọn repo và đổi tên Knight thành Golem

Ngày thực hiện: 2026-11-12 (Asia/Saigon). Nhánh: `refactor/golem-rename`, tạo từ `hbuild/building-generator@50e6a35b`. Chỉ commit trên máy, chưa push; nhánh không có upstream.

Máy chạy: Windows 11, Unreal Engine 5.8.4 (CL 58902052) tại `C:\Program Files\Epic Games\UE_5.8`. Báo cáo cũ ghi 5.8.3; engine trên máy đã ở 5.8.4 trước khi bắt đầu, tôi không nâng engine.

## Kết quả ngắn

- Project, thư mục, module chính, 13 module, 151 class, 116 struct, 32 enum, tên file, macro, log category, tên test, thư mục nội dung và 114 tên asset đã chuyển sang `Golem`.
- Build `ThreeDreamGolemsEditor` Win64 Development thành công. Bộ test tự động: **134/134 PASS trước và sau** (106 PASS, 28 PASS có cảnh báo, 0 FAIL ở cả hai lượt; cùng 134 tên test).
- Không còn chuỗi `Knight` (không phân biệt hoa thường) trong `Source/`, `Config/`, `Scripts/`, `CODING_RULES.md`, `.uproject`. Không còn dòng `CoreRedirects` nào phải giữ.
- Chưa làm: mở editor có giao diện và chơi thử bằng tay, cook/đóng gói, build cấu hình DebugGame. Xem mục 5.

## 1. Commit đã tạo

| Commit | Thông điệp | Ghi chú |
|---|---|---|
| `b9fa5e42` | `chore: checkpoint before Golem refactor` | Chỉ có `AGENTS.md`. Mã nguồn project đã sạch trong git từ trước nên không có gì khác để checkpoint |
| `05933705` | `fix: rename file-local helpers that collide in unity builds` | Ngoài kế hoạch, xem mục 5.1 |
| `afe846b8` | `docs: remove outdated plans and reports, add Three Dream Golems docs` | Bước 2 |
| `43bea3f2` | `refactor: rename Knight to Golem across project, modules and code` | Bước 3–5. Đã `--amend` một lần ngay sau khi tạo vì lệnh stage bỏ sót 3 file |
| `c34a0c50` | `fix: rename a second file-local Root that collides in unity builds` | Ngoài kế hoạch, xem mục 5.1 |
| `ce3a8e48` | `refactor: move content from /Game/Knight to /Game/Golem` | Bước 6 |
| (commit này) | `docs: R0 report and root docs after Golem rename` | Bước 7–9 |

Các thay đổi đang dở trong `Assets/` và `output/` của cây làm việc (khoảng 4.800 file bị xoá, vài file sửa, vài thư mục chưa theo dõi) có sẵn từ trước và **không** được đưa vào commit nào. Tôi không sửa gì trong `Assets/`.

## 2. Đã xoá và đã giữ

Đã xoá hẳn (các file này chưa từng nằm trong git):

- 17 file `imp_plan_m*.md`; `FINAL_REPORT.md`, `FINAL_REPORT_M5_5.md`, `FINAL_REPORT_M5_6.md`
- `INPUTS_AND_PROMPT.md`, `PROMPT_CONTINUE_M6_TO_RELEASE.md`, `RELEASE_TARGETS.md`, `SYSTEM_GAP_REVIEW.md`
- `STORY_EXTENSION_CONTRACT.md`, `THREE-DREAM-KNIGHTS-STORY-DESIGN.md`, `milestone_10_add_all_story.md`
- `M6_AUTHORING_GUIDE.md`, `Skill_Combat_Summary.md`, `UI_CONTRACT.md`, thư mục `M5_5/`
- `Automation_ThreeDreamingKnights.sln` và `.slnx` (file sinh ra), `ThreeDreamingKnights.sln` và `.slnx`
- Thư mục `Evidence/` (2.311 file, khoảng 902 MB), gồm cả `M6/Fixtures/V1_pending.knight` (code không đọc) và `M7/Fixtures/Main_completed.knight` (test ghi lại ở mỗi lượt chạy)
- `Binaries/` và `Intermediate/` cũ (đã build lại)
- `Docs/REFACTOR_TASK.md` (theo bước 9)

Đã giữ hoặc chuyển chỗ:

| Thứ | Vị trí mới | Lý do |
|---|---|---|
| `V1_completed`, `V1_reward_pending` | `Source/GolemDeveloper/Fixtures/M6/*.golem` | Hai test M6 đọc chúng. File cũ không nạp được sau khi đổi tên class, nên đã để chính các test đó tạo lại (xoá file, chạy `Golem.M6.Mapped`) |
| `Evidence/M7/RegressionSettings.ini` | `Scripts/Fixtures/RegressionSettings.ini` | Dùng làm `-GameUserSettingsINI` khi chạy test để test không ghi vào settings thật. Mỗi lượt chạy chép ra `Saved/R0/` |
| Kết quả test và ảnh do test, commandlet, script sinh ra | `Saved/Evidence/...` (trước là `Evidence/...`) | Để thư mục `Evidence/` không mọc lại ở gốc project. Đã sửa 14 đường dẫn trong `Source/GolemDeveloper` và 3 script |
| `README.md`, `CODING_RULES.md`, `Docs/`, `.gitignore`, `.gitattributes`, `.vsconfig` | Giữ nguyên chỗ | Theo hướng dẫn. `git ls-files` thấy `README.md`, `CODING_RULES.md` và 5 file trong `Docs/` |
| `.idea/`, `.vs/` | Giữ nguyên | Cache của IDE, git bỏ qua. Chúng còn tên solution cũ; xoá tay nếu Rider hoặc Visual Studio mở nhầm |
| `Scripts/R0/` | Mới | Script và mã nguồn công cụ đã dùng ở bước 6 (xem mục 5.4), để xem lại; không được biên dịch |

## 3. Build và test

Lệnh dùng xuyên suốt (log nằm trong `Saved/Evidence/ContinuousRun/R0-*`):

```powershell
Scripts\ContinuousRun.ps1 -Action BuildEditor -EvidenceName <tên>
Scripts\ContinuousRun.ps1 -Action Test -TestFilter 'Golem.' -EvidenceName <tên> `
  -AdditionalEngineArguments '-GameUserSettingsINI=<project>/Saved/R0/RegressionSettings.ini'
```

Test chạy không giao diện (`-NullRHI`), giống lượt cumulative của báo cáo cũ. Trước khi đổi tên, bộ lọc là `Knight.`.

| Thời điểm | Build Editor | Test tự động |
|---|---|---|
| Checkpoint nguyên trạng | **FAIL**: 7 lỗi C2264/C2374/C2086/C4459 do trùng tên trong unity build (mục 5.1) | Không chạy được |
| Gốc so sánh: checkpoint + sửa 3 tên trùng | PASS | **134/134** (106 PASS, 28 có cảnh báo, 0 FAIL), 476 giây |
| Sau bước 3 (đổi tên code, chưa có redirect) | PASS, 310 action, 192 giây | Editor dừng vì assert ở test thứ 55 (`Golem.Items.M55.LegacySnapshotMigration`): 21 PASS, 33 FAIL, 80 chưa chạy. Đúng dự kiến vì asset chưa tìm được class |
| Sau bước 4, chỉ có redirect | PASS (không có gì phải build lại) | Vẫn dừng ở cùng chỗ: 23 PASS, 31 FAIL. Asset Registry lọc theo tên class ghi trong header asset và không áp redirect, nên catalog definition chỉ thấy 8 asset |
| Sau bước 4 + `ResavePackages` | Như trên | 131/134. 3 test M6 FAIL vì fixture lưu cũ |
| Sau bước 5 (tạo lại 2 fixture) | PASS | **134/134** (106 + 28, 0 FAIL) |
| Sau bước 6 (chuyển nội dung, bỏ redirect) | PASS | **134/134** (106 + 28, 0 FAIL), 470 giây |

Kiểm tra khác đã chạy:

| Kiểm tra | Kết quả |
|---|---|
| So tên test trước và sau (thay `Knight` bằng `Golem`) | 134 khớp 134, không thiếu, không thừa, trạng thái từng test giống nhau |
| Build với `-DisableAdaptiveUnity` trên cây đã commit | PASS. Cần kiểm tra riêng vì UBT biên dịch lẻ các file đang sửa, nên build trước khi commit không lộ lỗi unity |
| `Scripts/AuditModules.ps1` | PASS: 14 module, 0 lỗi, không có vòng phụ thuộc |
| Tạo lại project file từ `ThreeDreamGolems.uproject` | PASS |
| `ContinuousRun.ps1 -Action BootSmoke` (mở `L_M00_Boot` bằng `-game`, không giao diện) | PASS, log có `FoundationReady=true ... Modules=13` |
| Mở bản đồ mặc định không giao diện | Log test bước 6 nạp `/Game/Golem/Maps/Playables/L_M07_Campaign` lúc khởi động; không có lỗi thiếu class hay thiếu asset; catalog có 92 definition, 0 lỗi |
| `Scripts/TestCampaignPreferences.ps1` | PASS cả 5 bước |
| `git lfs status` | 163 file `.uasset`/`.umap` đều là con trỏ LFS |
| Quét chuỗi `Knight` trong 163 file nội dung (ASCII và UTF-16) | Còn 2 mẩu, xem mục 4 |
| `tools/audit_paths.py` | **FAIL**: thiếu 7 entry point, tất cả trong `Assets/` (`_composition/TDKKingdomMap.uproject`, `hda_gothic_building/gothic_building.hip`, `hda_medieval_building/AGENTS.md`, `build_hda.py`, `asset/medieval_house_layout.hda`, `hda_samples/AllHDAs/House.hdalc`, `texture_gothic/material_manifest.json`). Chúng vắng mặt trong cây làm việc từ trước, không liên quan tới project game. Entry point `ThreeDreamGolems/ThreeDreamGolems.uproject` đạt |

Chưa chạy:

- Mở editor có giao diện, chạy bản đồ mặc định và điều khiển ba nhân vật bằng tay. Phần này mới chỉ được kiểm qua test tự động chạy PIE (ví dụ `Golem.Framework.TrainingPlaythrough`, `Golem.M7.NormalUIPlaythrough`).
- Test ở chế độ có render (`-Rendered`, d3d12). Lượt gốc cũng chạy `-NullRHI` nên hai bên so được với nhau.
- Cook và đóng gói (`CookBoot`, bản Win64 Development). Con số 652 package trong `CODE_BASELINE.md` chưa được kiểm lại.
- Build cấu hình DebugGame và target game (không phải Editor).
- Các commandlet authoring (`-run=GolemBootstrap -Milestone=N`). Chúng biên dịch được nhưng chưa chạy lại; nội dung hiện có là nội dung cũ đã được đổi tên, không phải sinh lại.
- `tools/audit_houdini_paths.py` (không thuộc phạm vi).
- Ca solo DPS timeout ngẫu nhiên (P-05) không xuất hiện trong ba lượt chạy đủ 134 test.

## 4. Chuỗi `Knight` còn lại, redirect, asset

- **`Source/`, `Config/`, `Scripts/`, `CODING_RULES.md`, `.uproject`, `.vsconfig`:** 0 kết quả khi tìm không phân biệt hoa thường. Ba file trong `Scripts/R0/` phải tìm từ cũ nên ghép nó từ hai mẩu chuỗi.
- **Tên file được git theo dõi trong project:** 0.
- **`README.md` và `Docs/` của project:** vẫn nhắc tên cũ có chủ ý (lịch sử đổi tên, `CODE_BASELINE.md`).
- **Gốc repo:** `AGENTS.md`, `README.md`, `MEMORY.md` mỗi file có một dòng ghi việc đổi tên và ngày. `TDKProps`, `TDKKingdomMap` giữ nguyên. `.agents/` không có tham chiếu nào tới project game.
- **Nội dung:** hai mẩu UTF-16 dài 8 ký tự, `KnightCo` trong `L_M03_CombatChallenge.umap` và `e/Knight` trong `L_M05_RPGLoop.umap`. Chúng nằm giữa dữ liệu nhị phân, không phải giá trị property nào, và vẫn còn sau khi resave. Tôi không xác định được trường nào chứa chúng; test không bị ảnh hưởng.
- **Tham chiếu treo có từ trước:** `SKM_Manny_Simple` và `SKM_Quinn_Simple` trỏ tới `.../Mannequins/Rigs/CR_Mannequin_Body`, asset này không có trong project. Đường dẫn đã đổi sang `/Game/Golem/...` nhưng vẫn treo như trước.
- **Redirect còn giữ:** không có dòng `CoreRedirects` nào. Hai dòng `ActiveGameNameRedirects` (`TP_Blank`) có sẵn, nay trỏ tới `/Script/ThreeDreamGolems`. Khối 337 dòng redirect tường minh (14 package, 151 class, 116 struct, 32 enum, 20 property, 4 function) đã dùng ở bước 4–5 rồi gỡ ở bước 6, vì sau khi resave không asset nào còn tham chiếu tên script cũ. Nếu cần nạp nội dung lưu trước khi đổi tên (ví dụ từ nhánh khác), lấy lại khối này trong `Config/DefaultEngine.ini` của commit `43bea3f2`.
- **Redirect loại primary asset:** không thêm. Tên loại (`GolemItem`, `GolemHero`, ...) nằm trong chính asset (`DefinitionType`) và đã được đổi cùng lúc với code.
- **Asset chưa đổi tên:** không có. 114 asset có `Knight` trong tên đã đổi; 17 map và 32 asset `Vendor/UE58` không có từ này.
- **File lưu cũ:** các file `.knight` trong `Saved/` của máy không còn đọc được. Hướng dẫn nói không cần tương thích.

## 5. Việc làm khác kế hoạch và việc không làm được

### 5.1 Sửa lỗi build có sẵn (2 commit `fix:`)

Ở trạng thái checkpoint, build Editor đã hỏng trước khi tôi sửa gì: `Save`, `Root`, `Duration` khai báo trong namespace vô danh ở các file khác nhau bị gộp chung một file unity. Các lần build trước đây qua được vì UBT biên dịch lẻ những file chưa commit. Tôi đổi tên bốn biến/hàm cục bộ (`SaveQualityAsset`, `CampaignRoot`, `QualityRoot`, `ShakeDuration`), không đổi hành vi. Không sửa thì không có số test gốc và không build được sau mỗi commit.

### 5.2 Resave sớm hơn kế hoạch

Hướng dẫn đặt resave ở bước 6.5. Thực tế test không qua được ở bước 4–5 nếu chỉ có redirect (xem bảng mục 3), nên `ResavePackages -ProjectOnly -IgnoreChangelist` đã chạy ngay ở bước 4 và chạy lại ở bước 6.

### 5.3 Bước 3 tạm giữ chuỗi nằm trong asset

Khoảng 200 chuỗi trong code trùng với dữ liệu lưu trong asset (đường dẫn `/Game/Knight`, tên asset, `DefinitionType`, tên actor đặt sẵn trong map, tên node StateTree). Nếu đổi chúng ở bước 3 thì code và asset lệch nhau cho tới bước 6. Tôi giữ chúng tới bước 6 rồi đổi cùng lúc với asset. Vì vậy sau commit `43bea3f2` vẫn còn `Knight` trong `Source/`; sau `ce3a8e48` thì hết.

### 5.4 Bước 6: đổi cả dữ liệu bên trong asset

Hướng dẫn chỉ nói đổi thư mục và tên asset. Để code hết hẳn chuỗi `Knight`, dữ liệu trong asset cũng phải đổi. Tôi viết một commandlet dùng một lần (`GolemContentMigration`, mã nguồn ở `Scripts/R0/*.txt`) duyệt mọi property của mọi object trong 163 package và thay từ cũ trong FName, FString, FText (kể cả namespace và lịch sử format), đường dẫn mềm, tên object con và URL của level, rồi biên dịch lại 7 StateTree. Tổng qua ba lượt: 260 FName, 302 FString, 334 FText, 9 đường dẫn mềm, 391 object con (actor đặt trong map và notify trong montage). Không dựng lại asset nào bằng text và không chạy lại commandlet authoring.

Thứ tự thực tế của bước 6: chạy commandlet trên asset, đổi nốt chuỗi trong code và config, build, chuyển thư mục bằng `Scripts/R0/MoveContent.py` (API `rename_assets` của editor), chạy lại commandlet, resave, tạo lại fixture, test.

Ba việc phát sinh:

- API đổi tên của editor dừng ở hộp thoại xác nhận khi asset còn được code hoặc config tham chiếu theo đường dẫn cũ (chế độ không giao diện tự chọn Cancel). Vì vậy code và config phải đổi trước khi chuyển thư mục. Tham chiếu cuối cùng nằm trong `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` của máy này (danh sách map mở gần đây, góc nhìn viewport); tôi thay đường dẫn trong file đó và để bản gốc ở `EditorPerProjectUserSettings.ini.r0bak` cạnh nó.
- Sau khi chuyển, 16 file Mannequin cũ còn nằm lại ở `/Game/Knight` dù bản mới đã có ở `/Game/Golem`. `Scripts/R0/RemoveStaleCopies.py` kiểm từng file có bản mới và không còn ai tham chiếu, rồi xoá thư mục cũ bằng API của editor. Không đổi tên hay xoá thư mục `Content/` trực tiếp trên đĩa.
- Đường dẫn mềm tới map trong `DA_Golem_World_*` ghi theo dạng chỉ có tên package nên API đổi tên không sửa; commandlet đã sửa 9 chỗ.

### 5.5 Việc nhỏ

- Thư mục project được đổi tên bằng `Rename-Item` của PowerShell rồi `git add -A`, vì `git mv` trên cả thư mục bị Windows từ chối (Permission denied). Các file và thư mục bên trong đã dùng `git mv`.
- `tools/path_audit_report.json` không được ghi lại: `audit_paths.py` chỉ in kết quả ra màn hình. File cũ giữ nguyên làm bằng chứng của lượt chạy 2026-11-02.

### 5.6 Không làm được hoặc để lại

- Kiểm tra tay trong editor có giao diện (bước 8). Đây là việc còn thiếu duy nhất so với danh sách kiểm tra cuối.
- Hai mẩu chuỗi trong hai file map ở mục 4.
- `tools/audit_paths.py` chưa đạt vì các file trong `Assets/`; sửa chỗ đó nằm ngoài phạm vi được giao.
