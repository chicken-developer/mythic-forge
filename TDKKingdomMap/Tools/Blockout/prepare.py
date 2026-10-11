"""Deterministic, dependency-free layout compiler. Design input is always layout.json.
Generates 16-bit Landscape data, explicit collision meshes, preview and validation.
"""
import argparse, hashlib, html, json, math, random, re, struct, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT/'Design/Blockout'
COLORS = {'Medieval':'#589bc9','Hybrid':'#65a278','Gothic':'#b45150',
          'Terrain':'#96a37b','Roads':'#ddc9a0','Walls':'#7d8490','Water':'#36758c','Roof':'#515768','Landmarks':'#dab66b'}

def inside(p, poly):
    x,y = p[:2]; hit = False
    for a,b in zip(poly, poly[1:]+poly[:1]):
        if (a[1]>y) != (b[1]>y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]: hit = not hit
    return hit
def project(p,a,b):
    dx,dy=b[0]-a[0],b[1]-a[1]; l2=dx*dx+dy*dy
    t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/l2)) if l2 else 0
    return math.hypot(p[0]-a[0]-dx*t,p[1]-a[1]-dy*t),t
def bounds(poly):
    return min(p[0] for p in poly),min(p[1] for p in poly),max(p[0] for p in poly),max(p[1] for p in poly)
def in_bounds(p, b): return b[0]<=p[0]<=b[2] and b[1]<=p[1]<=b[3]

def triangulate_polygon(points):
    indices=list(range(len(points))); result=[]
    cross=lambda a,b,c:(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    area=sum(a[0]*b[1]-a[1]*b[0] for a,b in zip(points,points[1:]+points[:1]))
    if area<0: indices.reverse()
    while len(indices)>3:
        for k,b in enumerate(indices):
            a=indices[k-1]; c=indices[(k+1)%len(indices)]
            if cross(points[a],points[b],points[c])<=1e-9: continue
            if any(all(cross(points[u],points[v],points[q])>=-1e-9 for u,v in ((a,b),(b,c),(c,a))) for q in indices if q not in (a,b,c)): continue
            result.append([a,b,c]); indices.pop(k); break
        else: raise ValueError('Cannot triangulate stair profile')
    result.append(indices); return result
def input_check(d):
    required=json.loads((BASE/'layout.schema.json').read_text())['required']
    missing=set(required)-d.keys()
    if missing: raise ValueError('Missing required fields: '+str(sorted(missing)))
    if (d['schema'],d['version'],d['units'])!=('thanh-a-layout',1,'m'): raise ValueError('Unsupported schema/version/units')
    if not isinstance(d['seed'],int) or not math.isfinite(d['scale']) or d['scale']<=0: raise ValueError('Invalid seed/scale')
    for key in ('foundation_cap_m','retaining_cap_m','terminal_extension_m','surface_tolerance_m'):
        value=d.get('geometry',{}).get(key,float('nan'))
        if not isinstance(value,(int,float)) or not math.isfinite(value) or value<=0: raise ValueError('Invalid geometry parameter '+key)
    settings=d.get('dimensions',{})
    for key in ('ShowDimensions','SelectedOnly','ShowElevation','ShowGrid'):
        if not isinstance(settings.get(key),bool): raise ValueError('Invalid dimension setting '+key)
    if not 0<settings.get('MinorGridSizeCm',0)<settings.get('GridSizeCm',0) or not 1<=settings.get('MaxLabels',0)<=256: raise ValueError('Invalid grid spacing/label count')
    for key in ('box','cylinder','ramp','grid_parent'):
        if not d.get('prototyping',{}).get(key,'').startswith('/Game/LevelPrototyping/'): raise ValueError('Invalid prototyping object path '+key)
    lighting=d.get('lighting',{})
    if len(lighting.get('sun_rotation_deg',[]))!=3: raise ValueError('Invalid daylight rotation')
    if any(not math.isfinite(lighting.get(k,float('nan'))) for k in ('sun_intensity_lux','sky_intensity','fill_intensity_lux','capture_exposure_bias','fog_density')): raise ValueError('Invalid lighting parameters')
    if lighting['sun_intensity_lux']<=0 or min(lighting['sky_intensity'],lighting['fill_intensity_lux'])<0: raise ValueError('Invalid lighting intensity')
    h=d['heightmap']; n=h['resolution']; section=h['subsection_quads']; sub=h['num_subsections']
    if section not in (7,15,31,63,127,255) or sub not in (1,2) or (n-1)%(section*sub): raise ValueError('Invalid Landscape section/resolution combination')
    if h['encoded_max_m']<=h['encoded_min_m'] or min(h['extent_m'])<=0: raise ValueError('Invalid heightmap extent/encoding')
    if ((n-1)//(section*sub))**2>1024: raise ValueError('Landscape exceeds 1024 components')
    objects=[]
    for key in ('land','water','architecture_regions','elevation_zones','roads','stairs','retaining_walls','landmarks','no_build','piers','plazas','samples'):
        objects+=d[key]
    objects += [d['city_boundary']] + d['walls']['openings']
    for o in objects:
        if not re.fullmatch('[a-z][a-z0-9_]*',o['id']): raise ValueError('Invalid ID '+o['id'])
        points=o.get('polygon_m',o.get('points_m',[]))
        for p in points:
            if len(p) not in (2,3) or not all(isinstance(v,(float,int)) and math.isfinite(v) for v in p): raise ValueError('Invalid coordinate '+o['id'])
        closed='polygon_m' in o
        if closed and len(points)<3: raise ValueError('Invalid polygon '+o['id'])
        edges=list(zip(points,points[1:]+(points[:1] if closed else [])))
        if any(math.dist(a[:2],b[:2])<.001 for a,b in edges): raise ValueError('Duplicate vertices '+o['id'])
        if closed:
            area=sum(a[0]*b[1]-a[1]*b[0] for a,b in edges)
            if abs(area)<1: raise ValueError('Degenerate polygon '+o['id'])
            def cross(a,b,c): return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
            for i,(a,b) in enumerate(edges):
                for j,(c,e) in enumerate(edges):
                    if j<=i+1 or (i==0 and j==len(edges)-1): continue
                    if cross(a,b,c)*cross(a,b,e)<0 and cross(c,e,a)*cross(c,e,b)<0: raise ValueError('Self-intersecting polygon '+o['id'])
        if 'width_m' in o and o['width_m']<=0: raise ValueError('Invalid width '+o['id'])
        if 'range_m' in o and not o['range_m'][0]<=o['elevation_m']<=o['range_m'][1]: raise ValueError('Elevation out of range '+o['id'])
    if len({o['id'] for o in objects})!=len(objects): raise ValueError('Duplicate input IDs')
def png(path,w,h,rows,depth=16,color=0):
    def chunk(t,data): return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data)&0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,depth,color,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+r for r in rows),6))+chunk(b'IEND',b''))

