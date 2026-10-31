from pathlib import Path
import json,time,hashlib,sys
import build_hda as B
import asset_loader as L
R=Path(__file__).resolve().parent
A=R/'assets';L.R=A
if __name__=='__main__':
 (R/'results').mkdir(exist_ok=True)
 files=sorted((A/'datasets').glob('*.json'))
 if '--sample' in sys.argv:files=[A/'datasets/01_townhouse_base.json']
 for f in files:
  start=time.perf_counter();cfg=B.normalize(json.loads(f.read_text()));f.write_text(json.dumps(cfg,indent=2))
  m=B.generate(cfg);assert m.faces
  path=A/'geometry'/(f.stem+'_high.obj');B.export_obj(m,path)
  mesh=L.read_obj(path);L.export(mesh,path)
  assert all(len(face)>=3 and all(0<=i<len(m.vertices) for i in face) for face in m.faces)
  assert all(B.math.isfinite(v) for pt in m.vertices for v in pt)
  info=dict(id=f.stem,config=cfg,features=m.features,vertices=len(m.vertices),polygons=len(m.faces),elapsed_seconds=time.perf_counter()-start,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),generator_sha256=hashlib.sha256((R/'build_hda.py').read_bytes()).hexdigest(),houdini_cooked='not_run',hda_packaged='not_run',geometry_exported=True,rendered=False)
  (R/'results'/(f.stem+'.json')).write_text(json.dumps(info,indent=2));print(f.stem,info['polygons'],round(info['elapsed_seconds'],2),flush=True)
 (R/'parameter_schema.json').write_text(json.dumps(B.SCHEMA,indent=2))
