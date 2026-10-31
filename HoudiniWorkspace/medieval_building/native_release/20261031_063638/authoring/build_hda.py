"""Storybook Blockout + Parameters 4.2 -- stdlib geometry core + portable Houdini Python SOP.
Y up, metres, front -Z. No external assets or pip packages required in Houdini.
Importing this file does not mutate a scene. See README_VI.md for measured limits.
"""
import argparse
import io
import hashlib
import json
import math
import random
from pathlib import Path

VERSION = '5.1.0-interior'
OWNER = 'quynh.storybook.blockout.4'

def spec(kind, default, lo=None, hi=None, enum=None, effect=''):
    s = {'type': kind, 'default': default, 'effect': effect}
    if lo is not None: s.update(min=lo, max=hi)
    if enum: s['enum'] = enum
    return s

SCHEMA = {'schema_version': 1, 'parameters': {
    'footprint': spec('string','rectangular',enum=['rectangular','round','l_shape','hybrid'],effect='Body and roof union'),
    'width_m': spec('number',5.4,4.5,8.0,effect='Main body X size; round diameter uses this'),
    'depth_m': spec('number',5.8,4.5,8.0,effect='Rectangular body Z size'),
    'floor_count': spec('integer',1,1,3,effect='Stacked wall storeys'),
    'floor_height_m': spec('number',2.8,2.6,3.4,effect='Storey spacing'),
    'wall_thickness_m': spec('number',0.22,0.16,0.35,effect='Actual wall reveals'),
    'upper_overhang_m': spec('number',0.0,0.0,0.7,effect='Jetty on rectangular upper storeys'),
    'tower_radius_m': spec('number',1.7,1.4,2.3,effect='Hybrid turret radius'),
    'tower_extra_height_m': spec('number',0.8,0.0,1.5,effect='Hybrid turret wall height'),
    'tower_taper': spec('number',0.0,0.0,0.24,effect='Round wall radius reduction at top'),
    'roof_shape': spec('string','gable',enum=['gable','hipped','conical'],effect='Implemented roof envelope'),
    'roof_height_m': spec('number',2.0,1.2,5.0,effect='Roof pitch / conical skirt height'),
    'eave_curve': spec('number',0.25,0.0,0.5,effect='Upturn of roof profile near eaves'),
    'eave_depth_m': spec('number',0.45,0.25,0.7,effect='Roof projection beyond wall'),
    'tile_size_m': spec('number',0.42,0.35,0.65,effect='Tiled roof density'),
    'timber_pattern': spec('string','simple',enum=['simple','cross_brace','curved_brace'],effect='Facade bracing geometry'),
    'balcony_layout': spec('string','none',enum=['none','front','wrap','ring'],effect='Deck, rail and supports'),
    'balcony_depth_m': spec('number',1.0,0.75,1.5,effect='Deck extension'),
    'foundation_mode': spec('string','stone',enum=['stone','stilts','hybrid'],effect='Stone terrace / braced posts'),
    'foundation_height_m': spec('number',0.35,0.2,3.2,effect='Entrance platform elevation'),
    'terrain_slope': spec('number',0.0,0.0,0.55,effect='Ground plane slope X for stilt footings'),
    'utility_type': spec('string','none',enum=['none','windmill','waterwheel'],effect='Rotor or wheel geometry'),
    'wheel_radius_m': spec('number',2.3,1.6,2.8,effect='Waterwheel radius'),
    'rotor_radius_m': spec('number',3.6,2.5,4.2,effect='Four sail rotor span'),
    'awning_enable': spec('boolean',False,effect='Canvas canopy and posts'),
    'entrance_canopy': spec('boolean',True,effect='Small tiled porch over the entrance'),
    'workshop_enable': spec('boolean',False,effect='Open lean-to with working bench'),
    'dormer_count': spec('integer',0,0,2,effect='Roof-mounted dormers and cut roof apertures'),
    'bay_window_enable': spec('boolean',False,effect='Supported projecting window'),
    'chimney_count': spec('integer',1,0,3,effect='Masonry stacks with pots'),
    'cupola_enable': spec('boolean',False,effect='Conical roof lantern with finial'),
    'palette': spec('string','warm_ivory',enum=['warm_ivory','ochre_teal','cool_slate'],effect='Per-family vertex colours'),
    'scale': spec('number',1.0,0.5,2.0,effect='Uniform global scale after construction'),
}, 'seed_controls': {'layout_seed': 'seeds.layout', 'decoration_seed': 'seeds.decoration'},
    'constraints': ['round requires conical; other footprints require gable/hipped',
    'l_shape requires gable and zero jetty; hybrid requires zero jetty',
    'balcony requires floor_count >= 2 except windmill service deck; ring requires round',
    'dormers require rectangular/L gable roof; wing dormer requires L footprint',
    'windmill requires round and no dormers; waterwheel supports rectangular/round',
    'cupola requires round; terrain_slope requires stilts/hybrid',
    'round: upper_overhang_m must be zero; depth_m is inactive']}

SCHEMA['parameters']['palette']=spec('string','storybook_red',enum=['storybook_red'],effect='Single approved style: terracotta, cream, dark timber, teal')
SCHEMA['parameters'].update({
 'wing_dormer_count':spec('integer',0,0,1,effect='One dormer on the L-plan secondary wing'),
 'door_width_m':spec('number',1.25,1.0,2.0,effect='Entrance width, limited by structural bay'),
 'detail_level':spec('string','high',enum=['low','high'],effect='High includes analytic edge chamfers, joinery and planters'),
 'bevel_width_m':spec('number',.025,.005,.06,effect='Bounded box chamfer, at most 12 percent of shortest dimension'),
 'dressing_enable':spec('boolean',True,effect='Lantern, barrel, flowerboxes and workshop bench'),
 'context_mode':spec('string','paving',enum=['paving','water_channel','cliff'],effect='Self-contained ground, mill race or support-compatible sloped rock'),
})
SCHEMA['constraints'] += ['windmill permits a one-floor service ring', 'water_channel requires waterwheel', 'cliff requires stilts or hybrid foundation']

# Architectural layout controls. D=door, W=window, .=solid wall. / separates floors.
SCHEMA['parameters'].update({
 'front_bays':spec('string','WD./W.W/W..',effect='Explicit front bays, bottom to top; D W . and /'),
 'right_bays':spec('string','.../.W./...',effect='Right facade; W . and /'),
 'rear_bays':spec('string','.W./.../.W.',effect='Rear facade; W . and /'),
 'left_bays':spec('string','.../W../...',effect='Left facade; W . and /'),
 'attic_window_enable':spec('boolean',False,effect='Optional single front gable opening; rear gable stays solid'),
 'shutters_enable':spec('boolean',False,effect='Optional shutters on facade windows'),
 'window_width_m':spec('number',.85,.55,1.2,effect='Window width capped by structural bay clearance'),
 'round_window_step':spec('integer',5,4,8,effect='Spacing of round tower windows; reserve front service bay'),
 'wing_width_ratio':spec('number',.70,.45,.85,effect='L wing width relative to main width'),
 'wing_depth_ratio':spec('number',.62,.50,.85,effect='L wing depth relative to main depth'),
 'wing_floor_count':spec('integer',1,1,3,effect='Independent L wing storeys, no taller than main'),
 'wing_roof_ratio':spec('number',.72,.55,1.0,effect='Secondary roof height relative to primary'),
 'workshop_width_m':spec('number',2.7,2.0,3.8,effect='Open work bay width, independent of house scale'),
 'workshop_depth_m':spec('number',3.2,2.5,5.0,effect='Open work bay length'),
})

SCHEMA['parameters'].update({
 'layout_input_enable':spec('boolean',False,effect='Read named box components from input 0; dimensions override body controls'),
 'window_layout_mode':spec('string','pattern',enum=['pattern','count'],effect='Explicit facade rows or requested windows per facade/storey'),
 'front_window_count':spec('integer',2,0,3,effect='Count mode: windows per front storey; doorway reserves one bay'),
 'right_window_count':spec('integer',1,0,3,effect='Count mode: windows per side storey'),
 'rear_window_count':spec('integer',1,0,3,effect='Count mode: rear windows per storey'),
 'left_window_count':spec('integer',1,0,3,effect='Count mode: left windows per storey'),
 'wing_offset_z_m':spec('number',-1.3,-4,4,effect='Secondary wing longitudinal placement'),
 'wing_overlap_m':spec('number',.65,.25,1.2,effect='Wing insertion into main body'),
 'workshop_offset_z_m':spec('number',.6,-2,2,effect='Open workshop longitudinal placement'),
 'tower_offset_z_m':spec('number',.8,-2,2,effect='Turret longitudinal placement'),
 'tower_inset_ratio':spec('number',.42,.1,.8,effect='Turret X center = half main width + radius times ratio'),
})

# Optional facade extension; old v4.1 configs migrate with legacy appearance.
FACADE_SCHEMA={
 'facade_style':spec('string','legacy',enum=['legacy','A','B','C'],effect='A timber rhythm; B functional bays; C hero accents, cumulative'),
 'facade_bay_width_m':spec('number',1.15,.7,1.8,effect='Maximum solid-panel stud spacing; opening grid is preserved'),
 'facade_accent_budget':spec('integer',2,0,6,effect='Maximum auto hero accents per rectangular facade across all floors'),
 'facade_module_density':spec('number',.65,0.,1.,effect='Fraction of eligible solid bays receiving functional modules'),
 'facade_overrides':spec('string','{}',effect='JSON mapping stable slot IDs to plain/studs/brace/infill/niche/service/sign/banner/crest/oriel/trellis. Example main.front.0.0'),
}
SCHEMA['parameters'].update(FACADE_SCHEMA)
INTERIOR_SCHEMA={
 'interior_enable':spec('boolean',True,effect='Floor openings, stairs and structural interior'),
 'interior_furniture':spec('boolean',True,effect='Beds, tables, storage and functional props'),
 'interior_partitions':spec('boolean',True,effect='Upper room partition with 0.9m doorway'),
 'interior_joist_spacing_m':spec('number',.65,.45,1.0,effect='Floor beam spacing'),
 'interior_stair_width_m':spec('number',.85,.75,.9,effect='Clear width of U stair flights'),
 'interior_style':spec('string','auto',enum=['auto','home','inn','workshop','mill'],effect='Functional furnishing kit')}
SCHEMA['parameters'].update(INTERIOR_SCHEMA)
MODULES={'plain','studs','brace','infill','niche','service','sign','banner','crest','oriel','trellis'}

EMBEDDED_BASELINE={'parameters': {'footprint': 'rectangular', 'width_m': 4.8, 'depth_m': 5.0, 'floor_count': 3, 'floor_height_m': 2.6, 'wall_thickness_m': 0.22, 'upper_overhang_m': 0.27, 'tower_radius_m': 1.7, 'tower_extra_height_m': 0.8, 'tower_taper': 0.0, 'roof_shape': 'gable', 'roof_height_m': 2.4, 'eave_curve': 0.22, 'eave_depth_m': 0.45, 'tile_size_m': 0.4, 'timber_pattern': 'curved_brace', 'balcony_layout': 'none', 'balcony_depth_m': 1.0, 'foundation_mode': 'stone', 'foundation_height_m': 0.35, 'terrain_slope': 0.0, 'utility_type': 'none', 'wheel_radius_m': 2.3, 'rotor_radius_m': 3.6, 'awning_enable': True, 'entrance_canopy': False, 'workshop_enable': False, 'dormer_count': 1, 'bay_window_enable': False, 'chimney_count': 2, 'cupola_enable': False, 'palette': 'storybook_red', 'scale': 1.0, 'door_width_m': 1.25, 'detail_level': 'high', 'bevel_width_m': 0.025, 'dressing_enable': True, 'context_mode': 'paving'}, 'seeds': {'layout': 42, 'decoration': 101}}
EMBEDDED_BASELINE['parameters']['wing_dormer_count']=0
for _k,_v in SCHEMA['parameters'].items():EMBEDDED_BASELINE['parameters'].setdefault(_k,_v['default'])
for _key,_value in EMBEDDED_BASELINE['parameters'].items():SCHEMA['parameters'][_key]['default']=_value

def normalize(config=None):
    if config is None:
        return {'parameters': {k:s['default'] for k,s in SCHEMA['parameters'].items()},
                'seeds': {'layout':42,'decoration':101}}
    if not isinstance(config,dict) or 'parameters' not in config or 'seeds' not in config:
        raise ValueError('Config needs full parameters and seeds mappings; use normalize() for defaults')
    p, seeds = dict(config['parameters']), dict(config['seeds'])
    for k,v in dict(FACADE_SCHEMA,**INTERIOR_SCHEMA).items():p.setdefault(k,v['default'])
    try: overrides=json.loads(p['facade_overrides'])
    except (ValueError,TypeError):raise ValueError('facade_overrides must be a JSON object')
    if not isinstance(overrides,dict) or any(not isinstance(k,str) or v not in MODULES for k,v in overrides.items()):raise ValueError('Invalid facade override module or ID')
    for key in overrides:
        bits=key.split('.')
        if len(bits)!=4 or bits[0] not in ('main','wing','tower') or bits[1] not in ('front','right','rear','left','ring') or not bits[2].isdigit() or not bits[3].isdigit():raise ValueError('Invalid facade slot: '+key)
    if set(p) != set(SCHEMA['parameters']):
        raise ValueError('Unknown/missing parameters: '+str(set(p)^set(SCHEMA['parameters'])))
    for k,s in SCHEMA['parameters'].items():
        v=p[k]; t=s['type']
        ok = ((t=='integer' and type(v) is int) or (t=='number' and type(v) in (float,int) and math.isfinite(v)) or
              (t=='string' and isinstance(v,str)) or (t=='boolean' and type(v) is bool))
        if not ok or ('enum' in s and v not in s['enum']) or ('min' in s and not s['min']<=v<=s['max']):
            raise ValueError('Invalid '+k+': '+repr(v))
    if set(seeds)!={'layout','decoration'} or any(type(v) is not int or not 0<=v<=2147483647 for v in seeds.values()):
        raise ValueError('seeds.layout and seeds.decoration must be integers 0..2147483647')
    for side,key in enumerate(['front_bays','right_bays','rear_bays','left_bays']):
        rows=p[key].split('/')
        if len(rows)!=3 or any(not 2<=len(row)<=4 or set(row)-set('WD.') for row in rows):
            raise ValueError(key+' needs three rows of 2-4 D/W/. bays separated by /')
        for floor,row in enumerate(rows):
            expected=1 if side==0 and floor==0 else 0
            if row.count('D')!=expected:raise ValueError('Exactly one ground front door; no other D: '+key)
            length=p['width_m'] if side%2==0 else p['depth_m']
            if length/len(row)<1.45:raise ValueError(key+' has too many structural bays for its wall length')
    if p['footprint']=='l_shape' and p['wing_floor_count']>p['floor_count']:
        raise ValueError('Secondary wing must not be taller than primary body')
    f=p['footprint']; r=p['roof_shape']; b=p['balcony_layout']
    checks=[(f=='round' and r!='conical','round requires conical roof'),
      (f!='round' and r=='conical','conical requires round footprint'),
      (f=='l_shape' and (r!='gable' or p['upper_overhang_m']!=0),'l_shape requires gable and zero upper_overhang_m'),
      (f in ('round','hybrid') and p['upper_overhang_m']!=0,'round/hybrid jetty is not supported'),
      (b!='none' and p['floor_count']<2 and p['utility_type']!='windmill','balcony needs at least 2 floors'),
      (b=='ring' and f!='round','ring balcony requires round'),
      (f=='round' and b not in ('none','ring'),'round balcony must be ring or none'),
      (p['dormer_count'] and (f not in ('rectangular','l_shape') or r!='gable'),'dormers require rectangular/L gable roof'),
      (p['wing_dormer_count']!=0,'v3 secondary wing dormers are disabled; use primary dormer_count'),
      (p['utility_type']=='windmill' and f!='round','windmill requires round'),
      (p['utility_type']=='waterwheel' and f not in ('rectangular','round'),'waterwheel requires rectangular or round'),
      (p['cupola_enable'] and f!='round','cupola requires round'),
      (p['terrain_slope'] and p['foundation_mode']=='stone','sloping ground requires stilts or hybrid'),
      (p['bay_window_enable'] and (p['floor_count']<2 or f=='round'),'bay window requires rectangular upper floor'),
      (p['upper_overhang_m'] and p['floor_count']==1,'jetty requires multiple floors')]
    checks += [(p['context_mode']=='water_channel' and p['utility_type']!='waterwheel','water_channel requires waterwheel'), (p['context_mode']=='cliff' and p['foundation_mode'] not in ('stilts','hybrid'),'cliff requires stilts/hybrid foundation')]
    for fail,msg in checks:
        if fail: raise ValueError(msg)
    return {'parameters':p,'seeds':seeds}

