"""Deterministic faux stylized surface library. No source screenshot pixels are used."""
import numpy as np
from scipy.ndimage import gaussian_filter
from PIL import Image
from pathlib import Path
import json,io

def save_png(im,path):
 b=io.BytesIO();im.save(b,format="PNG");Path(path).write_bytes(b.getvalue())
R=Path(__file__).resolve().parent
PALETTE={'plaster':(222,219,210),'wood':(98,79,76),'tile':(76,119,195),'stone':(142,145,154),'teal':(72,116,117),'glass':(99,127,165),'metal':(75,72,65),'canvas':(81,137,141),'sail':(225,211,180),'leaf':(79,130,49),'flower':(231,174,61),'light':(255,213,135),'rock':(121,139,145),'water':(67,145,161),'ember':(255,114,39)}
METRES={'plaster':7.0,'wood':1.6,'tile':.8,'stone':1.1,'rock':3.0}

def noise(rng,n,sigma):
 a=gaussian_filter(rng.normal(size=(n,n)),sigma,mode='wrap');return a/(a.std()+1e-8)

def create(n=1024):
 out=R/'assets'/'textures';out.mkdir(exist_ok=True);manifest={};yy,xx=np.mgrid[0:n,0:n]/n
 for i,(name,base) in enumerate(PALETTE.items()):
  rng=np.random.default_rng(4100+i);broad=noise(rng,n,n/22);fine=noise(rng,n,2);mid=noise(rng,n,n/90)
  height=.5+.025*mid;shade=1+.025*broad+.006*fine
  color=np.ones((n,n,3))*base
  if name=='wood':
   wave=yy*18+.6*np.sin(xx*2*np.pi)+.2*np.sin(xx*6*np.pi)+.10*mid
   groove=np.maximum(0,np.cos(wave*2*np.pi))**18
   shade=1+.045*broad+.012*mid-.07*groove
   height=.5+.04*mid-.13*groove
  elif name=='plaster':
   # Large worn patches with warm cream body, darker undercoat and occasional blue stone.
   worn=broad>1.95;edge=(broad>1.90)&(~worn)
   color[worn]=[184,171,153];color[edge]=[236,228,213]
   exposed=broad>2.45
   row=np.floor(yy*7);col=np.floor(xx*5+(row%2)*.5)
   mortar=(np.mod(yy*7,1)<.055)|(np.mod(xx*5+(row%2)*.5,1)<.045)
   stonevar=.92+.12*np.sin(col*17+row*29)
   for k,c in enumerate([126,129,139]):color[:,:,k][exposed]=c*stonevar[exposed]
   color[exposed&mortar]=[86,91,90]
   height=.6-.12*worn-.1*exposed+.025*mid
   # Sparse broad crack lines; all periodic and reproducible.
   cracks=(np.abs(np.sin(2*np.pi*(xx*2+.08*np.sin(yy*4*np.pi))))<.006)&(mid>.1)
   shade=1+.022*mid-.16*cracks;height-=.04*cracks
  elif name in ('stone','rock'):
   shade=1+.07*broad+.035*mid
   vein=np.maximum(0,.075-np.abs(np.sin(xx*2*np.pi+yy*4*np.pi+.3*mid)))/.075
   shade-=.045*vein;height=.5+.07*mid+.03*broad
  elif name=='tile':
   grain=np.maximum(0,np.cos(yy*60*np.pi+.4*np.sin(xx*4*np.pi)))**12
   shade=1+.035*broad+.01*mid-.025*grain;height=.5+.035*mid-.025*grain
  elif name in ('sail','canvas'):
   shade=1+.018*broad+.012*np.sin(xx*240*np.pi)*np.sin(yy*240*np.pi);height=.5+.012*mid
  color=np.clip(color*shade[:,:,None],0,255).astype('uint8')
  # Portable OpenGL tangent normal from height, intentionally shallow stylized relief.
  gx=(np.roll(height,-1,1)-np.roll(height,1,1))*7;gy=(np.roll(height,-1,0)-np.roll(height,1,0))*7
  normal=np.stack([-gx,gy,np.ones_like(gx)],axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
  rough=np.clip(.82+.035*broad,0,1)
  if name=='glass':rough[:]=.25
  if name=='metal':rough[:]=.48
  for suffix,data in [('basecolor',color),('normal',np.uint8((normal*.5+.5)*255)),('roughness',np.uint8(rough*255)),('height',np.uint8(np.clip(height,0,1)*255))]:save_png(Image.fromarray(data),out/(name+'_'+suffix+'.png'))
  manifest[name]={'tile_size_m':METRES.get(name,1.0),'basecolor':f'textures/{name}_basecolor.png','normal_opengl':f'textures/{name}_normal.png','roughness':f'textures/{name}_roughness.png','height':f'textures/{name}_height.png','normal_strength':.35,'metallic':.65 if name=='metal' else 0.0}
 (R/'assets'/'materials.json').write_text(json.dumps(manifest,indent=2));return manifest
if __name__=='__main__':create()
