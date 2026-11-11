# R0. Dọn repo và đổi tên Knight thành Golem

Cập nhật 2026-11-12. Đây là hướng dẫn cho agent chạy trên máy có repo (Codex hoặc Claude Code). Làm lần lượt từng bước. Xong thì viết báo cáo theo bước 9 và xoá file này trong commit cuối.

## Quyết định của chủ dự án

Chủ dự án đã duyệt ba điều sau vào ngày 2026-11-12. Chúng thay cho quy tắc cũ "giữ nguyên tên project và module".

1. **Đổi tên toàn bộ:** project, thư mục, module chính, 13 module `Knight*`, tiền tố class, tên file và thư mục nội dung.
2. **Xoá hẳn tài liệu và nội dung cũ.** Không lưu trữ lại.
3. **Cho phép commit tài liệu lên git:** `README.md`, `CODING_RULES.md` và mọi thứ dưới `Docs/`.

Chỉ commit trên máy. **Không push** khi chủ dự án chưa yêu cầu.

## Phạm vi

- **Trong phạm vi:** thư mục project game (hiện là `ThreeDreamingKnights/`), cùng `AGENTS.md`, `MEMORY.md`, `README.md` và `tools/` ở gốc repo.
- **Ngoài phạm vi:** `Assets/` (gồm `TDKProps`, `TDKKingdomMap`, các HDA Houdini). Giữ nguyên tên và nội dung ở đó. Không sửa plugin của engine hay bên thứ ba.
- **Không làm trong mốc này:** thay đổi gameplay. Việc đó thuộc R1 trở đi trong [MILESTONES.md](MILESTONES.md).

## Bảng đổi tên

Áp dụng theo đúng thứ tự từ trên xuống, có phân biệt hoa thường.

| # | Cũ | Mới | Ghi chú |
|---|---|---|---|
| 1 | `ThreeDreamingKnights` | `ThreeDreamGolems` | Thư mục, `.uproject`, module chính, hai file `.Target.cs`, tên class target |
| 2 | `THREEDREAMINGKNIGHTS` | `THREEDREAMGOLEMS` | Macro `_API` của module chính |
| 3 | `Three Dreaming Knights`, `Three Dream Knights` | `Three Dream Golems` | Chuỗi hiển thị |
| 4 | `KNIGHT` | `GOLEM` | Macro `KNIGHTCORE_API` và tương tự |
| 5 | `Knights` | `Golems` | |
| 6 | `Knight` | `Golem` | Module, class, struct, enum, file, log category |
| 7 | `knight` | `golem` | Xem danh sách trước khi thay: đuôi file lưu `.knight`, khoá config, tên slot |

Trước khi chạy dòng 7, in ra mọi chỗ có `knight` chữ thường và đọc qua. Từ nào không phải tên riêng của project thì giữ.

## Bước 1. Chuẩn bị

1. Đóng Unreal Editor, Rider và Visual Studio.
2. Chạy `git status`. Báo cáo cũ ghi rằng công việc từ mốc M6 trở đi chưa được commit, nên cây làm việc có thể đang có nhiều thay đổi.
3. Tạo nhánh `refactor/golem-rename` từ trạng thái hiện tại.
4. Commit nguyên trạng những gì `.gitignore` hiện cho phép, với thông điệp `chore: checkpoint before Golem refactor`. Đây là điểm quay lại cho mã nguồn.
5. Ghi lại số test hiện có để so sánh sau: chạy bộ test tự động một lần nếu máy build được, lưu số PASS và FAIL.

## Bước 2. Xoá tài liệu và nội dung cũ

Các file dưới đây chưa từng được commit vì bị `.gitignore` chặn, nên xoá là mất hẳn. Chủ dự án đã chấp nhận điều đó. Những thông tin còn giá trị đã được chép sang [CODE_BASELINE.md](CODE_BASELINE.md).

Xoá trong thư mục project:

- Mọi file `imp_plan_m*.md` (17 file)
- `FINAL_REPORT.md`, `FINAL_REPORT_M5_5.md`, `FINAL_REPORT_M5_6.md`
- `INPUTS_AND_PROMPT.md`, `PROMPT_CONTINUE_M6_TO_RELEASE.md`
- `RELEASE_TARGETS.md`, `SYSTEM_GAP_REVIEW.md`
- `STORY_EXTENSION_CONTRACT.md`, `THREE-DREAM-KNIGHTS-STORY-DESIGN.md`, `milestone_10_add_all_story.md`
- `M6_AUTHORING_GUIDE.md`, `Skill_Combat_Summary.md`, `UI_CONTRACT.md`
- Thư mục `M5_5/`
- Hai file solution `Automation_ThreeDreamingKnights.sln` và `.slnx` nếu chúng là file sinh ra

Giữ lại: `README.md`, `CODING_RULES.md`, `Docs/`, `.gitignore`, `.gitattributes`, `.vsconfig`.

