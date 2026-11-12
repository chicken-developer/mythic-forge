# CODING_RULES — Unreal Engine Project Template

Template version: 3.0 · Scope: Unreal Engine C++ / Blueprint / Assets

## 1. Mandatory AI contract

MUST đọc toàn bộ `CODING_RULES.md`, `{ArchitectureFile}` và `{TaskFile}` đang làm trước khi generate/sửa code. File này đặt tại repository root; trong task pack có thể nằm cạnh các task. Dev đính kèm cả ba nếu AI chưa có repo. Nếu attachment khác bản trong repo, so sánh version/nội dung; không âm thầm chọn bản cũ.

- Thiếu file này: tìm đúng file trong repo/attachments; nếu vẫn thiếu, báo `BLOCKED_MISSING_CODING_RULES` và yêu cầu cung cấp trước khi sinh code. Không tự dựng lại rules theo trí nhớ.
- Khi resume sau context compaction hoặc nhận sửa code ở lượt mới, đọc lại file hiện hành trước khi sửa; không cần lặp lại sau mỗi tool call trong cùng lượt.
- Subagent được giao code phải nhận path và yêu cầu đọc cùng rules. Agent chính review integrated diff theo rules trước handoff.
- Đầu lượt xác nhận ngắn rules version/path và scope toàn roadmap. Giữa các chặng ghi compliance/evidence nội bộ; chỉ xuất báo cáo tổng ở cuối lượt theo workflow architecture. Không claim kiểm tra tự động nếu chỉ review thủ công.
- User decision/ADR được duyệt quản lý ngoại lệ; Tài liệu architecture quản lý architecture, file này quản lý naming/style và coding guardrails, milestone quản lý scope/acceptance. Xung đột architecture không được tự ghi đè; giữ contract hiện tại, ghi decision pending và làm tiếp phần độc lập theo workflow. Không lấy ví dụ/template làm lý do ghi đè project contracts.
- MUST là bắt buộc. SHOULD là mặc định, được lệch khi có lý do kỹ thuật ghi trong report. Không sửa rules hoặc disable checks chỉ để làm thay đổi hiện tại pass.

## 2. Áp dụng template vào project

Template này không xác định tên game, engine version, module count, gameplay model hoặc lựa chọn framework. Các thông tin đó lấy từ project architecture và repository thực tế.

| Tham số | Nguồn xác định / cách dùng |
|---|---|
| `{ProjectPrefix}` | Prefix type đã được project chấp nhận; nếu project cố ý không dùng prefix thì bỏ phần này, không tự tạo prefix mới |
| `{PROJECT_PREFIX}` | Dạng UPPER_SNAKE_CASE tương ứng dùng cho project macros |
| `{Module}` / `{MODULE_API}` | Module chứa code và export macro thực tế |
| `{ArchitectureFile}` | Tài liệu architecture do project chỉ định; mặc định `README.md` nếu file đó chứa architecture |
| `{TaskFile}` | File task/milestone hiện tại; không bắt buộc một naming scheme milestone cụ thể |
| Engine / toolchain version | `.uproject`, build configuration và environment được xác nhận |
| Optional systems | Chỉ áp dụng GAS, Gameplay Tags, multiplayer, UI framework, Niagara/Houdini nếu project sử dụng |

Các token trong `{...}` chỉ là ký hiệu template: resolve trước khi generate code, không để nguyên vào source/assets. Không rename existing types để khớp ví dụ. Nếu thông tin đã có trong repo, tự đọc và áp dụng; ghi pending khi mâu thuẫn hoặc thiếu quyết định ảnh hưởng public contracts, tiếp tục phần độc lập; chỉ dừng sớm nếu không còn đường tiếp tục hợp lệ. Ví dụ generic dưới đây minh họa style, không yêu cầu project triển khai feature đó.

File này quy định coding style/guardrails; vị trí ASC, identity ownership, số nhân vật, AI framework, UI architecture, network topology và persistence model phải theo architecture của project. Không tự cài thêm framework chỉ để áp dụng template.

## 3. Naming C++ — MUST

Identifiers và comments code dùng tiếng Anh rõ nghĩa; report trao đổi với dev có thể tiếng Việt. Dùng thuật ngữ domain trong architecture; phân biệt player, controller, character và entity theo ownership của project.

