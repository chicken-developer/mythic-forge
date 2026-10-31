"""Actual v5.1 geometry preview. Cutaway is a render-only face visibility selection."""
from pathlib import Path
import os,sys,time,json,hashlib
import numpy as np
import scene_render as S
import asset_loader as L
import build_hda as B
R=Path(__file__).resolve().parent;A=R/'assets';L.R=A
KEY='01_townhouse_base'
def subset(m,keep):
 from types import SimpleNamespace
 fields=['faces','materials','names','normals','uvs','tangents','colors']
 return SimpleNamespace(vertices=m.vertices,**{f:[getattr(m,f)[i] for i in keep] for f in fields})
def main(mode='cutaway',width=960):
 t=time.perf_counter();cfg=json.loads((A/'datasets'/(KEY+'.json')).read_text());p=cfg['parameters'];base=p['foundation_height_m'];fh=p['floor_height_m']
 obj=A/'geometry'/(KEY+'_high.obj');m=L.read_obj(obj);v=np.array(m.vertices);keep=[]
 if mode=='room':sm=m
 else:
  clipped=B.Mesh(cfg);clipped.scale=1.0;clipped.high=False
  maxy=base+fh-.01 if mode=='ground' else base+p['floor_count']*fh-.01
  for i,face in enumerate(m.faces):
   pts=v[face];center=pts.mean(0);name=m.names[i];inside=name.startswith('int_')
   if not inside and (center[2]<-p['depth_m']/2+.3 or center[0]>p['width_m']/2-.3):continue
   poly=[]
   for aa,bb in zip(pts,np.roll(pts,-1,axis=0)):
    ina=aa[1]<=maxy;inb=bb[1]<=maxy
    if ina:poly.append(aa.tolist())
    if ina!=inb:poly.append((aa+(bb-aa)*(maxy-aa[1])/(bb[1]-aa[1])).tolist())
   if len(poly)>=3:clipped.face(poly,m.materials[i],name)
  temp=R/'previews';temp.mkdir(exist_ok=True);viewobj=temp/(KEY+'_'+mode+'_section.obj');B.export_obj(clipped,viewobj);sm=L.read_obj(viewobj)
 s=S.Scene(sm);keep=sm.faces
 if mode=='ground':camera={'eye':[8,7,-10],'target':[0,1.3,.0],'horizontal_fov':40}
 elif mode=='room':camera={'eye':[1.35,base+1.65,-2.05],'target':[-.4,base+1.25,1.1],'horizontal_fov':78}
 else:camera={'eye':[12,10,-19],'target':[0,4.1,0],'horizontal_fov':38}
 if mode=='room':
  # Clip geometry against the near plane before rasterization; preserve floor/walls crossing camera.
  clipped=B.Mesh(cfg);clipped.scale=1.;clipped.high=False
  eye=np.array(camera['eye']);forward=S.unit(np.array(camera['target'])-eye)
  for i,face in enumerate(m.faces):
   pts=v[face];poly=[]
   for aa,bb in zip(pts,np.roll(pts,-1,axis=0)):
    da=np.dot(aa-eye,forward)-.201;db=np.dot(bb-eye,forward)-.201
    if da>=0:poly.append(aa.tolist())
    if (da>=0)!=(db>=0):poly.append((aa+(bb-aa)*da/(da-db)).tolist())
   if len(poly)>=3:clipped.face(poly,m.materials[i],m.names[i])
  temp=R/'previews';temp.mkdir(exist_ok=True);vp=temp/(KEY+'_room_nearclip.obj');B.export_obj(clipped,vp);s=S.Scene(L.read_obj(vp));keep=s.faces
  S.INTERIOR_FILL=True
 # Neutral presentation floor supports the cutaway; no exterior vegetation hiding interior.
 if mode!='room':
  for x in range(-30,30,3):
   for z in range(-30,30,3):s.face([[x,-.06,z],[x,-.06,z+3],[x+3,-.06,z+3],[x+3,-.06,z]],[.42,.45,.46],'presentation_ground')
  S.sky=lambda W,H,rays:np.broadcast_to(np.array([.48,.54,.59]),(H,W,3)).copy()
 out=R/'previews';out.mkdir(exist_ok=True);path=out/(KEY+'_'+mode+'.png')
 S.SUN=S.unit([-.55,.85,-.65])
 info=S.render(s,camera,path,width,int(width*.625))
 info.update(interior_ambient_fill=(.68 if mode=='room' else .48),preview_script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),mode=mode,view_type='intact interior camera' if mode=='room' else 'render-only cutaway; exported geometry remains complete',source_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),renderer_source_sha256=hashlib.sha256((R/'scene_render.py').read_bytes()).hexdigest(),elapsed_seconds=time.perf_counter()-t,source_polygons=len(m.faces),visible_polygons=len(keep),provenance='offline_geometry_preview')
 (out/(path.stem+'.json')).write_text(json.dumps(info,indent=2));print(info,flush=True)
if __name__=='__main__':main(sys.argv[1] if len(sys.argv)>1 else 'ground',int(sys.argv[2]) if len(sys.argv)>2 else 960)
