"""Read-only verification of the canonical saved Houdini scene."""
from pathlib import Path
import argparse
import hashlib
import json
import hou
import numpy as np

P=Path(__file__).resolve().parents[2];ap=argparse.ArgumentParser();ap.add_argument('family',choices=['Medieval','Gothic']);args=ap.parse_args()
family=args.family;canonical=P.parent/('hda_'+family.lower()+'_building');path=canonical/(family.lower()+'_building.hip')
hou.hipFile.load(str(path),suppress_save_prompt=True,ignore_load_warnings=True)
asset=hou.node('/obj/medieval_building/generator' if family=='Medieval' else '/obj/gothic_building_v2_1/generator');assert asset
asset.cook(force=True);assert not asset.errors(),asset.errors()
geo=asset.geometry();normals=np.asarray(geo.vertexFloatAttribValues('N')).reshape(-1,3)
assert np.isfinite(normals).all() and np.max(abs(np.linalg.norm(normals,axis=1)-1))<1e-3
version=asset.type().definition().sections()['CatalogDetailVersion'].contents()
published=json.loads((P/'Saved/PropsDetail'/(family.lower()+'_source_publish.json')).read_text())
assert version==published['version']
lights=len({name for name in geo.primStringAttribValues('name') if 'light_source' in name});assert lights
report={'status':'passed','family':family,'hip':str(path),'hip_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'version':version,'preset':asset.evalParm('building_preset'),'lights':lights,'invalid_normals':0,'runtime':hou.applicationVersionString()}
(P/'RenderTests/PropsDetail'/(family.lower()+'_source_scene.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