| Đối tượng | Convention | Ví dụ |
|---|---|---|
| Actor | `A` + `{ProjectPrefix}` + role | `A{ProjectPrefix}Character`, `A{ProjectPrefix}PartyState` |
| UObject / component | `U` + `{ProjectPrefix}` + role | `U{ProjectPrefix}AbilitySystemComponent`, `U{ProjectPrefix}EquipmentComponent` |
| Struct | `F` + `{ProjectPrefix}` + role | `F{ProjectPrefix}ItemInstance`, `F{ProjectPrefix}InventoryResult` |
| Enum | `E` + `{ProjectPrefix}` + concept | `E{ProjectPrefix}LifeState::Downed` |
| Interface | U/I pair với cùng stem | `U{ProjectPrefix}InteractableInterface`, `I{ProjectPrefix}InteractableInterface` |
| Delegate type / instance | `FOn{ProjectPrefix}<Event>` / `On<Event>` | `FOn{ProjectPrefix}HealthChanged`, `OnHealthChanged` |
| Function | PascalCase, động từ + đối tượng | `ApplyDamage`, `GrantAbilitySet`, `TryEquipItem` |
| Bool member/local/parameter | b + PascalCase | `bIsDowned`, `bCanRevive` |
| Bool query | Is/Has/Can + concept | `IsDowned()`, `HasAuthority()`, `CanRevive()` |
| Other member/local/parameter | PascalCase, không m_/s_/trailing underscore | `TargetActor`, `NewHealth`, `ItemDefinition` |
| Parameter nhập có nguy cơ shadow | In + concept | `In{ProjectPrefix}Id` |
| Output parameter | Out + concept | `OutFailureReason` |
| RPC | Server/Client/Multicast + action | `ServerRequestEquipItem` |
| RepNotify | OnRep_ + property | `OnRep_LifeState` |
| Event handler | Handle + event | `HandleHealthChanged` |
| Log category | Log{ProjectPrefix} + domain | `Log{ProjectPrefix}Combat` |
| Project macro | {PROJECT_PREFIX}_ + UPPER_SNAKE_CASE | `{PROJECT_PREFIX}_ATTRIBUTE_ACCESSORS` |

Engine-required overrides/types/macros giữ nguyên tên; không rename engine function để hợp policy. `UFUNCTION` implementation dùng `_Implementation`, `_Validate` chỉ khi API/specifier thực tế yêu cầu. Gameplay tag symbols được phép underscore để phản ánh hierarchy. Tên GAS/UE phổ biến như ASC, HUD, UI được phép; tránh viết tắt khó đoán như TmpObj/Mgr/DoStuff.

File theo tên type chính, bỏ Unreal type prefix: `{ProjectPrefix}EquipmentComponent.h/.cpp`. Một reflected primary class mỗi header; struct nhỏ liên quan có thể cùng header. Enum reflected dùng `UENUM(BlueprintType)` + `enum class ... : uint8` khi Blueprint cần; non-reflected enum chọn underlying type theo dữ liệu. Không reuse giá trị enum đã persist cho ý nghĩa khác.

## 4. Layout, formatting và includes — MUST

- Allman braces: `{` xuống dòng cho type/function/if/for/while/switch. Luôn braces cho control blocks, kể cả guard return.
- Indent bằng tab, hiển thị 4 spaces. Không trộn tabs/spaces ở đầu dòng; spaces cho căn continuation khi cần. UTF-8, newline cuối file, không trailing whitespace. Tôn trọng newline policy hiện có của repo.
- SHOULD giới hạn khoảng 120 ký tự/dòng; wrap signatures/specifiers/logs dài tại ranh giới dễ đọc. Không phá macro vì ép line length.
- Space sau dấu phẩy và quanh binary operators: `Category = "{ProjectPrefix}|Combat"`, `Health > 0.0f`. Float dùng `0.0f`, khởi tạo `nullptr`, không magic numbers gameplay.
- `#pragma once`; header tự đủ dependencies. Include định nghĩa parent và value types cần complete type; forward declare pointer/reference types nếu phù hợp UHT.
- `<File>.generated.h` là include cuối cùng trong header reflected. Không include header sau nó.
- `.cpp` include matching header đầu tiên, rồi project/engine/plugin dependencies thành nhóm nhất quán; không dựa vào transitive includes hoặc unity build.
- `GENERATED_BODY()` ở đầu reflected type. Access sections `public`, `protected`, `private`; API trước, lifecycle/extension hooks tiếp theo, implementation data/helpers sau. Giữ function definitions theo thứ tự hợp lý với declaration.
- Dùng `override` cho overrides; `const` cho queries không mutate observable state; truyền struct/container đọc-only bằng `const T&` khi có ích. Không ép reference cho mọi scalar/handle nhỏ.
- Tránh `auto` khi che type quan trọng với ownership; cho phép iterator hoặc template type quá dài. Dùng UE containers/types cho UE-facing APIs: `TArray`, `TMap`, `FName`, `FText`, `int32`.
- Không commented-out code, empty doc blocks hoặc TODO thay implementation bắt buộc. Comment giải thích contract/lý do, không lặp lại từng dòng code.

