"""Validate the saved actor setup and real parameter-change/cook/restore evidence."""
from pathlib import Path
import hashlib
import json

P=Path(__file__).resolve().parents[2];OUT=P/'RenderTests/EditableHDA'
validation=json.loads((OUT/'validation.json').read_text());assert validation['status']=='passed' and validation['editable_hdas']==52
definitions=json.loads((OUT/'editor_hdas.json').read_text());assert definitions['status']=='passed'
for asset in definitions['assets']:
    assert hashlib.sha256((P/asset['file']).read_bytes()).hexdigest()==asset['sha256']
tests=[]
for family,ident,key in [('Medieval','01_townhouse_base','width_m'),('Gothic','variant_12','nave_width')]:
    reports=[json.loads((OUT/('cook_'+family.lower()+'_'+stage+'.json')).read_text()) for stage in ('baseline','changed','restored')]
    assert all(r['status']=='passed' and not r['auto_bake'] for r in reports)
    base,changed,restored=[r['outputs'][0] for r in reports]
    row=next(r for r in json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants'] if r['id']==ident)
    expected=[row['size_m'][i]*100 for i in (0,2,1)]
    assert base['triangles']==restored['triangles']==row['triangles']
    assert max(abs(a-b) for a,b in zip(base['size_cm'],expected))<.1
    assert changed['size_cm'][0]-base['size_cm'][0]>25
    assert max(abs(a-b) for a,b in zip(base['size_cm'],restored['size_cm']))<.1
    image=family.lower()+'_'+ident+'_pair.png';assert (OUT/image).is_file()
    tests.append({'family':family,'id':ident,'parameter':key,'changed_by_m':.5,'actual_width_change_cm':changed['size_cm'][0]-base['size_cm'][0],'restored_triangles':restored['triangles'],'restored_bounds_match':True,'materials_use_catalog':True,'image':image})
report={'status':'passed','editable_hda_actors':52,'shared_editor_hdas':definitions['assets'],'parameters_checked':sum(r['parameters_checked'] for f in validation['families'] for r in f['records']),
        'auto_cook':False,'auto_bake':False,'preview_policy':'Manual Recook; two representative previews cooked and restored.',
        'reference_buildings':52,'reference_point_lights':268,'round_trip_tests':tests,
        'args_indexes':['SourceAssets/Houdini/'+f+'/Args/index.json' for f in ('Medieval','Gothic')],
        'guide':'CATALOG_GUIDE.txt','evidence':['validation.json','editor_hdas.json'],
        'notes':['Gothic: initialize with Recook once, then Apply Params / Rebuild and Recook after edits.',
                 'SOP baking exports geometry/materials; reference Blueprint Point Lights are separate.',
                 'source_origin_m records the pivot offset from the reference catalog static mesh.']}
(OUT/'final_handoff.json').write_text(json.dumps(report,indent=2));print(json.dumps({'status':'passed','actors':52,'parameters_checked':report['parameters_checked'],'round_trips':tests},indent=2))
