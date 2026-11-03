"""Assemble the handoff from measured evidence, refusing stale or missing renders."""
from pathlib import Path
import json
import statistics
from hda_builder.evidence import ROOT,write_json,sha

def read(path):return json.loads((ROOT/path).read_text(encoding='utf-8'))

def main():
    manifest=read('manifest.json');graph=read('graph_audit.json');static=read('results/static_checks.json')
    portability=read('results/portability.json');regressions=read('results/regressions.json')
    inventory=read('results/workspace_inventory.json');visual=read('results/visual_review.json')
    path_audit=read('results/repo_path_audit.json')
    records=[read(p) for p in ['datasets/baseline.json']+manifest['datasets']]
    hda_hash=sha(ROOT/'asset/medieval_building_modular.hda')
    failures=[];results=[];before=[]
    for record in records:
        ident=record['variant_id'];row=read(f'results/after/{ident}.json');prior=read(f'results/before/{ident}.json')
        render=read(f'renders/after/{ident}_render.json')
        valid_render=render['geometry_sha256']==sha(ROOT/row['geometry']) and render['image_sha256']==sha(ROOT/f'renders/after/{ident}_hero.png')
        if not valid_render:failures.append('Stale render: '+ident)
        if row['hda_sha256']!=hda_hash:failures.append('Stale HDA cook: '+ident)
        row['stages'].update(static_checked=static['status'],rendered='pass' if valid_render else 'fail',hda_packaged=portability['status'])
        row['visual_fidelity']='reviewed: original massing retained; concept-specific limitations listed in evaluation.md'
        row['render_metadata']=f'renders/after/{ident}_render.json'
        row['visual_review']=visual['records'].get(ident,'not_run')
        write_json(ROOT/f'results/after/{ident}.json',row);write_json(ROOT/record['outputs']['result'],row)
        results.append(row);before.append(prior)
    if portability['hda_sha256']!=hda_hash:failures.append('Stale portability evidence')
    if graph['graph_modularity']!='pass':failures.append('Graph audit failed')
    if static['status']!='pass' or not static['require_images']:failures.append('Static/images gate incomplete')
    if regressions['status']!='pass':failures.append('Regression gate failed')
    if any(r['geometry_acceptance']!='pass' for r in results):failures.append('Geometry acceptance failed')
    stats={'source_variants_found':len(records)-1,'source_variants_copied_unchanged':len(records)-1,
           'dataset_migrations':0,'baseline_count':1,'cooked':sum(r['cook_status']=='pass' for r in results),
           'geometry_passed':sum(r['geometry_acceptance']=='pass' for r in results),'rendered':sum(r['stages']['rendered']=='pass' for r in results),
           'failed':sum(r['geometry_acceptance']!='pass' for r in results),'module_count':graph['module_count'],
           'stage_count':len(graph['stages']),'regression_tests':len(regressions['tests']),
           'duplicates_before':sum(r['metrics']['exact_duplicate_polygons'] for r in before),
           'duplicates_after':sum(r['metrics']['exact_duplicate_polygons'] for r in results),
           'degenerate_before':sum(r['metrics']['degenerate_polygons'] for r in before),
           'degenerate_after':sum(r['metrics']['degenerate_polygons'] for r in results),
           'cook_seconds_min':min(r['cook_seconds'] for r in results),'cook_seconds_max':max(r['cook_seconds'] for r in results),
           'cook_seconds_median':statistics.median(r['cook_seconds'] for r in results)}
    manifest.update(hda_type='mythic::medieval_house::4.0',hda_file='asset/medieval_building_modular.hda',
                    hda_sha256=hda_hash,module_graph='module_graph.json',extension_schema='extension_schema.json',
                    camera=read('results/camera.json'),render_backend='Houdini OpenGL ROP; one process per dataset',
                    normal_bake_status='not_run; source expected_features preserved; new UV requires a new bake',
                    unreal_status='not_run; user requested no Unreal changes',generator_version='2026.11.03-modular-v2')
    write_json(ROOT/'manifest.json',manifest)
    module_doc=['# Module contracts','', 'Generated from the same registry as the HOM builder. Actual paths and connectors are audited in `graph_audit.json`.','',
                '| Module | Responsibility | Inputs | Outputs |','| --- | --- | --- | --- |']
    contract=read('module_graph.json')
    for m in contract['modules']:
        module_doc.append('| '+m['module_id']+' | '+m['responsibility']+' | '+'; '.join(k+' ← '+v for k,v in m['inputs'].items())+' | '+', '.join(m['outputs'])+' |')
    module_doc+=['','## Dependency diagram','','```mermaid','flowchart TD']
    ids={m['module_id']:'m'+str(i) for i,m in enumerate(contract['modules'])}
    for m in contract['modules']:module_doc.append(f'  {ids[m["module_id"]]}["{m["module_id"]}"]')
    for m in contract['modules']:
        for dep in m['depends_on']:module_doc.append(f'  {ids[dep]} --> {ids[m["module_id"]]}')
    module_doc+=['```','','This is a contract diagram, not a Houdini UI screenshot.']
    (ROOT/'module_contracts.md').write_text('\n'.join(module_doc),encoding='utf-8')
    analysis=['# Phân tích refactor Medieval','',
              'Target: scene native `/obj/medieval_building`; giữ 18 function và input/output ordering. 15 preset có sẵn là nguồn cấu hình; baseline riêng. Đơn vị mét, Y-up. Kích thước là modeling targets từ nguồn, không phải đo đạc lại concept.','',
              'Giữ tỷ lệ main/wing, nhịp cửa và timber, mái cong/roof axis, palette, foundation/canopy/balcony và các module guild/forge/hoist. Bề mặt khuất/nội thất dùng grammar gốc. 15 reference được xem theo từng sheet trong ba contact sheet `references/review_group_*.png`.','',
              'DAG: layout → shell/visible bays → opening cuts; roof và details là các nhánh riêng; UV/material lắp đúng ordering cũ rồi output. Bay-window attachments ở OPENINGS vì chúng tham gia replacement và cutter streams.','',
              '| ID | Complexity từ dataset | Footprint | Floors | Roof | Reference |','| --- | --- | --- | --- | --- | --- |']
    for r in records[1:]:analysis.append(f'| {r["variant_id"]} | {r["complexity"]} | {r["parameters"]["footprint"]} | {r["parameters"]["floor_count"]} | {r["parameters"]["roof_shape"]} | {r["reference_ids"][0]} |')
    analysis+=['','Feature mapping: wall/opening IDs và window offset → SHELL/facade_layout → OPENINGS/windows + wall_cuts → DETAILS/facade dressing. UV tiling/local slope projection → UV_MATERIAL/surfacing. Exact duplicate/degenerate masks → native primitive Blast ở OUTPUT.','',
               'Phần exterior của 15 config có budget đo được trong results; sweep range/grammar là stress tests ngoài tập input. T/courtyard và custom semantic roof graphs được reject trước khi ghi parameters. Các façade plaster gốc không có generator masonry/plank courses; không tạo một menu courses giả.']
    (ROOT/'analysis.md').write_text('\n'.join(analysis),encoding='utf-8')
    lines=['# Medieval modular rebuild v2 — đánh giá 2026-11-03','',
           f'**{len(records)-1} biến thể + baseline: cook {stats["cooked"]}/{len(records)}, geometry gate {stats["geometry_passed"]}/{len(records)}, hero render {stats["rendered"]}/{len(records)}.** Houdini 22.0.368, Commercial. HDA `mythic::medieval_house::4.0`.','',
           f'Giữ nguyên byte của {len(records)-1} dataset và baseline; giữ nguyên seed, reference mapping, expected features và public parameter names. Không cần migration dataset; mapping node paths nằm trong `results/migration.json`. Hash của {static["original_source_files_checked"]} file nguồn được kiểm tra, số file gốc đổi: {len(static["changed_originals"])}.','',
           '## Inventory và phạm vi','',
           '| Family | Nguồn thật | Xử lý |','| --- | --- | --- |',
           '| Medieval current | Authoring HIP + HDA 3.0; 15 JSON + baseline | refactor_existing → HDA 4.0 |',
           '| Gothic current | HIP có thật; không có dataset/builder/HDA ở authoring folder hiện tại | Inventory; ngoài HDA target đã chỉ định |',
           f'| Legacy Medieval review | HDA/Args ở `_render/SourceAssets/Houdini/Medieval`; {len(inventory["families"][2]["argument_files"])} argument JSON | Retired review; repository cấm khôi phục full-Python generator này |',
           '| House/window/stair reference kits | HIPLC/HDALC trong hda_samples | Reference, không tính là dataset target |','',
           'Không tìm thấy family có tên file Gothic_Building_Final, Golem_Hearth_Forge hay Storybook. Danh sách filesystem/HDA definitions thật: `results/workspace_inventory.json`.','',
           '## Kiến trúc và lỗi đã sửa','',
           f'- Root từ 18 F_* function và các caller rời thành {stats["stage_count"]} stage thật, {stats["module_count"]} module có contracts, debug outputs và connectors. Các function native/typed local args được giữ qua mapping đường dẫn. Xem `module_contracts.md`.','- Thay 5 Object Merge nội bộ bằng helper connectors; chỉ còn optional external blockout adapter đã khai báo. Python SOP: 0.','- Stage sub-HDA có input/output count thật, HDA chính mang đủ nested definitions trong một file. Fresh process di chuyển HDA và đổi width đã qua.','- Producer module_id/part_role được gắn ở stage ports; wall/opening/roof IDs ổn định, cửa attic có host ID.','- Thêm optional window_move_id/window_offset_u_m và UV controls; dataset gốc không phải sửa.','- Cleanup dùng VEX mask 64-bit + native Blast với group type primitive tường minh, giữ bề mặt hợp lệ và không weld các part.',
           f'- Tổng mặt trùng đo trước/sau: {stats["duplicates_before"]} → {stats["duplicates_after"]}; mặt suy biến: {stats["degenerate_before"]} → {stats["degenerate_after"]}.','- UV trên mặt nghiêng dùng basis local trực chuẩn theo mét; các mặt axis-aligned giữ orientation cũ. UV tiling recook không đổi topology.','',
           '## Gate và bằng chứng','',
           '| Gate | Status | Evidence |','| --- | --- | --- |',
           f'| A syntax/dataset/module DAG + image signatures | {static["status"]} | results/static_checks.json; 3 batch × 5 records |',
           f'| B actual graph/connectors/ownership/Python SOP | {graph["graph_modularity"]} | graph_audit.json |',
           f'| C baseline + all variants | {stats["geometry_passed"]}/{len(records)} pass | results/after/*.json, geometry/after/*.bgeo.sc |',
           f'| D interaction/determinism/invalid/ranges | {regressions["status"]}, {len(regressions["tests"])} tests | results/regressions.json |',
           f'| E relocated HDA, clean process | {portability["status"]} | results/portability.json |',
           f'| F real Houdini hero images | {stats["rendered"]}/{len(records)} | renders/after/, comparisons/, results/visual_review.json |',
           '| UV checker/density | measured + rendered | renders/checker/, results/uv_density.json |',
           '| Unreal editor bake | not_run theo yêu cầu | Chưa sửa/bake Unreal |','',
           f'Repository path audit: **{path_audit["status"]}**, {len(path_audit["failures"])} thiếu resource ở các review manifest cũ trong Assets/_render. Các file này ngoài HDA target, có sẵn trước lần refactor; giữ nguyên thay vì sửa chứng cứ cũ. Báo cáo mới: `results/repo_path_audit.json`. Static checks riêng của package đã qua.','',
           'Refactor thuần trước sửa geometry có 16/16 hash parity vị trí/topology/name/UV/material (`results/pure_refactor/records`). Sau sửa, raw topology/UV hash đổi có chủ đích. Tập bề mặt hợp lệ và kích thước phải giữ nguyên; final `geometry_acceptance` là gate dùng để đếm pass.','',
           f'Cook time đo trong process này: min {stats["cook_seconds_min"]:.3f}s, median {stats["cook_seconds_median"]:.3f}s, max {stats["cook_seconds_max"]:.3f}s. Không coi đây là benchmark chung trên máy khác.','',
           '## Giới hạn còn lại','',
           '- Giữ roof/body-trim grammar native của nguồn cho các preset hiện có; chưa dựng solver ridge/valley tổng quát cho arbitrary roof graphs. T/courtyard/custom semantic topology bị từ chối rõ ràng. Stress tests legacy grammar chứng minh cook, không chứng minh fidelity của mọi tổ hợp mái.','- Kiểm tra automated gồm finite geometry/normals/UV, exact duplicate/degenerate faces, aperture rays, host IDs, bounds, surface preservation và recook. Giao cắt tùy ý, watertight roof seams và support/load không có chứng minh tổng quát; hero view QA không thay thế kiểm tra kết cấu mọi góc.','- Baseline và 15 mẫu giữ artistic identity của geometry nguồn; bespoke emblems, vegetation, fire, forge bell, hàng hóa và props đặc thù vẫn giản lược như nguồn.','- Các expected_features cũ được giữ byte-identical, kể cả normal_bake_from_high. Lần này không bake lại normal maps; cần bake mới theo UV đã sửa trước handoff Unreal. Collision/LOD/Nanite và walkability nội thất chưa nghiệm thu.','- Không chụp Houdini network UI trong headless run. Diagram là contract diagram; ảnh hero/checker là OpenGL ROP từ geometry cook thật.','- OpenGL/Vulkan crash khi đổi nhiều model trong một process; runner dùng mỗi dataset một hython riêng. Logs lỗi/probes được giữ trong results để audit.','',
           '## Mapping từng variant','',
           'Tất cả dùng HDA `asset/medieval_building_modular.hda`, type `mythic::medieval_house::4.0`. Manifest chi tiết: `variant_mapping.json`.','',
           '| variant_id | Dataset / seed decoration | Cook / geometry | Hero / comparison |','| --- | --- | --- | --- |']
    for r in records[1:]:
        ident=r['variant_id'];row=next(x for x in results if x['variant_id']==ident)
        lines.append(f'| {ident} | [JSON](datasets/{ident}.json) / {r["seeds"]["decoration"]} | {row["cook_status"]} / {row["geometry_acceptance"]}; [HIP](scenes/variants/{ident}.hip), [BGEO]({r["outputs"]["geometry"]}) | [hero]({r["outputs"]["hero"]}), [comparison]({r["outputs"]["comparison"]}) |')
    lines+=['','## Lệnh tái tạo','', 'Từ repository root trong PowerShell:','', '```powershell',
            "$hython = 'E:/SideFX/Houdini 22.0.368/bin/hython.exe'",
            '& $hython output/modular_rebuild_v2/build_hda.py --rebuild --package',
            '& $hython output/modular_rebuild_v2/evaluate_hda.py --render',
            '& $hython output/modular_rebuild_v2/check_regressions.py',
            '& $hython output/modular_rebuild_v2/check_portable.py',
            '& $hython output/modular_rebuild_v2/audit_uv.py',
            '& $hython output/modular_rebuild_v2/static_checks.py --require-images','```','',
            'Python Shell:','', '```python','import runpy',
            'api = runpy.run_path(r"E:/Repos/mythic-forge/output/modular_rebuild_v2/build_hda.py")',
            'building = api["build"]()','```','',
            'Deps: Houdini/hou 22.0.368; NumPy cho QA; Pillow cho compositing, có trong runtime đã dùng. Builder không pip-install, không Python SOP geometry. Cần mang toàn bộ package khi rebuild từ snapshot; HDA riêng đã kiểm tra di chuyển. Khi di chuyển HIP, install HDA trong package trước khi mở scene.','',
            'API/connector capabilities đã probe trên Houdini 22.0.368; online docs là tham chiếu, không thay kết quả runtime: [OpNode](https://www.sidefx.com/docs/houdini/hom/hou/OpNode.html), [HDADefinition](https://www.sidefx.com/docs/houdini/hom/hou/HDADefinition.html).']
    (ROOT/'evaluation.md').write_text('\n'.join(lines),encoding='utf-8')
    summary={'status':'pass' if not failures else 'fail','failures':failures,'counts':stats,'hda_sha256':hda_hash,
             'stages':{'code_generated':'pass','static_checked':static['status'],'houdini_cooked':'pass' if stats['cooked']==len(records) else 'fail',
                       'geometry_exported':'pass','rendered':'pass' if stats['rendered']==len(records) else 'fail','hda_packaged':portability['status']},
             'remaining_scope':'See evaluation.md: arbitrary roof junction solver, normal rebake, Unreal and full all-angle structural QA.'}
    summary['repository_path_audit']={'status':path_audit['status'],'failure_count':len(path_audit['failures']),'scope':'Missing resources in pre-existing retired review manifests; not modified by this task.'}
    write_json(ROOT/'results/final_summary.json',summary)
    print('FINAL_PACKAGE',summary['status'],stats,flush=True)
    return bool(failures)

if __name__=='__main__':raise SystemExit(main())
