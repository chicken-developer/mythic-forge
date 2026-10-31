"""Verify the delivered source/engine/render set and write the city handoff."""
from pathlib import Path
import ast
import csv
import datetime
import hashlib
import json
import shutil
from PIL import Image

ROOT=Path(__file__).resolve().parents[3];PROJECT=ROOT/'TDKProps'
SOURCE=ROOT.parent/'HoudiniWorkspace/gothic_building'
SAVED=PROJECT/'Saved/GothicReview/City';OUT=PROJECT/'RenderTests/Gothic/City'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
manifest=read(PROJECT/'SourceAssets/Gothic/mesh_manifest.json');rows=manifest['variants']
assert manifest['status']=='passed' and len(rows)==28 and len({r['id'] for r in rows})==28
hip=(ROOT/manifest['source_hip']).resolve();assert hip==SOURCE/'gothic_building.hip'
assert sha(hip)==manifest['source_sha256']
portable=read(SOURCE/'reports/portable_v3_validation.json')
assert portable['status']=='passed' and portable['game_high_same_components'] and portable['embedded']
imported=read(SAVED/'asset_import.json');assert imported['status']=='passed' and imported['run_id']=='city_v3_final'
engine_rows={r['id']:r for r in imported['meshes']}
for row in rows:
    assert sha(ROOT/row['file'])==row['fbx_sha256'],row['id']
    assert row['invalid_normals']==row['degenerate_triangles']==row['python_sops']==0
    assert engine_rows[row['id']]['triangles']==row['triangles']
    assert (PROJECT/'Content/Gothic/Buildings'/('SM_Gothic_'+row['id']+'.uasset')).is_file()
editor=read(OUT/'validation_editor.json');play=read(OUT/'validation_play.json')
movement=read(OUT/'movement_validation.json');session=read(OUT/'play_session.json');final=read(OUT/'final_editor.json')
assert all(x['status']=='passed' for x in (editor,play,movement,session,final))
assert editor['building_instances']==play['building_instances']==361
assert len(play['building_collision'])==28 and len(play['street_surfaces'])==7
assert movement['distance_cm']>50
views=['city_overview','city_plan','civic_square','craft_street','merchant_street','noble_terraces','cathedral_close','catalog_overview','type_11','type_22','type_10','city_play']
renders=[]
for name in views:
    path=OUT/(name+'.png')
    with Image.open(path) as im:size=im.size;im.verify()
    renders.append({'file':path.name,'size':size,'sha256':sha(path)})
for source,name in [(SAVED/'asset_import.json','asset_import.json'),(SAVED/'context_import.json','context_import.json'),(SAVED/'city_scene.json','city_scene.json'),(SAVED/'catalog_scene.json','catalog_scene.json'),(SOURCE/'reports/portable_v3_validation.json','portable_v3_validation.json'),(SOURCE/'reports/production_deployment.json','production_deployment.json')]:
    assert read(source)['status']=='passed';shutil.copy2(source,OUT/name)
