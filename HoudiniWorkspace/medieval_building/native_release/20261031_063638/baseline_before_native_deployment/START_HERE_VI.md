# Bộ bàn giao Houdini + Codex

Đây là source package để TẠO HDA tại máy có Houdini; ZIP chưa chứa HDA/HIP native đã được kiểm thử. Nền tảng v5.1 nội thất, 6 loại nhà × 4 biến thể = 24 preset; mục tiêu một HDA dùng chung.

## Chạy nhanh
Giải nén toàn bộ, mở thư mục này bằng Codex và yêu cầu đọc AGENTS.md. Không cần cài lại skill từ tài khoản cũ.

Kiểm tra source bằng Python thường:

```text
python create_hda.py --check-only
```

Trong Houdini Command Line Tools, chạy mẫu đầu:

```text
hython create_hda.py --preset 01_townhouse_base --output-dir output/sample_01
```

PowerShell nếu hython chưa có trong PATH (thay đường dẫn bằng bản Houdini thực tế):

```powershell
& "C:\Program Files\Side Effects Software\Houdini <version>\bin\hython.exe" .\create_hda.py --output-dir .\output\sample_01
```

Sau khi mẫu chạy thành công, dùng đúng file asset vừa xuất để kiểm tra trong process khác:

```text
hython verify_hda.py output/sample_01/Storybook_Building.hda --report output/sample_01/verify.json
```

Đổi `.hda` thành `.hdalc` hoặc `.hdanc` theo file THỰC TẾ của license. Sau khi kiểm tra mẫu/relocation:

```text
hython create_hda.py --all-presets --output-dir output/all_presets_01
```

Thư mục output phải chưa tồn tại. Mỗi lần chạy dùng tên mới để giữ checkpoint. Đường dẫn output tương đối tính từ thư mục terminal; assets/source tìm theo vị trí script.

## Kết quả dự kiến trên máy Houdini
- Một `Storybook_Building.hda` hoặc suffix theo license; type `quynh::storybook_blockout::5.1`.
- Một demo HIP theo license; một node instance hiện preset đã chọn.
- BGEO cho mẫu được cook; với --all-presets là 24 BGEO.
- build_report.json ghi runtime, license, số point/primitive, hash, thời gian và lỗi từng preset.
- HDA chứa source Python SOP, ParameterSchema.json và Presets.json. 24 preset là dữ liệu JSON nhúng, CHƯA có menu chọn preset chuyên biệt trên UI; có thể dùng setParms/apply_config hoặc nhờ Codex bổ sung UI sau khi cook thành công.

## Trong gói
build_hda.py: generator stdlib + HOM; create_hda.py: launcher; verify_hda.py: verifier độc lập; assets/datasets: 24 preset; assets/geometry: OBJ/MTL kiểm chứng offline; assets/textures: texture; previews: 3 ảnh nội thất và scene recipe; results + validation: kết quả offline; AGENTS.md: hướng dẫn Codex; BLOCKOUT.md: input contract; RENDER_TEST_WORKFLOW.md: quy trình preview nhẹ.

Native geometry chỉ cần Python của Houdini/stdlib. Render CPU tùy chọn cần numpy, scipy, Pillow (requirements-preview.txt); không tự pip cài vào Python hệ thống của Houdini.

## Trạng thái và giới hạn
Launcher mới đã compile và chạy --check-only, nhưng chưa chạy HOM thực tế. Có thể cần Codex chỉnh tương thích phiên bản Houdini sau khi thấy lỗi thực. Chưa kiểm tra collision/NavMesh/headroom mọi biến thể. Shader native và binding Unreal chưa tạo; texture có trong gói để Codex gán và kiểm tra. Geometry/HDA hiện dùng normals và topology v5, không phải bevel v6.

Tài liệu HOM đối chiếu khi chuẩn bị launcher (online hiện là Houdini 22, không xác nhận tương thích bản cài trên máy):
https://www.sidefx.com/docs/houdini/hom/hou/hipFile.html
https://www.sidefx.com/docs/houdini/hom/hou/Node.html
