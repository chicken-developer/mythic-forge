"""Restore reviewed material assignments after an FBX reimport."""
from pathlib import Path
import json
import unreal as u
ROOT=Path('E:/Repos/mythic-forge')
report=json.loads((ROOT/'TDKProps/Saved/GothicReview/materials.json').read_text())
records=json.loads((ROOT/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())['variants']
assert len(records)==10
walls=['Limestone','RosePlaster','SagePlaster','PaleStone','Plaster','DarkStone','BluePlaster','BluePlaster','WarmStone','PaleStone']
roofs=['Slate','Oxblood','Copper','Slate','Copper','Oxblood','Slate','Oxblood','Copper','Slate']
report['assignments']=[]
for j,row in enumerate(records):
    mesh=u.load_asset('/Game/Gothic/Buildings/SM_Gothic_'+row['id'])
    choices={'stone':walls[j],'trim':'Ivory','roof':roofs[j],'glass':'AmberGlass','metal':'Iron','gold':'Brass','floor':'Limestone','wood':'Walnut','interior':'Plaster'}
    assignments=[]
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.material_slot_name).replace('Gothic_','')
        material=u.load_asset('/Game/Gothic/Materials/Instances/MI_Gothic_'+choices[key])
        assert material is not None
        mesh.set_material(i,material)
        assignments.append({'slot':key,'material':choices[key]})
    u.EditorAssetLibrary.save_loaded_asset(mesh)
    report['assignments'].append({'id':row['id'],'slots':assignments})
(ROOT/'TDKProps/Saved/GothicReview/materials.json').write_text(json.dumps(report,indent=2))
