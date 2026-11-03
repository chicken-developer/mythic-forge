"""Collect completed native/export/editor evidence and update the catalog handoff."""
from pathlib import Path
import hashlib
import json
from PIL import Image

P=Path(__file__).resolve().parents[2]
OUT=P/'RenderTests/SkyTown'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    editor=read(OUT/'unreal_validation.json')
    assert editor['status']=='passed' and len(editor['meshes'])==100
    assert [r['buildings'] for r in editor['catalogs']]==[24,28]
    materials=read(OUT/'materials.json');hdas=read(OUT/'unreal_hdas.json')
    assert materials['status']==hdas['status']=='passed'
    exports=[];sources=[]
    verified={(r['family'],r['quality'],r['id']):r for r in editor['meshes']}
    for family,qualities,count in [('Medieval',('low','normal','high'),24),('Gothic',('normal',),28)]:
        for quality in qualities:
            path=P/'SourceAssets'/family/('mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json')
            manifest=read(path);rows=manifest['variants']
            assert len(rows)==count and manifest['status']=='passed'
            assert all(r['opening_clearance']['blocked_openings']==0 and r['skytown_assets'] for r in rows)
            assert all(verified[(family,quality,r['id'])]['source_fbx_sha256']==r['fbx_sha256'] for r in rows)
            manifest['source']=str(P.parent/('hda_'+family.lower()+'_building'))
            path.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
            exports.append({'family':family,'quality':quality,'meshes':len(rows),
                'version':manifest['version'],'opening_support_pairs_checked':sum(r['opening_clearance']['opening_support_pairs_checked'] for r in rows),
                'blocked_openings':0,'skytown_assets':sorted({a for r in rows for a in r['skytown_assets']})})
        portable=read(P/'RenderTests/PropsDetail'/(family.lower()+'_portable_hda.json'))
        assert portable['status']=='passed'
        published=read(P/'Saved/PropsDetail'/(family.lower()+'_source_publish.json'))
        assert published['status']=='passed'
        assert hashlib.sha256(Path(published['hda']).read_bytes()).hexdigest()==published['hda_sha256']==portable['hda_sha256']
        sources.append({'family':family,'version':published['version'],'hda':published['hda'],
            'hip':published['hip'],'portable_presets_verified':[r['id'] for r in portable['records']]})
    captures=[]
    for filename in ('Medieval_after.png','Gothic_after.png','SM_House02_reference.png',
                     'medieval_joint.png','gothic_joint.png','medieval_interior.png','gothic_interior.png','gothic_wide_balcony.png'):
        path=OUT/filename
        metadata=read(path.with_suffix('.json'))
        with Image.open(path) as image:
            size=list(image.size)
        captures.append({'file':path.relative_to(P).as_posix(),'size_px':size,
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),**metadata})
    report={'status':'passed','building_presets':52,'static_meshes':100,
        'exports':exports,'sources':sources,'material_instances':len(materials['materials']),
        'unreal_hdas':hdas['assets'],'captures':captures}
    (OUT/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    guide=P/'CATALOG_GUIDE.txt'
    text=guide.read_text(encoding='utf-8')
    heading='SKYTOWN ADAPTATION — CURRENT\n'
    if heading in text:text=text.split('\n\nPROP CATALOGS',1)[-1];text='PROP CATALOGS'+text if not text.startswith('PROP CATALOGS') else text
    intro='''SKYTOWN ADAPTATION — CURRENT

24 Medieval presets (Low/Normal/High) và 28 Gothic presets đã cập nhật.
Thanh chống ban công tựa lên cột/trụ giữa ô cửa, có dầm đỡ và mặt tựa phẳng.
Mái che theo ô cửa chính; cột tựa lên móng đá, mép sau chừa khoảng trên vòm cửa.
Cửa tầng 2 mở ra ban công; khung cửa chừa khoảng hở với cột và phào tầng.
Gỗ, vữa, đá và mái dùng texture tileable của /Game/SkyTown.
Đồ nội thất và trang trí lấy geometry/UV/material từ props SkyTown, được bake
vào building mesh và nhúng trong HDA để recook độc lập. Package SkyTown gốc
là nguồn vật liệu cho Unreal. Các Blueprint và HDA giữ nguyên tên asset/type.
Khi recook/bake HDA, unreal_material giữ liên kết vật liệu và UV của SkyTown.

Nguồn hiện tại: Medieval 5.4.0-skytown-joinery; Gothic 3.3.0-skytown-joinery.
Hai map catalog và cách chỉnh tham số/bake bên dưới tiếp tục được dùng.
Ảnh Unreal thực tế, kiểm tra 100 mesh và kết quả: RenderTests/SkyTown/summary.json.
Tools/Props/skytown_catalog.py: materials, refresh_hdas, capture, validate.
Tools/Props/validate_skytown_editor.py: kiểm tra asset/map đã lưu trong editor riêng.

'''
    guide.write_text(intro+text,encoding='utf-8')
    print(json.dumps({'status':'passed','presets':52,'meshes':100,'captures':len(captures)},indent=2))


if __name__=='__main__':main()