class Compiler:
    def __init__(self,d):
        self.d=d; self.actors=[]; self.meshes={}; self.ids=set(); self.buildings=[]
        self.land=[(r,bounds(r['polygon_m'])) for r in d['land']]
        self.zones=[(r,bounds(r['polygon_m'])) for r in sorted(d['elevation_zones'], key=lambda r:r['priority'])]
        self.segments=[]
        for r in d['roads']+d['stairs']:
            for i,(a,b) in enumerate(zip(r['points_m'],r['points_m'][1:])):
                margin=r['width_m']/2+.8
                bb=(min(a[0],b[0])-margin,min(a[1],b[1])-margin,max(a[0],b[0])+margin,max(a[1],b[1])+margin)
                self.segments.append((r,a,b,bb))
        self.reservations=[r['polygon_m'] for r in d['no_build']]
        for l in d['landmarks']:
            x,y=l['position_m'][:2]; w,h=l['size_m'][:2]; c=9
            self.reservations.append([[x-w/2-c,y-h/2-c],[x+w/2+c,y-h/2-c],[x+w/2+c,y+h/2+c],[x-w/2-c,y+h/2+c]])
        self.city=d['city_boundary']['polygon_m']
    def height(self,p,roads=True,roadbed=False):
        z=-6
        for r,bb in self.land:
            if in_bounds(p,bb) and inside(p,r['polygon_m']): z=r['base_elevation_m']
        for r,bb in self.zones:
            if in_bounds(p,bb) and inside(p,r['polygon_m']): z=r['elevation_m']
        for plaza in self.d['plazas']:
            if math.dist(p[:2],plaza['center_m'])<=plaza['radius_m']: z=plaza['elevation_m']
        if roads:
            nearest=1e9
            for r,a,b,bb in self.segments:
                if not in_bounds(p,bb): continue
                dist,t=project(p,a,b)
                if dist<=r['width_m']/2+.8 and dist<nearest:
                    nearest=dist
                    if r.get('step_rise_m'):
                        n=math.ceil(abs(b[2]-a[2])/r['step_rise_m']); t=math.ceil(t*n)/n
                    z=a[2]+t*(b[2]-a[2])
            if roadbed and nearest<1e9: z-=self.d.get('roadbed_depth_m',.25)
        return z
    def actor(self,id,folder,mesh,pos,size=(1,1,1),yaw=0,color=None,collision=True,**kw):
        assert id not in self.ids, id
        self.ids.add(id)
        self.actors.append(dict(id=id,folder=folder,mesh=mesh,position_m=pos,size_m=size,yaw_deg=yaw,
                                color=color or COLORS.get(folder,COLORS['Terrain']),collision=collision,**kw))
    def box(self,id,folder,center,size,**kw): self.actor(id,folder,'box',center,size,**kw)
    def segment_box(self,id,folder,a,b,width,height,top=None,**kw):
        mid=[(a[0]+b[0])/2,(a[1]+b[1])/2,0]; mid[2]=(top if top is not None else self.height(mid))+height/2
        self.box(id,folder,mid,[math.dist(a[:2],b[:2]),width,height],yaw=math.degrees(math.atan2(b[1]-a[1],b[0]-a[0])),**kw)
    def mesh(self,id,folder,verts,tris,color=None):
        self.meshes[id]=dict(vertices_m=verts,triangles=tris)
        self.actor(id,folder,id,[0,0,0],color=color)
    def road(self,r):
        points=r['points_m']; width=r['width_m']; vs=[]; ts=[]
        dirs=[]
        for a,b in zip(points,points[1:]):
            length=math.dist(a[:2],b[:2]); dirs.append(((b[0]-a[0])/length,(b[1]-a[1])/length))
        for i,p in enumerate(points):
            incoming=dirs[max(0,i-1)]; outgoing=dirs[min(i,len(dirs)-1)]
            na=(-incoming[1],incoming[0]); nb=(-outgoing[1],outgoing[0]); denominator=1+na[0]*nb[0]+na[1]*nb[1]
            if denominator<.08: raise ValueError(r['id']+': corner needs an explicit switchback landing')
            offset=[(na[j]+nb[j])*width/2/denominator for j in range(2)]
            q=list(p); seam=self.d['geometry']['terminal_extension_m']
            if i==0: q[0]-=outgoing[0]*seam; q[1]-=outgoing[1]*seam
            if i==len(points)-1: q[0]+=incoming[0]*seam; q[1]+=incoming[1]*seam
            vs.extend([[q[0]+offset[0],q[1]+offset[1],q[2]+.04],[q[0]-offset[0],q[1]-offset[1],q[2]+.04],
                       [q[0]+offset[0],q[1]+offset[1],q[2]-.4],[q[0]-offset[0],q[1]-offset[1],q[2]-.4]])
        for i in range(len(points)-1):
            a=i*4; b=a+4
            ts.extend([[a,a+1,b+1],[a,b+1,b],[a+2,b+2,b+3],[a+2,b+3,a+3],
                       [a,b,b+2],[a,b+2,a+2],[a+1,a+3,b+3],[a+1,b+3,b+1]])
        b=len(vs)-4; ts.extend([[0,2,3],[0,3,1],[b,b+1,b+3],[b,b+3,b+2]])
        self.mesh(r['id'],'Roads',vs,ts)
    def reserved(self,x,y,w,h):
        c=self.d['clearance_m']; pts=[[x-w/2-c,y-h/2-c],[x+w/2+c,y-h/2-c],[x+w/2+c,y+h/2+c],[x-w/2-c,y+h/2+c],[x,y]]
        for p in pts:
            if not inside(p,self.city): return True
            if any(inside(p,r['polygon_m']) for r in self.d['water']): return True
            if any(inside(p,r) for r in self.reservations): return True
            if any(math.dist(p,q['center_m'])<q['radius_m']+c for q in self.d['plazas']): return True
            if any(project(p,a,b)[0]<r['width_m']/2+c for r,a,b,bb in self.segments): return True
            if any(project(p,a,b)[0]<self.d['walls']['thickness_m']/2+10+c for a,b in zip(self.city,self.city[1:]+self.city[:1])): return True
        # Catch a corridor or reservation passing through the middle of a footprint.
        bb=(x-w/2-c,y-h/2-c,x+w/2+c,y+h/2+c)
        for r,a,b,_ in self.segments:
            n=max(2,math.ceil(math.dist(a[:2],b[:2])/2))
            for i in range(n+1):
                p=[a[0]+(b[0]-a[0])*i/n,a[1]+(b[1]-a[1])*i/n]
                if in_bounds(p,bb): return True
        for poly in self.reservations:
            if any(in_bounds(p,bb) for p in poly): return True
        return False
    def building(self,id,x,y,z,w,h,height,roof,style,tower=False,landmark=False):
        folder='Landmarks' if landmark else style; col=COLORS[style]
        clearance=self.d['geometry']['foundation_cap_m']
        self.box(id+'_foundation',folder,[x,y,z-.5+clearance/2],[w+.5,h+.5,1+clearance],color=COLORS['Walls'])
        bx=x-w*.125 if style=='Medieval' else x; bw=w*.75 if style=='Medieval' else w
        self.box(id+'_body',folder,[bx,y,z+height/2],[bw,h,height],color=col)
        self.actor(id+'_roof',folder,'roof',[bx,y,z+height],[bw,h,roof],color=COLORS['Roof'])
        if tower:
            for k,dx in enumerate([-w*.39,w*.39]):
                tw=w*.23; th=height*1.35
                self.box(id+f'_tower{k}',folder,[x+dx,y+h*.32,z+th/2],[tw,tw,th],color=col)
                self.actor(id+f'_spire{k}',folder,'pyramid',[x+dx,y+h*.32,z+th],[tw*1.12,tw*1.12,roof*1.6],color=COLORS['Roof'])
        if style=='Medieval':
            self.box(id+'_annex',folder,[x+w*.325,y-h*.1,z+height*.3],[w*.35,h*.45,height*.6],color=col)
            self.actor(id+'_annex_roof',folder,'roof',[x+w*.325,y-h*.1,z+height*.6],[w*.35,h*.45,roof*.7],color=COLORS['Roof'])
        self.buildings.append(dict(id=id,center_m=[x,y,z],size_m=[w,h,height],style=style,landmark=landmark))
    def stair(self,s):
        a,b=s['points_m']; length=math.dist(a[:2],b[:2]); n=math.ceil(abs(b[2]-a[2])/s['step_rise_m']); landing=s['landing_m']/2
        profile=[[-landing,a[2]+.04],[0,a[2]+.04]]
        for i in range(n):
            z=a[2]+(b[2]-a[2])*(i+1)/n+.04
            profile.extend([[length*i/n,z],[length*(i+1)/n,z]])
        profile += [[length+landing,b[2]+.04],[length+landing,b[2]-.4],[length,b[2]-.4],[0,a[2]-.4],[-landing,a[2]-.4]]
        # Remove collinear profile points, including the duplicated endpoint tread.
        cleaned=[]
        for p in profile:
            if not cleaned or math.dist(p,cleaned[-1])>1e-8: cleaned.append(p)
        changed=True
        while changed:
            changed=False
            for i,p in enumerate(cleaned):
                pa=cleaned[i-1]; pb=cleaned[(i+1)%len(cleaned)]
                if abs((p[0]-pa[0])*(pb[1]-p[1])-(p[1]-pa[1])*(pb[0]-p[0]))<1e-9:
                    cleaned.pop(i); changed=True; break
        profile=cleaned
        area=sum(p[0]*q[1]-p[1]*q[0] for p,q in zip(profile,profile[1:]+profile[:1]))
        if area<0: profile.reverse()
        side=triangulate_polygon(profile); count=len(profile); dx=(b[0]-a[0])/length; dy=(b[1]-a[1])/length
        vertices=[[a[0]+dx*x-dy*y,a[1]+dy*x+dx*y,z] for y in (-s['width_m']/2,s['width_m']/2) for x,z in profile]
        triangles=side+[[c+count,b+count,a+count] for a,b,c in side]
        for i in range(count):
            j=(i+1)%count; triangles.extend([[i,i+count,j+count],[i,j+count,j]])
        self.mesh(s['id'],'Roads',vertices,triangles)
    def compile(self):
        for r in self.d['roads']: self.road(r)
        for s in self.d['stairs']:
            self.stair(s)
        for p in self.d['plazas']:
            self.actor(p['id'],'Roads','cylinder',p['center_m']+[p['elevation_m']-.2+.04],[p['radius_m']*2,p['radius_m']*2,.4])
        for l in self.d['landmarks']:
            x,y,z=l['position_m']; w,h,height=l['size_m']; self.building(l['id'],x,y,z,w,h,height,l['roof_m'],l['style'],l['towers'],True)
        for r in self.d['architecture_regions']:
            style=r['architecture_style']; rule=self.d['building_styles'][style]; space=rule['spacing_m']; bb=bounds(r['polygon_m'])
            for ix in range(math.floor(bb[0]/space),math.ceil(bb[2]/space)):
                for iy in range(math.floor(bb[1]/space),math.ceil(bb[3]/space)):
                    id=f"{r['id']}_{ix}_{iy}"; rng=random.Random(f"{self.d['seed']}:{id}")
                    if rng.random()>rule['density']: continue
                    x=(ix+.5)*space+rng.uniform(-2,2); y=(iy+.5)*space+rng.uniform(-2,2)
                    w=rng.uniform(*rule['size_x_m']); h=rng.uniform(*rule['size_y_m'])
                    corners=[[x-w/2,y-h/2],[x+w/2,y-h/2],[x+w/2,y+h/2],[x-w/2,y+h/2]]
                    if not all(inside(p,r['polygon_m']) for p in corners) or self.reserved(x,y,w,h): continue
                    zs=[self.height(p) for p in corners]
                    if max(zs)-min(zs)>.25: continue
                    height=rng.uniform(*rule['height_m']); roof=rng.uniform(*rule['roof_m']); every=rule['tower_every']
                    self.building(id,x,y,max(zs),w,h,height,roof,style,every>0 and (ix+iy)%every==0)
        wall=self.d['walls']; pts=wall['points_m']
        for k,(a,b) in enumerate(zip(pts,pts[1:]+pts[:1])):
            length=math.dist(a,b); n=math.ceil(length/6)
            for i in range(n):
                pa=[a[j]+(b[j]-a[j])*i/n for j in range(2)]; pb=[a[j]+(b[j]-a[j])*(i+1)/n for j in range(2)]; mid=[(pa[j]+pb[j])/2 for j in range(2)]
                if any(project(o['center_m'],pa,pb)[0]<o['width_m']/2 for o in wall['openings']): continue
                self.segment_box(f'wall_{k:02}_{i:03}','Walls',pa,pb,wall['thickness_m'],wall['height_m'])
            if not any(math.dist(a,o['center_m'])<o['width_m']/2+wall['tower_radius_m'] for o in wall['openings']):
                z=self.height(a); rad=wall['tower_radius_m']; th=wall['tower_height_m']
                self.actor(f'wall_tower_{k:02}','Walls','cylinder',a+[z+th/2],[rad*2,rad*2,th])
                self.actor(f'wall_tower_{k:02}_cap','Walls','pyramid',a+[z+th],[rad*1.7,rad*1.7,5],color=COLORS['Roof'])
        for o in wall['openings']:
            nearest=min(zip(pts,pts[1:]+pts[:1]),key=lambda ab:project(o['center_m'],*ab)[0]); a,b=nearest
            angle=math.atan2(b[1]-a[1],b[0]-a[0]); x,y=o['center_m']; w=o['width_m']; ground=o['ground_m']; ch=o['clear_height_m']
            for j,side in enumerate([-1,1]):
                off=side*(w/2+2.0)
                self.box(o['id']+f'_jamb{j}','Walls',[x+math.cos(angle)*off,y+math.sin(angle)*off,ground+6],[4,7,12],yaw=math.degrees(angle))
            self.box(o['id']+'_lintel','Walls',[x,y,ground+ch+1],[w+8,7,2],yaw=math.degrees(angle))
        for r in self.d['retaining_walls']:
            for k,(a,b) in enumerate(zip(r['points_m'],r['points_m'][1:])):
                l=math.dist(a,b); n=math.ceil(l/4); dx=(b[0]-a[0])/l; dy=(b[1]-a[1])/l
                for i in range(n):
                    mid=[a[0]+dx*l*(i+.5)/n,a[1]+dy*l*(i+.5)/n]
                    if any(project(mid,pa,pb)[0]<road['width_m']/2+2 for road,pa,pb,_ in self.segments): continue
                    za=self.height([mid[0]-dy*2,mid[1]+dx*2],False); zb=self.height([mid[0]+dy*2,mid[1]-dx*2],False)
                    height=abs(za-zb)
                    if height<1: continue
                    cap=self.d['geometry']['retaining_cap_m']
                    self.box(r['id']+f'_{k}_{i}','Terrain',[*mid,min(za,zb)+height/2+cap/2],[l/n,r['thickness_m'],height+cap],yaw=math.degrees(math.atan2(dy,dx)),color=COLORS['Walls'])
        for p in self.d['piers']:
            self.segment_box(p['id'],'Roads',p['a_m'],p['b_m'],p['width_m'],.8,top=p['elevation_m']-.8+.04)
        extent=self.d['heightmap']['extent_m']; origin=self.d['heightmap']['min_xy_m']
        self.box('water_datum','Terrain',[origin[0]+extent[0]/2,origin[1]+extent[1]/2,self.d['water_datum_m']-.05],extent+[.1],color=COLORS['Water'],collision=False)
        return self
    def validate(self):
        errors=[]; slopes=[]
        if self.d['schema']!='thanh-a-layout' or self.d['version']!=1 or self.d['units']!='m': errors.append('Unsupported schema/version/units')
        expected={'red':'Gothic','blue':'Medieval','green':'Hybrid'}
        ids=[]
        for key in ['land','water','architecture_regions','elevation_zones','plazas','roads','retaining_walls','stairs','landmarks','no_build','piers','samples']:
            for r in self.d[key]:
                ids.append(r['id'])
                if 'polygon_m' in r:
                    if len(r['polygon_m'])<3: errors.append(r['id']+': short polygon')
                if 'architecture_style' in r and r['architecture_style']!=expected[r['reference_color']]: errors.append('Architecture mapping '+r['id'])
        ids += [o['id'] for o in self.d['walls']['openings']]
        if len(ids)!=len(set(ids)): errors.append('Duplicate design IDs')
        pawn=self.d['pawn']; limit=pawn['design_ramp_angle_deg']
        for r in self.d['roads']:
            for a,b in zip(r['points_m'],r['points_m'][1:]):
                run=math.dist(a[:2],b[:2]); rise=abs(b[2]-a[2]); angle=math.degrees(math.atan2(rise,run)); required=rise/math.tan(math.radians(limit))
                slopes.append(dict(id=r['id'],angle_deg=round(angle,3),run_m=round(run,3),required_run_m=round(required,3)))
                if angle>limit+.01: errors.append(f"Ramp {r['id']} {angle:.2f} exceeds {limit}")
                if r['width_m']<2*pawn['radius_m']+1: errors.append('Narrow road '+r['id'])
        for s in self.d['stairs']:
            if s['step_rise_m']>pawn['max_step_m']: errors.append('Step too high '+s['id'])
        # Required graph: exact shared profile nodes; adjacent plaza connects its entries.
        graph={}
        def node(p): return tuple(round(v,2) for v in p)
        for r in self.d['roads']:
            for a,b in zip(r['points_m'],r['points_m'][1:]):
                graph.setdefault(node(a),set()).add(node(b)); graph.setdefault(node(b),set()).add(node(a))
        start=node(self.d['roads'][0]['points_m'][1]); seen={start}; stack=[start]
        while stack:
            p=stack.pop()
            for q in graph.get(p,[]):
                if q not in seen: seen.add(q); stack.append(q)
        for name in self.d['routes']['required']+self.d['routes']['branches']:
            r=next(r for r in self.d['roads'] if r['id']==name)
            if not any(node(p) in seen for p in r['points_m']): errors.append('Disconnected route '+name)
        samples=[]
        for p in self.d['samples']:
            actual=self.height(p['position_m']); samples.append(dict(id=p['id'],expected_m=p['ground_m'],compiled_m=actual))
            if abs(actual-p['ground_m'])>.1: errors.append('Wrong design ground '+p['id'])
        # Collision checks happen in Unreal; this is explicit design validation only.
        result=dict(status='pass' if not errors else 'fail',execution='Python design validation; no Unreal collision queried',
                    errors=errors,road_slopes=slopes,max_angle_deg=max(s['angle_deg'] for s in slopes),
                    connected_nodes=len(seen),samples=samples,building_count=len(self.buildings),actor_count=len(self.actors))
        return result
    def heightmap(self,out):
        h=self.d['heightmap']; n=h['resolution']; low=h['encoded_min_m']; high=h['encoded_max_m']; extent=h['extent_m']; origin=h['min_xy_m']; raw=bytearray(); rows=[]
        for iy in range(n):
            y=origin[1]+iy*extent[1]/(n-1); row=[]
            for ix in range(n):
                x=origin[0]+ix*extent[0]/(n-1); z=self.height([x,y],roadbed=True)
                v=round((z-low)/(high-low)*65536)
                if not 0<=v<=65535: raise ValueError('Height outside encoding range')
                row.append(v)
            raw.extend(struct.pack('<'+'H'*n,*row)); rows.append(struct.pack('>'+'H'*n,*row))
        (out/'terrain.r16').write_bytes(raw); png(out/'terrain.png',n,n,rows)
        # Encoded 32768 is the midpoint; each unit is ZScale/128 centimetres.
        meta=dict(schema='thanh-a-heightmap',version=1,resolution=[n,n],horizontal_extent_m=extent,
                  encoded_elevation_range_m=[low,high],xy_scale_cm=[v*100/(n-1) for v in extent],
                  z_scale=(high-low)*100/512,actor_offset_cm=[origin[0]*100,origin[1]*100,(low+high)*50],
                  orientation='row 0 = South (-Y), columns go West to East (+X); no rotation; do not flip Y',
                  raw_endianness='little',num_subsections=h['num_subsections'],subsection_quads=h['subsection_quads'],
                  components_per_axis=(n-1)//(h['num_subsections']*h['subsection_quads']),
                  equation='world_z_cm = actor_z_cm + (uint16 - 32768) * z_scale / 128',
                  source='designed elevation polygons and explicit route profiles; no image grayscale, no noise',
                  roadbed_depth_m=self.d.get('roadbed_depth_m',.25),samples=self.d['samples'])
        (out/'heightmap_metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
    def preview(self,out):
        parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="1250" viewBox="-520 -540 1100 1100">',
               '<rect x="-520" y="-540" width="1100" height="1100" fill="#24343a"/>',
               '<g transform="scale(1,-1)">']
        def poly(pts,col,opacity=1,stroke='#bec5b4'):
            parts.append(f'<polygon points="'+ ' '.join(f'{p[0]},{p[1]}' for p in pts)+f'" fill="{col}" fill-opacity="{opacity}" stroke="{stroke}" stroke-width="1"/>')
        def label(x,y,s,size=8):
            parts.append(f'<g transform="translate({x},{y}) scale(1,-1)"><text text-anchor="middle" font-size="{size}" fill="#fff" paint-order="stroke" stroke="#293239" stroke-width="2">{html.escape(s)}</text></g>')
        for r in self.d['land']: poly(r['polygon_m'],'#75836b')
        for r in self.d['architecture_regions']: poly(r['polygon_m'],COLORS[r['architecture_style']],.68)
        for r in self.d['elevation_zones']:
            poly(r['polygon_m'],'none',1, '#e4d886')
            bb=bounds(r['polygon_m']); label((bb[0]+bb[2])/2,(bb[1]+bb[3])/2,f"{r['id']} +{r['elevation_m']}m")
        for r in self.d['no_build']: poly(r['polygon_m'],'#866947',.8)
        for r in self.d['roads']+self.d['stairs']:
            parts.append('<polyline points="'+' '.join(f'{p[0]},{p[1]}' for p in r['points_m'])+f'" fill="none" stroke="#eed7a5" stroke-width="{r["width_m"]}" stroke-linejoin="round"/>')
            a=r['points_m'][len(r['points_m'])//2]; label(*a[:2],r['id'],7)
        for p in self.d['plazas']:
            parts.append(f'<circle cx="{p["center_m"][0]}" cy="{p["center_m"][1]}" r="{p["radius_m"]}" fill="#ddc9a0"/>'); label(*p['center_m'],p['id']+f' +{p["elevation_m"]}m')
        for b in self.buildings:
            x,y,z=b['center_m']; w,h=b['size_m'][:2]
            poly([[x-w/2,y-h/2],[x+w/2,y-h/2],[x+w/2,y+h/2],[x-w/2,y+h/2]],COLORS[b['style']],1)
            if b['landmark']: label(x,y,b['id'],10)
        pts=self.d['walls']['points_m']; parts.append('<polyline points="'+' '.join(f'{p[0]},{p[1]}' for p in pts+[pts[0]])+'" fill="none" stroke="#c1c5ca" stroke-width="4"/>')
        for o in self.d['walls']['openings']:
            x,y=o['center_m']; parts.append(f'<circle cx="{x}" cy="{y}" r="{o["width_m"]/2}" fill="#edc56d"/>'); label(x,y+14,o['id'])
        parts += ['</g>','<text x="-490" y="-505" fill="#fff" font-size="20">THANH A — interpreted layout v1 (estimated)</text>',
            '<text x="-490" y="-480" fill="#b45150" font-size="14">RED: GOTHIC</text><text x="-280" y="-480" fill="#589bc9" font-size="14">BLUE: MEDIEVAL (land)</text><text x="0" y="-480" fill="#65a278" font-size="14">GREEN: HYBRID</text>',
            '<text x="-490" y="-460" fill="#fff" font-size="11">+X East | +Y North | origin Plaza | elevation polygons independent of architecture</text>',
            '<text x="-490" y="505" fill="#fff" font-size="12">Water datum 0m assumed. Ruler: 200m / 240px. Main map only; perspective inset excluded.</text>',
            '<path d="M -450 470 H -250" stroke="white" stroke-width="3"/><text x="-450" y="490" fill="white" font-size="10">200 m</text>', '</svg>']
        (out/'layout_preview.svg').write_text('\n'.join(parts),encoding='utf-8')
    def manifest(self,out,fingerprint,source_layout):
        byid={r['id']:r.get('design_points_m',r['points_m']) for r in self.d['roads']}
        walk=byid['main_axis'][1:]+byid['council_approach'][1:]+list(reversed(byid['council_approach']))[1:]
        walk+=byid['upper_east'][1:]+byid['port_descent'][1:]+byid['port_access'][1:]
        walk+=list(reversed(byid['port_access']))[1:]+list(reversed(byid['port_descent']))[1:]+list(reversed(byid['upper_east']))[1:]
        walk+=list(reversed(byid['main_axis'][5:]))[1:]
        walk+=byid['residential'][1:]+list(reversed(byid['residential']))[1:]+byid['craft'][1:]+list(reversed(byid['craft']))[1:]
        scene=dict(schema='thanh-a-scene',version=1,units='m',fingerprint=fingerprint,level='/Game/Maps/Blockout/L_ThanhA_Blockout',
                   ownership_tag='ThanhA.Blockout.v1',heightmap='terrain.r16',heightmap_metadata='heightmap_metadata.json',lighting=self.d['lighting'],geometry=self.d['geometry'],prototyping=self.d['prototyping'],dimensions=self.d['dimensions'],buildings=self.buildings,
                   meshes=self.meshes,actors=self.actors,samples=[dict(p,landscape_ground_m=self.height(p['position_m'],roadbed=True)) for p in self.d['samples']],routes=self.d['roads'],stairs=self.d['stairs'],pawn=self.d['pawn'],capture_views=self.d['capture_views'],playtest_waypoints_m=walk,player_start_m=byid['main_axis'][1])
        scene['layout_md5']=hashlib.md5(source_layout.read_bytes()).hexdigest()
        scene['compiler_md5']=hashlib.md5(Path(__file__).read_bytes()).hexdigest()
        scene['heightmap_md5']=hashlib.md5((out/'terrain.r16').read_bytes()).hexdigest()
        (out/'scene.json').write_text(json.dumps(scene,separators=(',',':'))+'\n')

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--layout',type=Path,default=BASE/'layout.json'); parser.add_argument('--validate-only',action='store_true'); args=parser.parse_args()
    d=json.loads(args.layout.read_text(encoding='utf-8-sig'))
    input_check(d)
    # Uniform plan scale is a design control; vertical elevation is separately editable.
    d=json.loads(json.dumps(d)); scale=d['scale']
    if scale!=1:
        def transform(o,key=''):
            if isinstance(o,dict):
                for k,v in o.items(): o[k]=transform(v,k)
            elif isinstance(o,list):
                if key in ('polygon_m','points_m'): return [[p[0]*scale,p[1]*scale]+p[2:] for p in o]
                if key in ('center_m','a_m','b_m','position_m','min_xy_m','extent_m'): return [o[0]*scale,o[1]*scale]+o[2:]
                return [transform(v) for v in o]
            elif isinstance(o,(float,int)) and key in ('width_m','radius_m','spacing_m','landing_m'): return o*scale
            return o
        transform(d)
    for r in d['roads']:
        original=r['points_m']; r['design_points_m']=original; expanded=[original[0]]; landing=r.get('landing_m',r['width_m']/2+.8)
        for a,b in zip(original,original[1:]):
            length=math.dist(a[:2],b[:2]); rise=abs(b[2]-a[2])
            if rise>0:
                required=rise/math.tan(math.radians(d['pawn']['design_ramp_angle_deg']))
                if length-2*landing<required: raise ValueError(f"{r['id']} requires a longer ramp or switchback after {landing}m landings; run={length}, required={required+2*landing}")
                expanded.extend([[a[0]+(b[0]-a[0])*landing/length,a[1]+(b[1]-a[1])*landing/length,a[2]],
                                 [b[0]-(b[0]-a[0])*landing/length,b[1]-(b[1]-a[1])*landing/length,b[2]]])
            expanded.append(b)
        r['points_m']=expanded
    c=Compiler(d).compile(); result=c.validate(); out=BASE/'Generated'; out.mkdir(exist_ok=True)
    (out/'design_validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('status','errors','building_count','actor_count','max_angle_deg')}))
    if result['errors']: raise SystemExit(1)
    if args.validate_only: return
    fingerprint=hashlib.sha256(args.layout.read_bytes()+Path(__file__).read_bytes()).hexdigest()[:16]
    c.heightmap(out); c.preview(out); c.manifest(out,fingerprint,args.layout)
    (out/'layout_compiled.json').write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
    (out/'buildings.json').write_text(json.dumps(c.buildings,indent=2)+'\n')
    print('Prepared '+fingerprint)
if __name__=='__main__': main()
