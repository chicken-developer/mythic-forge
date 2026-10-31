"""Recook actual portable Gothic HDA lamp components without re-exporting unchanged FBX."""
from pathlib import Path
import argparse
import json
import hou
from light_layout import sources

P=Path(__file__).resolve().parents[2];R=P/'Saved/PropsDetail';ap=argparse.ArgumentParser();ap.add_argument('--ids',nargs='+',required=True);args=ap.parse_args()
path=P/'SourceAssets/Houdini/Gothic/gothic_building.hda';hou.hda.installFile(str(path))
obj=hou.node('/obj').createNode('geo','light_audit',run_init_scripts=False);asset=obj.createNode('quynh::gothic_building::3.0','generator')
records=R/'Gothic/normal'
for ident in args.ids:
    asset.parm('building_preset').set(ident);asset.parm('mesh_quality').set('game');asset.hdaModule().apply_preset({'node':asset});asset.cook(force=True)
    assert not asset.errors(),asset.errors();geo=asset.geometry();row=json.loads((records/(ident+'.json')).read_text())
    assert len(geo.prims())==row['triangles'],(ident,len(geo.prims()),row['triangles'])
    lights=[];origin=row['source_origin_m']
    for source in sources(geo):
        lights.append({'name':source['name'],'position_m':[source['position_m'][i]-origin[i] for i in range(3)],'lumens':650.,'temperature':2700.,'radius_m':4.5})
    before=len(row['interior_lights']);assert len(lights)>=before,(ident,before,len(lights))
    row['interior_lights']=lights;(records/(ident+'.json')).write_text(json.dumps(row,indent=2))
    report={'id':ident,'previous_lights':before,'actual_lights':len(lights),'triangles_unchanged':row['triangles'],'status':'passed'}
    folder=R/'LightAudit';folder.mkdir(exist_ok=True);(folder/(ident+'.json')).write_text(json.dumps(report,indent=2))
    print('LIGHTS',ident,before,'->',len(lights),flush=True)
