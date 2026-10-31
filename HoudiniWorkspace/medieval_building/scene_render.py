"""Reproducible perspective hero scenes from the existing Storybook OBJ meshes.
CPU rasterizer: perspective-correct UVs, directional shadow map, sky fill, SSAO,
atmospheric distance haze. No generative replacement of the buildings.
"""
from pathlib import Path
from types import SimpleNamespace
import json,math,hashlib,os,sys,io
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter
import asset_loader as L
R=Path(__file__).resolve().parent;ASSETS=R/'assets'
L.R=ASSETS

def unit(v):
 v=np.asarray(v,float);return v/(np.linalg.norm(v)+1e-12)
def save(im,p):
 b=io.BytesIO();im.save(b,format='PNG');Path(p).write_bytes(b.getvalue())

class Scene:
 def __init__(self,m):
  self.vertices=list(m.vertices);self.faces=list(m.faces);self.materials=list(m.materials);self.names=list(m.names);self.normals=list(m.normals);self.uvs=list(m.uvs);self.tangents=list(m.tangents);self.colors=[None]*len(m.faces);self.vertex_normals=[None]*len(m.faces)
 def face(self,pts,color,name='environment',normals=None):
  self.vertex_normals.append(normals);pts=np.asarray(pts);n=unit(np.cross(pts[1]-pts[0],pts[2]-pts[0]));start=len(self.vertices);self.vertices.extend(pts.tolist());self.faces.append(list(range(start,start+len(pts))));self.normals.append(n);self.materials.append('_environment');self.names.append(name);self.colors.append(np.asarray(color));self.uvs.append(np.zeros((len(pts),2)));self.tangents.append(([1,0,0],[0,0,1]))
 def beam(self,a,b,r,color):
  a=np.array(a);b=np.array(b);d=unit(b-a);u=unit(np.cross(d,[0,0,1] if abs(d[2])<.9 else [1,0,0]));v=np.cross(d,u);sides=6
  aa=[a+r*(u*math.cos(t*math.tau/sides)+v*math.sin(t*math.tau/sides)) for t in range(sides)];bb=[x+(b-a) for x in aa]
  for i in range(sides):j=(i+1)%sides;self.face([aa[i],aa[j],bb[j],bb[i]],color)
  self.face(bb,color)
 def ball(self,c,scale,color,seed=0):
  rng=np.random.default_rng(seed);c=np.array(c);scale=np.array(scale);seg=18;nr=10;grid=[]
  for j in range(nr+1):
   p=math.pi*max(.001,min(nr-.001,j))/nr;row=[]
   for i in range(seg):
    a=i*math.tau/seg;v=np.array([math.sin(p)*math.cos(a),math.cos(p),math.sin(p)*math.sin(a)])
    bulge=1+.06*math.sin(a*3+p*4+seed)+.04*math.sin(a*5-p*3)
    row.append(c+v*scale*bulge)
   grid.append(row)
  for j in range(nr):
   for i in range(seg):
    k=(i+1)%seg;pts=np.array([grid[j][k],grid[j+1][k],grid[j+1][i],grid[j][i]])
    ns=(pts-c)/scale**2;ns/=np.linalg.norm(ns,axis=1,keepdims=True)
    self.face(pts,color,'rock_smooth',ns)
 def leaf(self,c,length,width,color,rng,normal=None):
  c=np.array(c);n=unit(rng.normal(size=3)+[0,.8,0]) if normal is None else unit(normal)
  u=unit(np.cross(n,[0,1,0] if abs(n[1])<.95 else [1,0,0]));v=np.cross(n,u)
  pts=[c+u*x*width+v*y*length for x,y in [(0,-.5),(-.23,-.28),(-.48,-.10),(-.36,.10),(-.24,.28),(0,.5),(.24,.28),(.36,.10),(.48,-.10),(.23,-.28)]]
  self.face(pts,color,'leaf',np.array([unit(n+[0,.9,0])]*len(pts)))
 def foliage(self,c,scale,rng,count=360):
  c=np.array(c);scale=np.array(scale)
  self.ball(c,scale*.77,[.16,.34,.10],seed=int(abs(c.sum()*100)))
  for i in range(count):
   n=unit(rng.normal(size=3));pos=c+n*scale*rng.uniform(.65,1.0)
   light=.85+.35*(n[1]*.5+.5)+rng.uniform(-.08,.08)
   self.leaf(pos,rng.uniform(.16,.29),rng.uniform(.09,.18),np.array([.19,.43,.13])*light,rng,n+[0,.35,0])