## 5. Modules và header boundaries

`Source/<Module>/Public/<Domain>/` chỉ chứa API thật sự cross-module. Private implementation/header ở `Private/<Domain>/`; public/private .h/.cpp tương ứng dễ tìm.

- Export macro đúng module: `{MODULE_API}`; không dùng export macro của module khác.
- Public Build.cs dependencies chỉ cho types/includes public API cần; private implementation dependencies dùng PrivateDependencyModuleNames. Runtime không phụ thuộc editor-only modules. Không thêm circular dependency để chữa compile error.
- Giữ module boundaries trong architecture; module nền tảng không import domain cấp cao. Không áp đặt số module hoặc topology từ template.
- Cross-domain commands/interfaces/events phải có owner rõ; không tạo library/subsystem global chứa mọi gameplay function. Không public header include toàn bộ subsystem graph cho tiện.
- Không blanket reformat/rename ngoài scope milestone. Nếu rename reflected classes/properties/assets hiện có, xử lý serialized references/redirects/migration phù hợp và verify assets.

## 6. Reflection, pointers và data ownership

- `UPROPERTY()` + `TObjectPtr<T>` cho strong UObject member references cần GC tracking trong reflected owners; `TWeakObjectPtr<T>` cho observer/non-owning references; `TSoftObjectPtr<T>`/`TSoftClassPtr<T>` khi cần asset reference load theo nhu cầu.
- Raw `T*` hợp lệ cho borrowed parameters/returns/local variables; không dùng raw member untracked để giữ UObject qua frames. `TObjectPtr` không thay thế stable IDs trong save/replication contracts.
- `TSubclassOf<T>` cho constrained class references. `CreateDefaultSubobject` trong constructor cho default components; `NewObject`/`SpawnActor` đúng lifecycle; không `new/delete` UObject.
- `VisibleAnywhere` cho component refs không muốn thay thế; `EditDefaultsOnly` cho tunable class defaults; `EditInstanceOnly` chỉ cho instance customization chủ ý. Không mặc định `EditAnywhere, BlueprintReadWrite` cho mọi field.
- Authoritative mutable state private/protected, expose query hoặc validated command. BlueprintReadOnly không thay thế server validation. Blueprint events dành cho extension/presentation theo contract, không trao ownership progression cho UI/Level Blueprint.
- Blueprint categories dùng `{ProjectPrefix}|<Domain>|<Group>`. Tooltip/doc cho exposed parameters có units/range/ownership quan trọng. User-facing text dùng `FText` và localization path; IDs dùng typed ID/FName/tag phù hợp, không dùng localized text làm key.

## 7. GAS, Gameplay Tags, AI và networking — khi áp dụng

Bỏ qua yêu cầu riêng của hệ thống project không sử dụng; không thêm dependencies chỉ để thỏa checklist. Authority rules áp dụng theo network model đã duyệt.

