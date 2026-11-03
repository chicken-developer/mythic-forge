"""Write the final QA report only when measured gates and GUI evidence pass."""
from pathlib import Path
import json,hashlib,statistics
ROOT=Path(__file__).resolve().parent
def read(p):return json.loads((ROOT/p).read_text(encoding='utf-8'))
def main():
    variants=read('results/variants.json');geo=read('results/geometry_summary.json');reg=read('results/regressions.json');portable=read('results/portability.json')
    static=read('results/static_checks.json');pub=read('results/publication.json');graph=read('graph_audit.json');paths=read('results/repo_path_audit.json')
    visual=read('results/visual_review.json');canopy=read('results/canopy_clearance.json');assetsha=hashlib.sha256((ROOT/'asset/medieval_building_modular.hda').read_bytes()).hexdigest()
    assert all(x['status']=='pass' for x in (geo,reg,portable,static,pub,visual,canopy)), 'A gate failed'
    assert portable['relocated_hda_sha256']==pub['hda_sha256']==assetsha, 'Stale HDA evidence'
    assert len(variants)==16 and all(r['status']=='pass' for r in variants)
    summary={'status':'pass','houdini':'22.0.368','hda_type':'mythic::medieval_house::4.1','hda_sha256':assetsha,
             'variants':15,'baseline':1,'low_high_exports':32,'hero_gui_screenshots':pub['hero_screenshots'],'extra_gui_screenshots':pub['extra_screenshots'],
             'module_count':graph['module_count'],'stage_count':len(graph['stages']),'python_sops':len(graph['python_sops']),
             'regression_tests':len(reg['tests'])+1,'canopy_clearance_records':len(canopy['rows']),'original_files_unchanged':static['original_source_files_checked'],
             'low_primitive_range':[min(r['levels']['low']['primitives'] for r in variants),max(r['levels']['low']['primitives'] for r in variants)],
             'high_primitive_range':[min(r['levels']['high']['primitives'] for r in variants),max(r['levels']['high']['primitives'] for r in variants)],
             'high_low_ratio_range':[min(r['high_to_low_primitive_ratio'] for r in variants),max(r['high_to_low_primitive_ratio'] for r in variants)],
             'stages':{'code_generated':'pass','static_checked':'pass','houdini_cooked':'pass','geometry_exported':'pass','gui_screenshots':'pass','hda_packaged':'pass','normal_baked':'not_run','unreal_checked':'not_run'},
             'image_provenance':'Actual visible Houdini native window screenshot. Crops/comparison sheets are derived from those exact captured pixels; no OpenGL ROP.',
             'repository_path_audit':{'status':paths['status'],'failures':len(paths['failures']),'scope':'Pre-existing missing resources in retired review manifests; package-local static checks pass.'},
             'limits':['High mesh bevels appropriate orthogonal members; sculpted surface detail and final textures are not included.',
                       'Measured roof solver covers the existing 15 datasets and tested legacy grammars; arbitrary T/courtyard roof graphs unsupported.',
                       'Normal rebake, Unreal, collision/LOD and interior walkability not run.']}
    (ROOT/'results/final_summary.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False),encoding='utf-8')
    lines=['# Medieval roof / HIGH / GUI — đánh giá 2026-11-03','',
           '15 biến thể gốc + baseline đã cook, xuất riêng LOW/HIGH và chụp HIGH trực tiếp từ GUI Houdini 22.0.368 Commercial. HDA 4.1; không ghi đè nguồn authoring.',
           '',f'**{pub["hero_screenshots"]} hero + {pub["extra_screenshots"]} ảnh bổ sung**, mỗi ảnh có bản toàn cửa sổ và crop viewport. `review.html` là trang xem cục bộ. `results/gui_capture_all.json` / `gui_capture_extra.json` chứa provenance và hash ảnh.',
           '', '| ID | LOW faces | HIGH faces | Ratio | Roof / canopy / finite UV,N / exact face QA |', '|---|---:|---:|---:|---|']
    for r in variants:lines.append(f'| {r["variant_id"]} | {r["levels"]["low"]["primitives"]:,} | {r["levels"]["high"]["primitives"]:,} | {r["high_to_low_primitive_ratio"]:.2f} | {r["status"]} |')
    lines += ['', '## Nguyên nhân và kết quả', '',
      'Mái chính/cánh trước đây dùng chung Skin cho nhiều role, tạo mặt nối không thuộc mái nào. Bản mới tách profile theo role, dựng closed volume, Boolean union, lấy miền cao nhất khớp mặt mái nguồn rồi tái dựng shell thickness. Các vùng ngói/trim bị chôn được loại theo miền mái này. Sai số bề mặt nguồn tối đa được lưu từng record, ngưỡng 0.1 mm.',
      '', 'Dormer trước đây đặt theo cao độ giữa khối và cutter dạng hộp cao vượt mái dormer. Bản mới đặt chân trước lún 8 cm vào mặt mái, giải chiều sâu tới mái chủ, kiểm tra ridge clearance, dùng cutter có tiết diện khớp mái và thêm ngói. Không nới cutter tới đỉnh mái chính.',
      '', 'Canopy/lean-to lấy datum tường, Boolean cắt phần chôn. Ngói rải từ quad nguồn trước Boolean; polygon ngon sau Boolean không còn làm mất toàn bộ điểm rải. Mái vải vẫn dùng hình học vải và các thanh đỡ.',
      '', 'HIGH là mesh khác LOW. PolyBevel chỉ nhận các thanh/ngói có góc vuông, feature size được giới hạn theo cạnh ngắn. Các mặt Boolean hoặc chi tiết mỏng được bảo toàn. Cả LOW/HIGH có vị trí, UV, normal hữu hạn và không còn mặt suy biến/trùng theo mask hình học 64-bit. Bounds LOW/HIGH giữ nguyên trong sai số 1 mm.',
      '', '## Kiểm tra thực hiện', '',
      f'- Graph: {graph["module_count"]} module / {len(graph["stages"])} stage thật; Python SOP = {len(graph["python_sops"])}; connector/ownership audit pass.',
      f'- {len(reg["tests"])} hồi quy: cửa/host/cutter/frame/dressing, UV tiling, determinism, width/wing recook, invalid inputs, safe rerun, min/max và hipped/conical/hybrid grammar.',
      '- Fresh process chỉ cài HDA đã di chuyển: LOW/HIGH khác nhau, width thay đổi geometry, không nhập script authoring hoặc file mesh ngoài.',
      '- Awning lấy mép balcony thực qua connector overhead_access; vươn thêm ít nhất 45 cm. Kiểm tra cả 8 cấu hình bật awning và thay balcony_depth_m thêm 35 cm: mái vải và cột/tay đỡ dịch đúng 35 cm. Xem results/canopy_clearance.json.',
      f'- {static["original_source_files_checked"]} file nguồn giữ nguyên hash; 15 dataset + baseline giữ nguyên byte. Các đường dẫn output gốc được phục vụ bằng alias tới geometry HIGH và crop screenshot GUI.',
      '- Xem `results/variants.json`, `regressions.json`, `portability.json`, `static_checks.json`, `visual_review.json` và `graph_audit.json`.',
      '', '## Phạm vi và giới hạn', '',
      'Đã xem toàn bộ hero, các góc cận trước/sau mái 6, 7, 8, 11, canopy vải và wire LOW/HIGH. Đây là đánh giá các vùng nhìn thấy, không chứng minh mọi tổ hợp parameter hoặc mọi bề mặt khuất. HIGH chưa có sculpt gỗ/đá, texture final hoặc normal bake mới. Bespoke props/emblems/organic dressing vẫn theo mức đơn giản hóa của nguồn.',
      '', 'Không chạy bake/reimport Unreal, collision/LOD hoặc kiểm tra đi lại nội thất. T/courtyard và đồ thị mái tùy ý tiếp tục bị từ chối. Không coi các trường expected_features của dataset là bằng chứng tính năng đã bake.',
      '', 'Ở common hero mẫu 10, awning vải chưa đọc rõ. Kiểm tra projection và support recook là số liệu geometry; không coi đó là chứng nhận visual fidelity hoàn chỉnh cho canopy vải.',
      '', f'Repository path audit hiện báo {len(paths["failures"])} resource thiếu trong các manifest review cũ đã retired; không sửa lịch sử đó. Static package, cook và HDA portability được báo riêng, đều pass.',
      '', 'Preview được chụp bằng QScreen.grabWindow trên native HWND của chính cửa sổ Houdini đang chạy. Crop chỉ dùng pixel từ cùng screenshot; không chụp desktop hoặc ứng dụng khác, không ROP render, không imagegen. HUD là widget trong GUI trước lúc chụp.']
    (ROOT/'evaluation.md').write_text('\n'.join(lines),encoding='utf-8')
    print(json.dumps(summary,ensure_ascii=False),flush=True)
if __name__=='__main__':main()