def terrain_h(x,z,cliff=False):
 dist=np.sqrt(np.asarray(x)**2+np.asarray(z)**2);fade=np.clip((dist-9)/14,0,1)
 h=(1.4*np.sin(np.asarray(x)*.075)+1.7*np.sin(np.asarray(z)*.07+.6)+.6*np.sin((np.asarray(x)+np.asarray(z))*.15))*fade-.10
 if cliff:
  a=np.clip((np.asarray(x)-5)/7,0,1);b=np.clip((-np.asarray(z)-2)/8,0,1);h-=a*a*(3-2*a)*4+b*b*(3-2*b)*4.5
 return h

def create_environment(s,cfg,seed=22):
 rng=np.random.default_rng(seed);p=cfg['parameters'];cliff=p['context_mode']=='cliff';w=p['width_m'];d=p['depth_m'];water=p['utility_type']=='waterwheel';door=0
 row=p['front_bays'].split('/')[0]
 if 'D' in row:door=-w/2+(row.index('D')+.5)*w/len(row)
 s.road_info=(door,d,cliff)
 def roadx(z):return door+.018*max(0,-z-d/2)**2
 xs=np.linspace(-85,85,100);zs=np.linspace(-75,100,100)
 for i in range(99):
  for j in range(99):
   x=(xs[i]+xs[i+1])/2;z=(zs[j]+zs[j+1])/2;isroad=z<-d/2 and abs(x-roadx(z))<1.2
   river=water and abs(x-(w/2+2.0+.7*np.sin(z*.12)))<1.3
   color=np.array([.29,.52,.10])*(.98+.04*rng.random())
   pts=[];ns=[]
   for xx,zz in [(xs[i],zs[j]),(xs[i+1],zs[j]),(xs[i+1],zs[j+1]),(xs[i],zs[j+1])]:
    channel=.65*np.clip((1.65-abs(xx-(w/2+2+.7*math.sin(zz*.12))))/.55,0,1) if water else 0
    pts.append((xx,terrain_h(xx,zz,cliff)-channel,zz))
    dx=(terrain_h(xx+.02,zz,cliff)-terrain_h(xx-.02,zz,cliff))/.04;dz=(terrain_h(xx,zz+.02,cliff)-terrain_h(xx,zz-.02,cliff))/.04;ns.append(unit([-dx,1,-dz]))
   s.face(pts[::-1],color,'terrain',np.array(ns[::-1]))
 # Match the actual triangulated ground, not only the analytic height field.
 def path_surface(x,z):
  ix=int(np.clip(np.searchsorted(xs,x)-1,0,98));iz=int(np.clip(np.searchsorted(zs,z)-1,0,98));u=(x-xs[ix])/(xs[ix+1]-xs[ix]);v=(z-zs[iz])/(zs[iz+1]-zs[iz])
  a=terrain_h(xs[ix],zs[iz],cliff);b=terrain_h(xs[ix+1],zs[iz],cliff);c=terrain_h(xs[ix+1],zs[iz+1],cliff);dd=terrain_h(xs[ix],zs[iz+1],cliff)
  return a+(b-a)*u+(dd-a)*v if u+v<=1 else c+(dd-c)*(1-u)+(b-c)*(1-v)
 # Continuous winding earth path, independent of terrain mesh tessellation.
 for j in range(80):
  z=-d/2-j*.45;zz=z-.45;x=roadx(z);xx=roadx(zz);width=1.18+.08*math.sin(j*.8)
  pts=[(a,path_surface(a,b)+.035,b) for a,b in [(x-width,z),(xx-width,zz),(xx+width,zz),(x+width,z)]]
  if not cliff:s.face(pts[::-1],[.72,.51,.32],'path')
 # Path stepping stones leading to door, deliberately sparse.
 for i in range(18):
  z=-d/2-1-i*.8;x=roadx(z)+rng.uniform(-.6,.6);y=terrain_h(x,z,cliff)
  s.ball((x,y-.015,z),(.28,.10,.22),(.51,.55,.50),i)
 if cliff:
  for i,(x,z) in enumerate([(-5,-4),(-3,-5),(-4,-6),(-6,0),(6,2),(7,4)]):
   y=terrain_h(x,z,cliff);s.ball((x,y+.35,z),(1.8,1.5,1.3),(.48,.56,.56),650+i)
 # Softly modeled rocks and layered, individually modeled foliage.
 for i in range(16):
  x=rng.uniform(-24,30);z=rng.uniform(6,45)
  if abs(x)<w/2+4 and z<d/2+5:continue
  h=terrain_h(x,z,cliff);sc=rng.uniform(.4,1.2);s.ball((x,h+sc*.3,z),(sc,sc*.65,sc*.8),(.55,.59,.63),i+300)
 treepos=[(-w/2-3,7),(w/2+5,10),(-15,18),(17,23),(-25,36),(28,40),(-35,55),(40,65)]
 for i,(x,z) in enumerate(treepos):
  y=terrain_h(x,z,cliff);h=rng.uniform(3.5,5.5);s.beam((x,y,z),(x,y+h*.8,z),.15,(.27,.20,.18))
  for k in range(7):
   a=k*2.4;dx=math.cos(a)*1.0;dz=math.sin(a)*.8;cy=y+h+(k%3)*.55
   s.beam((x,y+h*.55,z),(x+dx,cy-.4,z+dz),.055,(.30,.22,.18))
   s.foliage((x+dx,cy,z+dz),(1.1,1.,1.05),rng,320)
 for i in range(20):
  side=-1 if i%2 else 1;x=side*(w/2+rng.uniform(.8,3.5));z=rng.uniform(-d/2,5);y=terrain_h(x,z,cliff)
  s.foliage((x,y+.38,z),(.65,.5,.55),rng,110)
 # Ivy: use actual facade/roof surface samples so leaves follow existing modules.
 orig=len(s.faces)
 vertices=np.array(s.vertices)
 for fi in range(min(orig,len(s.materials))):
  if s.materials[fi] not in ('plaster','tile'):continue
  face=s.faces[fi];pts=vertices[face];c=pts.mean(axis=0);n=np.array(s.normals[fi])
  if n[2]>-.35 and n[0]<.5:continue
  if c[1]<1.5:continue
  # Deliberate narrow vertical drapes, not uniform noise over every surface.
  stripe=math.sin(c[0]*1.45+c[1]*.24)
  if stripe<.83 or c[1]>p['floor_count']*p['floor_height_m']+p['foundation_height_m']+1:continue
  area=np.linalg.norm(np.cross(pts[1]-pts[0],pts[-1]-pts[0]));count=min(50,int(area*24))
  for k in range(count):
   a,b=rng.random(2)
   if a+b>1:a,b=1-a,1-b
   pos=pts[0]+a*(pts[1]-pts[0])+b*(pts[-1]-pts[0])+n*.065
   s.leaf(pos,.20,.15,np.array([.24,.47,.12])*rng.uniform(.8,1.15),rng,n+[0,.4,0])
 # Fences flank the trail without cutting across it.
 for side in [-1,1]:
  last=None
  for i in range(5):
   z=-7-i*1.9;x=roadx(z)+side*3.8;y=terrain_h(x,z,cliff)
   s.beam((x,y,z),(x+.07,y+.9,z),.065,(.43,.29,.14))
   if last:
    for hh in [.36,.7]:s.beam((last[0],last[1]+hh,last[2]),(x,y+hh,z),.045,(.47,.32,.17))
   last=(x,y,z)
 # Dense curved grass blades and small five-petal flowers.
 for i in range(17000):
  x=rng.uniform(-23,25);z=rng.uniform(-20,24)
  if abs(x)<w/2+.6 and abs(z)<d/2+.5:continue
  if z<-d/2 and abs(x-roadx(z))<1.35:continue
  if water and abs(x-(w/2+2+.7*np.sin(z*.12)))<1.7:continue
  y=terrain_h(x,z,cliff)+.015;h=rng.uniform(.12,.32);col=np.array([.25,.49,.11])*rng.uniform(.87,1.12)
  a=rng.uniform(0,math.tau);u=np.array([math.cos(a),0,math.sin(a)])*.025;lean=np.array([math.sin(a),0,math.cos(a)])*.10;c=np.array([x,y,z]);mid=c+[0,h*.65,0]+lean*.35;tip=c+[0,h,0]+lean
  ns=np.array([[0,1,0]]*4);s.face([c-u,c+u,mid+u*.5,mid-u*.5],col,'grass',ns);s.face([mid-u*.5,mid+u*.5,tip],col*1.06,'grass',ns[:3])
  if i%33==0:
   c+=np.array([0,h*.75,0]);color=[.94,.93,.84] if i%3 else [.92,.57,.74]
   for k in range(5):
    a=k*math.tau/5;u=np.array([math.cos(a),0,math.sin(a)])*.075;v=np.array([-math.sin(a),0,math.cos(a)])*.032
    s.face([c,c+u+v,c+u*1.7+[0,.015,0],c+u-v],color,'flower',ns)
 if water:
  for j in range(90):
   z=-35+j*.8;zz=z+.8;x=w/2+2+.7*math.sin(z*.12);xx=w/2+2+.7*math.sin(zz*.12)
   ya=terrain_h(x,z,False)-.20;yb=terrain_h(xx,zz,False)-.20
   s.face([(x-1.25,ya,z),(xx-1.25,yb,zz),(xx+1.25,yb,zz),(x+1.25,ya,z)],[.16,.49,.57],'river')
 return {'environment_seed':seed,'road_door_x':door,'environment':'meadow, winding path, fences, clustered trees, rocks, flowers'+(', river' if water else '')}