- Nếu dùng GAS, vị trí ASC, OwnerActor/AvatarActor và initialization/reinitialization theo architecture. Làm rõ hành vi khi possession, respawn hoặc travel; không mặc định ASC ở Character hay PlayerState.
- Nếu dùng Gameplay Tags, khai báo qua registry được project chọn; không tạo nhiều cơ chế đăng ký song song cho cùng taxonomy.
- Tag path PascalCase phân cấp: `Ability.Primary.Activate`, `State.Combat.Downed`, `Input.Ability.Primary`; symbol có thể `Ability_Primary_Activate`. Đây là ví dụ taxonomy; reuse tags hiện hành trước khi thêm.
- Input tags diễn tả hành động, không hardcode `LMB`/phím số làm semantic ability identity; bindings nằm ở input configuration của project.
- Attributes replication đúng GAS API, RepNotify và accessor conventions; grant/revoke giữ source handles và idempotency. Không dùng GAS làm database cho ví tiền, sách, tarot, cờ cốt truyện hay save.
- Ability end/cancel cleanup tasks, delegates, reservations, input/camera state theo owner. AI framework gọi action qua contract của project; tái sử dụng action logic khi hành vi tương đương.
- RPC names thể hiện hướng; server validate ownership, target, range/state, cost/revision thích hợp. `Reliable` có lý do, không dùng cho stream cosmetic tần suất cao. Không thêm RPC chỉ vì dữ liệu có thể replicate bằng state.
- RepNotify phục vụ state convergence/presentation; không đặt reward transaction vào callback có thể chạy lại. Late join nhận persistent state; multicast event không thay snapshot/state replication.
- Damage/progression/collision gameplay do authoritative domain quyết định. Client VFX không quyết định hit/damage; không replicate từng particle cosmetic.
- Kiểm tra API theo version engine/plugin đang cài. Không đoán methods/properties hoặc sao chép API từ version khác rồi coi code là compile-ready.

## 8. Lifecycle, error handling và UI

- Bind delegates/timers có teardown đối xứng; lưu handles khi cần. Chỉ unbind registration do owner tạo, không RemoveAll trên shared object làm mất listener khác.
- Async callback giữ weak owner/cancellation token; kiểm tra lifetime và request generation để không apply stale result sau travel/owner change. Không capture raw `this` vào callback có thể sống lâu hơn owner mà thiếu bảo đảm lifetime.
- UI: initial snapshot rồi subscription; unbind/rebind khi controlled entity/source đổi. Không actor polling bằng widget Tick. Dùng UI architecture đã được project chọn; không tự đưa thêm presentation architecture song song.
- Queries không có side effects ngầm. `Try...` trả bool + failure details hoặc typed result khi có nhiều lý do; không bool success giả.
- `check/checkf` cho invariant lập trình, `ensure` cho invariant recoverable phù hợp; dữ liệu user/network/save lỗi phải trả failure an toàn, không assert thay validation.
- Logs theo category domain, có stable ID/context hữu ích, tránh spam Tick và secrets. Không nuốt failure; report error ở boundary chịu trách nhiệm.
- Giao dịch DreamCoin (mua sách, nâng vũ khí, mua dream stone) và phần thưởng đêm phải giữ transaction/atomicity/revision semantics của architecture; không cập nhật UI trước rồi coi đó là authoritative result.

## 9. Assets, Blueprint và VFX naming

Asset pattern: `<Prefix>_<DomainOrSubject>_<Purpose>[_Variant]`. PascalCase sau prefix, ASCII, không spaces, không tên `NewBlueprint`, `FinalFinal`, `Copy`.

| Asset | Prefix / ví dụ |
|---|---|
| Actor Blueprint / component BP | `BP_{ProjectPrefix}TrainingDummy` / `BPC_{ProjectPrefix}Interaction` |
| Gameplay Ability / Effect / Cue | `GA_Action_LightAttack`, `GE_Combat_Damage`, `GC_Action_Impact` |
| Data Asset / Data Table | `DA_{ProjectPrefix}_IceDefinition`, `DT_Items_Balance` |
| Input Action / Mapping Context | `IA_Combat_Dodge`, `IMC_{ProjectPrefix}_Gameplay` |
| Widget / Anim Blueprint / Montage | `WBP_Gameplay_HUD`, `ABP_{ProjectPrefix}`, `AM_Action_Combo` |
| Static / Skeletal mesh | `SM_Arena_Pillar`, `SK_{ProjectPrefix}` |
| Material / instance / function | `M_Effect_Master`, `MI_Effect_Ice`, `MF_Effect_Mask` |
| Texture | `T_Effect_Ice_BaseColor`, `T_Effect_Ice_Normal` |
| Niagara System / Emitter | `NS_Action_Impact`, `NE_Action_Dust` |
| Map | `L_TestArena` |
| HDA source file / Houdini scene | `HDA_{ProjectPrefix}_BuildingGenerator.hda`, `{ProjectPrefix}_BuildingGenerator.hip` |

Có convention từ asset pack ngoài thì giữ nguyên trong namespace vendor, không rename hàng loạt. Tên generated HDA output phải ổn định theo publish contract; seed/version lưu metadata/preset, không tạo tên ngẫu nhiên mỗi lần bake. HDA source/cache tách khỏi runtime content; runtime references trỏ output đã publish. Không buộc dùng Houdini nếu milestone không cần.

