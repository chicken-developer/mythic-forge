from pathlib import Path
import json,hashlib,xml.etree.ElementTree as ET
import numpy as np
from PIL import Image
root=Path('E:/Repos/mythic-forge')
src=root/'SubstanceWorkspace/src'
r={'status':'passed','graphs':[],'normal_format':'DirectX','resolution':[2048,2048],'orm_channels':{'R':'AO','G':'Roughness','B':'Metallic'}}
for graph in json.loads((src/'material_manifest.json').read_text())['graphs']:
 name=graph['name'];maps={}
 for c in ['BaseColor','Normal','ORM','Height','Roughness','Metallic','AO']:
  path=src/'exports'/('T_Gothic_'+name+'_'+c+'.png')
  with Image.open(path) as im:
   im.load();assert im.size==(2048,2048),(path,im.size)
   maps[c]=np.array(im)
 def gray8(a):
  a=a[:,:,0] if a.ndim==3 else a
  return (a.astype(float)/257.).round().astype(int) if a.dtype==np.uint16 else a.astype(int)
 errors={c:int(np.max(np.abs(maps['ORM'][:,:,i].astype(int)-gray8(maps[c])))) for i,c in enumerate(['AO','Roughness','Metallic'])}
 assert max(errors.values())<=1,(name,errors)
 r['graphs'].append({'name':name,'outputs':7,'orm_channel_max_error':errors})
tree=ET.parse(src/'Gothic_Stylized.sbs')
assert len(tree.findall('./content/graph'))==10
r['native_graph_count']=10
r['source_sha256']=hashlib.sha256((src/'Gothic_Stylized.sbs').read_bytes()).hexdigest()
r['sbsar_sha256']=hashlib.sha256((src/'Gothic_Stylized.sbsar').read_bytes()).hexdigest()
source=json.loads((root/'TDKProps/SourceAssets/Gothic/mesh_manifest.json').read_text())
assert hashlib.sha256((root/'HoudiniWorkspace/gothic_building/gothic_building.hip').read_bytes()).hexdigest()==source['source_sha256']
r['source_hip_preserved']=True
(root/'TDKProps/Saved/GothicReview/texture_validation.json').write_text(json.dumps(r,indent=2))
print('Validated 70 textures; ORM error <= 1/255; 10 native graphs; source HIP unchanged')