SUN=unit([-.65,1.,-.75])
def shadow_map(s,size=3072,extent=42.):
 v=np.asarray(s.vertices);right=unit(np.cross([0,1,0],SUN));up=np.cross(SUN,right);q=np.stack([v@right,v@up,v@SUN],1);q[:,:2]=q[:,:2]/extent*size+size/2;buf=np.full((size,size),-1e9)
 for fi,face in enumerate(s.faces):
  if s.names[fi] in ('grass','flower','leaf'):continue
  for k in range(1,len(face)-1):
   tri=q[[face[0],face[k],face[k+1]]];a,b,c=tri
   x0=max(0,int(tri[:,0].min()));x1=min(size-1,int(np.ceil(tri[:,0].max())));y0=max(0,int(tri[:,1].min()));y1=min(size-1,int(np.ceil(tri[:,1].max())))
   if x0>x1 or y0>y1:continue
   den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
   if abs(den)<1e-8:continue
   x,y=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5);aa=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den;bb=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;cc=1-aa-bb;z=aa*a[2]+bb*b[2]+cc*c[2];view=buf[y0:y1+1,x0:x1+1];mask=(aa>=0)&(bb>=0)&(cc>=0)&(z>view);view[mask]=z[mask]
 return buf,right,up,extent

def sky(W,H,rays):
 elev=np.clip(rays[:,:,1],0,1);blue=np.array([.10,.36,.73]);horizon=np.array([.72,.84,.86]);t=np.clip(elev*4.0,0,1);out=horizon[None,None,:]*(1-t[:,:,None])+blue[None,None,:]*t[:,:,None]
 # Multi-scale cloud density with soft edge illumination.
 xx,yy=np.meshgrid(np.linspace(0,1,W),np.linspace(0,1,H));rng=np.random.default_rng(18);noise=np.zeros((H,W))
 for sigma,amp in [(W/9,1),(W/30,.35),(W/90,.13),(W/250,.04)]:
  layer=gaussian_filter(rng.normal(size=(H,W)),sigma);noise+=layer/(layer.std()+1e-8)*amp
 density=noise+.35*np.cos(xx*9)-.65-np.clip((.16-yy)*5,0,.7)
 cloud=np.clip(density*2.3,0,1);cloud*=np.clip((.8-yy)*8,0,1)
 lit=np.clip(.93+np.gradient(noise,axis=0)*8,.76,1)
 out=out*(1-cloud[:,:,None])+lit[:,:,None]*np.array([.96,.97,1.])*cloud[:,:,None]

 return out

