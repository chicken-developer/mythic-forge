"""Verify the saved portable HDA, preset switching, and shared Game/High design."""
from pathlib import Path
import json
import hou
from production_pipeline import validate

ROOT=Path(__file__).parent
hou.hipFile.load(str(ROOT/'gothic_building.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
asset=hou.node('/obj/gothic_building_v2_1/generator');assert asset.type().name()=='quynh::gothic_building::3.0'
module=asset.hdaModule();assert len(module.PRESETS)==29
assert 'PRODUCTION_SOURCE' in asset.type().definition().sections()['PythonModule'].contents()
asset.parm('building_preset').set('variant_11');module.apply_preset({'node':asset})
game=validate(asset);game_parts=set(asset.geometry().primStringAttribValues('name'))
asset.parm('mesh_quality').set('high');module.rebuild(asset)
high=validate(asset);high_parts=set(asset.geometry().primStringAttribValues('name'))
assert game_parts==high_parts,'High introduced different architectural components'
assert high['triangles']>game['triangles']
assert max(abs(a-b) for a,b in zip(game['min_m']+game['max_m'],high['min_m']+high['max_m']))<.02
asset.parm('mesh_quality').set('game');asset.parm('building_preset').set('variant_22');module.apply_preset({'node':asset})
manor=validate(asset)
report={'status':'passed','type':asset.type().name(),'preset_count':28,'embedded':True,'game_high_same_components':True,'small_house_game':game,'small_house_high':high,'noble_manor':manor}
(ROOT/'reports/portable_v3_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('PORTABLE_V3_PASSED',game['triangles'],high['triangles'],manor['triangles'],flush=True)