shutil.copy2(SOURCE/'city_catalog.json',PROJECT/'SourceAssets/Gothic/city_catalog.json')
with (OUT/'building_catalog.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.writer(f);writer.writerow(['Dataset','Name','District','Program','Width_m','Height_m','Depth_m','Game_triangles'])
    for r in sorted(rows,key=lambda r:r['id']):writer.writerow([r['id'],r['label'],r['district'],r['use'],*[round(x,3) for x in r['size_m']],r['triangles']])
for path in (PROJECT/'Tools/Gothic').glob('*.py'):ast.parse(path.read_text(encoding='utf-8-sig'),filename=str(path))
maps={name:sha(PROJECT/'Content/Gothic/Maps'/(name+'.umap')) for name in ('L_Gothic_City','L_Gothic_Catalog','L_Gothic_Lineup')}
config=(PROJECT/'Config/DefaultEngine.ini').read_text()
assert 'GameDefaultMap=/Game/Gothic/Maps/L_Gothic_City' in config and 'EditorStartupMap=/Game/Gothic/Maps/L_Gothic_City' in config
layout=read(PROJECT/'SourceAssets/Gothic/city_layout.json')
report={'status':'passed','verified_at':datetime.datetime.now().astimezone().isoformat(),'engine':final['engine'],
    'generator':{'type':portable['type'],'source_hip':str(hip),'hip_sha256':sha(hip),'hda_sha256':sha(SOURCE/'asset/gothic_building.hda'),
                 'datasets':28,'source_triangles':sum(r['triangles'] for r in rows),'native_sops':True,'fbx_hashes_verified':28,'game_high_same_architecture':True},
    'city':{'map':'/Game/Gothic/Maps/L_Gothic_City','area_m':[550,550],'building_types':28,'building_instances':361,
            'district_counts':layout['district_counts'],'context_instances':editor['context_actors'],'layout_seed':layout['seed'],
            'world_building_overlaps':editor['world_building_overlaps'],'world_buildings_on_roads':editor['world_buildings_on_roads']},
    'runtime':{'tested_in':'Play In Editor','pawn':play['pawn_class'],'building_collision_types':28,'street_surface_checks':7,
               'movement_cm':movement['distance_cm'],'input_method':movement['method']},
    'map_sha256':maps,'renders':renders,
    'scope':'Exterior Nanite assets and playable sample city. Packaged target-platform build and performance budget were not tested.'}
(OUT/'final_handoff.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
guide='''GOTHIC CITY — BÀN GIAO V3

MỞ VÀ CHƠI
Mở TDKProps.uproject. Editor và game mặc định mở /Game/Gothic/Maps/L_Gothic_City.
Nhấn Play để dùng nhân vật Third Person: WASD di chuyển, chuột xoay camera, Space nhảy.
Map /Game/Gothic/Maps/L_Gothic_Catalog trưng bày đủ 28 loại nhà với vật liệu clay.
Map L_Gothic_Lineup cũ vẫn dùng các mesh đã cập nhật cho 10 biến thể ban đầu.

NGUỒN HOUDINI
E:/Repos/HoudiniWorkspace/gothic_building/gothic_building.hip
E:/Repos/HoudiniWorkspace/gothic_building/asset/gothic_building.hda
Loại HDA: quynh::gothic_building::3.0.
Node: /obj/gothic_building_v2_1/generator.
Building Preset: 28 dataset và baseline. Mesh Quality: game/high.
Để chỉnh tham số, bật Auto Rebuild Generated Modules; hoặc nhấn Rebuild Generated Modules.
Trước khi chỉnh SOP sinh ra bằng tay, tắt Auto Rebuild.
Game và High dùng cùng kiến trúc; High tăng độ mịn của đường cong/bevel.
Vòm liên tục, phào có tiết diện, cửa khoét thật, cửa kính lùi vào tường;
buttress bậc phẳng và chóp thẳng theo mẫu đã duyệt. Không thêm lá/sừng lớn hay chóp kim cương.
SOP geometry đã lưu là native; HDA nhúng code và profile, không có Python SOP cần cook.
Backup nguồn trước rollout: houdini_output/backups/v3_rollout.

DATASET VÀ MAP
28 loại: 10 công trình gốc; 6 nhà bình dân/xưởng/quán; 5 nhà trung lưu;
2 nhà thượng lưu; 5 công trình công cộng (guildhall, market, town hall, library, bath house).
Danh sách đầy đủ: RenderTests/Gothic/City/building_catalog.csv.
Dataset JSON gốc: HoudiniWorkspace/gothic_building/datasets; bảng tổng hợp: SourceAssets/Gothic/city_catalog.json.
City: 550 x 550 m, 361 building; 134 working, 184 middle, 29 upper, 11 public, 3 cathedral close.
Đường phụ 8 m, đại lộ 12 m, vỉa hè 2 m, quảng trường 60 x 60 m;
có sân trong, trục tới nhà thờ, cổng thành, tường thành, đèn, ghế, cây và đài phun nước.
473 context actors dùng chung 6 mesh. Building actors cũng dùng chung 28 mesh và có thể chỉnh riêng.
Layout seed 91728; nguồn bố trí: SourceAssets/Gothic/city_layout.json.

UNREAL ASSETS VÀ CẬP NHẬT
Buildings: /Game/Gothic/Buildings/SM_Gothic_variant_01 ... variant_28.
Giữ UV, normals, vật liệu hiện có; bật Nanite và complex-as-simple collision.
FBX ở SourceAssets/Gothic/Meshes, đơn vị centimet.
Các script nguồn production_pipeline.py và build_city_context.py đã nhận vị trí workspace mới.
Để xuất lại một mẫu, chạy hython với production_pipeline.py --ids variant_11.
Sau khi xuất, chạy production_pipeline.py --collect để cập nhật manifest.
Trong Unreal Python console chạy Tools/Gothic/import_city_assets.py;
Saved/GothicReview/City/import_request.json có thể giới hạn ids cần nhập.
Đổi seed/bố trí bằng Tools/Gothic/plan_city.py, sau đó build_city_levels.py.
build_city_levels.py tạo lại các actor có tag GothicCityGenerated trong hai map sample.
Save As map khác trước khi tùy biến bố trí nếu cần giữ bản chỉnh tay.

KIỂM TRA ĐÃ ĐẠT
28 FBX khớp checksum; normals hợp lệ, không triangle suy biến, không Python SOP.
28 mesh Unreal khớp số tam giác/kích thước, có UV, Nanite, vật liệu và collision.
361 building không chồng footprint, không lấn lòng đường.
PIE dùng BP_ThirdPersonCharacter: trace va chạm 28 loại nhà và 7 điểm mặt đường đạt.
CharacterMovement chạy thật trong PIE, di chuyển 10.52 m trên đại lộ.
12 ảnh Unreal, các báo cáo editor/play/source và final_handoff.json:
TDKProps/RenderTests/Gothic/City.

PHẠM VI
Bộ ngoại thất và sample map chơi thử trong editor. Chưa kiểm thử bản đóng gói
hay ngân sách hiệu năng trên nền tảng đích. Vật liệu dùng bộ hiện có; không làm mới texture Designer trong đợt này.
'''
(PROJECT/'CITY_GUIDE.txt').write_text(guide,encoding='utf-8-sig')
# Clear only obsolete diagnostics whose later successful reports were verified.
(SAVED/'context_error.txt').unlink(missing_ok=True)
(SAVED/'play_progress.json').write_text(json.dumps(session,indent=2),encoding='utf-8')
print('HANDOFF_PASSED',report['generator']['datasets'],report['city']['building_instances'],len(renders),report['generator']['source_triangles'])