def render(s,camera,path,W=1600,H=1000):
 output_size=(W,H);W=int(W*1.5);H=int(H*1.5)
 eye=np.array(camera['eye']);target=np.array(camera['target']);fwd=unit(target-eye);right=unit(np.cross(fwd,[0,1,0]));up=np.cross(right,fwd);v=np.array(s.vertices);rel=v-eye;depth=rel@fwd;focal=W/(2*math.tan(math.radians(camera['horizontal_fov'])/2))
 q=np.stack([W/2+(rel@right)/depth*focal,H/2-(rel@up)/depth*focal,depth],1)
 xx,yy=np.meshgrid((np.arange(W)-W/2)/focal,-(np.arange(H)-H/2)/focal);rays=fwd[None,None,:]+xx[:,:,None]*right+yy[:,:,None]*up;rays/=np.linalg.norm(rays,axis=2,keepdims=True)
 pixels=sky(W,H,rays);zbuf=np.full((H,W),np.inf);world=np.zeros((H,W,3),np.float32);normals=np.zeros_like(world);albedo=np.zeros_like(world)
 print('shadow pass',len(s.faces),flush=True);shadow,sr,su,extent=shadow_map(s)
 for fi,face in enumerate(s.faces):
  if s.names[fi]=='leaf' and len(face)==10:
   bounds=q[face,:2]
   if np.ptp(bounds[:,0])<9 and np.ptp(bounds[:,1])<9:face=[face[j] for j in [0,2,5,8]]
  normal=np.asarray(s.normals[fi]);env=s.colors[fi] is not None
  if not env:tex,nmap=L.texture(s.materials[fi]);tangent,bitangent=s.tangents[fi]
  for k in range(1,len(face)-1):
   ids=[face[0],face[k],face[k+1]];tri=q[ids];a,b,c=tri
   if tri[:,2].min()<.2:continue
   x0=max(0,int(tri[:,0].min()));x1=min(W-1,int(np.ceil(tri[:,0].max())));y0=max(0,int(tri[:,1].min()));y1=min(H-1,int(np.ceil(tri[:,1].max())))
   if x0>x1 or y0>y1:continue
   den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
   if abs(den)<1e-8:continue
   x,y=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5);aa=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den;bb=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;cc=1-aa-bb
   inv=aa/a[2]+bb/b[2]+cc/c[2];zz=1/np.maximum(inv,1e-12);view=zbuf[y0:y1+1,x0:x1+1];mask=(aa>=0)&(bb>=0)&(cc>=0)&(zz<view)
   if not mask.any():continue
   weights=np.stack([aa[mask]/a[2],bb[mask]/b[2],cc[mask]/c[2]],1)*zz[mask,None];pos=weights@v[ids];nn=np.broadcast_to(normal,(len(pos),3)).copy()
   if s.vertex_normals[fi] is not None:
    nn=weights@np.asarray(s.vertex_normals[fi])[[0,k,k+1]];nn/=np.linalg.norm(nn,axis=1,keepdims=True)
   if env:
    col=np.broadcast_to(s.colors[fi],(len(pos),3)).copy()
    if s.names[fi]=='terrain' and s.road_info[2]:
     door,dd,_=s.road_info;rx=door+.018*np.maximum(0,-pos[:,2]-dd/2)**2;edge=1.18-np.abs(pos[:,0]-rx);maskroad=np.clip(edge/.08,0,1)*(pos[:,2]<-dd/2)
     earth=np.array([.72,.51,.32])*(1+.025*np.sin(pos[:,0]*14+np.sin(pos[:,2]*8)))[:,None]
     col=col*(1-maskroad[:,None])+earth*maskroad[:,None]
   else:
    uv=weights@s.uvs[fi][[0,k,k+1]];ix=(uv[:,0]*512).astype(int)%512;iy=(-uv[:,1]*512).astype(int)%512;col=tex[iy,ix].copy()
    if s.materials[fi] in ('wood','tile','stone'):col*=.90+.20*(int(hashlib.sha256(s.names[fi].encode()).hexdigest()[:6],16)%100)/100
    bump=nmap[iy,ix];nn+=.24*bump[:,0,None]*tangent+.24*bump[:,1,None]*bitangent;nn/=np.linalg.norm(nn,axis=1,keepdims=True)
   view[mask]=zz[mask];world[y0:y1+1,x0:x1+1][mask]=pos;normals[y0:y1+1,x0:x1+1][mask]=nn;albedo[y0:y1+1,x0:x1+1][mask]=col
 valid=np.isfinite(zbuf);pos=world[valid];nn=normals[valid];shx=(pos@sr/extent+.5)*shadow.shape[0];shy=(pos@su/extent+.5)*shadow.shape[0];sd=pos@SUN;ix=shx.astype(int);iy=shy.astype(int);lit=np.zeros(len(pos));inside=(ix>1)&(iy>1)&(ix<shadow.shape[0]-2)&(iy<shadow.shape[0]-2)
 for dx,dy in [(0,0),(1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,-1),(1,-1),(-1,1)]:
  val=np.ones(len(pos));val[inside]=(sd[inside]+.08>=shadow[iy[inside]+dy,ix[inside]+dx]);lit+=val/9
 diffuse=np.maximum(0,nn@SUN);amb=(.68 if globals().get('INTERIOR_FILL',False) else .48)+.10*np.maximum(nn[:,1],0);lighting=amb[:,None]*(np.array([1.06,.98,.86]) if globals().get('INTERIOR_FILL',False) else np.array([.89,.96,1.08]))+(.58*diffuse*lit)[:,None]*np.array([1.07,1.015,.89]);col=albedo[valid]*lighting
 # Atmospheric perspective separates background silhouettes from the asset.
 dist=zbuf[valid];fog=1-np.exp(-np.maximum(0,dist-55)*.007);col=col*(1-fog[:,None])+np.array([.65,.79,.82])*fog[:,None]
 pixels[valid]=col
 # Bounded local contact occlusion; shadows are already true light-space shadows.
 occ=np.zeros((H,W));count=np.zeros((H,W))
 for radius in [3,7,14]:
  for dx,dy in [(radius,0),(-radius,0),(0,radius),(0,-radius)]:
   other=np.roll(zbuf,(dy,dx),(0,1));good=valid&np.isfinite(other);delta=np.zeros_like(zbuf);np.subtract(zbuf,other,out=delta,where=good);occ+=good*(delta>.03)*(delta<.5);count+=good
 pixels[valid]*=(1-.14*occ/np.maximum(count,1))[valid,None]
 # Gentle display exposure; retain saturated grass and sky without clipped plaster.
 pixels=np.clip(pixels*1.08,0,1)**.92;save(Image.fromarray(np.uint8(pixels*255)).resize(output_size,Image.Resampling.LANCZOS),path)
 return {'camera':camera,'resolution':list(output_size),'supersampling':1.5,'scene_faces':len(s.faces),'shadow_map':[3072,3072],'light_direction_to_sun':SUN.tolist(),'renderer':'CPU perspective-correct UV rasterizer with sun shadow map, sky ambient, contact AO, distance haze'}

