"""Numpy/Pillow software z-buffer renderer. Uses exact supplied vertices/faces.
These are geometry previews, not Houdini or photoreal material renders.
"""
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path

def project(v, direction=None):
    view=np.array(direction if direction is not None else [-.72,.40,-.65],dtype=float);view/=np.linalg.norm(view)
    right=np.cross([0,1,0],view);right/=np.linalg.norm(right);up=np.cross(view,right)
    return np.asarray(v)@np.array([right,up,view]).T

def frame(meshes, direction=None):
    bounds=np.concatenate([project(m.vertices,direction) for m in meshes]);lo=bounds.min(0);hi=bounds.max(0)
    return {'lo':lo.tolist(),'hi':hi.tolist(),'projection':'orthographic','view_direction':direction if direction is not None else [-.72,.40,-.65],'resolution':[1100,900],'light_direction':[-.4,.8,-.5],'ambient':.48}

def render(m,path,camera,title=''):
    W,H=camera['resolution'];lo=np.array(camera['lo']);hi=np.array(camera['hi']);scale=min((W-100)/(hi[0]-lo[0]),(H-120)/(hi[1]-lo[1]));vs=project(m.vertices,camera.get('view_direction'));xy=vs.copy();xy[:,0]=(vs[:,0]-(lo[0]+hi[0])/2)*scale+W/2;xy[:,1]=H-55-(vs[:,1]-lo[1])*scale
    pixels=np.full((H,W,3),233,dtype=np.uint8);zbuf=np.full((H,W),-np.inf,dtype=np.float32)
    light=np.array(camera['light_direction']);light/=np.linalg.norm(light)
    # Soft neutral floor, fixed for all records.
    pixels[:,:,0]=235;pixels[:,:,1]=233;pixels[:,:,2]=227
    vertices=np.asarray(m.vertices)
    for idx,face in enumerate(m.faces):
        pts=vertices[face];n=np.cross(pts[1]-pts[0],pts[2]-pts[0]);nl=np.linalg.norm(n)
        if nl<1e-10:continue
        n/=nl;intensity=.48+.52*max(0,np.dot(n,light));color=np.clip(np.array(m.colors[idx])*intensity*255,0,255).astype(np.uint8)
        if m.materials[idx]=='glass':color=(np.array(m.colors[idx])*180).astype(np.uint8)
        for k in range(1,len(face)-1):
            a,b,c=xy[[face[0],face[k],face[k+1]]];xmin=max(0,int(min(a[0],b[0],c[0])));xmax=min(W-1,int(max(a[0],b[0],c[0]))+1);ymin=max(0,int(min(a[1],b[1],c[1])));ymax=min(H-1,int(max(a[1],b[1],c[1]))+1)
            if xmax<=xmin or ymax<=ymin:continue
            den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den)<1e-8:continue
            yy,xx=np.mgrid[ymin:ymax+1,xmin:xmax+1];xx=xx+.5;yy=yy+.5
            wa=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den;wb=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;wc=1-wa-wb
            zz=wa*a[2]+wb*b[2]+wc*c[2];target=zbuf[ymin:ymax+1,xmin:xmax+1];mask=(wa>=-1e-6)&(wb>=-1e-6)&(wc>=-1e-6)&(zz>target)
            target[mask]=zz[mask];pixels[ymin:ymax+1,xmin:xmax+1][mask]=color
    im=Image.fromarray(pixels);draw=ImageDraw.Draw(im)
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',20) if Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf').exists() else ImageFont.load_default()
    draw.text((24,16),title,fill='#202b33',font=font);draw.text((24,44),'OFFLINE GEOMETRY PREVIEW | '+camera.get('scale_label','fixed scale')+' | Y-up, metres',fill='#665747',font=font)
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);im.save(path)

def compare(reference,hero,path,title):
    canvas=Image.new('RGB',(1800,940),'#f1eee7');draw=ImageDraw.Draw(canvas)
    for im,x in [(Image.open(reference),0),(Image.open(hero),900)]:
        im.thumbnail((890,840));canvas.paste(im,(x+(900-im.width)//2,65+(840-im.height)//2))
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',22) if Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf').exists() else ImageFont.load_default()
    draw.text((22,16),'INPUT / '+title,fill='#273743',font=font);draw.text((922,16),'GENERATED MESH / offline, Houdini cook not run',fill='#273743',font=font)
    Path(path).parent.mkdir(parents=True,exist_ok=True);canvas.save(path)