Blueprint graphs dùng functions/components có trách nhiệm rõ; variables/functions theo naming C++ tương ứng. Không fake `.uasset/.umap` bằng text. Asset placeholders phải hợp lệ và nối được vào playable; debug assets không trở thành hard dependency shipping vô ý.

## 10. Ví dụ formatting dùng chung

Ví dụ độc lập minh họa tên biến, const input và guard clause; không đại diện gameplay contract hoặc feature cần triển khai:

```cpp
bool IsIndexInRange(const int32 Index, const int32 Count)
{
	if (Count <= 0)
	{
		return false;
	}

	return Index >= 0 && Index < Count;
}
```

Trong code thật, đặt helper vào owner/namespace phù hợp và ưu tiên API sẵn có của container khi đã giải quyết cùng nhu cầu.

## 11. Definition of compliance — checks nội bộ và bàn giao tổng

- [ ] Đã đọc đúng rules version/path, architecture và task; không ghi đè architecture bằng ví dụ trong template.
- [ ] Naming, prefixes, file paths, access/export macros, formatting và includes đúng rules.
- [ ] Header dependencies/Build.cs không circular; editor tooling không lọt runtime.
- [ ] Reflection, GC references, async callbacks, delegates/timers có lifetime/cleanup rõ.
- [ ] Authority/RPC/replication/GAS/input/UI tuân thủ contract; không placeholder success.
- [ ] Assets có naming/ownership rõ, không fake files; acceptance và regression của task vẫn đạt; playable checks áp dụng khi task có gameplay deliverable.
- [ ] Review integrated diff; chạy UHT/UBT và affected checks khi khả dụng. `git diff --check` chỉ kiểm whitespace, không chứng minh toàn bộ style/architecture đúng.
- [ ] Report check đã chạy, kết quả/evidence và NOT_RUN/BLOCKED chính xác. Build pass không thay dev manual acceptance.
- [ ] Không stage/commit/push hoặc refactor ngoài scope nếu chưa được yêu cầu.

File Markdown này là instruction contract, không phải linter tự động. Nếu sau này bổ sung formatter/linter/CI, cấu hình phải phản ánh rules đã duyệt và ghi rõ coverage; không tuyên bố các check chưa tồn tại.

## 12. C++-first và thực thi liên tục

Template không hardcode tên project, số milestone hoặc engine version; lấy từ architecture. Khi project yêu cầu chạy toàn roadmap, `{TaskFile}` là chặng hiện tại trong cùng long task, không là điểm dừng.

- Đọc toàn roadmap rồi thực hiện tuần tự; giữa các chặng tự build/test/fix, update checkpoint và đi tiếp. Không yêu cầu manual acceptance/commit cho từng chặng. Progress updates vẫn được phép.
- Ưu tiên C++ cho logic, components, framework, abilities, AI tasks, data services, UI behavior/ViewModels, tests và editor automation khi API hỗ trợ. Không trì hoãn logic native khả thi thành việc dev nối Blueprint.
- Dùng C++ defaults/config/reflected properties và data definitions để giữ tuning data-driven. Không hardcode theo tên actor/nhân vật/map để tránh tạo asset.
- Blueprint/content assets chỉ làm lớp composition/presentation/data cần thiết; animation/material/Niagara/StateTree/map assets phải hợp lệ. Ưu tiên C++ không cho phép thay framework đã chọn hoặc fake binary assets.
- Native widgets và editor factories/commandlets có thể giảm wiring thủ công; chỉ dùng API được engine thực hỗ trợ. Python/build scripts được phép cho authoring/orchestration nếu phù hợp. Script chưa execute phải ghi chưa verify.
- Ưu tiên complete integrated behavior; compile pass không thay playable/runtime verification. Nếu thiếu môi trường/assets/accounts, ghi pending với tác động và tiếp tục phần độc lập; không dùng stub success để vượt dependency.
- Một final report duy nhất theo architecture, chứa pending deduplicated và manual checklist. Checkpoints/logs là evidence nội bộ; không tạo hàng loạt report bàn giao. Sau feedback cuối, cập nhật chính report đó.
- Giữ generic placeholders; không đưa thông tin sản phẩm, tên repo tham khảo hoặc quyết định architecture riêng vào template.