def main(index,kind="base"):
 families=['townhouse','round_workshop','village_inn','windmill','watermill','cliff_house'];family=families[index-1];key=f'{index:02}_{family}_{kind}';cfg=json.loads((ASSETS/'datasets'/(key+'.json')).read_text());obj=ASSETS/'geometry'/(key+'_high.obj');m=L.read_obj(obj);s=Scene(m);env=create_environment(s,cfg,22+index)
 # Hero camera low above ground, with enough margin for rotor and chimneys.
 vv=np.asarray(m.vertices);lo=vv.min(0);hi=vv.max(0);center=(lo+hi)/2;height=hi[1]-lo[1]
 target=center.copy();target[1]=lo[1]+height*.48
 direction=unit([.62,.02,-1.]);fov=48;aspect=int(os.environ.get('WIDTH','1920'))/int(os.environ.get('HEIGHT','1200'));vfov=2*math.atan(math.tan(math.radians(fov)/2)/aspect)
 distance=max(height/(2*math.tan(vfov/2))/.83,(hi[0]-lo[0]+hi[2]-lo[2])*.75/(2*math.tan(math.radians(fov)/2))/.83)+2
 eye=target+direction*distance;eye[1]=lo[1]+height*.30
 for attempt in range(35):
  fwd=unit(target-eye);right=unit(np.cross(fwd,[0,1,0]));up=np.cross(right,fwd);rel=vv-eye;dep=rel@fwd
  px=(rel@right)/dep/math.tan(math.radians(fov)/2);py=(rel@up)/dep/math.tan(vfov/2)
  if max(abs(px).max(),abs(py).max())<.88:break
  distance*=1.04;eye=target+direction*distance;eye[1]=lo[1]+height*.30
 eye=eye.tolist();target=target.tolist()
 camera={'eye':eye,'target':target,'horizontal_fov':fov,'projection':'perspective'}
 out=R/'renders';out.mkdir(exist_ok=True);info=render(s,camera,out/(key+'_hero.png'),int(os.environ.get('WIDTH','1600')),int(os.environ.get('HEIGHT','1000')))
 info.update(env,building_obj=str(obj.relative_to(ASSETS)),building_source_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),geometry_modified=False,props_rebuilt=True,material_revision='5.0-blue-stylized',building_vertices=len(m.vertices),building_polygons=len(m.faces),family=family)
 (out/(key+'_scene.json')).write_text(json.dumps(info,indent=2));print('DONE',key,flush=True)
if __name__=='__main__':main(int(sys.argv[1]),sys.argv[2] if len(sys.argv)>2 else 'base')
