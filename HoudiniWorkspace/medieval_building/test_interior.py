from pathlib import Path
import json,hashlib,importlib.util,sys
import build_hda as B
R=Path(__file__).resolve().parent
cfg=json.loads((R/'assets/datasets/01_townhouse_base.json').read_text())
def digest(m):return hashlib.sha256(json.dumps([m.vertices,m.faces,m.names,m.materials],separators=(',',':')).encode()).hexdigest()
if __name__=='__main__':
 out={};m=B.generate(cfg);m2=B.generate(cfg);assert digest(m)==digest(m2);out['same_seed_geometry_deterministic']=True
 p=cfg['parameters'];x0,x1,z0,z1,run,sw=B.stair_rect_bounds(0,0,p['width_m'],p['depth_m'],p)
 assert sw>=.75 and run/(max(7,B.math.ceil(p['floor_height_m']/.36)))>=.20
 for f,name in zip(m.faces,m.names):
  if not name.startswith('int_floor_plank'):continue
  pts=[m.vertices[i] for i in f]
  if min(a[1] for a in pts)<p['foundation_height_m']+p['floor_height_m']-.01:continue
  xa,xb=min(a[0] for a in pts),max(a[0] for a in pts);za,zb=min(a[2] for a in pts),max(a[2] for a in pts)
  assert min(xb,x1)-max(xa,x0)<=1e-6 or min(zb,z1)-max(za,z0)<=1e-6,'Floor crosses stairwell'
 out['upper_floor_void_not_covered_by_planks']=True
 assert m.features['interior_floor_openings']==p['floor_count']-1
 out['opening_count_matches_storeys']=True
 off=json.loads(json.dumps(cfg));off['parameters']['interior_enable']=False
 moff=B.generate(off);assert not any('int_' in n for n in moff.names)
 out['interior_toggle_removes_all_interior_modules']=True
 nf=json.loads(json.dumps(cfg));nf['parameters']['interior_furniture']=False;mn=B.generate(nf)
 assert not any(n.startswith(('int_bed','int_table','int_shelf')) for n in mn.names)
 assert mn.features['interior_floor_openings']==m.features['interior_floor_openings'];out['furniture_toggle_preserves_structure']=True
 bad=json.loads(json.dumps(cfg));bad['parameters']['interior_stair_width_m']=.1
 try:B.normalize(bad);raise AssertionError('Invalid stair width accepted')
 except ValueError:out['invalid_width_rejected']=True
 compile(B._source(),'<portable_core>','exec')
 out['portable_core_compiles']=True;out['houdini_cook']='not_run';out['navmesh_collision_headroom_full_validation']='not_run'
 (R/'interior_tests.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
