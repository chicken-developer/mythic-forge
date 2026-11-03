Gothic stylized materials

Mở Gothic_Stylized.sbs trong Adobe Substance 3D Designer.
Package chứa 10 graph procedural native; không dùng ảnh AI làm texture nguồn.
Chỉnh node trong vùng Patterns and surface wear, PBR response, rồi Export.

exports/ chứa 70 PNG 2K. Unreal dùng BaseColor, Normal và ORM của mỗi material.
BaseColor dùng sRGB; Normal và ORM dùng dữ liệu linear.
Normal DirectX. ORM: Red=Ambient Occlusion, Green=Roughness, Blue=Metallic.
Gothic_Stylized.sbsar là package đã cook từ nguồn này.

Ref gốc: ../gothic_stylized/refs.
Scene kiểm tra: ../_render/Content/Gothic/Maps/L_Gothic_Lineup.umap.
Ảnh và báo cáo: ../_render/RenderTests/Gothic/.
build_gothic_materials.py chạy trong Python Editor của Designer để tạo lại.
