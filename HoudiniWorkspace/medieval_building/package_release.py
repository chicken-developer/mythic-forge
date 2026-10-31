from pathlib import Path
import json,hashlib,zipfile,math
from PIL import Image
R=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
records=[]
for f in sorted((R/'assets/datasets').glob('*.json')):
 result=R/'results'/(f.stem+'.json');info=json.loads(result.read_text());obj=R/'assets/geometry'/(f.stem+'_high.obj')
 v=[];faces=[];names=set();vn=0
 for line in obj.read_text().splitlines():
  p=line.split()
  if not p:continue
  if p[0]=='v':v.append([float(x) for x in p[1:4]])
  elif p[0]=='f':faces.append([int(x.split('/')[0])-1 for x in p[1:]])
  elif p[0]=='g':names.add(p[1])
  elif p[0]=='vn':vn+=1
 assert len(v)==info['vertices'] and len(faces)==info['polygons']
 assert sha(obj)==info['source_sha256'] and sha(R/'build_hda.py')==info['generator_sha256']
 assert v and faces and all(math.isfinite(x) for pt in v for x in pt)
 assert all(len(face)>=3 and all(0<=i<len(v) for i in face) for face in faces)
 assert any('int_' in n for n in names)
 info['interior_module_groups']=len([n for n in names if 'int_' in n])
 info['feature_count_scope']='main body and attached circular tower; wing geometry present but counters are not aggregated'
 info['rendered']=(f.stem=='01_townhouse_base');info['geometry_validation']='passed';result.write_text(json.dumps(info,indent=2))
 records.append(dict(id=f.stem,config=str(f.relative_to(R)),geometry=str(obj.relative_to(R)),result=str(result.relative_to(R)),vertices=len(v),polygons=len(faces),source_sha256=sha(obj)))
assert len(records)==24
previews=[]
for f in sorted((R/'previews').glob('*.png')):
 with Image.open(f) as im:im.load();assert im.size==(1920,1200)
 j=json.loads(f.with_suffix('.json').read_text());assert j['source_sha256']==records[0]['source_sha256']
 assert j['renderer_source_sha256']==sha(R/'scene_render.py')
 assert j['preview_script_sha256']==sha(R/'render_interior.py')
 previews.append(dict(file=str(f.relative_to(R)),metadata=str(f.with_suffix('.json').relative_to(R)),elapsed_seconds=j['elapsed_seconds'],sha256=sha(f)))
assert len(previews)==3
manifest=dict(version='5.1-interior',baseline='restored v5',records=records,previews=previews,geometry_count=24,render_count=3,hda_packaged='not_run',houdini_cooked='not_run',image_provenance='offline_geometry_preview',limitations='Basic procedural interiors; not collision/NavMesh or full headroom validated')
(R/'manifest.json').write_text(json.dumps(manifest,indent=2))
(R/'validation.json').write_text(json.dumps(dict(geometry_records_passed=len(records),decoded_images=len(previews),source_hashes_match=True,geometry_test_results=json.loads((R/'interior_tests.json').read_text())),indent=2))
out=R.parent/'Storybook_Interior_v5_1.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=5) as z:
 for p in R.rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts:z.write(p,Path(R.name)/p.relative_to(R))
print(out,out.stat().st_size)