**Thư mục `Evidence/`:** trước khi xoá, tìm trong `Source/` và `Scripts/` mọi tham chiếu tới `Evidence`. Nếu test hoặc script đọc fixture từ đó, chuyển đúng các file được đọc vào `Source/GolemDeveloper/` hoặc `Scripts/` rồi sửa đường dẫn. Sau đó xoá `Evidence/`. Nếu không chắc một file có được dùng hay không thì giữ file đó và ghi vào báo cáo.

`Saved/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Build/` là sản phẩm build. Xoá `Binaries/` và `Intermediate/` ở bước 5; không cần động tới phần còn lại.

Commit: `docs: remove outdated plans and reports, add Three Dream Golems docs`. Commit này gồm cả `.gitignore` mới, `README.md`, `CODING_RULES.md` và `Docs/`.

## Bước 3. Đổi tên mã nguồn

1. **Đổi nội dung file văn bản** theo bảng đổi tên. Áp dụng cho: `.h`, `.cpp`, `.cs`, `.ini`, `.uproject`, `.ps1`, `.py`, `.json`, `.md`, `.vsconfig`.
   - Bỏ qua: `.git/`, `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `Content/`, `.vs/`, `.idea/`, và toàn bộ `Assets/`.
   - Bỏ qua thư mục `Docs/` và file `README.md` của project: chúng đã viết theo tên mới và có nhắc tên cũ có chủ ý.
   - `CODING_RULES.md` có được thay.
2. **Đổi tên file và thư mục** bằng `git mv`, đi từ sâu nhất lên: file trong `Source/`, rồi 13 thư mục module, thư mục module chính, hai file `.Target.cs`, file `.uproject`, cuối cùng là thư mục project thành `ThreeDreamGolems/`.
3. **Kiểm tra các chỗ dễ sót:**
   - Mỗi file `*.Build.cs`: tên class và danh sách module phụ thuộc.
   - File `.uproject`: 14 tên module.
   - Macro `IMPLEMENT_MODULE` và `IMPLEMENT_PRIMARY_GAME_MODULE`.
   - Tên log category, tên test tự động (bộ lọc test cũ bắt đầu bằng `Knight`).
   - GameplayTag khai báo trong C++ hoặc `.ini` có gốc `Knight`.
   - `Scripts/AuditModules.ps1`, `Scripts/ContinuousRun.ps1`, `Scripts/TestCampaignPreferences.ps1`.
4. Tìm lại `Knight` trong `Source/`, `Config/`, `Scripts/`. Kết quả mong đợi: chỉ còn trong các dòng redirect ở bước 4.

## Bước 4. Redirect cho asset

Asset nhị phân trong `Content/` còn tham chiếu tới tên class và module cũ. Thêm vào `Config/DefaultEngine.ini`:

```ini
[CoreRedirects]
+PackageRedirects=(OldName="/Script/ThreeDreamingKnights",NewName="/Script/ThreeDreamGolems")
+PackageRedirects=(OldName="/Script/KnightCore",NewName="/Script/GolemCore")
; ...một dòng cho mỗi module trong 13 module Knight*
+ClassRedirects=(OldName="Knight",NewName="Golem",MatchSubstring=true)
+StructRedirects=(OldName="Knight",NewName="Golem",MatchSubstring=true)
+EnumRedirects=(OldName="Knight",NewName="Golem",MatchSubstring=true)
```

Lưu ý:

- Xác nhận cú pháp `MatchSubstring` với tài liệu Core Redirects của đúng phiên bản engine đang dùng trước khi dựa vào nó. Nếu engine không hỗ trợ cho một loại redirect, sinh danh sách redirect tường minh từ các tên class, struct, enum có trong `Source/`.
- Property và function của Blueprint có `Knight` trong tên cũng cần redirect. Liệt kê chúng từ các macro `UPROPERTY` và `UFUNCTION` đã đổi tên.
- Sửa hai dòng `ActiveGameNameRedirects` hiện có để trỏ tới `/Script/ThreeDreamGolems`.
- Trong `Config/DefaultGame.ini`, loại primary asset `KnightDefinition` đổi thành `GolemDefinition`. Thêm redirect loại primary asset trong `AssetManagerSettings` nếu asset hoặc file lưu còn dùng tên cũ.
- Sửa `GameInstanceClass`, `GlobalDefaultGameMode`, `GameUserSettingsClassName`, `AssetManagerClassName` theo tên mới.
- Không cần giữ tương thích với file lưu cũ. Fixture lưu có đuôi `.knight` dùng cho test thì tạo lại hoặc đổi đuôi cùng với code đọc nó.

Redirect kiểu khớp chuỗi con làm chậm lúc tải. Chúng là tạm thời: sau khi resave ở bước 6, thử gỡ ba dòng `MatchSubstring` và mở lại project. Nếu sạch lỗi thì bỏ hẳn.

## Bước 5. Build và test

1. Xoá `Binaries/`, `Intermediate/`, các file `.sln` và `.slnx`. Tạo lại project file từ `.uproject` mới.
2. Build `ThreeDreamGolemsEditor`, Win64, Development. Sửa lỗi biên dịch do đổi tên cho tới khi build xong.
3. Mở bản đồ mặc định bằng editor không giao diện và đọc log: không được có lỗi thiếu class hay thiếu asset.
4. Chạy bộ test tự động với bộ lọc mới (`Golem`). So với số ghi ở bước 1. Báo cáo cũ ghi 134/134 PASS; ca solo DPS trong ma trận encounter từng timeout ngẫu nhiên, nếu gặp lại thì ghi nhận, không sửa trong mốc này.
5. Chạy `Scripts/AuditModules.ps1`: 14 module, không có vòng phụ thuộc.

Commit: `refactor: rename Knight to Golem across project, modules and code`.

## Bước 6. Đổi thư mục nội dung

Làm sau khi bước 5 đã sạch. Đây là commit riêng để có thể lùi lại độc lập.

1. Trong editor, chạy bằng Python (plugin PythonScriptPlugin đã bật): đổi `/Game/Knight` thành `/Game/Golem` bằng API đổi tên thư mục của editor, sửa redirector, lưu tất cả. **Không** đổi tên thư mục `Content/Knight` trực tiếp trên đĩa.
2. Trong cùng lượt, đổi tên những asset có `Knight` trong tên. Nếu số lượng lớn hoặc có rủi ro, để lại và liệt kê trong báo cáo.
3. Sửa đường dẫn trong `Config/`: `GameDefaultMap`, `EditorStartupMap`, 12 dòng `MapsToCook`, `DirectoriesToAlwaysCook`, `GameplayCueNotifyPaths`, `Directories` của `PrimaryAssetTypesToScan`.
4. Sửa mọi đường dẫn `/Game/Knight` viết cứng trong `Source/` và `Scripts/`.
5. Resave toàn bộ package để ghi tên class mới vào asset. Build lại, mở bản đồ mặc định, chạy lại bộ test.
6. File `.uasset` và `.umap` do git-lfs quản lý: kiểm tra `git lfs status` trước khi commit.

Nếu bước này thất bại và không sửa được trong phạm vi hợp lý, hoàn tác riêng bước 6, giữ `/Game/Knight` và ghi rõ trong báo cáo.

Commit: `refactor: move content from /Game/Knight to /Game/Golem`.

## Bước 7. Cập nhật gốc repo

1. `AGENTS.md`: sửa đường dẫn project thành `ThreeDreamGolems/`; bỏ đoạn tạm nói về mốc R0 đang chờ; giữ quy tắc không đổi tên `TDKProps`, `TDKKingdomMap`.
2. `README.md` và `MEMORY.md`: sửa đường dẫn và tên project; thêm một dòng ghi ngày đổi tên.
3. `tools/audit_paths.py` và cấu hình của nó: sửa đường dẫn, rồi chạy `python tools/audit_paths.py`. Các file báo cáo JSON cũ trong `tools/` là kết quả của lượt chạy trước, để script tự ghi lại.
4. Tìm `ThreeDreamingKnights` trong `.agents/`. Chỉ sửa đường dẫn tới project game; không sửa nội dung skill Houdini.

## Bước 8. Kiểm tra cuối

- `git status` thấy `README.md`, `CODING_RULES.md` và `Docs/**` được theo dõi.
- Tìm `Knight` (không phân biệt hoa thường) trong `Source/`, `Config/`, `Scripts/` và các file ở gốc repo. Chỗ nào còn thì phải có lý do, ghi trong báo cáo.
- Editor mở được, bản đồ mặc định chạy được, ba nhân vật điều khiển được.
- Chưa push.

## Bước 9. Báo cáo

Tạo `Docs/REFACTOR_REPORT.md` gồm:

1. Danh sách commit đã tạo.
2. Những gì đã xoá, và những gì giữ lại kèm lý do.
3. Kết quả build và test: lệnh đã chạy, số PASS và FAIL trước và sau. Thứ gì chưa chạy thì ghi là chưa chạy.
4. Những chỗ còn chuỗi `Knight`, redirect còn giữ, asset chưa đổi tên.
5. Việc không làm được và lý do.

Cập nhật trạng thái R0 trong [MILESTONES.md](MILESTONES.md) và dòng "Mốc đang làm" trong `README.md` của project. Xoá file `REFACTOR_TASK.md` này.

Commit: `docs: R0 report and root docs after Golem rename`.

## Khi gặp vấn đề

- Không tạo file `.uasset` hoặc `.umap` bằng text để vá lỗi thiếu asset.
- Không tắt test để có kết quả PASS. Test của hệ thống sắp bỏ vẫn giữ tới mốc R1.
- Nếu một bước không làm được, dừng ở trạng thái build được gần nhất, ghi lại trong báo cáo và hỏi chủ dự án.
