"""Batch cook/export/measurement. --offline uses the exact shared polygon core.
Rendering/comparison only: install numpy + Pillow in the running Python.
Houdini build/export without --render needs stdlib + hou only.
"""
import argparse
import hashlib
import json
import math
import platform
import time
import traceback
from pathlib import Path
import build_hda as B

CAMERA={'projection':'orthographic','eye_direction':[1.2,.88,-1.65],
        'light_direction':[-.5,1,-.7], 'background':[239,234,222],
        'resolution':[1000,1000],'framing':'common projected maximum bounds',
        'backend':'software_zbuffer_flat_diffuse'}

def metrics(m):
    finite=all(math.isfinite(v) for p in m.vertices for v in p)
    degenerate=0;duplicates=0;seen=set();triangles=0
    for face in m.faces:
        area=0
        for j in range(1,len(face)-1):
            n=B.cross(B.sub(m.vertices[face[j]],m.vertices[face[0]]),B.sub(m.vertices[face[j+1]],m.vertices[face[0]]))
            area+=math.sqrt(B.dot(n,n))/2;triangles+=1
        degenerate+=area<1e-10
        key=tuple(sorted(tuple(round(v,6) for v in m.vertices[i]) for i in face))
        duplicates+=key in seen;seen.add(key)
    bounds=[[min(p[i] for p in m.vertices) for i in range(3)],[max(p[i] for p in m.vertices) for i in range(3)]]
    return {'point_count':len(m.vertices),'polygon_count':len(m.faces),'triangle_count':triangles,
        'bounds_m':bounds,'finite_coordinates':finite,'degenerate_faces':degenerate,
        'exact_duplicate_faces':duplicates,'uv_corner_count':sum(len(a) for a in m.uvs),
        'material_families':sorted(set(m.materials)),'part_count':len(set(m.names)),
        'geometry_sha256':B.geometry_hash(m)}

def projection(vertices,camera):
    import numpy as np
    eye=np.array(camera['eye_direction'],float);eye/=np.linalg.norm(eye)
    right=np.cross([0,1,0],eye);right/=np.linalg.norm(right);up=np.cross(eye,right)
    v=np.asarray(vertices,float)
    return np.stack((v@right,v@up,v@eye),axis=1)

def frame_for(meshes,camera):
    import numpy as np
    allpoints=np.concatenate([projection(m.vertices,camera) for m in meshes])
    low=allpoints[:,:2].min(axis=0);hi=allpoints[:,:2].max(axis=0)
    center=(low+hi)/2;span=max(hi-low)*1.12
    return {'center':center.tolist(),'span':float(span)}

def render(m,path,frame,camera=None,wire=False):
    import numpy as np
    from PIL import Image,ImageDraw,ImageFont
    camera=camera or CAMERA;W,H=camera['resolution'];q=projection(m.vertices,camera)
    scale=min(W,H)*.88/frame['span'];center=frame['center']
    q[:,0]=(q[:,0]-center[0])*scale+W/2;q[:,1]=H*.49-(q[:,1]-center[1])*scale
    zbuf=np.full((H,W),-np.inf);pixels=np.full((H,W,3),camera['background'],np.uint8)
    light=np.array(camera['light_direction'],float);light/=np.linalg.norm(light)
    # Every face is triangulated from the same ordered vertex array used by HOM.
    for fi,face in enumerate(m.faces):
        normal=np.asarray(m.normals[fi]);shade=.48+.52*max(0,float(normal@light))
        col=np.clip(np.array(m.colors[fi])*255*shade,0,255).astype(np.uint8)
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
            view[mask]=depth[mask];pixels[ymin:ymax+1,xmin:xmax+1][mask]=col
            if wire:
                edge=mask&(np.minimum(np.minimum(aa,bb),cc)<.045)
                pixels[ymin:ymax+1,xmin:xmax+1][edge]=[54,60,62]
    # Screen-space contact occlusion, bounded in world depth; no mesh changes.
    valid=np.isfinite(zbuf);occ=np.zeros((H,W),float);count=np.zeros((H,W),float)
    for radius in [3,7,14,25]:
        for dx,dy in [(radius,0),(-radius,0),(0,radius),(0,-radius),(radius,radius),(-radius,-radius)]:
            other=np.roll(zbuf,(dy,dx),(0,1));good=valid & np.isfinite(other)
            if dy>0:good[:dy,:]=False
            if dy<0:good[dy:,:]=False
            if dx>0:good[:,:dx]=False
            if dx<0:good[:,dx:]=False
            delta=np.zeros_like(zbuf);np.subtract(other,zbuf,out=delta,where=good)
            occ+=good*(delta>.07)*(delta<.85)*np.exp(-np.abs(delta)*2)
            count+=good
    shade=1-.48*occ/np.maximum(count,1)
    pixels[valid]=np.clip(pixels[valid]*shade[valid,None],0,255).astype(np.uint8)
    im=Image.fromarray(pixels);path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);im.save(path)
    return im

def font(size):
    from PIL import ImageFont
    for path in ['/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf','C:/Windows/Fonts/arial.ttf']:
        if Path(path).exists():return ImageFont.truetype(path,size)
    return ImageFont.load_default(size=size)

def comparison(source,hero,out,record,provenance):
    from PIL import Image,ImageDraw,ImageOps
    im=Image.new('RGB',(1800,1100),(247,244,236));draw=ImageDraw.Draw(im)
    draw.text((45,25),record['variant_id']+' / '+record.get('label',''),font=font(30),fill=(47,49,47))
    for x,path in [(40,source),(920,hero)]:
        pic=Image.open(path).convert('RGB');pic=ImageOps.contain(pic,(840,875));im.paste(pic,(x+(840-pic.width)//2,100+(875-pic.height)//2))
    draw.text((40,76),'INPUT / '+', '.join(record.get('reference_ids',[])),font=font(18),fill=(88,83,74))
    draw.text((920,76),provenance,font=font(18),fill=(88,83,74))
    p=record['parameters'];line='%s | %s floors | %s | %s | %s'%(p['footprint'],p['floor_count'],p['roof_shape'],p['balcony_layout'],p['utility_type'])
    draw.text((45,1010),line,font=font(22),fill=(47,49,47))
    draw.text((45,1053),'Fixed batch camera / polygon geometry / stylized textures and vegetation excluded',font=font(18),fill=(88,83,74))
    Path(out).parent.mkdir(parents=True,exist_ok=True);im.save(out)
