"""Render existing v4.2 OBJ geometry with UV-mapped faux materials; no mesh regeneration."""
import json,hashlib,math,os,io
from pathlib import Path
from types import SimpleNamespace
import numpy as np
from PIL import Image,ImageDraw
import render_utils as E
from materials import PALETTE,METRES,save_png
R=Path(__file__).resolve().parent;SOURCE=Path(os.environ.get('SOURCE_PACKAGE',str(R)));TEXTURES={}

def read_obj(path):
 vertices=[];faces=[];mats=[];names=[];name='part';mat='plaster'
 for line in path.read_text().splitlines():
  p=line.split()
  if not p:continue
  if p[0]=='v':vertices.append(list(map(float,p[1:4])))
  elif p[0]=='g':name=p[1]
  elif p[0]=='usemtl':mat=p[1]
  elif p[0]=='f':faces.append([int(x.split('/')[0])-1 for x in p[1:]]);mats.append(mat);names.append(name)
 mesh=SimpleNamespace(vertices=vertices,faces=faces,materials=mats,names=names,normals=[],uvs=[],tangents=[],colors=[])
 v=np.array(vertices)
 for face,mat,name in zip(faces,mats,names):
  pts=v[face];normal=np.cross(pts[1]-pts[0],pts[2]-pts[0]);normal/=np.linalg.norm(normal)
  if mat in ('plaster','stone','rock'):
   axis=int(np.argmax(np.abs(normal)));u=np.eye(3)[2 if axis==0 else 0];u=u-normal*np.dot(u,normal);u/=np.linalg.norm(u);origin=np.zeros(3)
  else:
   edges=np.roll(pts,-1,axis=0)-pts;u=edges[np.argmax(np.linalg.norm(edges,axis=1))];u/=np.linalg.norm(u);origin=pts[0]
  vv=np.cross(normal,u);uv=np.stack([(pts-origin)@u,(pts-origin)@vv],axis=1)/METRES.get(mat,1.)
  if mat not in ('plaster','stone','rock'):
   h=int(hashlib.sha256(name.encode()).hexdigest()[:8],16);uv+=np.array([h%997/997,(h//997)%991/991])
  mesh.uvs.append(uv);mesh.normals.append(normal);mesh.tangents.append((u,vv));mesh.colors.append(PALETTE[mat])
 return mesh

def export(mesh,path):
 # Preserve all vertex coordinates and polygon indices; replace only UV/material binding.
 path.parent.mkdir(exist_ok=True)
 with io.StringIO() as f:
  f.write('# v4.2 geometry, faux reference materials v4.3\nmtllib '+path.with_suffix('.mtl').name+'\n')
  for v in mesh.vertices:f.write('v '+' '.join('%.7f'%a for a in v)+'\n')
  for uv in mesh.uvs:
   for u,v in uv:f.write('vt %.7f %.7f\n'%(u,v))
  for n in mesh.normals:f.write('vn '+' '.join('%.7f'%a for a in n)+'\n')
  off=1
  for i,(face,mat,name) in enumerate(zip(mesh.faces,mesh.materials,mesh.names)):
   f.write('g '+name+'\nusemtl '+mat+'\nf '+' '.join(f'{v+1}/{off+j}/{i+1}' for j,v in enumerate(face))+'\n');off+=len(face)
  path.write_bytes(f.getvalue().encode())
 with path.with_suffix('.mtl').open('w') as f:
  for mat in sorted(set(mesh.materials)):
   f.write(f'newmtl {mat}\nKd 1 1 1\nKa 0.2 0.2 0.2\nKs 0.1 0.1 0.1\nNs 16\nmap_Kd ../textures/{mat}_basecolor.png\nbump -bm 0.12 ../textures/{mat}_height.png\n\n')

def texture(mat):
 if mat not in TEXTURES:
  color=np.array(Image.open(R/'textures'/f'{mat}_basecolor.png').resize((512,512),Image.Resampling.LANCZOS))/255.
  norm=np.array(Image.open(R/'textures'/f'{mat}_normal.png').resize((512,512),Image.Resampling.LANCZOS))/127.5-1
  TEXTURES[mat]=(color,norm)
 return TEXTURES[mat]

def render(m,path,frame,camera):
 W,H=camera['resolution'];q=E.projection(m.vertices,camera);scale=min(W,H)*.88/frame['span'];center=frame['center']
 q[:,0]=(q[:,0]-center[0])*scale+W/2;q[:,1]=H*.49-(q[:,1]-center[1])*scale
 zbuf=np.full((H,W),-np.inf);pixels=np.zeros((H,W,3),np.uint8);pixels[:]=camera['background']
 light=np.array(camera['light_direction'],float);light/=np.linalg.norm(light)
 for fi,face in enumerate(m.faces):
  normal=np.array(m.normals[fi]);u,v=m.tangents[fi];mat=m.materials[fi];tex,nmap=texture(mat)
  grain_variation=.96+.08*(int(hashlib.sha256(m.names[fi].encode()).hexdigest()[:6],16)%100)/100 if mat in ('wood','tile','stone') else 1.
  for k in range(1,len(face)-1):
   tri=q[[face[0],face[k],face[k+1]]];a,b,c=tri
   xmin=max(0,int(np.floor(tri[:,0].min())));xmax=min(W-1,int(np.ceil(tri[:,0].max())))
   ymin=max(0,int(np.floor(tri[:,1].min())));ymax=min(H-1,int(np.ceil(tri[:,1].max())))
   if xmin>xmax or ymin>ymax:continue
   den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
   if abs(den)<1e-8:continue
   xx,yy=np.meshgrid(np.arange(xmin,xmax+1)+.5,np.arange(ymin,ymax+1)+.5)
   aa=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
   bb=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;cc=1-aa-bb
   depth=aa*a[2]+bb*b[2]+cc*c[2];view=zbuf[ymin:ymax+1,xmin:xmax+1]
   mask=(aa>=-1e-6)&(bb>=-1e-6)&(cc>=-1e-6)&(depth>view)
   if not mask.any():continue
   uv=m.uvs[fi][[0,k,k+1]];st=aa[mask,None]*uv[0]+bb[mask,None]*uv[1]+cc[mask,None]*uv[2]
   ix=(st[:,0]*512).astype(int)%512;iy=(-st[:,1]*512).astype(int)%512
   bump=nmap[iy,ix];norm=normal[None,:]+.35*bump[:,0,None]*u[None,:]+.35*bump[:,1,None]*v[None,:];norm/=np.linalg.norm(norm,axis=1,keepdims=True)
   diffuse=np.maximum(0,norm@light);lit=.58+.42*diffuse
   col=tex[iy,ix]*lit[:,None]*grain_variation
   # Cool sky fill in shadows, restrained warm sunlight.
   col*=np.stack([1.+.025*diffuse,np.ones_like(diffuse),1.+.035*(1-diffuse)],axis=1)
   if mat=='glass':col+=np.maximum(0,norm@np.array([.4,.4,-.82]))[:,None]*np.array([.10,.12,.16])
   view[mask]=depth[mask];pixels[ymin:ymax+1,xmin:xmax+1][mask]=np.uint8(np.clip(col*255,0,255))
 valid=np.isfinite(zbuf);occ=np.zeros((H,W));count=np.zeros((H,W))
 for radius in [3,8,16,30]:
  for dx,dy in [(radius,0),(-radius,0),(0,radius),(0,-radius),(radius,radius),(-radius,-radius)]:
   other=np.roll(zbuf,(dy,dx),(0,1));good=valid&np.isfinite(other)
   if dy>0:good[:dy,:]=False
   if dy<0:good[dy:,:]=False
   if dx>0:good[:,:dx]=False
   if dx<0:good[:,dx:]=False
   delta=np.zeros_like(zbuf);np.subtract(other,zbuf,out=delta,where=good)
   occ+=good*(delta>.05)*(delta<1.0)*np.exp(-np.abs(delta)*1.8);count+=good
 shade=1-.65*occ/np.maximum(count,1);pixels[valid]=np.clip(pixels[valid]*shade[valid,None],0,255).astype('uint8')
 save_png(Image.fromarray(pixels),path)

def main():
 cat=json.loads((R/'catalog.json').read_text());out=R/'renders';out.mkdir(exist_ok=True);results=[];cam=dict(E.CAMERA,resolution=[1200,1200],background=[232,236,233],light_direction=[-.5,1,-.65])
 for fam in cat['families']:
  if os.environ.get('FAMILY') and fam['family_id']!=os.environ['FAMILY']:continue
  items=[]
  for file in fam['datasets']:
   cfg=json.loads((R/file).read_text());key=cfg['variant_id'];source=SOURCE/'geometry'/(key+'_high.obj');mesh=read_obj(source);items.append((cfg,mesh))
  frame=E.frame_for([m for c,m in items],cam)
  sheet=Image.new('RGB',(2400,2510),'#e8ece9');d=ImageDraw.Draw(sheet);d.text((25,15),fam['family_id'].upper()+' | BASE / SMALL / COMPACT / LARGE',font=E.font(32),fill='#293943')
  for i,(cfg,m) in enumerate(items):
   key=cfg['variant_id'];export(m,R/'geometry'/(key+'_high.obj'));path=out/(key+'.png')
   if not (os.environ.get('REUSE_RENDERS') and path.exists()):render(m,path,frame,cam)
   x=i%2*1200;y=70+i//2*1220;sheet.paste(Image.open(path),(x,y));ImageDraw.Draw(sheet).text((x+25,y+15),cfg['label'],font=E.font(27),fill='#293943')
   results.append({'variant_id':key,'source_obj_sha256':hashlib.sha256((SOURCE/'geometry'/(key+'_high.obj')).read_bytes()).hexdigest(),'vertices':len(m.vertices),'polygons':len(m.faces),'geometry_changed':False,'uvs_updated':True,'camera':dict(cam,frame=frame),'renderer':'software z-buffer, texture/normal sampling + screen-space AO','native_houdini_unreal_render':False});print('rendered',key,flush=True)
  save_png(sheet,out/(fam['family_id']+'_four_samples.png'))
 (R/('results_'+(os.environ.get('FAMILY') or 'all')+'.json')).write_text(json.dumps(results,indent=2))
if __name__=='__main__':main()
