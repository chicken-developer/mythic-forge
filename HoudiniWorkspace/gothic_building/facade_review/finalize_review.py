"""Verify saved source, imported counts, final renders, and the actual PIE report."""
from pathlib import Path
import ast
import hashlib
import json
import hou
from PIL import Image

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).parent
EXPORT=ROOT/'TDKProps/SourceAssets/Gothic/FacadeReview'
RENDERS=ROOT/'TDKProps/RenderTests/Gothic/FacadeReview'
SAVED=ROOT/'TDKProps/Saved/GothicReview/Facade'
manifest=json.loads((EXPORT/'facade_manifest.json').read_text())
engine=json.loads((SAVED/'unreal_review.json').read_text())
play=json.loads((RENDERS/'play_validation.json').read_text())
assert manifest['status']=='passed' and play['status']=='passed'
assert manifest['variants'][1]['components']==manifest['variants'][2]['components']
source_hash=hashlib.sha256((HERE.parent/'gothic_building.hip').read_bytes()).hexdigest()
assert source_hash==manifest['source_hip_sha256'],'Canonical building source changed'
for row in manifest['variants']:
    assert hashlib.sha256((ROOT/row['file']).read_bytes()).hexdigest()==row['sha256']
    actual=next(r for r in engine['meshes'] if r['name']==row['name'])
    assert actual['triangles']==row['triangles']
    if row['name']!='Legacy':
        assert all(row[k]==0 for k in ['boundary_edges','nonmanifold_edges','degenerate_triangles','invalid_vertex_normals','inverted_or_zero_volume_shells'])
        assert actual['nanite']
hou.hipFile.load(str(HERE/'gothic_facade_review.hip'),suppress_save_prompt=True)
graphs={}
for label in ['02_REVISED_GAME','03_HIGH_DETAIL_SOURCE']:
    obj=hou.node('/obj/'+label);assert obj is not None
    nodes=list(obj.allSubChildren())
    assert not any(n.type().name().startswith('python') for n in nodes)
    assert not any(any(s in n.name() for s in ['crocket','finial','clustered_pier']) for n in nodes)
    graphs[label]={'native_nodes':len(nodes),'python_sops':0,'sweeps':sum(n.type().name().startswith('sweep') for n in nodes),'bevels':sum(n.type().name().startswith('polybevel') for n in nodes),'booleans':sum(n.type().name().startswith('boolean') for n in nodes)}
assert not hou.node('/obj/03_HIGH_DETAIL_SOURCE').isDisplayFlagSet()
images=[]
for name in ['comparison','legacy_arch','game_arch','legacy_pinnacle','game_pinnacle','game_cornice','game_threequarter','high_pinnacle','high_arch']:
    path=RENDERS/(name+'.png')
    with Image.open(path) as im:
        assert im.size==(2560,1440),(name,im.size)
        im.verify()
    images.append(path.relative_to(ROOT).as_posix())
for path in [HERE/'build_facade.py',ROOT/'TDKProps/Tools/Gothic/build_facade_review.py',ROOT/'TDKProps/Tools/Gothic/capture_facade_review.py',ROOT/'TDKProps/Tools/Gothic/check_facade_play.py']:
    ast.parse(path.read_text(encoding='utf-8'))
report={'status':'passed','stage':'Facade geometry prototype for visual review','canonical_building_hip_preserved':True,'ten_preset_rollout':False,'normal_map_baked':False,'normals':'Area-weighted vertex normals imported from FBX; gray material has no normal texture','uvs':'Metric planar UVs for this geometry review; bake UVs and material production are outside this prototype','houdini_source':'HoudiniWorkspace/gothic_building/facade_review/gothic_facade_review.hip','unreal_map':engine['map'],'native_graphs':graphs,'source_geometry':manifest['variants'],'unreal_meshes':engine['meshes'],'pie':play,'renders':images}
report['concept_revision']=manifest['concept_revision']
(RENDERS/'review_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(RENDERS/'unreal_review.json').write_text(json.dumps(engine,indent=2),encoding='utf-8')
(RENDERS/'concept_revision.json').write_text(json.dumps(manifest['concept_revision'],indent=2),encoding='utf-8')
print('FACADE_DELIVERY_VALIDATED',json.dumps(graphs),flush=True)