def add(a,b): return tuple(x+y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def mul(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a): return mul(a,1/max(math.sqrt(dot(a,a)),1e-12))
def lerp(a,b,t): return add(a,mul(sub(b,a),t))

def clip(poly,plane,inside=True):
    """Clip a convex polygon to n.P <= d (or complementary half-space)."""
    if not poly: return []
    n,d=plane; out=[]
    for a,b in zip(poly,poly[1:]+poly[:1]):
        da=dot(n,a)-d; db=dot(n,b)-d
        ia=(da<=1e-8) if inside else (da>=-1e-8)
        ib=(db<=1e-8) if inside else (db>=-1e-8)
        if ia: out.append(a)
        if ia!=ib and abs(da-db)>1e-12: out.append(lerp(a,b,da/(da-db)))
    clean=[]
    for a in out:
        if not clean or dot(sub(a,clean[-1]),sub(a,clean[-1]))>1e-16: clean.append(a)
    if len(clean)>2 and dot(sub(clean[0],clean[-1]),sub(clean[0],clean[-1]))<1e-16: clean.pop()
    return clean if len(clean)>=3 else []

def subtract_volume(poly,planes):
    if hasattr(planes,'bounds'):
        lo,hi=planes.bounds
        if any(max(v[k] for v in poly)<lo[k]-1e-8 or min(v[k] for v in poly)>hi[k]+1e-8 for k in range(3)):return [poly]
        for n,d in planes:
            if all(dot(n,v)>d+1e-8 for v in poly):return [poly]
        if all(all(dot(n,v)<d-1e-8 for v in poly) for n,d in planes):return []
    out=[]; rem=poly
    for plane in planes:
        piece=clip(rem,plane,False)
        if piece: out.append(piece)
        rem=clip(rem,plane,True)
        if not rem: break
    return out

PALETTES={'storybook_red':{'plaster':(.90,.82,.63),'wood':(.27,.19,.12),'tile':(.76,.24,.105),'stone':(.58,.58,.51),'teal':(.09,.43,.43)}}

class Mesh:
    def __init__(self,cfg):
        self.high=cfg['parameters']['detail_level']=='high'; self.bevel=cfg['parameters']['bevel_width_m']
        self.vertices=[]; self.faces=[]; self.names=[]; self.materials=[]; self.colors=[]
        self.uvs=[]; self.normals=[]; self.lookup={}; self.counter={}; self.clippers=[]
        self.palette=dict(PALETTES[cfg['parameters']['palette']])
        self.palette.update(glass=(0.09,0.17,0.18),metal=(0.18,0.19,0.17),canvas=(0.13,0.48,0.50),sail=(0.91,0.84,0.65))
        self.palette.update(ember=(1.0,.32,.025),water=(.15,.40,.47))
        self.palette.update(canvas=(.08,.47,.48),glass=(.15,.27,.28),leaf=(.28,.43,.13),flower=(.94,.66,.20),light=(1.,.71,.22),rock=(.59,.56,.45))
        self.seed=cfg['seeds']['decoration']; self.scale=cfg['parameters']['scale']
        self.features={'floors':cfg['parameters']['floor_count'],'windows':0,'doors':0,'dormers':0,'chimneys':0,'rotor_sails':0,'wheel_paddles':0}
    def id(self,stem):
        n=self.counter.get(stem,0); self.counter[stem]=n+1
        return '%s_%04d'%(stem,n)
    def clean(self):
        # Remove exact internal coincident caps created by adjacent closed modules.
        # Do not weld near-but-distinct parts or attempt to union intersecting beams.
        seen={};discard=set()
        for i,face in enumerate(self.faces):
            area=sum(math.sqrt(dot(cross(sub(self.vertices[face[j]],self.vertices[face[0]]),sub(self.vertices[face[j+1]],self.vertices[face[0]])),cross(sub(self.vertices[face[j]],self.vertices[face[0]]),sub(self.vertices[face[j+1]],self.vertices[face[0]]))))/2 for j in range(1,len(face)-1))
            if area<1e-10:
                discard.add(i);continue
            key=tuple(sorted(face))
            if key in seen:
                old=seen[key];discard.add(i)
                if dot(self.normals[old],self.normals[i])<-.99:discard.add(old)
            else:seen[key]=i
        keep=[i for i in range(len(self.faces)) if i not in discard]
        for field in ['faces','names','materials','colors','uvs','normals']:
            data=getattr(self,field);setattr(self,field,[data[i] for i in keep])
    def face(self,points,mat,name):
        polys=[points]
        for vol in self.clippers:
            polys=[piece for poly in polys for piece in subtract_volume(poly,vol)]
        for poly in polys:
            if len(poly)<3: continue
            norm=unit(cross(sub(poly[1],poly[0]),sub(poly[2],poly[0])))
            if dot(norm,norm)<0.1: continue
            ids=[]
            for pt in poly:
                pt=mul(pt,self.scale); key=tuple(round(v,7) for v in pt)
                if key not in self.lookup:
                    self.lookup[key]=len(self.vertices); self.vertices.append(pt)
                ids.append(self.lookup[key])
            if len(set(ids))<3: continue
            self.faces.append(ids); self.names.append(name); self.materials.append(mat); self.normals.append(norm)
            # Per-face orthonormal UV basis, metre-scale, independent of the world orientation.
            u=unit(sub(poly[1],poly[0])); v=cross(norm,u)
            self.uvs.append([(dot(sub(pt,poly[0]),u),dot(sub(pt,poly[0]),v),0.0) for pt in poly])
            h=int(hashlib.sha256((name+str(self.seed)).encode()).hexdigest()[:8],16)
            shade=0.92+(h%1000)/1000*0.16
            self.colors.append(tuple(min(c*shade,1) for c in self.palette[mat]))
    def box(self,c,size,mat='wood',name=None,axes=None):
        if self.high and mat in ('wood','stone','metal'):
            return chamfer_box(self,c,size,mat,name,axes)
        name=name or self.id(mat); axes=axes or [(1,0,0),(0,1,0),(0,0,1)]
        p=[add(c,add(mul(axes[0],x*size[0]/2),add(mul(axes[1],y*size[1]/2),mul(axes[2],z*size[2]/2))))
            for x,y,z in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        flip=dot(cross(axes[0],axes[1]),axes[2])<0
        for f in [(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(0,1,5,4),(3,7,6,2)]:
            self.face([p[i] for i in (reversed(f) if flip else f)],mat,name)
    def beam(self,a,b,w=0.16,d=None,mat='wood',name=None):
        along=unit(sub(b,a)); ref=(0,1,0) if abs(along[1])<.95 else (1,0,0)
        u=unit(cross(along,ref)); v=cross(u,along)
        self.box(mul(add(a,b),.5),(w,math.sqrt(dot(sub(b,a),sub(b,a))),d or w),mat,name,[u,along,v])
    def slab(self,poly,thick,mat,name=None,direction=(0,-1,0)):
        name=name or self.id(mat); q=[add(p,mul(direction,thick)) for p in poly]
        n=cross(sub(poly[1],poly[0]),sub(poly[2],poly[0]))
        if dot(n,direction)>0: poly=list(reversed(poly)); q=list(reversed(q))
        self.face(poly,mat,name); self.face(list(reversed(q)),mat,name)
        for i in range(len(poly)):
            j=(i+1)%len(poly); self.face([poly[i],q[i],q[j],poly[j]],mat,name)
    def tube(self,c,r,h,mat='wood',segments=16,axis='y',inner=0,name=None):
        name=name or self.id(mat)
        def pt(rad,a,t):
            p=(rad*math.cos(a),t,rad*math.sin(a))
            if axis=='x': p=(t,p[0],p[2])
            if axis=='z': p=(p[0],p[2],t)
            return add(c,p)
        for i in range(segments):
            a=2*math.pi*i/segments; b=2*math.pi*(i+1)/segments
            p=[pt(r,a,-h/2),pt(r,b,-h/2),pt(r,b,h/2),pt(r,a,h/2)]
            # Orient faces explicitly by centroid radial direction.
            normal=cross(sub(p[1],p[0]),sub(p[2],p[0])); mid=mul(add(p[0],p[2]),.5)
            radial=sub(mid,c)
            if dot(normal,radial)<0:p.reverse()
            self.face(p,mat,name)
            if inner:
                q=[pt(inner,a,-h/2),pt(inner,b,-h/2),pt(inner,b,h/2),pt(inner,a,h/2)]
                nn=cross(sub(q[1],q[0]),sub(q[2],q[0]))
                if dot(nn,radial)>0:q.reverse()
                self.face(q,mat,name)
            for t in [-h/2,h/2]:
                q=[pt(r,a,t),pt(r,b,t),pt(inner,b,t),pt(inner,a,t)] if inner else [pt(r,a,t),pt(r,b,t),pt(0,0,t)]
                target=(0,1 if t>0 else -1,0)
                if axis=='x':target=(target[1],0,0)
                if axis=='z':target=(0,0,target[1])
                if dot(cross(sub(q[1],q[0]),sub(q[2],q[0])),target)<0:q.reverse()
                self.face(q,mat,name)

def rectangular_volume(cx,cz,w,d,bottom,top,roof=0):
    planes=[((1,0,0),cx+w/2),((-1,0,0),-cx+w/2),((0,0,1),cz+d/2),((0,0,-1),-cz+d/2),((0,-1,0),-bottom)]
    if roof:
        slope=roof/(w/2)
        planes += [((slope,1,0),top+roof+slope*cx),((-slope,1,0),top+roof-slope*cx)]
    else: planes.append(((0,1,0),top))
    return planes

def round_volume(cx,cz,r,bottom,top,segments=24):
    return [((math.cos(2*math.pi*(i+.5)/segments),0,math.sin(2*math.pi*(i+.5)/segments)),
             r*math.cos(math.pi/segments)+cx*math.cos(2*math.pi*(i+.5)/segments)+cz*math.sin(2*math.pi*(i+.5)/segments)) for i in range(segments)]+[((0,-1,0),-bottom),((0,1,0),top)]

def wall_bay(m,a,b,y,h,p,opening='window',brace=True):
    length=math.sqrt(dot(sub(b,a),sub(b,a))); u=unit(sub(b,a)); n=cross((0,1,0),u); thick=p['wall_thickness_m']
    def at(x,v,offset=0):return add(add(a,mul(u,x)),add((0,y+v,0),mul(n,offset)))
    def cell(x0,x1,v0,v1):
        if x1-x0<1e-6 or min(v1)-max(v0)<-10: return
        poly=[at(x0,v0[0]),at(x1,v0[1]),at(x1,v1[1]),at(x0,v1[0])]
        m.slab(poly,thick,'plaster',direction=mul(n,-1))
    if opening is None:
        cell(0,length,(0,0),(h,h))
    else:
        isdoor=opening=='door'; width=min(p['door_width_m'] if isdoor else p['window_width_m'],length*(.84 if isdoor else .47))
        low=0 if isdoor else (.25 if h<2.0 else .95)
        high=2.1 if isdoor else (h-.18 if h<2.0 else min(h-.48,2.08))
        arch=.38 if isdoor else .18; left=(length-width)/2; right=left+width
        cell(0,left,(0,0),(h,h)); cell(right,length,(0,0),(h,h))
        if low:cell(left,right,(0,0),(low,low))
        curve=[]; slices=12
        for i in range(slices+1):
            t=i/slices; x=left+width*t
            curve.append((x,high-arch+arch*math.sqrt(max(0,1-(2*t-1)**2))))
        for (x0,v0),(x1,v1) in zip(curve,curve[1:]):cell(x0,x1,(v0,v1),(h,h))
        frame='stone' if isdoor else 'wood'
        for x in [left-.08,right+.08]:m.beam(at(x,low,.04),at(x,high-arch,.04),.17,mat=frame)
        for k,((x0,v0),(x1,v1)) in enumerate(zip(curve,curve[1:])):
            m.beam(at(x0,v0+.06,.045),at(x1,v1+.06,.045),.18,mat=frame,name=m.id('arch'))
        # Door and glass are separate inset inserts, not solid wall behind an opening.
        for i in range(6):
            x0=left+width*i/6+.012; x1=left+width*(i+1)/6-.012
            ztop=high-arch+arch*math.sqrt(max(0,1-((i+.5)/3-1)**2))
            m.slab([at(x0,low+.02,-.10),at(x1,low+.02,-.10),at(x1,ztop-.05,-.10),at(x0,ztop-.05,-.10)],.065,'wood' if isdoor else 'glass',direction=mul(n,-1))
        if isdoor:
            m.features['doors']+=1
            for ht in [.55,1.45]:m.beam(at(left+.06,ht,-.012),at(right-.06,ht,-.012),.055,mat='metal')
            m.box(at(right-.15,1.0,.025),(.06,.12,.06),'metal')
        else:
            m.features['windows']+=1
            m.beam(at(left,low-.05,.05),at(right,low-.05,.05),.17)
            m.beam(at(length/2,low,.0),at(length/2,high-.09,.0),.055)
            m.beam(at(left,(low+high)/2,.0),at(right,(low+high)/2,.0),.05)
            for side in ([-1,1] if p['shutters_enable'] else []):
                x=left-width*.24-.12 if side<0 else right+width*.24+.12
                for k in range(3):
                    xx=x+(k-1)*width*.14
                    m.box(at(xx,(low+high-arch)/2,.10),(width*.13,high-arch-low,.075),'teal',axes=[u,(0,1,0),n])
                for yy in [low+.15,high-arch-.15]:m.beam(at(x-width*.22,yy,.16),at(x+width*.22,yy,.16),.045,mat='metal')
    # Horizontal and vertical framing, bracing stays clear of openings.
    for x in [0,length]:
        m.beam(at(x,.05,.015),at(x,h,.015),.20)
    for v in [.04,h-.03]:m.beam(at(0,v,.02),at(length,v,.02),.20)
    if brace and opening is None and p['timber_pattern']!='simple':
        for side in [-1,1]:
            x=0 if side<0 else length; end=x-side*(length*.86)
            if p['timber_pattern']=='cross_brace':m.beam(at(x,.25,.035),at(end,h-.15,.035),.12)
            else:
                prev=at(x,h-.72,.035)
                for i in range(1,9):
                    t=i/8; now=at(x+(end-x)*(1-math.cos(t*math.pi/2)),h-.72+.57*math.sin(t*math.pi/2),.035)
                    m.beam(prev,now,.13);prev=now

def roof_height(x,z,w,d,h,shape,curve):
    q=abs(x)/(w/2)
    if shape=='hipped':
        short=min(w,d);q=max(0,(abs(x)-(w-short)/2)/(short/2),(abs(z)-(d-short)/2)/(short/2))
    return h*(1-q)+curve*q**4

def rect_roof(m,cx,cz,w,d,y,p,shape=None,dormer_holes=None):
    shape=shape or p['roof_shape'];w+=2*p['eave_depth_m'];d+=2*p['eave_depth_m'];rh=p['roof_height_m'];curve=p['eave_curve'];tile=p['tile_size_m'];clips=m.clippers[:]
    if dormer_holes:m.clippers+=dormer_holes
    nx=max(4,math.ceil(w/tile));nx+=nx%2;nz=max(4,math.ceil(d/tile));zw=d/nz
    # Staggered tile courses. Gable tiles are planar quads; hips triangulate.
    for i in range(nx):
        x0=-w/2+w*i/nx;x1=-w/2+w*(i+1)/nx
        for j in range(nz):
            z0=-d/2+j*zw;z1=z0+zw
            base=[(cx+x,y+roof_height(x,z,w,d,rh,shape,curve),cz+z) for x,z in [(x0,z0),(x1,z0),(x1,z1),(x0,z1)]]
            if shape=='gable':m.slab(base,.13,'wood',m.id('roof_shell'))
            else:
                for tri in [[base[0],base[1],base[2]],[base[0],base[2],base[3]]]:m.slab(tri,.13,'wood',m.id('roof_shell'))
        offset=.5 if i%2 else 0
        for j in range(-1,nz+1):
            z0=max(-d/2,-d/2+(j+offset)*zw)+.014;z1=min(d/2,-d/2+(j+1+offset)*zw)-.014
            if z1-z0<.05:continue
            xa=x0+.014;xb=x1-.014
            top=[(cx+x,y+.055+roof_height(x,z,w,d,rh,shape,curve),cz+z) for x,z in [(xa,z0),(xb,z0),(xb,z1),(xa,z1)]]
            name=m.id('roof_tile')
            if shape=='gable':m.slab(top,.065,'tile',name)
            else:
                for tri in [[top[0],top[1],top[2]],[top[0],top[2],top[3]]]:m.slab(tri,.065,'tile',name)
    m.clippers=clips
    for z in [-d/2,d/2]:
        for side in [-1,1]:
            prev=(cx,y+roof_height(0,z,w,d,rh,shape,curve),cz+z)
            for i in range(1,13):
                x=side*w/2*i/12;now=(cx+x,y+roof_height(x,z,w,d,rh,shape,curve),cz+z);m.beam(prev,now,.16);prev=now
    for side in [-1,1]:m.beam((cx+side*w/2,y+curve,cz-d/2),(cx+side*w/2,y+curve,cz+d/2),.18)
    if shape=='gable':
        for j in range(nz):m.tube((cx,y+rh+.08,cz-d/2+(j+.5)*zw),.12,zw-.025,'tile',10,'z',inner=.07,name=m.id('ridge_cap'))

def cone_roof(m,cx,cz,r,y,p,h=None,top_radius=.035):
    h=h or p['roof_height_m'];r+=p['eave_depth_m'];steps=max(5,math.ceil(h/p['tile_size_m']));n=32
    def at(t,a,offset=0):
        rr=r*(1-t)+top_radius*t
        return(cx+rr*math.cos(a),y+h*t+p['eave_curve']*(1-t)**4+offset,cz+rr*math.sin(a))
    for j in range(steps):
        for i in range(n):
            a=2*math.pi*(i+(j%2)*.5)/n;b=2*math.pi*(i+1+(j%2)*.5)/n;t=j/steps;u=(j+1)/steps
            poly=[at(t,a),at(t,b),at(u,b),at(u,a)]
            m.slab(poly,.13,'wood',m.id('cone_shell'))
            q=[at(t+.006,a+.006,.04),at(t+.006,b-.006,.04),at(u-.006,b-.006,.04),at(u-.006,a+.006,.04)]
            m.slab(q,.05,'tile',m.id('cone_tile'))
    m.tube((cx,y+.1,cz),r,.16,'wood',32,inner=max(.1,r-.14))
    m.tube((cx,y+h+.16,cz),.09,.4,'metal',8)

def rail(m,a,b,y):
    length=math.sqrt(dot(sub(b,a),sub(b,a)));n=max(1,math.ceil(length/1.1))
    for i in range(n+1):
        q=lerp(a,b,i/n);m.beam(add(q,(0,y,0)),add(q,(0,y+1.0,0)),.13)
        m.box(add(q,(0,y+1.02,0)),(.19,.07,.19))
    for h in [.33,.86]:m.beam(add(a,(0,y+h,0)),add(b,(0,y+h,0)),.10)
    n=max(1,math.ceil(length/.28))
    for i in range(1,n):
        q=lerp(a,b,i/n);m.beam(add(q,(0,y+.3,0)),add(q,(0,y+.9,0)),.055)

def deck(m,cx,cz,w,d,y,rails=True):
    n=math.ceil(w/.25)
    for i in range(n):m.box((cx-w/2+(i+.5)*w/n,y-.10,cz),(w/n-.015,.18,d),'wood',m.id('deck_plank'))
    for z in [-d/2+.1,d/2-.1]:m.beam((cx-w/2,y-.24,cz+z),(cx+w/2,y-.24,cz+z),.22)
    if rails:
        rail(m,(cx-w/2,0,cz-d/2),(cx+w/2,0,cz-d/2),y)
        for x in [-w/2,w/2]:rail(m,(cx+x,0,cz-d/2),(cx+x,0,cz+d/2),y)

def stairs(m,x,z,top,width=1.25,material='stone',bottom=0):
    n=max(1,math.ceil((top-bottom)/.19));run=.30;rise=(top-bottom)/n
    for i in range(n):
        ht=bottom+(i+1)*rise
        if material=='stone':m.box((x,(bottom+ht)/2,z-(n-i-.5)*run),(width,ht-bottom,run-.008),'stone',m.id('step'))
        else:m.box((x,ht-.07,z-(n-i-.5)*run),(width,.14,run),'wood',m.id('step'))
    if material=='wood':
        for side in [-1,1]:
            a=(x+side*width/2,bottom,z-n*run);b=(x+side*width/2,top,z)
            m.beam(a,b,.20);m.beam(add(a,(0,1,0)),add(b,(0,1,0)),.10)
            for t in [0,.5,1]:
                q=lerp(a,b,t);m.beam(q,add(q,(0,1,0)),.11)

def stone_band(m,cx,cz,w,d,y,height,round_r=None):
    rows=max(1,math.ceil(height/.36));hh=height/rows
    for j in range(rows):
        if round_r:
            n=max(12,math.ceil(2*math.pi*round_r/.65))
            for i in range(n):
                a=2*math.pi*(i+.5+(j%2)*.5)/n;u=(-math.sin(a),0,math.cos(a));v=(math.cos(a),0,math.sin(a))
                m.box((cx+round_r*math.cos(a),y+(j+.5)*hh,cz+round_r*math.sin(a)),(2*round_r*math.sin(math.pi/n)-.025,hh-.025,.27),'stone',axes=[u,(0,1,0),v])
        else:
            for side in [-1,1]:
                n=math.ceil(w/.65)
                for i in range(n):m.box((cx-w/2+(i+.5)*w/n,y+(j+.5)*hh,cz+side*d/2),(w/n-.025,hh-.025,.25),'stone')
                n=math.ceil((d-.3)/.65)
                for i in range(n):m.box((cx+side*w/2,y+(j+.5)*hh,cz-(d-.3)/2+(i+.5)*(d-.3)/n),(.25,hh-.025,(d-.3)/n-.025),'stone')

def foundation(m,p):
    w=p['width_m'];d=p['depth_m'];h=p['foundation_height_m'];f=p['footprint'];mode=p['foundation_mode']
    if f=='round':
        r=w/2+.30;m.tube((0,h-.12,0),r,.24,'stone',32)
        if mode in ('stone','hybrid'):stone_band(m,0,0,0,0,0,h,round_r=r-.1)
    else:
        m.box((0,h-.12,0),(w+.5,.24,d+.5),'stone',m.id('plinth'))
        if mode in ('stone','hybrid'):stone_band(m,0,0,w+.28,d+.28,0,h if mode=='stone' else h*.48)
    if mode in ('stilts','hybrid'):
        for x in [-w/2,0,w/2]:
            for z in [-d/2,d/2]:
                ground=-p['terrain_slope']*(x+w/2)
                m.box((x,ground+.10,z),(.50,.20,.50),'stone')
                m.beam((x,ground+.2,z),(x,h-.1,z),.26)
                m.beam((x,ground+.35,z),(x+.7 if x<w/2 else x-.7,h-.15,z),.17)
        deck(m,0,-d/2-.65,w+1.0,1.3,h,False)
        for x in [-w/2-.3,w/2+.3]:
            ground=-p['terrain_slope']*(x+w/2)
            m.beam((x,ground,-d/2-1.15),(x,h,-d/2-1.15),.22)
            m.beam((x,ground+.2,-d/2-1.15),(0,h-.25,-d/2-1.15),.16)
        # Central opening preserves entrance access through terrace railing.
        rail(m,(-w/2-.45,0,-d/2-1.25),(-.8,0,-d/2-1.25),h)
        rail(m,(.8,0,-d/2-1.25),(w/2+.45,0,-d/2-1.25),h)
        if p['context_mode']=='cliff':cliff_stairs(m,p)
        else:stairs(m,0,-d/2-1.3,h,material='wood')
    else:
        doorx=door_x(p)
        stairs(m,doorx,-(w/2 if f=='round' else d/2)-.25,h)

def door_x(p):
    if p['footprint']=='round':return 0
    row=p['front_bays'].split('/')[0]
    return -p['width_m']/2+(row.index('D')+.5)*p['width_m']/len(row)

def facade_row(p,side,floor,main=True):
    if not main:return '.W' if floor else '..'
    row=p[['front_bays','right_bays','rear_bays','left_bays'][side]].split('/')[floor]
    if p['window_layout_mode']=='pattern':return row
    n=len(row);cells=['.']*n
    door=side==0 and floor==0
    if door:cells[row.index('D')]='D'
    count=p[['front_window_count','right_window_count','rear_window_count','left_window_count'][side]]
    free=[i for i in range(n) if cells[i]!='D']
    if count>len(free):raise ValueError('Requested window count exceeds facade bays; add bays or reduce count')
    order=free[::2]+free[1::2]
    for i in order[:count]:cells[i]='W'
    return ''.join(cells)


def slot_choice(m,p,slot,opening,floor,side,bay,length,blocked=False,curved=False):
    overrides=json.loads(p['facade_overrides']);explicit=slot in overrides
    if not hasattr(m,'facade_slots'):m.facade_slots={}
    token=int(hashlib.sha256((str(m.seed)+slot).encode()).hexdigest()[:8],16)
    style=p['facade_style'];module='plain'
    if style!='legacy' and opening is None and not blocked:
        module=['studs','brace','infill'][token%3]
        if style in ('B','C') and token/4294967296 < p['facade_module_density']:
            module=['niche','infill','trellis'][token%3]
        if style in ('B','C') and floor==0 and side==0 and bay==0 and length>=1.6 and p['foundation_height_m']<=.7 and not curved and slot.startswith('main.'):module='service'
        if style=='C' and not curved and floor>0:
            face=slot.rsplit('.',2)[0]
            if not hasattr(m,'facade_budgets'):m.facade_budgets={}
            if m.facade_budgets.get(face,0)<p['facade_accent_budget']:
                module=['sign','crest','banner'][token%3]
                if floor>0 and side==2 and length>=1.6 and p['balcony_layout']!='wrap':module='oriel'
                m.facade_budgets[face]=m.facade_budgets.get(face,0)+1
    if explicit:
        module=overrides[slot]
        if blocked and module!='plain':raise ValueError(slot+': reserved by utility, attachment or roof junction')
        if opening is not None and module!='plain':raise ValueError(slot+': preserve existing door/window; choose a solid slot')
        if module=='service' and (floor!=0 or side!=0 or curved or length<1.6 or p['foundation_height_m']>.7):raise ValueError(slot+': service requires accessible front ground bay >=1.6m and base <=0.7m')
        if module=='oriel' and (floor==0 or curved or side!=2 or length<1.6 or p['balcony_layout']=='wrap'):raise ValueError(slot+': oriel requires upper rear solid bay >=1.6m without wrap balcony')
        if curved and module not in ('plain','studs','infill','niche','crest'):raise ValueError(slot+': module unsupported on curved/tapered wall')
    if curved and module not in ('plain','studs','infill','niche','crest'):module='infill'
    m.facade_slots[slot]={'module':module,'opening':opening,'blocked':blocked}
    return module

def facade_module(m,a,b,y,h,p,opening,module,slot,allow_brace=True):
    u=unit(sub(b,a));n=cross((0,1,0),u);length=math.sqrt(dot(sub(b,a),sub(b,a)))
    def at(x,v,z=0):return add(a,add(mul(u,x),add((0,y+v,0),mul(n,z))))
    def box(x,v,z,w,hh,d,mat='wood',name='part'):
        m.box(at(x,v,z),(w,hh,d),mat,slot+'_'+name,axes=[u,(0,1,0),n])
    def beam(x,v,z,xx,vv,zz,width=.10,mat='wood'):
        m.beam(at(x,v,z),at(xx,vv,zz),width,mat=mat,name=slot+'_brace')
    start=len(m.faces)
    q=dict(p,timber_pattern='simple') if module!='plain' else p
    if module=='niche':
        # Remove only outer part of thick shell; retain a real recessed solid back.
        c=at(length/2,h*.54,.04);half=min(.34,length*.28)
        planes=[]
        for axis,extent in [(u,half),((0,1,0),.55),(n,p['wall_thickness_m']*.58+.04)]:
            planes += [(axis,dot(axis,c)+extent),(mul(axis,-1),dot(mul(axis,-1),c)+extent)]
        old=m.clippers[:];m.clippers=old+[planes]
        wall_bay(m,a,b,y,h,q,None,False);m.clippers=old
        box(length/2,h*.54-.57,.035,half*2+.16,.11,.23,'stone','niche_sill')
        for x in [length/2-half-.04,length/2+half+.04]:box(x,h*.54,.02,.09,1.12,.13,'stone','niche_jamb')
        box(length/2,h*.54+.57,.02,half*2+.16,.10,.13,'stone','niche_lintel')
    else:
        wall_bay(m,a,b,y,h,q,'door' if module=='service' else ('window' if module=='oriel' else opening),module=='plain' and allow_brace)
    if opening is None and module in ('studs','brace','infill','trellis','service','sign','banner','crest','oriel'):
        if module=='studs':
            count=max(1,math.ceil(length/p['facade_bay_width_m']))
            for i in range(1,count):box(length*i/count,h/2,.04,.10,h-.2,.13,name='stud')
            box(length/2,.65,.04,length-.2,.10,.13,name='sill_band')
        if module=='brace':beam(.17,.3,.06,length-.17,h-.2,.06,.13)
        if module=='infill':
            count=max(2,math.ceil((length-.24)/.20))
            for i in range(count):box(.12+(length-.24)*(i+.5)/count,.48,.065,(length-.24)/count-.012,.77,.10,name='plank')
            box(length/2,.91,.1,length-.2,.12,.16,name='infill_cap')
        if module=='service':
            box(length/2,-.09,.30,min(length-.2,1.65),.18,.65,'stone','step')
            box(length/2,1.0,-.025,.055,1.85,.055,'metal','door_split')
        if module=='trellis':
            for i in range(4):box(length*(i+1)/5,h*.45,.12,.045,h*.65,.055,name='trellis_u')
            for i in range(5):box(length/2,.35+i*h*.14,.14,length*.72,.045,.055,name='trellis_v')
            for i in range(7):
                x=length*(.25+.5*((i*3)%7)/7);v=.45+(i%4)*h*.15
                box(x,v,.20,.18,.21,.08,'leaf','ivy')
        if module=='sign':
            beam(length/2,h-.25,.04,length/2,h-.25,.65,.09,'metal')
            beam(length/2,h-.25,.60,length/2,h-.76,.60,.035,'metal')
            box(length/2,h-1.,.60,.62,.48,.10,'wood','sign')
            box(length/2,h-1.,.66,.27,.24,.025,'flower','sign_emblem')
        if module=='banner':
            box(length/2,h-.35,.15,.75,.07,.08,'metal','banner_rod')
            box(length/2,h-1.,.13,.58,1.15,.035,'canvas','banner')
            box(length/2,h-1.,.16,.15,.28,.025,'flower','banner_crest')
        if module=='crest':
            box(length/2,h*.57,.11,.55,.65,.12,'stone','crest')
            beam(length/2-.15,h*.57,.19,length/2+.15,h*.57,.19,.065,'tile')
            beam(length/2,h*.57-.22,.19,length/2,h*.57+.22,.19,.065,'tile')
        if module=='oriel':
            width=min(1.25,length-.35);low=.78;high=min(h-.45,2.15);z=.55
            box(length/2,low-.10,z/2,width+.13,.18,z+.2,name='oriel_base')
            m.slab([at(length/2-width/2-.12,high+.32,-.05),at(length/2+width/2+.12,high+.32,-.05),at(length/2+width/2+.12,high+.08,z+.15),at(length/2-width/2-.12,high+.08,z+.15)],.08,'tile',slot+'_oriel_roof')
            box(length/2,(low+high)/2,z,width,high-low,.055,'glass','oriel_glass')
            for x in [length/2-width/2,length/2,length/2+width/2]:box(x,(low+high)/2,z+.04,.075,high-low+.08,.09,name='oriel_mullion')
            for x in [length/2-width/2,length/2+width/2]:
                box(x,(low+high)/2,z/2,.08,high-low,z,'glass','oriel_side')
                beam(x,.2,.02,x,low-.2,z,.13)
            box(length/2,low,z+.03,width,.08,.09,name='oriel_sill')
            box(length/2,high,z+.03,width,.08,.09,name='oriel_header')
    # Stable address retained on every facade primitive, independent of random selection.
    for i in range(start,len(m.names)):m.names[i]=slot+'__'+m.names[i]


def interior_style(p):
    if p['interior_style']!='auto':return p['interior_style']
    if p['utility_type'] in ('windmill','waterwheel'):return 'mill'
    if p['footprint']=='round':return 'workshop'
    if p['footprint']=='l_shape':return 'inn'
    return 'home'

def interior_table(m,x,y,z,width=1.1):
    for i in range(4):m.box((x,y+.76,z+(i-1.5)*.18),(width,.08,.17),'wood',m.id('int_table_top'))
    for a in [-1,1]:
        for b in [-1,1]:m.box((x+a*(width/2-.10),y+.37,z+b*.24),(.09,.74,.09),'wood',m.id('int_table_leg'))
    for dz in [-.67,.67]:
        m.box((x,y+.44,z+dz),(width,.08,.28),'wood',m.id('int_bench_seat'))
        for dx in [-width*.35,width*.35]:m.box((x+dx,y+.21,z+dz),(.12,.42,.22),'wood',m.id('int_bench_leg'))

def interior_shelf(m,x,y,z,width=1.15):
    for dx in [-width/2,width/2]:m.box((x+dx,y+.83,z),(.09,1.66,.33),'wood',m.id('int_shelf_upright'))
    for k in range(4):
        h=y+.16+k*.46;m.box((x,h,z),(width,.06,.36),'wood',m.id('int_shelf_board'))
        if k<3:
            for j in range(3):
                xx=x+(j-1)*width*.25
                m.tube((xx,h+.14,z),.095,.22,'tile',12,name=m.id('int_pot'))
                m.tube((xx,h+.27,z),.055,.05,'tile',12,inner=.035,name=m.id('int_pot_lip'))

def interior_bed(m,x,y,z):
    m.box((x,y+.32,z),(1.05,.18,1.9),'wood',m.id('int_bed_frame'))
    m.box((x,y+.49,z),(.98,.18,1.8),'sail',m.id('int_mattress'))
    m.box((x,y+.60,z-.20),(1.0,.045,1.25),'canvas',m.id('int_blanket'))
    m.box((x,y+.61,z+.62),(.65,.14,.34),'sail',m.id('int_pillow'))
    m.box((x,y+.66,z+.96),(1.08,.9,.09),'wood',m.id('int_headboard'))
    for a in [-.43,.43]:
        for b in [-.8,.8]:m.box((x+a,y+.15,z+b),(.12,.30,.12),'wood',m.id('int_bed_leg'))

def stair_rect_bounds(cx,cz,w,d,p):
    sw=p['interior_stair_width_m'];tw=2*sw+.16
    x1=cx+w/2-p['wall_thickness_m']-.10;x0=x1-tw
    z1=cz+d/2-p['wall_thickness_m']-.10
    run=min(1.9,d-2*p['wall_thickness_m']-1.35)
    return x0,x1,z1-run-sw,z1,run,sw

def interior_rect_level(m,cx,cz,w,d,ww,dd,y,floor,p):
    fh=p['floor_height_m'];th=p['wall_thickness_m'];xlo=cx-ww/2+th;xhi=cx+ww/2-th;zlo=cz-dd/2+th;zhi=cz+dd/2-th
    x0,x1,z0,z1,run,sw=stair_rect_bounds(cx,cz,w,d,p)
    # Upper floors surround an actual rectangular void. Four closed plates, capped reveals.
    rects=[(xlo,xhi,zlo,zhi)] if floor==0 else [(xlo,x0,zlo,zhi),(x1,xhi,zlo,zhi),(x0,x1,zlo,z0),(x0,x1,z1,zhi)]
    for xa,xb,za,zb in rects:
        if xb-xa<.005 or zb-za<.005:continue
        # Narrow boards with structural thickness; gaps are 3mm.
        count=max(1,math.ceil((xb-xa)/.28))
        for i in range(count):
            a=xa+(xb-xa)*i/count;b=xa+(xb-xa)*(i+1)/count
            m.box(((a+b)/2,y+.04,(za+zb)/2),(b-a-.003,.08,zb-za),'wood',m.id('int_floor_plank'))
        if floor>0:
            n=max(1,math.ceil((zb-za)/p['interior_joist_spacing_m']))
            for j in range(n):m.box(((xa+xb)/2,y-.10,za+(j+.5)*(zb-za)/n),(xb-xa,.20,.12),'wood',m.id('int_floor_joist'))
    # Posts and perimeter beams independent from facade ornament.
    for x in [xlo+.11,xhi-.11]:
        for z in [zlo+.11,zhi-.11]:m.box((x,y+fh/2,z),(.17,fh,.17),'wood',m.id('int_post'))
    for z in [zlo+.10,zhi-.10]:
        spans=[(xlo,xhi)] if z<cz or floor==p['floor_count']-1 else [(xlo,x0),(x1,xhi)]
        for a,b in spans:
            if b-a>.02:m.box(((a+b)/2,y+fh-.15,z),(b-a,.22,.18),'wood',m.id('int_perimeter_beam'))
    if floor<p['floor_count']-1:
        n=max(7,math.ceil(fh/.36));rise=fh/(2*n);tread=run/n
        # Two opposing flights, intermediate landing at rear; top arrives on front floor.
        for flight in range(2):
            xx=x0+sw/2 if flight==0 else x1-sw/2
            for k in range(n):
                zz=z0+(k+.5)*tread if flight==0 else z0+run-(k+.5)*tread
                hh=y+.08+(k+1+flight*n)*rise
                m.box((xx,hh-.045,zz),(sw,.09,tread+.008),'wood',m.id('int_stair_tread'))
            za=z0 if flight==0 else z0+run;zb=z0+run if flight==0 else z0
            ya=y+.08+flight*fh/2;yb=ya+fh/2
            for dx in [-sw/2+.06,sw/2-.06]:m.beam((xx+dx,ya-.12,za),(xx+dx,yb-.12,zb),.12,name=m.id('int_stair_stringer'))
            # Handrail and uprights on both sides; terminate at landing, never cross passage.
            for dx in [-sw/2+.025,sw/2-.025]:
                m.beam((xx+dx,ya+.9,za),(xx+dx,yb+.9,zb),.055,name=m.id('int_stair_handrail'))
                for t in [0,.33,.67,1]:
                    zz=za+(zb-za)*t;yy=ya+(yb-ya)*t
                    m.beam((xx+dx,yy,zz),(xx+dx,yy+.9,zz),.045,name=m.id('int_stair_baluster'))
        m.box(((x0+x1)/2,y+.08+fh/2-.05,z0+run+sw/2),(x1-x0,.10,sw),'wood',m.id('int_stair_midlanding'))
        for x in [x0+.08,x1-.08]:m.box((x,y+fh/4,z1-.10),(.14,fh/2,.14),'wood',m.id('int_landing_post'))
        m.features['interior_stair_flights']=m.features.get('interior_stair_flights',0)+2
    if floor>0:
        # Guard open sides; front opening remains clear for arrival.
        for xa,za,xb,zb in [(x0,z0,x0,z1),(x0,z1,x1,z1),(x1,z1,x1,z0)]:
            m.beam((xa,y+1,za),(xb,y+1,zb),.065,name=m.id('int_void_guard'))
            length=math.hypot(xb-xa,zb-za)
            for j in range(math.ceil(length/.35)+1):
                t=j/max(1,math.ceil(length/.35));x=xa+(xb-xa)*t;z=za+(zb-za)*t
                m.beam((x,y+.08,z),(x,y+1,z),.045,name=m.id('int_void_baluster'))
        m.features['interior_floor_openings']=m.features.get('interior_floor_openings',0)+1
    # Upper sleeping room occupies left rear; doorway stays on clear circulation strip.
    if p['interior_partitions'] and floor>0 and x0-xlo>1.45:
        z=cz-.30;left=xlo+.02;right=x0-.12;door=min(.9,right-left-.3);end=right-door
        if end>left:m.box(((left+end)/2,y+1.05,z),(end-left,2.1,.10),'plaster',m.id('int_partition'))
        m.box(((left+right)/2,y+(2.1+fh-.22)/2,z),(right-left,fh-.22-2.1,.10),'plaster',m.id('int_partition_header'))
        for x in [end,right]:m.box((x,y+1.05,z),(.07,2.1,.14),'wood',m.id('int_partition_jamb'))
    if p['interior_furniture']:
        style=interior_style(p);x=xlo+.72;z=zhi-1.08
        if floor>0:
            interior_bed(m,x,y+.08,z)
            interior_shelf(m,x,y+.08,zlo+.25,min(1.15,x0-xlo-.20))
        else:
            interior_table(m,x,y+.08,cz-.35,min(1.2,x0-xlo-.25))
            interior_shelf(m,x,y+.08,zhi-.25,min(1.25,x0-xlo-.20))
            if style=='inn':m.box((x,y+.55,cz+1),(1.25,.94,.48),'wood',m.id('int_inn_counter'))
            if style in ('workshop','mill'):
                m.tube((x,y+.32,cz+1),.48,.48,'stone',20,name=m.id('int_millstone_base'))
                m.tube((x,y+.61,cz+1),.50,.13,'stone',20,name=m.id('int_millstone'))
    m.features['interior_levels']=m.features.get('interior_levels',0)+1

def interior_round_level(m,cx,cz,r,y,floor,p,main=True):
    # Central spiral circulation, annular upper floors. Small attached turret uses own stair.
    fh=p['floor_height_m'];outer=r-p['wall_thickness_m']-.03;hole=min(.98,outer*.57);n=32
    if floor==0:m.tube((cx,y+.04,cz),outer,.08,'wood',32,name=m.id('int_round_floor'))
    else:m.tube((cx,y+.04,cz),outer,.08,'wood',32,inner=hole,name=m.id('int_round_floor'))
    for i in range(8):
        a=i*math.tau/8
        if floor>0:m.beam((cx+(hole+.06)*math.cos(a),y-.10,cz+(hole+.06)*math.sin(a)),(cx+(outer-.07)*math.cos(a),y-.10,cz+(outer-.07)*math.sin(a)),.14,name=m.id('int_radial_joist'))
        x=cx+(outer-.08)*math.cos(a);z=cz+(outer-.08)*math.sin(a)
        m.box((x,y+fh/2,z),(.12,fh,.12),'wood',m.id('int_round_post'))
    if floor<p['floor_count']-1:
        steps=max(14,math.ceil(fh/.18));radius=hole-.025
        m.tube((cx,y+fh/2,cz),.09,fh,'wood',16,name=m.id('int_spiral_post'))
        for i in range(steps):
            a=-math.pi/2+i*math.tau/steps;b=-math.pi/2+(i+1)*math.tau/steps;h=y+.08+(i+1)*fh/steps
            m.slab([(cx+.11*math.cos(a),h,cz+.11*math.sin(a)),(cx+radius*math.cos(a),h,cz+radius*math.sin(a)),(cx+radius*math.cos(b),h,cz+radius*math.sin(b)),(cx+.11*math.cos(b),h,cz+.11*math.sin(b))],.06,'wood',m.id('int_spiral_tread'))
            pt=(cx+radius*math.cos(b),h,cz+radius*math.sin(b));end=add(pt,(0,.9,0))
            m.beam(pt,end,.035,name=m.id('int_spiral_baluster'))
            if i:m.beam(prev,end,.05,name=m.id('int_spiral_rail'))
            prev=end
        m.features['interior_stair_flights']=m.features.get('interior_stair_flights',0)+1
    if floor>0:
        # Front arc is access; guards cover remaining circumference.
        for i in range(2,30):
            a=-math.pi/2+i*math.tau/32;b=-math.pi/2+(i+1)*math.tau/32
            aa=(cx+hole*math.cos(a),y+1,cz+hole*math.sin(a));bb=(cx+hole*math.cos(b),y+1,cz+hole*math.sin(b))
            m.beam(aa,bb,.05,name=m.id('int_round_guard'));m.beam(add(aa,(0,-.92,0)),aa,.04,name=m.id('int_round_baluster'))
        m.features['interior_floor_openings']=m.features.get('interior_floor_openings',0)+1
    if p['interior_furniture'] and outer>1.7:
        interior_shelf(m,cx,y+.08,cz+outer-.38,min(1.5,outer))
        # Workbench follows left side; maintain central stair envelope.
        xx=cx-outer+.35
        m.box((xx,y+.84,cz),(.50,.10,1.35),'wood',m.id('int_round_workbench'))
        for dz in [-.5,.5]:m.box((xx,y+.43,cz+dz),(.12,.78,.12),'wood',m.id('int_workbench_leg'))
    m.features['interior_levels']=m.features.get('interior_levels',0)+1

def rectangular_body(m,cx,cz,w,d,bottom,p,others=None,main=True):
    floors=p['floor_count'];fh=p['floor_height_m'];over=p['upper_overhang_m'];clips=m.clippers[:]
    m.clippers=others or []
    for floor in range(floors):
        ext=over if floor>0 else 0;ww=w+2*ext;dd=d+2*ext;y=bottom+floor*fh
        corners=[(cx-ww/2,0,cz-dd/2),(cx+ww/2,0,cz-dd/2),(cx+ww/2,0,cz+dd/2),(cx-ww/2,0,cz+dd/2)]
        for side in range(4):
            a=corners[side];b=corners[(side+1)%4];length=math.sqrt(dot(sub(b,a),sub(b,a)));row=facade_row(p,side,floor,main);nb=len(row)
            for bay in range(nb):
                aa=lerp(a,b,bay/nb);bb=lerp(a,b,(bay+1)/nb)
                opening={'D':'door','W':'window','.':None}[row[bay]]
                # Access to occupied balcony; clear utility wall, lean-to roof and bay window attachment.
                if main and floor==1 and side==0 and p['balcony_layout'] in ('front','wrap') and bay==nb//2:opening='door'
                if main and side==1 and ((p['utility_type']=='waterwheel' and floor==0) or (p['workshop_enable'] and floor==0) or (p['bay_window_enable'] and floor==1)):opening=None
                slot=('main' if main else 'wing')+'.'+['front','right','rear','left'][side]+'.'+str(floor)+'.'+str(bay)
                # Conservatively reserve joining facades and utility/attachment zones.
                blocked=(main and ((p['footprint']=='hybrid' and side==1) or (p['footprint']=='l_shape' and side==1) or (side==1 and (p['utility_type']!='none' or p['workshop_enable'] or p['bay_window_enable'])))) or (not main and side==0)
                module=slot_choice(m,p,slot,opening,floor,side,bay,math.sqrt(dot(sub(bb,aa),sub(bb,aa))),blocked)
                facade_module(m,aa,bb,y,fh,p,opening,module,slot)
                if floor==0 and opening!='door' and module!='service':
                    length=math.sqrt(dot(sub(bb,aa),sub(bb,aa)));along=unit(sub(bb,aa));outward=cross((0,1,0),along)
                    nblocks=max(2,math.ceil(length/.65))
                    for ib in range(nblocks):
                        center=add(lerp(aa,bb,(ib+.5)/nblocks),add((0,y+.19,0),mul(outward,.025)))
                        m.box(center,(length/nblocks-.025,.36,.18),'stone',axes=[along,(0,1,0),outward])
        # Floor plates inside the shell; no full box wall behind holes.
        if p['interior_enable']: interior_rect_level(m,cx,cz,w,d,ww,dd,y,floor,p)
        else:m.box((cx,y+.04,cz),(ww-2*p['wall_thickness_m'],.08,dd-2*p['wall_thickness_m']),'wood',m.id('floor'))
        if floor>0 and ext:
            for x in [-w/2,w/2]:
                for z in [-d/2,d/2]:m.beam((cx+x,y-.8,cz+z),(cx+x+math.copysign(ext,x),y-.10,cz+z+math.copysign(ext,z)),.20)
    top=bottom+floors*fh;ww=w+2*over;dd=d+2*over
    # Close the gap between storey datum and raised roof/eave underside.
    edges=[((cx+ww/2,cz-dd/2),(cx+ww/2,cz+dd/2)),((cx-ww/2,cz+dd/2),(cx-ww/2,cz-dd/2))]
    if p['roof_shape']=='hipped':edges += [((cx-ww/2,cz-dd/2),(cx+ww/2,cz-dd/2)),((cx+ww/2,cz+dd/2),(cx-ww/2,cz+dd/2))]
    for a,b in edges:
        along=unit((b[0]-a[0],0,b[1]-a[1]));outward=cross((0,1,0),along)
        for i in range(16):
            x0=a[0]+(b[0]-a[0])*i/16;z0=a[1]+(b[1]-a[1])*i/16
            x1=a[0]+(b[0]-a[0])*(i+1)/16;z1=a[1]+(b[1]-a[1])*(i+1)/16
            h0=roof_height(x0-cx,z0-cz,ww+2*p['eave_depth_m'],dd+2*p['eave_depth_m'],p['roof_height_m'],p['roof_shape'],p['eave_curve'])-.13
            h1=roof_height(x1-cx,z1-cz,ww+2*p['eave_depth_m'],dd+2*p['eave_depth_m'],p['roof_height_m'],p['roof_shape'],p['eave_curve'])-.13
            if min(h0,h1)>0:m.slab([(x0,top,z0),(x1,top,z1),(x1,top+h1,z1),(x0,top+h0,z0)],p['wall_thickness_m'],'plaster',direction=mul(outward,-1))
    if p['roof_shape']=='gable':
        for z,reverse in [(cz-dd/2,False),(cz+dd/2,True)]:
            # Gable closes to the same sampled roof envelope, including curvature.
            gable_clips=m.clippers[:]
            if p['roof_height_m']>1.7 and p['attic_window_enable'] and not reverse:
                m.clippers += [rectangular_volume(cx,z,.64,p['wall_thickness_m']*2+.2,top+.43,top+1.33)]
            n=20
            for i in range(n):
                x0=-ww/2+ww*i/n;x1=-ww/2+ww*(i+1)/n
                h0=roof_height(x0,0,ww+2*p['eave_depth_m'],dd,p['roof_height_m'],'gable',p['eave_curve'])-.13
                h1=roof_height(x1,0,ww+2*p['eave_depth_m'],dd,p['roof_height_m'],'gable',p['eave_curve'])-.13
                m.slab([(cx+x0,top,z),(cx+x1,top,z),(cx+x1,top+h1,z),(cx+x0,top+h0,z)],p['wall_thickness_m'],'plaster',direction=(0,0,-1 if reverse else 1))
            m.clippers=gable_clips
            if p['roof_height_m']>1.7 and p['attic_window_enable'] and not reverse:
                dz=-.045 if not reverse else .045
                m.box((cx,top+.88,z-dz),(.62,.88,.045),'glass',m.id('attic_glass'))
                for dx in [-.37,.37]:m.beam((cx+dx,top+.39,z+dz),(cx+dx,top+1.37,z+dz),.11)
                for yy in [top+.39,top+1.37]:m.beam((cx-.37,yy,z+dz),(cx+.37,yy,z+dz),.11)
                m.beam((cx,top+.43,z+dz),(cx,top+1.33,z+dz),.045)
                m.beam((cx,top,z),(cx,top+.33,z),.18)
                m.beam((cx,top+1.43,z),(cx,top+p['roof_height_m']-.14,z),.18)
                m.features['windows']+=1
            else:m.beam((cx,top,z),(cx,top+p['roof_height_m']-.14,z),.18)
    if p['roof_shape']=='gable':
        for z in [cz-dd/2-.025,cz+dd/2+.025]:
            for sign in [-1,1]:m.beam((cx+sign*ww*.40,top+.12,z),(cx,top+p['roof_height_m']-.20,z),.14)
    if p['interior_enable']:
        for z in [cz-d*.3,cz,cz+d*.3]:
            y=top+.14;rise=p['roof_height_m']-.30
            for sign in [-1,1]:m.beam((cx+sign*(w/2-.22),y,z),(cx,y+rise,z),.14,name=m.id('int_roof_rafter'))
            m.beam((cx-w/2+.22,y,z),(cx+w/2-.22,y,z),.18,name=m.id('int_roof_tie'))
            m.beam((cx,y,z),(cx,y+rise,z),.13,name=m.id('int_roof_kingpost'))
    holes=[]
    if main and p['dormer_count']:
        for k in range(p['dormer_count']):
            z=cz+(k-(p['dormer_count']-1)/2)*2.25;x=cx+ww*.29
            yy=top+roof_height(x-cx,z-cz,ww+2*p['eave_depth_m'],dd+2*p['eave_depth_m'],p['roof_height_m'],'gable',p['eave_curve'])-.22
            holes.append(rectangular_volume(x,z,1.25,1.15,yy,top+p['roof_height_m']+1))
    rect_roof(m,cx,cz,ww,dd,top,p,dormer_holes=holes)
    if main:
        for k in range(p['dormer_count']):
            z=cz+(k-(p['dormer_count']-1)/2)*2.25;x=cx+ww*.29
            yy=top+roof_height(x-cx,z-cz,ww+2*p['eave_depth_m'],dd+2*p['eave_depth_m'],p['roof_height_m'],'gable',p['eave_curve'])-.22
            wall_bay(m,(x+.64,0,z-.59),(x+.64,0,z+.59),yy,1.35,p,'window',False)
            for zz in [z-.59,z+.59]:m.box((x,yy+.55,zz),(1.28,1.1,.17),'plaster')
            pp=dict(p,roof_height_m=.65,eave_depth_m=.12,eave_curve=.05,roof_shape='gable',scale=1.0)
            temp=Mesh({'parameters':pp,'seeds':{'layout':42,'decoration':m.seed}})
            rect_roof(temp,0,0,1.18,1.28,yy+1.35,pp)
            for face,mat,name in zip(temp.faces,temp.materials,temp.names):
                m.face([(x+temp.vertices[j][2],temp.vertices[j][1],z-temp.vertices[j][0]) for j in face],mat,'dormer_%d_'%k+name)
            m.slab([(x+.64,yy+1.35,z-.59),(x+.64,yy+1.35,z+.59),(x+.64,yy+1.96,z)],.16,'plaster',direction=(-1,0,0))
            m.features['dormers']+=1
        for k in range(p['chimney_count']):
            x=cx-ww*.23;z=cz+(k-(p['chimney_count']-1)/2)*1.65
            base=top+roof_height(x-cx,z-cz,ww+2*p['eave_depth_m'],dd+2*p['eave_depth_m'],p['roof_height_m'],p['roof_shape'],p['eave_curve'])-.2
            chimney(m,x,z,base,1.4)
    m.clippers=clips

def chimney(m,x,z,y,h):
    m.box((x,y+h/2,z),(.48,h,.52),'plaster',m.id('chimney'))
    for j in range(3):m.box((x,y+h-.3+j*.14,z),(.59,.11,.63),'tile')
    for dx in [-.13,.13]:m.tube((x+dx,y+h+.18,z),.11,.33,'tile',10,inner=.065)
    m.features['chimneys']+=1

def round_body(m,cx,cz,r,bottom,p,extra=0,others=None,main=True):
    clips=m.clippers[:];m.clippers=others or [];fh=p['floor_height_m'];height=p['floor_count']*fh+extra
    # Faceted structural bays taper by floor; floor heights remain horizontal.
    segments=16
    for j in range(p['floor_count']):
        y=bottom+j*fh;hh=fh+(extra if j==p['floor_count']-1 else 0)
        r0=r*(1-p['tower_taper']*j/p['floor_count']);r1=r*(1-p['tower_taper']*(j+1)/p['floor_count'])
        for i in range(segments):
            a=-math.pi/2-math.pi/segments+2*math.pi*i/segments;b=a+2*math.pi/segments
            aa=(cx+r0*math.cos(a),0,cz+r0*math.sin(a));bb=(cx+r0*math.cos(b),0,cz+r0*math.sin(b))
            start=len(m.vertices);old_lookup=m.lookup
            # Local bay geometry transformed analytically to a continuously tapering shell.
            local=Mesh({'parameters':dict(p,scale=1.0),'seeds':{'decoration':m.seed,'layout':42}})
            opening='door' if main and i==0 and (j==0 or (j==1 and p['balcony_layout']=='ring')) else ('window' if i%p['round_window_step']==2 else None)
            slot=('main' if main else 'tower')+'.ring.'+str(j)+'.'+str(i)
            blocked=(not main and i in (9,10,11,12,13,14,15)) or (p['utility_type']=='windmill' and i in (0,1,15))
            module=slot_choice(m,p,slot,opening,j,4,i,math.sqrt(dot(sub(bb,aa),sub(bb,aa))),blocked,True)
            facade_module(local,aa,bb,y,hh,p,opening,module,slot,False)
            for face,mat,name in zip(local.faces,local.materials,local.names):
                poly=[]
                for idx in face:
                    x,yy,z=local.vertices[idx];t=max(0,min(1,(yy-y)/hh));ratio=1+(r1/r0-1)*t
                    poly.append((cx+(x-cx)*ratio,yy,cz+(z-cz)*ratio))
                m.face(poly,mat,'tower_%d_%d_'%(j,i)+name)
            m.features['windows']+=local.features['windows'];m.features['doors']+=local.features['doors']
        if p['interior_enable']:interior_round_level(m,cx,cz,r0,y,j,p,main)
        else:m.tube((cx,y+.06,cz),r0-p['wall_thickness_m'],.12,'wood',24)
        m.tube((cx,y+hh-.04,cz),r1+.08,.19,'wood',24,inner=r1-.13)
    rt=r*(1-p['tower_taper']);top=bottom+height
    if p['interior_enable']:
        for i in range(12):
            a=i*math.tau/12;rim=rt-p['wall_thickness_m']
            m.beam((cx+rim*math.cos(a),top+.08,cz+rim*math.sin(a)),(cx,top+p['roof_height_m']-.20,cz),.12,name=m.id('int_cone_rafter'))
    cone_roof(m,cx,cz,rt,top,p,top_radius=.65 if main and p['cupola_enable'] else .035)
    if main and p['cupola_enable']:
        cy=top+p['roof_height_m']-.10
        m.tube((cx,cy+.26,cz),.65,.58,'teal',16,inner=.49)
        cone_roof(m,cx,cz,.62,cy+.57,dict(p,eave_depth_m=.16),h=.65)
    if main:
        for k in range(p['chimney_count']):chimney(m,cx-rt*.55,cz+rt*.3+k*.65,top+p['roof_height_m']*.40,1.2)
    m.clippers=clips

def balcony(m,p):
    mode=p['balcony_layout'];w=p['width_m'];d=p['depth_m'];depth=p['balcony_depth_m'];y=p['foundation_height_m']+p['floor_height_m']
    if mode=='none':return
    if mode=='ring':
        r=w/2*(1-p['tower_taper']/p['floor_count']);n=32
        for i in range(n):
            a=2*math.pi*i/n;b=2*math.pi*(i+1)/n
            poly=[(rr*math.cos(t),y,rr*math.sin(t)) for rr,t in [(r,a),(r+depth,a),(r+depth,b),(r,b)]]
            m.slab(poly,.18,'wood',m.id('ring_deck'))
            rail(m,((r+depth)*math.cos(a),0,(r+depth)*math.sin(a)),((r+depth)*math.cos(b),0,(r+depth)*math.sin(b)),y)
            if i%4==0:m.beam((r*math.cos(a),y-1.1,r*math.sin(a)),((r+depth-.1)*math.cos(a),y-.15,(r+depth-.1)*math.sin(a)),.18)
    else:
        deck(m,0,-d/2-depth/2,w+.25,depth,y)
        for x in [-w*.4,0,w*.4]:m.beam((x,y-1.1,-d/2),(x,y-.2,-d/2-depth+.1),.20)
        if mode=='wrap':
            deck(m,w/2+depth/2,0,depth,d,y)
            for z in [-d*.4,0,d*.4]:m.beam((w/2,y-1.1,z),(w/2+depth-.1,y-.2,z),.2)

def canopy(m,x,z,y,width,depth):
    # Cloth is a sagged thin shell; visible poles and corner connections.
    steps=12
    for i in range(steps):
        a=-width/2+width*i/steps;b=-width/2+width*(i+1)/steps
        def pt(xx,zz):return(x+xx,y-.25*(1-(2*xx/width)**2)-.30*(zz/depth),z-zz)
        m.slab([pt(a,0),pt(b,0),pt(b,depth),pt(a,depth)],.025,'canvas')
    for xx in [x-width/2,x+width/2]:
        m.beam((xx,y-.3,z-depth),(xx,y-2.4,z-depth),.10)
        m.beam((xx,y,z),(xx,y-.3,z-depth),.08)

def utility(m,p):
    kind=p['utility_type'];base=p['foundation_height_m'];w=p['width_m'];d=p['depth_m']
    if kind=='windmill':
        r=p['rotor_radius_m'];cy=base+p['floor_count']*p['floor_height_m']+p['roof_height_m']*.50
        z=-w/2-max(p['eave_depth_m'],p['balcony_depth_m'])-.65
        back=-w*.22
        m.tube((0,cy,(z+back)/2),.18,back-z,'wood',12,'z');m.tube((0,cy,z-.1),.34,.22,'metal',16,'z',inner=.18)
        for i in range(4):
            angle=math.pi/4+i*math.pi/2;along=(math.cos(angle),math.sin(angle),0);side=(-math.sin(angle),math.cos(angle),0);center=(0,cy,z)
            def pt(rr,ss,zz=0):return add(center,add(mul(along,rr),add(mul(side,ss),(0,0,zz))))
            m.beam(pt(.2,0),pt(r,0),.11)
            poly=[pt(.65,.07),pt(r,.07),pt(r,.85),pt(.65,.23)]
            m.slab(poly,.025,'sail',direction=(0,0,1))
            for a,b in zip(poly,poly[1:]+poly[:1]):m.beam(a,b,.065)
            for t in [.35,.6,.85]:m.beam(pt(r*t,.02,-.025),pt(r*t,.23+.62*(r*t-.65)/(r-.65),-.025),.04)
        m.features['rotor_sails']=4
    if kind=='waterwheel':
        r=p['wheel_radius_m'];cx=w/2+.70;cy=base+r*.78;cz=.15
        for x in [cx-.45,cx+.45]:
            m.tube((x,cy,cz),r,.16,'wood',32,'x',inner=r-.22)
            m.tube((x,cy,cz),r-.12,.035,'metal',32,'x',inner=r-.18)
            for i in range(10):
                a=2*math.pi*i/10;m.beam((x,cy,cz),(x,cy+(r-.16)*math.cos(a),cz+(r-.16)*math.sin(a)),.15)
        for i in range(24):
            a=2*math.pi*i/24
            m.box((cx,cy+r*math.cos(a),cz+r*math.sin(a)),(1.10,.13,.40),'wood',axes=[(1,0,0),(0,math.cos(a),math.sin(a)),(0,-math.sin(a),math.cos(a))])
        m.tube((cx,cy,cz),.20,2.1,'wood',12,'x')
        m.box((cx+.95,(base+cy)/2,cz),(.48,cy-base,.52),'stone')
        m.features['wheel_paddles']=24

class RoofClip(list):
    def __init__(self,planes,bounds):super().__init__(planes);self.bounds=bounds

def gable_envelope(cx,cz,w,d,bottom,top,p,rotate=False):
    # Exact piecewise-linear profile sampled at the same X courses as rect_roof.
    # Solid body and roof overhang are separate: eaves never erase walls to ground.
    ew=w+2*p['eave_depth_m'];ed=d+2*p['eave_depth_m'];nx=max(4,math.ceil(ew/p['tile_size_m']));nx+=nx%2
    volumes=[]
    def transform(planes,bounds):
        if not rotate:
            return RoofClip([(n,k+n[0]*cx+n[2]*cz) for n,k in planes],((bounds[0][0]+cx,bounds[0][1],bounds[0][2]+cz),(bounds[1][0]+cx,bounds[1][1],bounds[1][2]+cz)))
        # local X -> -world Z, local Z -> world X
        return RoofClip([((n[2],n[1],-n[0]),k+n[2]*cx-n[0]*cz) for n,k in planes],((cx+bounds[0][2],bounds[0][1],cz-bounds[1][0]),(cx+bounds[1][2],bounds[1][1],cz-bounds[0][0])))
    body=rectangular_volume(0,0,w,d,bottom,top)
    volumes.append(transform(body,((-w/2,bottom,-d/2),(w/2,top,d/2))))
    for i in range(nx):
        a=-ew/2+ew*i/nx;b=-ew/2+ew*(i+1)/nx
        ya=top+roof_height(a,0,ew,ed,p['roof_height_m'],'gable',p['eave_curve']);yb=top+roof_height(b,0,ew,ed,p['roof_height_m'],'gable',p['eave_curve'])
        slope=(yb-ya)/(b-a)
        planes=[((1,0,0),b),((-1,0,0),-a),((0,0,1),ed/2),((0,0,-1),ed/2),((0,-1,0),-(top-.14)),((-slope,1,0),ya-slope*a)]
        volumes.append(transform(planes,((a,top-.14,-ed/2),(b,max(ya,yb),ed/2))))
    return volumes

def cone_envelope(cx,cz,r,bottom,top,p):
    # Each frustum uses the same height samples and stagger angle as cone_roof.
    result=[RoofClip(round_volume(cx,cz,r,bottom,top,16),((cx-r,bottom,cz-r),(cx+r,top,cz+r)))]
    radius=r+p['eave_depth_m'];tip=.035;h=p['roof_height_m'];steps=max(5,math.ceil(h/p['tile_size_m']));n=32
    for j in range(steps):
        t=j/steps;u=(j+1)/steps
        ra=radius*(1-t)+tip*t;rb=radius*(1-u)+tip*u
        ya=top+h*t+p['eave_curve']*(1-t)**4;yb=top+h*u+p['eave_curve']*(1-u)**4
        # Extend first section to underside to include roof thickness/eave ring.
        low=ya
        if j==0:
            result.append(RoofClip(round_volume(cx,cz,ra,top-.14,ya,32),((cx-ra,top-.14,cz-ra),(cx+ra,ya,cz+ra))))
        planes=[((0,-1,0),-low),((0,1,0),yb)]
        ap=math.cos(math.pi/n);slope=(ra-rb)*ap/(yb-ya)
        for i in range(n):
            a=2*math.pi*(i+.5+(j%2)*.5)/n;nx=math.cos(a);nz=math.sin(a)
            planes.append(((nx,slope,nz),ra*ap+slope*ya+cx*nx+cz*nz))
        result.append(RoofClip(planes,((cx-ra-.2,low,cz-ra-.2),(cx+ra+.2,yb,cz+ra+.2))))
    return result

def generate(config=None):
    cfg=normalize(config);p=cfg['parameters'];m=Mesh(cfg);w=p['width_m'];d=p['depth_m'];base=p['foundation_height_m'];top=base+p['floor_count']*p['floor_height_m']
    foundation(m,p)
    if p['footprint']=='round':round_body(m,0,0,w/2,base,p)
    elif p['footprint']=='l_shape':
        # Orthogonal second wing: local X -> -Z, local Z -> +X.
        ew=w*p['wing_width_ratio'];ed=d*p['wing_depth_ratio'];ex=w/2+ew/2-p['wing_overlap_m'];ez=p['wing_offset_z_m']
        wingtop=base+p['wing_floor_count']*p['floor_height_m'];wingroof=p['roof_height_m']*p['wing_roof_ratio']
        body1=gable_envelope(0,0,w,d,base,top,p)
        wp=dict(p,roof_height_m=wingroof)
        body2=gable_envelope(ex,ez,ed,ew,base,wingtop,wp,rotate=True)
        rectangular_body(m,0,0,w,d,base,p,body2)
        localcfg={'parameters':dict(p,scale=1.0,floor_count=p['wing_floor_count'],roof_height_m=wingroof,dormer_count=0,chimney_count=0),'seeds':cfg['seeds']};wing=Mesh(localcfg)
        rectangular_body(wing,0,0,ed,ew,base,localcfg['parameters'],main=False)
        m.clippers=body1
        for face,mat,name in zip(wing.faces,wing.materials,wing.names):
            poly=[(ex+wing.vertices[j][2],wing.vertices[j][1],ez-wing.vertices[j][0]) for j in face]
            m.face(poly,mat,'wing_'+name)
        m.clippers=[]
        m.features['windows']+=wing.features['windows']
        m.facade_slots.update(wing.facade_slots)
        stone_band(m,ex,ez,ew,ed,0,base)
    elif p['footprint']=='hybrid':
        r=p['tower_radius_m'];tx=w/2+r*p['tower_inset_ratio'];tz=p['tower_offset_z_m']
        tp=dict(p,tower_taper=0.0,roof_height_m=1.5,cupola_enable=False)
        tower=cone_envelope(tx,tz,r,base,top+p['tower_extra_height_m'],tp)
        body=gable_envelope(0,0,w,d,base,top,p) if p['roof_shape']=='gable' else [rectangular_volume(0,0,w,d,base,top,p['roof_height_m'])]
        rectangular_body(m,0,0,w,d,base,p,tower)
        round_body(m,tx,tz,r,base,tp,p['tower_extra_height_m'],body,False)
        stone_band(m,tx,tz,0,0,0,base,round_r=r)
    else:rectangular_body(m,0,0,w,d,base,p)
    # Prevent balcony rails/decks from traversing occupied secondary volumes.
    if p['footprint']=='l_shape':m.clippers=body2
    elif p['footprint']=='hybrid':m.clippers=tower
    balcony(m,p)
    m.clippers=[]
    utility(m,p)
    if p['entrance_canopy']:
        x=door_x(p)
        z=-(w/2 if p['footprint']=='round' else d/2)-.5;y=base+2.38
        rect_roof(m,x,z,1.55,1.05,y,dict(p,roof_shape='gable',roof_height_m=.48,eave_depth_m=.12,eave_curve=.04),shape='gable')
        for dx in [-.63,.63]:m.beam((x+dx,y-.65,z+.45),(x+dx,y-.1,z-.4),.13)
    if p['awning_enable']:canopy(m,-w*.17,-(w/2 if p['footprint']=='round' else d/2)-.12,base+2.5,2.1,1.45)
    if p['workshop_enable']:
        x=w/2+p['workshop_width_m']/2;z=p['workshop_offset_z_m'];y=base+2.5
        deck(m,x,z,p['workshop_width_m'],p['workshop_depth_m'],base,False)
        for dx in [-p['workshop_width_m']/2+.1,p['workshop_width_m']/2-.1]:
            for dz in [-p['workshop_depth_m']/2+.1,p['workshop_depth_m']/2-.1]:m.beam((x+dx,base,z+dz),(x+dx,y,z+dz),.16)
        pp=dict(p,roof_height_m=1.2,eave_depth_m=.20,eave_curve=.05,roof_shape='gable')
        rect_roof(m,x,z,p['workshop_width_m']-.1,p['workshop_depth_m']-.1,y,pp)
    if p['bay_window_enable']:
        x=w/2+.45;z=d*.22;y=base+p['floor_height_m']+.80
        m.box((x,y-.1,z),(.95,.20,1.35),'wood')
        wall_bay(m,(x+.49,0,z-.65),(x+.49,0,z+.65),y-.65,1.9,p,'window',False)
        for zz in [z-.65,z+.65]:m.box((x,y+.3,zz),(.9,1.8,.16),'plaster')
        rect_roof(m,x,z,.95,1.35,y+1.25,dict(p,roof_height_m=.6,eave_depth_m=.15,eave_curve=.05,roof_shape='gable'))
        for zz in [z-.5,z+.5]:m.beam((w/2,y-1,zz),(x+.4,y-.2,zz),.16)
    # Seeded construction pegs are geometry variation; main layout remains reference-controlled.
    rng=random.Random(cfg['seeds']['layout'])
    for i in range(8):
        jitter=rng.uniform(-.025,.025)
        if p['footprint']=='round':
            angle=-math.pi/2+i*math.pi/6+jitter*.1
            radius=w/2*math.cos(math.pi/12)+.025
            x=radius*math.cos(angle);z=radius*math.sin(angle)
        else:x=-w*.42+i*w*.12+jitter;z=-d/2-.12
        m.box((x,base+.10,z),(.05,.05,.06),'metal',m.id('peg'))
    unknown=set(json.loads(p['facade_overrides']))-set(getattr(m,'facade_slots',{}))
    if unknown:raise ValueError('Unknown facade slot IDs: '+str(sorted(unknown)))
    style_details(m,p)
    m.clean()
    return m

def chamfer_box(m,c,size,mat,name=None,axes=None):
    """Analytic 26-face chamfered cuboid; original bounds, planar major faces.
    No subdivision, no normal averaging across separate objects, no corner collapse.
    """
    axes=axes or [(1,0,0),(0,1,0),(0,0,1)];name=name or m.id(mat)
    h=[v/2 for v in size];r=min(m.bevel,min(size)*.12)
    def emit(poly,normal):
        center=tuple(sum(p[j] for p in poly)/len(poly) for j in range(3))
        u=unit(sub(poly[0],center));v=cross(normal,u)
        poly=sorted(poly,key=lambda p:math.atan2(dot(sub(p,center),v),dot(sub(p,center),u)))
        out=[add(c,add(mul(axes[0],p[0]),add(mul(axes[1],p[1]),mul(axes[2],p[2])))) for p in poly]
        if dot(cross(axes[0],axes[1]),axes[2])<0:out.reverse()
        m.face(out,mat,name)
    for axis in range(3):
        a,b=[j for j in range(3) if j!=axis]
        for sg in [-1,1]:
            poly=[]
            for sa,sb in [(-1,-1),(1,-1),(1,1),(-1,1)]:
                p=[0,0,0];p[axis]=sg*h[axis];p[a]=sa*(h[a]-r);p[b]=sb*(h[b]-r);poly.append(p)
            normal=[0,0,0];normal[axis]=sg;emit(poly,normal)
    for a,b in [(0,1),(0,2),(1,2)]:
        k=3-a-b
        for sa in [-1,1]:
            for sb in [-1,1]:
                poly=[]
                for sk,which in [(-1,0),(1,0),(1,1),(-1,1)]:
                    p=[0,0,0];p[a]=sa*(h[a]-(r if which else 0));p[b]=sb*(h[b]-(0 if which else r));p[k]=sk*(h[k]-r);poly.append(p)
                normal=[0,0,0];normal[a]=sa;normal[b]=sb;emit(poly,unit(normal))
    for sx in [-1,1]:
        for sy in [-1,1]:
            for sz in [-1,1]:
                sg=[sx,sy,sz];poly=[]
                for k in range(3):poly.append([sg[j]*(h[j]-(0 if j==k else r)) for j in range(3)])
                emit(poly,unit(sg))

def barrel(m,x,y,z,r=.36,h=.85):
    seg=16 if m.high else 10; rings=5 if m.high else 2
    for i in range(seg):
        a=2*math.pi*(i+.025)/seg;b=2*math.pi*(i+.975)/seg
        for j in range(rings):
            def at(t,ang):return(x+r*(.83+.17*math.sin(math.pi*t))*math.cos(ang),y+h*t,z+r*(.83+.17*math.sin(math.pi*t))*math.sin(ang))
            m.slab([at(j/rings,a),at(j/rings,b),at((j+1)/rings,b),at((j+1)/rings,a)],.018,'wood',m.id('barrel_stave'),direction=(-math.cos((a+b)/2),0,-math.sin((a+b)/2)))
    for t in [.12,.82]:m.tube((x,y+h*t,z),r*(.83+.17*math.sin(math.pi*t))+.018,.075,'metal',seg,inner=r*.76,name=m.id('barrel_hoop'))
    m.tube((x,y+h-.02,z),r*.82,.04,'wood',seg,name=m.id('barrel_lid'))

def crate(m,x,y,z,s=.65):
    for j in range(4):
        for zz in [-s/2,s/2]:m.box((x,y+s*(j+.5)/4,z+zz),(s,s/4-.015,.06),'wood',m.id('crate_plank'))
        for xx in [-s/2,s/2]:m.box((x+xx,y+s*(j+.5)/4,z),(.06,s/4-.015,s),'wood',m.id('crate_plank'))
    m.box((x,y+s,z),(s,.06,s),'wood')
    m.beam((x-s*.42,y+.05,z-s/2-.04),(x+s*.42,y+s-.05,z-s/2-.04),.075,name=m.id('crate_brace'))

def bench(m,x,y,z,width=1.6):
    for i in range(4):m.box((x,y+.85,z+(i-1.5)*.18),(width,.09,.17),'wood',m.id('worktop'))
    for a in [-1,1]:
        for b in [-1,1]:m.box((x+a*(width/2-.13),y+.42,z+b*.24),(.13,.84,.13),'wood',m.id('bench_leg'))
    m.beam((x-width*.4,y+.24,z),(x+width*.4,y+.24,z),.10)

def planter(m,x,y,z):
    m.box((x,y,z),(1.0,.26,.36),'wood',m.id('flowerbox'))
    rng=random.Random(m.seed+int((x+30)*100+y*100))
    for i in range(12):
        a=(x+rng.uniform(-.45,.45),y+.15,z+rng.uniform(-.14,.14));b=add(a,(rng.uniform(-.12,.12),rng.uniform(.12,.38),rng.uniform(-.12,.12)))
        m.beam(a,b,.015,mat='leaf')
        m.slab([b,add(b,(.11,.06,.06)),add(b,(.19,.02,0)),add(b,(.08,-.01,-.06))],.008,'leaf',m.id('leaf'))
        if i%3==0:m.tube(add(b,(0,.02,0)),.06,.03,'flower',6)


def lantern(m,x,y,z):
    m.beam((x,y+.4,z+.45),(x,y+.4,z-.1),.065,mat='metal',name=m.id('lantern_bracket'))
    m.beam((x,y+.4,z-.1),(x,y+.17,z-.1),.035,mat='metal')
    m.box((x,y-.07,z-.1),(.19,.36,.19),'light',m.id('lantern_glass'))
    for dx in [-.105,.105]:
        for dz in [-.105,.105]:m.beam((x+dx,y-.29,z-.1+dz),(x+dx,y+.15,z-.1+dz),.027,mat='metal')
    m.box((x,y-.3,z-.1),(.27,.055,.27),'metal')
    m.slab([(x-.17,y+.14,z-.27),(x+.17,y+.14,z-.27),(x+.17,y+.14,z+.07),(x-.17,y+.14,z+.07)],.05,'metal')

def cliff_stairs(m,p):
    w=p['width_m'];d=p['depth_m'];top=p['foundation_height_m'];slope=p['terrain_slope'];run_step=.30
    n=max(1,math.ceil((top+slope*w/2)/(.18+slope*run_step)));run=n*run_step
    bottom=-slope*(w/2-run);rise=(top-bottom)/n;z=-d/2-1.95
    # Switchback along X terminates exactly on the analytic terrain surface.
    for i in range(n):
        x=-run+(i+.5)*run_step;y=bottom+(i+1)*rise
        m.box((x,y-.07,z),(run_step-.008,.14,1.1),'wood',m.id('cliff_tread'))
    for dz in [-.52,.52]:
        a=(-run,bottom,z+dz);b=(0,top,z+dz)
        m.beam(a,b,.18,name=m.id('cliff_stringer'));m.beam(add(a,(0,1,0)),add(b,(0,1,0)),.10)
        for t in [0,.5,1]:
            q=lerp(a,b,t);m.beam(q,add(q,(0,1,0)),.10)
    deck(m,.3,z+.3,1.2,1.15,top,False)
    for xx in [-.2,.8]:
        ground=-slope*(xx+w/2)
        m.beam((xx,ground,z+.3),(xx,top-.1,z+.3),.18)
    m.features['stair_landing_error_m']=abs(bottom+slope*(-run+w/2))

def context_geometry(m,p):
    w=p['width_m'];d=p['depth_m'];base=p['foundation_height_m'];mode=p['context_mode']
    if mode=='paving':
        # Limited irregular edge plaza, no old perimeter fences or district yard.
        for i in range(math.ceil((w+2.6)/.65)):
            for j in range(4):
                x=-w/2-1.3+(i+.5)*.65;z=-d/2-.35-j*.62
                if (i+j)%7==0 and j==3:continue
                m.box((x,.035,z),(.60,.07,.56),'stone',m.id('paving'))
    elif mode=='water_channel':
        x=w/2+.7;bottom=-.45
        m.box((x,bottom,.3),(2.35,.12,d+4),'rock',m.id('millrace_bed'))
        m.box((x,-.20,.3),(1.80,.035,d+3.8),'water',m.id('water_surface'))
        for side in [-1,1]:
            for j in range(math.ceil((d+4)/.65)):
                z=-(d+4)/2+(j+.5)*.65+.3
                # Inner bank is lower below the wheel to keep paddle clearance.
                height=.42 if side==-1 else .75
                for k in range(2):m.box((x+side*1.16,bottom+(k+.5)*height/2,z),(.38,height/2-.025,.61),'stone',m.id('millrace_bank'))
        for j in range(8):
            z=-d/2-1.1+j*.18;m.beam((x-.5,-.17,z),(x+.2,-.17,z+.09),.014,mat='sail',name=m.id('water_ripple'))
        # Ground deck at entrance elevation; timber posts support it.
        deck(m,0,-d/2-.8,w+.2,1.6,base,False)
        for xx in [-w*.42,w*.42]:
            m.beam((xx,-.1,-d/2-1.4),(xx,base-.1,-d/2-1.4),.20)
        m.features['millrace']=1
    else:
        # Terrain surface exactly follows the footing datum used by foundation().
        # Rock is a bounded faceted wedge, not a runtime terrain sampler.
        x0=-w/2-1.1;x1=w/2+p['tower_radius_m']*2+1.0;z0=-d/2-2.8;z1=d/2+1
        floor=-p['terrain_slope']*(x1+w/2)-3.5;nx=9;nz=7
        def height(x,z):return -p['terrain_slope']*(x+w/2)
        for i in range(nx):
            for j in range(nz):
                xa=x0+(x1-x0)*i/nx;xb=x0+(x1-x0)*(i+1)/nx;za=z0+(z1-z0)*j/nz;zb=z0+(z1-z0)*(j+1)/nz
                q=[(xa,height(xa,za),za),(xb,height(xb,za),za),(xb,height(xb,zb),zb),(xa,height(xa,zb),zb)]
                m.face(list(reversed(q)),'rock',m.id('cliff_surface'))
        # Faceted escarpment with controlled uneven middle silhouette.
        outline=[(x0,z0),(x1,z0),(x1,z1),(x0,z1)]
        rng=random.Random(m.seed)
        for a,b in zip(outline,outline[1:]+outline[:1]):
            for i in range(10):
                x=a[0]+(b[0]-a[0])*i/10;z=a[1]+(b[1]-a[1])*i/10
                xx=a[0]+(b[0]-a[0])*(i+1)/10;zz=a[1]+(b[1]-a[1])*(i+1)/10
                q=[(x,height(x,z),z),(xx,height(xx,zz),zz),(xx+.15,floor,zz+.12),(x-.15,floor,z-.12)]
                m.face([q[0],q[1],q[2]],'rock',m.id('cliff_face'));m.face([q[0],q[2],q[3]],'rock',m.id('cliff_face'))
        # Hybrid turret footing extends down to rock instead of hanging in space.
        if p['footprint']=='hybrid':
            r=p['tower_radius_m'];tx=w/2+r*p['tower_inset_ratio'];tz=p['tower_offset_z_m']
            foot=height(tx+r,tz)
            if foot<0:stone_band(m,tx,tz,0,0,foot,-foot,round_r=r)
        m.features['cliff_context']=1

def style_details(m,p):
    w=p['width_m'];d=p['depth_m'];b=p['foundation_height_m'];f=p['footprint']
    context_geometry(m,p)
    if not p['dressing_enable']:return
    if f=='round':
        radius=w/2;front=-radius;doorx=0
        barrel(m,radius*.62,b,-radius*.85,.30,.75)
        lantern(m,-.85,b+2.0,front-.15)
    else:
        doorx=door_x(p);front=-d/2
        barrel(m,-w*.40,b,front-.5,.30,.78)
        lantern(m,doorx+.8,b+2.05,front-.20)
        if m.high:
            for floor in range(p['floor_count']):
                ext=p['upper_overhang_m'] if floor else 0
                ww=w+2*ext;dd=d+2*ext;row=facade_row(p,0,floor);n=len(row)
                for j in range(n):
                    if row[j]!='W' or (floor==1 and p['balcony_layout']!='none'):continue
                    x=-ww/2+(j+.5)*ww/n
                    planter(m,x,b+floor*p['floor_height_m']+.90,-dd/2-.20)
    if p['workshop_enable']:
        x=w/2+p['workshop_width_m']/2;z=p['workshop_offset_z_m'];bench(m,x,b,z,1.7)
        for j in range(3):m.box((x+(j-1)*.35,b+1.0,z),(.22,.18,.28),'stone',m.id('workshop_supply'))
        crate(m,x-.75,b,z+1.0,.55)
        canopy(m,x,z-p['workshop_depth_m']/2-.05,b+2.3,p['workshop_width_m']-.5,.8)
    if f=='l_shape':
        x=-w*.37;z=front-.35;y=b+p['floor_height_m']-.25
        m.beam((x,y+.5,z+.2),(x,y+.5,z-.6),.10)
        m.box((x,y,z-.6),(.72,.85,.10),'wood',m.id('inn_sign'))
        m.tube((x,y,z-.67),.20,.035,'sail',12,'z',name=m.id('inn_tankard'))
    if m.high and f!='round':
        for x in [-w/2,w/2]:
            for floor in range(p['floor_count']):
                y=b+(floor+1)*p['floor_height_m']-.15
                m.box((x,y,-d/2-.14),(.23,.24,.035),'metal',m.id('frame_plate'))
                for dx in [-.06,.06]:m.tube((x+dx,y,-d/2-.17),.021,.024,'metal',8,'z',name=m.id('frame_peg'))
    m.features['approved_style']='storybook_red_teal'


def geometry_hash(m):
    return hashlib.sha256(json.dumps([m.vertices,m.faces,m.names,m.materials,m.colors,m.uvs],separators=(',',':')).encode()).hexdigest()

def export_obj(m,path):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with io.StringIO() as f:
        f.write('# Storybook shared geometry core '+VERSION+'; Y up, metres\nmtllib '+path.with_suffix('.mtl').name+'\n')
        for v in m.vertices:f.write('v %.7f %.7f %.7f\n'%tuple(v))
        for uv in m.uvs:
            for u,v,_ in uv:f.write('vt %.7f %.7f\n'%(u,v))
        for n in m.normals:f.write('vn %.7f %.7f %.7f\n'%tuple(n))
        offset=1;prev=None
        for i,(face,mat,name) in enumerate(zip(m.faces,m.materials,m.names)):
            f.write('g '+name+'\n')
            if mat!=prev:f.write('usemtl '+mat+'\n');prev=mat
            f.write('f '+' '.join('%d/%d/%d'%(idx+1,offset+j,i+1) for j,idx in enumerate(face))+'\n');offset+=len(face)
        path.write_bytes(f.getvalue().encode('utf8'))
    with path.with_suffix('.mtl').open('w') as f:
        for name,c in m.palette.items():f.write('newmtl %s\nKd %.4f %.4f %.4f\nKa .2 .2 .2\nNs 10\n\n'%((name,)+c))

def write_hou_geometry(m,geo):
    import hou
    geo.clear()
    for name,typ,val in [('name',hou.attribType.Prim,''),('material_family',hou.attribType.Prim,''),('Cd',hou.attribType.Prim,(1.,1.,1.)),('N',hou.attribType.Vertex,(0.,1.,0.)),('uv',hou.attribType.Vertex,(0.,0.,0.))]:geo.addAttrib(typ,name,val)
    pts=[]
    for v in m.vertices:
        pt=geo.createPoint();pt.setPosition(v);pts.append(pt)
    groups={mat:geo.createPrimGroup(mat) for mat in set(m.materials)}
    for i,face in enumerate(m.faces):
        poly=geo.createPolygon()
        # HOM polygon winding is opposite the right-hand OBJ core. Keep N and UVs.
        for j in reversed(range(len(face))):
            vtx=poly.addVertex(pts[face[j]]);vtx.setAttribValue('uv',m.uvs[i][j]);vtx.setAttribValue('N',m.normals[i])
        poly.setAttribValue('name',m.names[i]);poly.setAttribValue('material_family',m.materials[i]);poly.setAttribValue('Cd',m.colors[i]);groups[m.materials[i]].add(poly)
    geo.addAttrib(hou.attribType.Global,'generator_version','')
    geo.addAttrib(hou.attribType.Global,'geometry_sha256','')
    geo.setGlobalAttribValue('generator_version',VERSION)
    geo.setGlobalAttribValue('geometry_sha256',geometry_hash(m))

def proxy_boxes(config):
    p=normalize(config)['parameters'];w=p['width_m'];d=w if p['footprint']=='round' else p['depth_m'];b=p['foundation_height_m'];h=p['floor_count']*p['floor_height_m']
    boxes=[dict(role='main',center=[0,b+h/2,0],size=[w,h,d])]
    if p['footprint']=='l_shape':
        ew=w*p['wing_width_ratio'];ed=d*p['wing_depth_ratio'];hh=p['wing_floor_count']*p['floor_height_m']
        boxes.append(dict(role='wing',center=[w/2+ew/2-p['wing_overlap_m'],b+hh/2,p['wing_offset_z_m']],size=[ew,hh,ed]))
    if p['footprint']=='hybrid':
        r=p['tower_radius_m'];hh=h+p['tower_extra_height_m']
        boxes.append(dict(role='tower',center=[w/2+r*p['tower_inset_ratio'],b+hh/2,p['tower_offset_z_m']],size=[2*r,hh,2*r]))
    if p['workshop_enable']:
        boxes.append(dict(role='workshop',center=[w/2+p['workshop_width_m']/2,b+1.25,p['workshop_offset_z_m']],size=[p['workshop_width_m'],2.5,p['workshop_depth_m']]))
    return boxes

def boxes_from_geometry(geo):
    attr=geo.findPrimAttrib('layout_role')
    if attr is None:raise ValueError('Input needs primitive string layout_role: main, wing, tower, workshop')
    groups={}
    for prim in geo.prims():groups.setdefault(prim.attribValue(attr),[]).extend(tuple(pt.position()) for pt in prim.points())
    boxes=[]
    for role,vs in groups.items():
        unique={tuple(round(x,6) for x in v) for v in vs}
        lo=[min(v[k] for v in vs) for k in range(3)];hi=[max(v[k] for v in vs) for k in range(3)]
        if len(unique)!=8 or any(any(min(abs(v[k]-lo[k]),abs(v[k]-hi[k]))>1e-5 for k in range(3)) for v in vs):raise ValueError(role+': expected one axis-aligned box with 8 corners; no rotated/merged multiple boxes per role')
        boxes.append(dict(role=role,center=[(a+b)/2 for a,b in zip(lo,hi)],size=[b-a for a,b in zip(lo,hi)]))
    return boxes

def resolve_layout(config,boxes):
    p=dict(config['parameters']);seeds=dict(config['seeds']);roles={}
    for box in boxes:
        role=box['role']
        if role not in ('main','wing','tower','workshop') or role in roles:raise ValueError('Unknown/duplicate box role: '+role)
        if len(box['center'])!=3 or len(box['size'])!=3 or not all(math.isfinite(v) for v in box['center']+box['size']) or min(box['size'])<=0:raise ValueError('Invalid box dimensions')
        roles[role]=box
    expected={'main'}
    if p['footprint']=='l_shape':expected.add('wing')
    if p['footprint']=='hybrid':expected.add('tower')
    if p['workshop_enable']:expected.add('workshop')
    if set(roles)!=expected:raise ValueError('Expected roles '+str(sorted(expected))+' for current footprint/workshop settings')
    main=roles['main'];x,y,z=main['center'];w,h,d=main['size'];base=y-h/2
    floors=h/p['floor_height_m']
    if abs(floors-round(floors))>1e-5:raise ValueError('Main height must be an integer multiple of floor_height_m')
    if p['scale']!=1:raise ValueError('Blockout mode requires scale=1; edit box dimensions instead')
    if p['footprint']=='round' and abs(w-d)>1e-5:raise ValueError('Round body needs square XZ proxy; ellipse unsupported')
    p.update(width_m=w,depth_m=d,floor_count=round(floors),foundation_height_m=base)
    for role,b in roles.items():
        if role=='main':continue
        bx,by,bz=b['center'];bw,bh,bd=b['size']
        if abs(by-bh/2-base)>1e-5:raise ValueError(role+' bottom must align with main bottom')
        if role=='wing':
            wf=bh/p['floor_height_m']
            if abs(wf-round(wf))>1e-5:raise ValueError('Wing height must follow floor grid')
            p.update(wing_width_ratio=bw/w,wing_depth_ratio=bd/d,wing_floor_count=round(wf),wing_offset_z_m=bz-z,wing_overlap_m=w/2+bw/2-(bx-x))
        elif role=='tower':
            if abs(bw-bd)>1e-5:raise ValueError('Turret proxy must be square in XZ')
            p.update(tower_radius_m=bw/2,tower_extra_height_m=bh-h,tower_offset_z_m=bz-z,tower_inset_ratio=((bx-x)-w/2)/(bw/2))
        else:
            if abs(bh-2.5)>1e-5 or abs((bx-x)-(w/2+bw/2))>1e-5:raise ValueError('Workshop is a 2.5m high attached open bay, immediately right of main')
            p.update(workshop_width_m=bw,workshop_depth_m=bd,workshop_offset_z_m=bz-z)
    # Recover decimal grid values to avoid 0.19999999999999996 range errors.
    p={k:round(v,6) if type(v) is float else v for k,v in p.items()}
    cfg=normalize({'parameters':p,'seeds':seeds})
    return cfg,(x,0,z)

def generate_from_layout(config,boxes):
    cfg,offset=resolve_layout(config,boxes);m=generate(cfg)
    m.vertices=[add(v,offset) for v in m.vertices]
    return m

def blockout_mesh(config,boxes):
    cfg,offset=resolve_layout(config,boxes);cfg['parameters']['detail_level']='low';m=Mesh(cfg)
    colors={'main':(.65,.70,.73),'wing':(.85,.62,.36),'tower':(.55,.72,.62),'workshop':(.56,.70,.8)}
    for b in boxes:
        start=len(m.faces);m.box(b['center'],b['size'],'plaster',b['role'])
        for i in range(start,len(m.colors)):m.colors[i]=colors[b['role']]
    # Foundation proxy is explicit: massing box bottom is architectural floor datum.
    p=cfg['parameters'];w=p['width_m'];d=p['depth_m'];h=p['foundation_height_m']
    m.box((offset[0],h/2,offset[2]),(w,h,d),'stone','foundation_proxy')
    return m


def _cook():
    import hou
    node=hou.pwd();parent=node.parent();p={}
    for k,s in SCHEMA['parameters'].items():
        parm=parent.parm(k)
        if parm is None:raise hou.NodeError('Missing parameter: '+k)
        value=parm.evalAsString() if s['type']=='string' else parm.eval()
        p[k]=bool(value) if s['type']=='boolean' else value
    cfg={'parameters':p,'seeds':{'layout':parent.evalParm('layout_seed'),'decoration':parent.evalParm('decoration_seed')}}
    offset=(0,0,0)
    if p['layout_input_enable']:
        if not node.inputs() or node.inputs()[0] is None:raise hou.NodeError('Layout input enabled but no geometry input connected')
        cfg,offset=resolve_layout(cfg, boxes_from_geometry(node.inputs()[0].geometry()))
    mesh=generate(cfg)
    if offset!=(0,0,0):mesh.vertices=[add(v,offset) for v in mesh.vertices]
    write_hou_geometry(mesh,node.geometry())

def _source():
    return Path(__file__).read_text(encoding='utf8').rsplit('\n# END_PORTABLE_CORE\n',1)[0]

# END_PORTABLE_CORE

def apply_config(asset_node,config):
    cfg=normalize(config)  # Validate completely before touching any scene parm.
    for k,s in SCHEMA['parameters'].items():
        if asset_node.parm(k) is None:raise ValueError('Asset missing '+k)
    asset_node.setParms(dict(cfg['parameters'],layout_seed=cfg['seeds']['layout'],decoration_seed=cfg['seeds']['decoration']))
    return asset_node

def build(config=None,parent_path='/obj',asset_name='storybook_red'):
    import hou
    cfg=normalize(config);parent=hou.node(parent_path)
    if parent is None:raise ValueError('Parent does not exist: '+parent_path)
    if not asset_name.replace('_','').isalnum():raise ValueError('asset_name must be alphanumeric/underscore')
    # All deletion is limited to our explicitly marked child. Foreign nodes are preserved.
    obj=parent.node(asset_name)
    if obj and obj.userData('generator_owner')!=OWNER:raise ValueError('Name is occupied by a user node: '+obj.path())
    if obj is None:
        obj=parent.createNode('geo',asset_name,run_init_scripts=False);obj.setUserData('generator_owner',OWNER)
    old=obj.node('generator')
    if old:
        if old.userData('generator_owner')!=OWNER:raise ValueError('generator child is not owned by this script')
        old.destroy()
    subnet=obj.createNode('subnet','generator');subnet.setUserData('generator_owner',OWNER)
    templates=hou.ParmTemplateGroup()
    for k,s in SCHEMA['parameters'].items():
        label=k.replace('_',' ').title();kind=s['type'];v=s['default']
        if kind=='string':t=hou.StringParmTemplate(k,label,1,default_value=(v,),menu_items=tuple(s.get('enum',[])),menu_labels=tuple(s.get('enum',[])))
        elif kind=='boolean':t=hou.ToggleParmTemplate(k,label,default_value=v)
        else:
            ctor=hou.IntParmTemplate if kind=='integer' else hou.FloatParmTemplate
            t=ctor(k,label,1,default_value=(v,),min=s['min'],max=s['max'],min_is_strict=True,max_is_strict=True)
        t.setHelp(s.get('effect',''))
        if k in ('width_m','depth_m','floor_count','foundation_height_m','wing_width_ratio','wing_depth_ratio','wing_floor_count','wing_offset_z_m','wing_overlap_m','tower_radius_m','tower_extra_height_m','tower_offset_z_m','tower_inset_ratio','workshop_width_m','workshop_depth_m','workshop_offset_z_m'):
            t.setConditional(hou.parmCondType.DisableWhen,'{ layout_input_enable == 1 }')
        if k.endswith('_window_count'):
            t.setConditional(hou.parmCondType.DisableWhen,'{ window_layout_mode != count } { footprint == round }')
        templates.append(t)
    for k in ['layout_seed','decoration_seed']:templates.append(hou.IntParmTemplate(k,k.replace('_',' ').title(),1,default_value=(42 if k=='layout_seed' else 101,),min=0,max=2147483647,min_is_strict=True,max_is_strict=True))
    subnet.setParmTemplateGroup(templates);apply_config(subnet,cfg)
    sop=subnet.createNode('python','construct_geometry')
    sop.setInput(0,subnet.indirectInputs()[0])
    if sop.parm('python') is None:raise RuntimeError('This Houdini Python SOP has no python code parameter')
    embedded=_source()+'\n_cook()\n';compile(embedded,'<embedded_storybook>','exec');sop.parm('python').set(embedded)
    out=subnet.createNode('null','OUT_BUILDING');out.setInput(0,sop);out.setDisplayFlag(True);out.setRenderFlag(True)
    subnet.setDisplayFlag(True);subnet.setRenderFlag(True);subnet.layoutChildren();obj.layoutChildren()
    if not cfg['parameters']['layout_input_enable']:
        out.cook(force=True)
        if out.errors():raise RuntimeError('\n'.join(out.errors()))
    return subnet

def export_asset(asset_node,output_path):
    import hou
    license_name=str(hou.licenseCategory()).lower()
    if 'apprentice' in license_name or 'education' in license_name:suffix='.hdanc'
    elif 'indie' in license_name:suffix='.hdalc'
    elif license_name.endswith('.commercial') or license_name=='commercial':suffix='.hda'
    else:raise RuntimeError('Unrecognized license category; refusing to guess an asset suffix: '+license_name)
    path=Path(output_path).with_suffix(suffix);path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():raise FileExistsError('Refusing to overwrite existing asset: '+str(path))
    if not hasattr(asset_node,'createDigitalAsset'):raise RuntimeError('createDigitalAsset unavailable')
    interface=asset_node.parmTemplateGroup()
    asset=asset_node.createDigitalAsset(name='quynh::storybook_blockout::5.1',hda_file_name=str(path.resolve()),description='Storybook v5.1 Interior',min_num_inputs=0,max_num_inputs=1)
    asset.type().definition().updateFromNode(asset)
    asset.type().definition().setParmTemplateGroup(interface)
    asset.type().definition().addSection('ParameterSchema.json',json.dumps(SCHEMA))
    return asset,str(path)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--dataset');ap.add_argument('--output-dir',default='output');ap.add_argument('--offline',action='store_true');ap.add_argument('--package',action='store_true');args=ap.parse_args()
    root=Path(__file__).resolve().parent;cfg=json.loads((root/args.dataset).read_text()) if args.dataset else None
    out=Path(args.output_dir);out=out if out.is_absolute() else root/out;out.mkdir(parents=True,exist_ok=True)
    input_boxes=cfg.get('layout_boxes') if cfg else None
    cfg=normalize(cfg);(out/'effective_config.json').write_text(json.dumps(cfg,indent=2))
    if args.offline:
        mesh=generate_from_layout(cfg,input_boxes) if cfg['parameters']['layout_input_enable'] and input_boxes else generate(cfg)
        export_obj(mesh,out/'building.obj')
    else:
        if cfg['parameters']['layout_input_enable']:raise ValueError('Use create_scene.py for live box inputs and HDA packaging')
        node=build(cfg);node.node('OUT_BUILDING').geometry().saveToFile(str(out/'building.bgeo.sc'))
        if args.package:print(export_asset(node,out/'storybook_building.hda')[1])

if __name__=='__main__':main()
