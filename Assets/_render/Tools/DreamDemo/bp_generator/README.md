# bp_generator: công cụ sinh Blueprint của DreamDemo

Project tạm, dùng một lần. Nó sinh hai Blueprint thuần (không cần module C++ nào để chạy):

| Asset | Nội dung |
|---|---|
| `/Game/DreamDemo/Blueprints/BP_DreamWorldState` | Toàn bộ logic chuyển Day, Night, Dream; cổng mơ; camera; giảm chuyển động; phím bấm |
| `/Game/DreamDemo/Blueprints/BP_DreamInstancedField` | Actor có một `InstancedStaticMeshComponent`, không có graph |

**Sau khi sinh xong, hai Blueprint là nguồn chính.** Team art sửa trực tiếp trong Blueprint editor. Chỉ chạy lại công cụ này khi muốn dựng lại graph từ đầu; khi đó mọi chỉnh sửa tay trong graph sẽ mất.

Vì sao cần C++ để sinh Blueprint: Unreal Python không tạo được node trong graph. `DreamBlueprintBuilder.cpp` tạo node K2 (event, gọi hàm, vòng lặp, cast) bằng API của editor, rồi biên dịch Blueprint. Kết quả không tham chiếu tới module này.

## Chạy lại

Không build trong thư mục này (để repo không dính `Binaries`, `Intermediate`). Chép ra chỗ tạm:

```
cp -r Assets/_render/Tools/DreamDemo/bp_generator <thư mục tạm>/DreamBPGen
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" DreamBPGenEditor Win64 Development -Project=<thư mục tạm>/DreamBPGen/DreamBPGen.uproject -WaitMutex -NoHotReloadFromIDE
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" <thư mục tạm>/DreamBPGen/DreamBPGen.uproject -run=pythonscript -script=<thư mục tạm>/DreamBPGen/generate.py -unattended -nullrhi
```

`generate_result.json` phải có `"errors": []`. Sau đó chép hai file trong `<thư mục tạm>/DreamBPGen/Content/DreamDemo/Blueprints/` vào `Assets/_render/Content/DreamDemo/Blueprints/` (đóng editor của `_render` trước), rồi chạy `Tools/DreamDemo/build_all.sh` để đặt lại actor trên map.

Lưu ý:

- Xoá các file `.uasset` cũ trong `Content/DreamDemo/Blueprints/` của project tạm trước khi sinh lại; công cụ không ghi đè.
- `-NoHotReloadFromIDE` cho phép build khi một editor khác của cùng engine đang mở.
