"""Gothic Building 3.0. Assembly planning core for the native SOP generator.
Units metres, Y up, front -Z. No external assets or pip packages required to build.
"""
import math, json, random, hashlib, pathlib, argparse
VERSION='3.3.0-skytown-joinery'
OWNER='quynh_gothic_1'
def num(default,lo,hi,integer=False):
    return dict(type='integer' if integer else 'number',default=default,min=lo,max=hi)
def menu(default,values): return dict(type='string',default=default,enum=values)
SCHEMA={
 'building_type':menu('church',['church','gatehouse','townhouse','tower','residence','public']),
 'plan_layout':menu('aisles',['hall','cross','aisles','cross_aisles']),
 'length_bays':num(5,2,7,True),'bay_length':num(4.,2.8,5.),
 'nave_width':num(8.,3.6,18.),'wall_height':num(9.,3.,14.),
 'wall_thickness':num(.35,.2,.6),'roof_pitch':num(50.,35.,65.),
 'tower_layout':menu('front_pair',['none','front_single','front_pair','front_pair_crossing']),
 'tower_floors':num(3,2,5,True),'tower_floor_height':num(4.,3.,4.8),
 'spire_height':num(6.,0.,10.),'town_floors':num(3,1,4,True),
 'dormer_count':num(2,0,3,True),'detail':num(.6,.2,1.),
 'stone_palette':menu('limestone',['limestone','sandstone','basalt']),
 'gold_accents':dict(type='boolean',default=False),'clock_enable':dict(type='boolean',default=False),
 'global_scale':num(1.,.1,10.),
 'facade_style':menu('tracery_front',['lancet','portal_gallery','tracery_front']),
 'window_pattern':menu('paired',['single','paired','rhythmic']),
 'tower_crown':menu('canopied',['pyramid','canopied','openwork']),
 'gate_composition':menu('fortified',['fortified','urban_asymmetric']),
 'roof_material':menu('slate',['slate','copper','oxblood']),
 'ridge_detail':menu('sparse',['sparse','crest']),
 'porch_depth':num(1.2,0.,2.4),
 'urban_program':menu('dwelling',['dwelling','shop','inn','manor','guildhall','market','townhall','library','bathhouse']),
 'urban_plan':menu('compact',['compact','deep','corner','courtyard']),
 'front_bays':num(3,2,6,True),'floor_height':num(3.1,2.7,4.2),
 'urban_depth':num(7.,4.,18.),'urban_roof':menu('gable',['gable','stepped','hip']),
 'balcony_enable':dict(type='boolean',default=False),'chimney_count':num(1,0,3,True),
 'interior_enable':dict(type='boolean',default=True),
 'interior_furniture':dict(type='boolean',default=True),
 'interior_lighting':dict(type='boolean',default=True)}
URBAN_DEFAULTS={k:SCHEMA[k]['default'] for k in ('urban_program','urban_plan','front_bays','floor_height','urban_depth','urban_roof','balcony_enable','chimney_count')}
LEGACY_DEFAULTS={'facade_style':'lancet','window_pattern':'single','tower_crown':'pyramid','gate_composition':'fortified','roof_material':'slate','ridge_detail':'crest','porch_depth':0.}
BASELINE={'parameters':{k:v['default'] for k,v in SCHEMA.items()},'seeds':{'layout':42,'decoration':101}}
PALETTES={'limestone':(.66,.60,.48),'sandstone':(.63,.47,.30),'basalt':(.14,.17,.19)}

def normalize(config=None):
    c=json.loads(json.dumps(BASELINE if config is None else config))
    p=c.get('parameters',{}); seeds=c.get('seeds',{})
    for k in ('interior_enable','interior_furniture','interior_lighting'):p.setdefault(k,True)
    for k,v in URBAN_DEFAULTS.items():p.setdefault(k,v)
    if set(p)==set(SCHEMA)-set(LEGACY_DEFAULTS):p.update(LEGACY_DEFAULTS)
    if set(p)!=set(SCHEMA): raise ValueError('Parameters must be complete; missing/unknown: '+str(set(p)^set(SCHEMA)))
    for k,s in SCHEMA.items():
        v=p[k]; t=s['type']
        if t=='boolean': good=isinstance(v,bool)
        elif t=='integer': good=type(v) is int
        elif t=='number': good=type(v) in (int,float) and math.isfinite(v)
        else: good=isinstance(v,str) and v in s['enum']
        if not good: raise ValueError('Invalid type/token: '+k)
        if 'min' in s and not s['min']<=v<=s['max']: raise ValueError('Out of range: '+k)
    if set(seeds)!={'layout','decoration'} or any(type(v) is not int or not 0<=v<2147483647 for v in seeds.values()): raise ValueError('seeds must contain layout and decoration integers [0,2147483646]')
    if p['building_type']!='church' and p['plan_layout']!='hall': raise ValueError('Non-church types require plan_layout=hall')
    if p['tower_layout']=='front_pair_crossing' and p['plan_layout'] not in ('cross','cross_aisles'): raise ValueError('Crossing tower requires cross plan')
    if p['building_type']!='church' and p['tower_layout']!='none': raise ValueError('Gatehouse/tower have intrinsic tower; set tower_layout=none')
    if p['clock_enable'] and p['building_type'] not in ('gatehouse','tower','public'): raise ValueError('Clock requires a gate, tower or civic building')
    if p['building_type']=='townhouse' and p['dormer_count']>0:
        headroom=(p['nave_width']*.65/2-.13)*math.tan(math.radians(p['roof_pitch']))
        if headroom<1.75:raise ValueError('Dormers require roof headroom >= 1.75 m; increase nave_width/roof_pitch or set dormer_count=0')
    if p['tower_crown']=='openwork' and p['building_type']=='church' and p['tower_layout']!='none':
        peak=p['wall_height']+p['nave_width']/2*math.tan(math.radians(p['roof_pitch']))
        tops=[p['tower_floors']*p['tower_floor_height']]
        if p['tower_layout']=='front_pair_crossing':tops.append(p['wall_height']+max(2,p['tower_floors']-1)*p['tower_floor_height'])
        if min(tops)<peak+.4:raise ValueError('Openwork crown must start above adjoining roof: increase tower_floors/tower_floor_height or lower roof_pitch')
    if p['gate_composition']=='urban_asymmetric' and p['building_type']!='gatehouse':raise ValueError('urban_asymmetric requires building_type=gatehouse')
    return {'parameters':p,'seeds':seeds}

def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(v):
    l=math.sqrt(sum(x*x for x in v)); return tuple(x/l for x in v) if l>1e-12 else (0,1,0)
class Mesh:
    def __init__(self,p,seed):
        self.vertices=[]; self.faces=[]; self.names=[]; self.materials=[]; self.colors=[]; self.p=p; self.seed=seed; self.features={'openings':0,'towers':0,'dormers':0,'flying_buttresses':0}
    def face(self,pts,name,mat='stone'):
        # Remove duplicate consecutive vertices; triangulation is performed on planar convex faces.
        q=[]
        for v in pts:
            if not q or sum((a-b)**2 for a,b in zip(v,q[-1]))>1e-18:q.append(tuple(v))
        if len(q)>2 and q[0]==q[-1]:q.pop()
        q=clean_polygon(q)
        if len(q)<3:return
        if sum(x*x for x in cross(sub(q[1],q[0]),sub(q[2],q[0])))<1e-18:return
        offset=len(self.vertices);self.vertices.extend(q);self.faces.append(list(range(offset,offset+len(q))));self.names.append(name);self.materials.append(mat)
        stone=PALETTES[self.p['stone_palette']]
        col={'stone':stone,'trim':tuple(min(1,c*1.16) for c in stone),'roof':{'slate':(.16,.22,.28),'copper':(.17,.39,.35),'oxblood':(.37,.12,.10)}[self.p.get('roof_material','slate')],'glass':(.055,.12,.15),'wood':(.21,.11,.055),'metal':(.65,.43,.12),'gold':(.85,.56,.13),'floor':(.45,.40,.33),'interior':stone}[mat]
        jitter=(int(hashlib.sha256((name+str(self.seed)).encode()).hexdigest()[:8],16)%101/100-.5)*.075
        self.colors.append(tuple(max(.02,min(.98,c+jitter)) for c in col))
    def solid(self,poly,vec,name,mat='stone'):
        """Extrude convex planar polygon; orient independently using solid centroid."""
        a=list(poly);b=[tuple(v[i]+vec[i] for i in range(3)) for v in a]; mid=tuple(sum(v[i] for v in a+b)/len(a+b) for i in range(3))
        fs=[a,b]+[[a[i],a[(i+1)%len(a)],b[(i+1)%len(a)],b[i]] for i in range(len(a))]
        for f in fs:
            n=cross(sub(f[1],f[0]),sub(f[2],f[0])); fc=tuple(sum(v[i] for v in f)/len(f) for i in range(3))
            if sum(n[i]*(fc[i]-mid[i]) for i in range(3))<0:f.reverse()
            self.face(f,name,mat)
    def box(self,x,y,z,w,h,d,name,mat='stone'):
        if min(w,h,d)<=1e-7:return
        self.solid([(x-w/2,y,z-d/2),(x+w/2,y,z-d/2),(x+w/2,y,z+d/2),(x-w/2,y,z+d/2)],(0,h,0),name,mat)
    def beam(self,a,b,r,name,mat='trim'):
        direction=unit(sub(b,a));aux=(0,1,0) if abs(direction[1])<.95 else (1,0,0); u=unit(cross(direction,aux));v=cross(direction,u)
        poly=[tuple(a[j]+r*(sx*u[j]+sy*v[j]) for j in range(3)) for sx,sy in [(-1,-1),(1,-1),(1,1),(-1,1)]]
        self.solid(poly,sub(b,a),name,mat)
    def spire(self,x,y,z,w,h,name,mat='roof'):
        if h<=0:return
        b=[(x-w/2,y,z-w/2),(x+w/2,y,z-w/2),(x+w/2,y,z+w/2),(x-w/2,y,z+w/2)]; apex=(x,y+h,z)
        self.face(b,name,mat)
        for i in range(4):self.face([b[i],apex,b[(i+1)%4]],name,mat)

def arch(x,w,spring):
    # Equilateral pointed arch: two circle segments, tangent to vertical jambs.
    return spring+math.sqrt(max(0,w*w-(abs(x)+w/2)**2))

def interior_lamp(m,x,y,z,name):
    if not m.p['interior_lighting']:return
    m.box(x,y+.16,z,.035,.40,.035,name+'_chain','metal')
    m.box(x,y-.10,z,.22,.22,.22,name+'_light_source','glass')
    for yy in (y-.14,y+.12):m.box(x,yy,z,.30,.04,.30,name+'_cap','metal')
    for dx,dz in ((-.12,-.12),(-.12,.12),(.12,-.12),(.12,.12)):
        m.box(x+dx,y-.12,z+dz,.025,.28,.025,name+'_frame','metal')

def interior_room(m,x,z,w,d,y,floor,levels,fh,program='dwelling',name='room',existing_floor=False):
    """Usable floor plates around two stair flights; furnishings stay left of circulation."""
    if not m.p['interior_enable']:return
    t=m.p['wall_thickness'];xl=x-w/2+t;xr=x+w/2-t;zl=z-d/2+t;zr=z+d/2-t
    clearw=xr-xl;cleard=zr-zl
    if min(clearw,cleard)<.8:return
    sw=min(.95,max(.32,(clearw-.20)/3));run=min(fh*.85,max(.45,cleard-sw-.30))
    x0=xr-2*sw-.12;z0=zr-run-sw-.10;z1=zr-.10
    rects=[(xl,xr,zl,zr)] if floor==0 else [(xl,x0,zl,zr),(x0,xr,zl,z0),(x0,xr,z1,zr)]
    if not existing_floor:
        for a,b,c,dz in rects:
            if min(b-a,dz-c)>.005:m.box((a+b)/2,y+.16,(c+dz)/2,b-a,.13,dz-c,name+'_floor','floor')
    if floor<levels-1:
        n=max(8,math.ceil(fh/.36));rise=fh/(2*n);tread=run/n
        for flight in (0,1):
            xx=x0+sw/2 if flight==0 else xr-sw/2
            for k in range(n):
                zz=z0+(k+.5)*tread if flight==0 else z0+run-(k+.5)*tread
                m.box(xx,y+.29+(k+1+flight*n)*rise-.07,zz,sw,.07,tread+.01,name+'_stair_tread','wood')
            a=(xx,y+.29+flight*fh/2,z0 if flight==0 else z0+run)
            b=(xx,y+.29+(flight+1)*fh/2,z0+run if flight==0 else z0)
            for side in (-1,1):
                aa=(a[0]+side*(sw/2-.04),a[1]+.85,a[2]);bb=(b[0]+side*(sw/2-.04),b[1]+.85,b[2])
                m.beam(aa,bb,.03,name+'_stair_handrail','wood')
                for v in (0.,.5,1.):
                    pt=tuple(aa[j]+v*(bb[j]-aa[j]) for j in range(3))
                    m.beam((pt[0],pt[1]-.85,pt[2]),pt,.025,name+'_stair_baluster','wood')
        m.box((x0+xr)/2,y+.29+fh/2-.08,z0+run+sw/2,xr-x0,.08,sw,name+'_midlanding','wood')
    if floor>0:
        for a,b in [((x0,y+.30,z0),(x0,y+1.25,z0)),((x0,y+.30,z1),(x0,y+1.25,z1))]:m.beam(a,b,.03,name+'_void_post','metal')
        m.beam((x0,y+1.25,z0),(x0,y+1.25,z1),.035,name+'_void_guard','wood')
    if m.p['interior_furniture'] and x0-xl>.65:
        fx=(xl+x0)/2;fz=zr-.65;fw=min(1.6,x0-xl-.16)
        if floor>0 and program in ('dwelling','inn','manor'):
            skytown_prop(m,'SM_Bed01',(fx,y+.85,fz-.35),(fw,1.12,1.75),name=name+'_bed')
        elif program=='bathhouse':
            skytown_prop(m,'SM_Bathtub01',(fx,y+.65,fz),(fw,.72,1.4),name=name+'_bath')
        else:
            skytown_prop(m,'SM_Furniture01_Table01',(fx,y+.69,fz),(fw,.80,.70),90,name+'_table')
            skytown_prop(m,'SM_Furniture01_Bench01',(fx,y+.54,fz-.8),(fw,.50,.28),90,name+'_bench')
        sz=zl+.22
        skytown_prop(m,'SM_Furniture01_Shelf01',(fx,y+1.12,sz),(fw,1.66,.36),90,name+'_shelf')
        if program=='library':
            for i in range(max(3,int(fw/.12))):
                xx=fx-fw/2+.08+i*.12
                for yy in (.38,.98,1.58):skytown_prop(m,'SM_Books01_Item01',(xx,y+yy+.18,sz),(.08,.36,.22),name=name+'_book')
    interior_lamp(m,x-w*.18,y+fh-.55,z,name+'_lamp')


def skytown_prop(m,asset,center,size,yaw=0,name='prop'):
    x,y,z=center;w,h,d=size
    tag='SKYTOWN|'+asset+'|'+str(yaw)+'|'+name+'_'+str(len(m.faces))
    m.box(x,y-h/2,z,w,h,d,tag,'wood')


def balcony_bracket(m,x,deck_bottom,reach):
    # Fitted diagonal with square contact faces on a solid bay pier and joist.
    m.box(x,deck_bottom-.20,-reach/2,.22,.20,reach+.10,'joint_balcony_bearing_joist','wood')
    tag='joint_opening_clear_balcony_knee_'+format(x,'.4f')+'_'+format(deck_bottom,'.4f')
    m.solid([(x-.09,deck_bottom-1.05,-.015),(x-.09,deck_bottom-.84,-.015),
             (x-.09,deck_bottom-.20,-reach+.12),(x-.09,deck_bottom-.20,-reach),
             (x-.09,deck_bottom-.40,-reach)],(.18,0,0),tag,'wood')
    m.box(x,deck_bottom-1.12,-.02,.26,.36,.12,'joint_balcony_pier_bearing','wood')
def panel(m,origin,u,normal,width,height,opening,name,door=False,bottom=.45,profile="lancet"):
    """Wall strip mesh around an actual through opening; no Boolean dependency."""
    t=m.p['wall_thickness']; o=origin
    def pt(x,y,depth=0):return tuple(o[i]+u[i]*x+(y if i==1 else 0)+normal[i]*depth for i in range(3))
    def prism(poly,tag,mat='stone',depth=t,offset=0):
        pieces=[poly]
        if door and name.startswith('nave_end') and height>=5.:
            # Carve convex circular opening out of wall strips above the portal.
            cy=height*.76;radius=min(width*.17,height*.15);inside=poly;pieces=[]
            for j in range(32):
                angle=2*math.pi*(j+.5)/32;aa=-math.cos(angle);bb=-math.sin(angle);cc=radius*math.cos(math.pi/32)-bb*cy
                outside=clip(inside,-aa,-bb,-cc)
                if len(outside)>=3:pieces.append(outside)
                inside=clip(inside,aa,bb,cc)
                if not inside:break
        for piece in pieces:m.solid([pt(x,y,offset) for x,y in piece],tuple(-n*depth for n in normal),name+tag,mat)
    ow=opening; sill=0 if door else bottom+height*.16; spring=min(height*.36,2.7) if door else height*(.38 if profile=="broad" else .61)
    # Open arch constrained to the panel envelope.
    ow=min(ow,(height-spring-.18)/.866)
    xs=[-ow/2+i*ow/12 for i in range(13)]; ys=[arch(x,ow,spring) for x in xs]
    prism([(-width/2,0),(-ow/2,0),(-ow/2,height),(-width/2,height)],'_left')
    prism([(ow/2,0),(width/2,0),(width/2,height),(ow/2,height)],'_right')
    if sill>0:prism([(-ow/2,0),(ow/2,0),(ow/2,sill),(-ow/2,sill)],'_sill')
    for i in range(12):
        prism([(xs[i],ys[i]),(xs[i+1],ys[i+1]),(xs[i+1],height),(xs[i],height)],'_archwall_%02d'%i)
    r=.075
    for side in [-1,1]:m.beam(pt(side*ow/2,sill,.05),pt(side*ow/2,spring,.05),r,name+'_jamb','trim')
    for i in range(12):m.beam(pt(xs[i],ys[i],.05),pt(xs[i+1],ys[i+1],.05),r,name+'_archtrim_%02d'%i,'trim')
    if not door:
        # Glazing occupies opening plane, not the wall shell; removable group.
        poly=[pt(-ow/2,sill,-t*.6),pt(ow/2,sill,-t*.6)]+[pt(x,y,-t*.6) for x,y in reversed(list(zip(xs,ys)))]
        m.face(poly,name+'_glazing','glass')
        for xx in ([-ow/6,ow/6] if ow>1. else [0]):m.beam(pt(xx,sill),pt(xx,arch(xx,ow,spring)),.035,name+'_mullion')
        for dx in ([-ow/6,ow/6] if ow>1. else []):
            rr=ow/6
            arc=[(dx+rr*math.cos(a*math.pi/12),spring-.15+rr*math.sin(a*math.pi/12)) for a in range(13)]
            for i in range(12):m.beam(pt(*arc[i]),pt(*arc[i+1]),.025,name+'_tracery')
        if m.p.get('facade_style','lancet')!='lancet' and ow>1.:
            centre=pt(0,spring+ow*.27,.035)
            for a in [math.pi/2,math.pi*7/6,math.pi*11/6]:
                c=tuple(centre[j]+u[j]*math.cos(a)*ow*.1+(math.sin(a)*ow*.1 if j==1 else 0) for j in range(3))
                ring(m,c,u,(0,1,0),ow*.115,.025,name+'_trefoil')
    if door and name.startswith('nave_end') and height>=5.:
        # Circular window shares the real hole cut above; front and rear use same grammar.
        rose(m,o[0],o[1]+height*.76,o[2]-.10,min(width*.17,height*.15),name+'_rose')
        m.features['openings']+=1
    m.features['openings']+=1

def pinnacle(m,x,y,z,h,name):
    m.box(x,y,z,.25,.35,.25,name+'_base','trim');m.spire(x,y+.35,z,.34,h,name,'trim')
def buttress(m,x,z,h,name):
    for j,(w,hh) in enumerate([(.75,.5),(.55,h*.45),(.4,h*.4),(.3,h*.15)]):
        y=0 if j==0 else .5+sum([h*.45,h*.4,h*.15][:j-1]);m.box(x,y,z,w,hh,.75 if j<2 else .55,name+str(j))
    pinnacle(m,x,h+.5,z,.9,name+'_finial')
def course_ring(m,x,y,z,w,d,height,extension,name,mat='trim'):
    """One stepped moulding around all corners, with no abutting end caps."""
    inner=.07;corner=min(.035,w*.025,d*.025)
    def rectangle(rx,rz,yy):
        c=min(corner,rx*.2,rz*.2)
        return [(x+a,yy,z+b) for a,b in [(-rx,-rz+c),(-rx+c,-rz),(rx-c,-rz),(rx,-rz+c),(rx,rz-c),(rx-c,rz),(-rx+c,rz),(-rx,rz-c)]]
    levels=[(0.,.70),(.15,.70),(.30,.25),(.50,.25),(.68,1.),(.86,1.),(1.,.82)]
    outer=[rectangle(w/2+extension*amount,d/2+extension*amount,y+height*f) for f,amount in levels]
    low=rectangle(w/2-inner,d/2-inner,y);high=rectangle(w/2-inner,d/2-inner,y+height)
    def emit(poly,target):
        if sum(a*b for a,b in zip(cross(sub(poly[1],poly[0]),sub(poly[2],poly[0])),target))<0:poly=list(reversed(poly))
        m.face(poly,name+'_continuous_moulding',mat)
    for k in range(len(outer)-1):
        for i in range(8):
            j=(i+1)%8;poly=[outer[k][i],outer[k][j],outer[k+1][j],outer[k+1][i]]
            center=tuple(sum(v[a] for v in poly)/4 for a in range(3));emit(poly,(center[0]-x,0,center[2]-z))
    for i in range(8):
        j=(i+1)%8;center=((low[i][0]+low[j][0])*.5,0,(low[i][2]+low[j][2])*.5)
        emit([low[i],high[i],high[j],low[j]],(x-center[0],0,z-center[2]))
        emit([outer[0][i],low[i],low[j],outer[0][j]],(0,-1,0))
        emit([outer[-1][i],outer[-1][j],high[j],high[i]],(0,1,0))
    m.features['closed_course_rings']=m.features.get('closed_course_rings',0)+1

def ring(m,c,u,v,r,th,name,mat='trim',segments=None):
    segments=segments or globals().get('CURVE_SEGMENTS',64)
    # A single closed tube also survives consumers (notably capped_gable) that
    # transform raw faces rather than preserving the native beam recipes.
    # Independent capped bars leave overlapping end faces at every ring joint.
    up=unit(cross(u,v));profile=[(-1,-.75),(-.75,-1),(.75,-1),(1,-.75),(1,.75),(.75,1),(-.75,1),(-1,.75)]
    miter=1/math.cos(math.pi/segments);rings=[];centers=[]
    for i in range(segments):
        angle=i*math.tau/segments;radial=tuple(u[j]*math.cos(angle)+v[j]*math.sin(angle) for j in range(3))
        center=tuple(c[j]+r*radial[j] for j in range(3));centers.append(center)
        rings.append([tuple(center[j]+a*th*miter*radial[j]+b*th*up[j] for j in range(3)) for a,b in profile])
    for i in range(segments):
        ii=(i+1)%segments;center=tuple((centers[i][j]+centers[ii][j])*.5 for j in range(3))
        for k in range(8):
            kk=(k+1)%8;poly=[rings[i][k],rings[ii][k],rings[ii][kk],rings[i][kk]]
            midpoint=tuple(sum(p[j] for p in poly)/4 for j in range(3));outward=sub(midpoint,center)
            if sum(a*b for a,b in zip(cross(sub(poly[1],poly[0]),sub(poly[2],poly[0])),outward))<0:poly.reverse()
            m.face(poly,name,mat)
def rose(m,x,y,z,r,name,clock=False):
    ring(m,(x,y,z),(1,0,0),(0,1,0),r,.09,name+'_rim','metal' if clock else 'trim')
    # Mounted clock face; rose facade is intentionally recessed behind tracery (documented).
    poly=[(x+r*.94*math.cos(i*math.pi/24),y+r*.94*math.sin(i*math.pi/24),z+.08) for i in range(48)]
    m.face(poly,name+'_disc','glass')
    for i in range(12 if clock else 8):
        a=i*2*math.pi/(12 if clock else 8)
        rr=r*.8 if clock else r*.22
        m.beam((x+rr*math.cos(a),y+rr*math.sin(a),z-.03),(x+r*.94*math.cos(a),y+r*.94*math.sin(a),z-.03),.035,name+'_spoke','metal' if clock else 'trim')
    if clock:
        m.beam((x,y,z-.08),(x,y+r*.65,z-.08),.05,name+'_minute','metal');m.beam((x,y,z-.09),(x+r*.45,y+r*.22,z-.09),.06,name+'_hour','metal')
    else:ring(m,(x,y,z-.03),(1,0,0),(0,1,0),r*.24,.04,name+'_centre')

def clip(poly,a,b,c):
    """Keep a*x+b*z+c >= 0."""
    out=[]
    for i,P in enumerate(poly):
        Q=poly[(i+1)%len(poly)];f=a*P[0]+b*P[1]+c;g=a*Q[0]+b*Q[1]+c
        if f>=-1e-9:out.append(P)
        if (f>1e-9 and g<-1e-9) or (f<-1e-9 and g>1e-9):
            t=f/(f-g);out.append((P[0]+t*(Q[0]-P[0]),P[1]+t*(Q[1]-P[1])))
    return out

def roof(m,x,z,w,d,y,pitch,name,axis='z',other=None):
    slope=math.tan(math.radians(pitch)); half=(w if axis=='z' else d)/2; peak=y+half*slope
    for s in [-1,1]:
        if axis=='z':poly=[(x if s>0 else x-w/2,z-d/2),(x+w/2 if s>0 else x,z-d/2),(x+w/2 if s>0 else x,z+d/2),(x if s>0 else x-w/2,z+d/2)];A=-s*slope;B=0;C=peak-A*x
        else:poly=[(x-w/2,z if s>0 else z-d/2),(x+w/2,z if s>0 else z-d/2),(x+w/2,z+d/2 if s>0 else z),(x-w/2,z+d/2 if s>0 else z)];A=0;B=-s*slope;C=peak-B*z
        pieces=[poly]
        if other:
            # Main and perpendicular roof have equal ridge height. In overlap retain only upper envelope.
            ox,oz,ow,od,oy,op,oa=other; os=math.tan(math.radians(op));pk=oy+(ow if oa=='z' else od)/2*os
            inside=poly;outside=[]
            for aa,bb,cc in [(1,0,-ox+ow/2),(-1,0,ox+ow/2),(0,1,-oz+od/2),(0,-1,oz+od/2)]:
                if not inside:break
                cut=clip(inside,-aa,-bb,-cc)
                if len(cut)>=3:outside.append(cut)
                inside=clip(inside,aa,bb,cc)
            pieces=outside
            if len(inside)>=3:
                for ss in [-1,1]:
                    q=clip(inside,ss if oa=='z' else 0,ss if oa=='x' else 0,-ss*(ox if oa=='z' else oz))
                    oaA=-ss*os if oa=='z' else 0;oaB=-ss*os if oa=='x' else 0;oaC=pk-oaA*ox-oaB*oz
                    q=clip(q,A-oaA,B-oaB,C-oaC)
                    if len(q)>=3:pieces.append(q)
        for i,q in enumerate(pieces):m.solid([(xx,A*xx+B*zz+C,zz) for xx,zz in q],(0,-.18,0),name+'_s%d_%d'%(s,i),'roof')
    if axis=='z':m.beam((x,peak+.04,z-d/2),(x,peak+.04,z+d/2),.08,name+'_ridge')
    else:m.beam((x-w/2,peak+.04,z),(x+w/2,peak+.04,z),.08,name+'_ridge')
    return peak

def gable(m,x,z,w,y,peak,name,rose_hole=True):
    cy=y+min(1.3,(peak-y)*.45);r=.68
    triangle=[(x-w/2,y),(x+w/2,y),(x,peak)]
    if rose_hole and cy-r>y and cy+r<peak:
        levels=[y]+[cy-r+2*r*i/24 for i in range(25)]+[peak]
        for j in range(len(levels)-1):
            lo,hi=levels[j:j+2];q=clip(clip(triangle,0,1,-lo),0,-1,hi)
            if len(q)<3:continue
            if lo>=cy-r-1e-8 and hi<=cy+r+1e-8:
                rl=math.sqrt(max(0,r*r-(lo-cy)**2));rh=math.sqrt(max(0,r*r-(hi-cy)**2))
                for sign in [-1,1]:
                    # x outside the linear circle chord at this y band.
                    slope=sign*(rh-rl)/(hi-lo);inter=x+sign*rl-slope*lo
                    qq=clip(q,sign,-sign*slope,-sign*inter)
                    if len(qq)>=3:m.solid([(xx,yy,z) for xx,yy in qq],(0,0,.3),name+'_ringwall')
            else:m.solid([(xx,yy,z) for xx,yy in q],(0,0,.3),name+'_wall')
        m.features['openings']+=1
    else:m.solid([(xx,yy,z) for xx,yy in triangle],(0,0,.3),name)
    for sign in [-1,1]:m.beam((x+sign*w/2,y,z-.05),(x,peak+.07,z-.05),.105,name+'_rake')
    pinnacle(m,x,peak,z,.7,name+'_cross')

def tower(m,x,z,width,floors,fh,spire,name,base=0,passage=False,clock=False,window_floor=None,obstruction_height=0):
    m.features['towers']+=1; t=m.p['wall_thickness'];h=floors*fh
    if not passage:m.box(x,base,z,width+.3,.35,width+.3,name+'_plinth')
    for f in range(floors):
        y=base+f*fh
        if not passage or f>0:interior_room(m,x,z,width,width,y,f,floors,fh,'workshop',name+'_interior')
        for side in range(4):
            a=side*math.pi/2;n=(math.sin(a),0,-math.cos(a));u=(math.cos(a),0,math.sin(a));origin=(x+n[0]*width/2,y,z+n[2]*width/2)
            isdoor=(passage and f==0 and side in [0,2]) or (window_floor is not None and base==0 and f==0 and side==0)
            if clock and f==floors-1 and side==0:
                m.box(x,y,z-width/2+t/2,width,fh,t,name+'_clock_wall')
            elif window_floor is not None and (y < window_floor or (y < obstruction_height and (side==2 or (side==1 if x<0 else side==3)))):
                # Solid bearing drum: no glazing can be sliced by an adjoining roof.
                m.solid([tuple(origin[j]+u[j]*xx+(yy if j==1 else 0) for j in range(3)) for xx,yy in [(-width/2,0),(width/2,0),(width/2,fh),(-width/2,fh)]],tuple(-q*t for q in n),name+'_bearing_wall')
            else:
                aperture=width*(.53 if isdoor or window_floor is None or f==floors-1 else .17)
                tag=name+'_f%d_s%d'%(f,side)
                if f==floors-1 and m.p['facade_style']!='lancet' and not isdoor and width>2.3:
                    for j in [-1,1]:
                        org=tuple(origin[k]+u[k]*j*width*.25 for k in range(3))
                        panel(m,org,u,n,width*.5,fh,width*.24,tag+'_belfry_'+str(j))
                else:panel(m,origin,u,n,width,fh,aperture,tag,isdoor)
                if f==floors-1 and m.p['facade_style']!='lancet':
                    for j in [-1,1]:
                        a=tuple(origin[k]+u[k]*j*width*.43+n[k]*.1 for k in range(3));b=tuple(a[k]+(fh-.2 if k==1 else 0) for k in range(3));m.beam(a,b,.09,tag+'_colonette')
        for sx in [-1,1]:
            for sz in [-1,1]:m.box(x+sx*(width/2-.12),y,z+sz*(width/2-.12),.38,fh,.38,name+'_corner')
        # Rim frame, avoiding a slab across the passage.
        course_ring(m,x,y+fh-.15,z,width,width,.22,.14,name+'_band')
    if m.p['facade_style']!='lancet':
        # Engaged stepped piers give the lower tower its own structural scale.
        for sx in [-1,1]:
            for sz in [-1,1]:
                for k,(fraction,thick) in enumerate([(0.,.52),(.30,.40),(.54,.27)]):
                    length=h*(.30 if k==0 else .24 if k==1 else .18)
                    m.box(x+sx*(width/2+.04),base+h*fraction,z+sz*(width/2+.04),thick,length,thick,name+'_stepped_buttress')
                pinnacle(m,x+sx*(width/2+.04),base+h*.72,z+sz*(width/2+.04),.55,name+'_buttress_terminal')
        yy=base+(floors-1)*fh
        for sign in [-1,1]:
            m.box(x,yy-.12,z+sign*(width/2+.06),width+.38,.16,.32,name+'_belfry_sill','trim')
            m.box(x+sign*(width/2+.06),yy-.12,z,.32,.16,width+.38,name+'_belfry_sill','trim')
    if clock:rose(m,x,base+h-fh*.55,z-width/2-.20,width*.34,name+'_clock',True)
    if spire:
        crown(m,x,base+h,z,width+.12,spire,name+'_crown',m.p.get('tower_crown','pyramid'))
        for sx in [-1,1]:
            for sz in [-1,1]:pinnacle(m,x+sx*width*.46,base+h,z+sz*width*.46,1.,name+'_turret')
        m.beam((x,base+h+spire,z),(x,base+h+spire+.6,z),.035,name+'_tip','metal')
    else:
        for s in [-1,1]:
            for i in range(5):
                a=-width/2+i*width/4
                m.box(x+a,base+h,z+s*width/2,.35,.5,.4,name+'_crenel')
                m.box(x+s*width/2,base+h,z+a,.4,.5,.35,name+'_crenel')

# --- Architectural assembly solver, version 1.1 ---
def plane_value(plane,point):return sum(a*b for a,b in zip(plane[:3],point))+plane[3]
def clip3(poly,plane):
    out=[]
    for i,a in enumerate(poly):
        b=poly[(i+1)%len(poly)];da=plane_value(plane,a);db=plane_value(plane,b)
        if da>=-1e-9:out.append(a)
        if (da>1e-9 and db<-1e-9) or (da<-1e-9 and db>1e-9):
            f=da/(da-db);out.append(tuple(a[j]+f*(b[j]-a[j]) for j in range(3)))
    return clean_polygon(out)
def clean_polygon(poly):
    q=[]
    for v in poly:
        if not q or sum((a-b)**2 for a,b in zip(v,q[-1]))>1e-16:q.append(tuple(v))
    if len(q)>2 and sum((a-b)**2 for a,b in zip(q[0],q[-1]))<1e-16:q.pop()
    while len(q)>=3:
        removed=False
        for i in range(len(q)):
            a,b,c=q[i-1],q[i],q[(i+1)%len(q)]
            if sum(v*v for v in cross(sub(b,a),sub(c,b)))<1e-18:
                q.pop(i);removed=True;break
        if not removed:break
    return q if len(q)>=3 else []
def box_volume(x0,x1,y0,y1,z0,z1,label):
    return dict(label=label,bounds=((x0,y0,z0),(x1,y1,z1)),planes=[(1,0,0,-x0),(-1,0,0,x1),(0,1,0,-y0),(0,-1,0,y1),(0,0,1,-z0),(0,0,-1,z1)])
def gable_volume(x0,x1,z0,z1,eave,peak,axis,label):
    v=box_volume(x0,x1,-.01,peak,z0,z1,label)
    center=(x0+x1)/2 if axis=='z' else (z0+z1)/2;half=(x1-x0)/2 if axis=='z' else (z1-z0)/2;slope=(peak-eave)/half
    for s in [-1,1]:v['planes'].append((-s*slope if axis=='z' else 0,-1,-s*slope if axis=='x' else 0,peak+s*slope*center))
    return v

def roof_slab_volumes(x0,x1,z0,z1,eave,peak,axis,label):
    result=[];center=(x0+x1)/2 if axis=='z' else (z0+z1)/2;half=(x1-x0)/2 if axis=='z' else (z1-z0)/2;slope=(peak-eave)/half
    for sign in [-1,1]:
        a=-sign*slope if axis=='z' else 0;b=-sign*slope if axis=='x' else 0;c=peak-(a*center if axis=='z' else b*center)
        v=box_volume(x0,x1,eave-.18,peak,z0,z1,label+'_slab_'+str(sign))
        v['planes'] += [(sign if axis=='z' else 0,0,sign if axis=='x' else 0,-sign*center),(a,-1,b,c),(-a,1,-b,-c+.18)]
        result.append(v)
    return result

def subtract_volume(poly,volume):
    low,high=volume['bounds']
    if any(max(p[j] for p in poly)<=low[j]+1e-8 or min(p[j] for p in poly)>=high[j]-1e-8 for j in range(3)):return [poly]
    # A coplanar boundary is retained. Exterior surfaces only are assembled;
    # this is not an arbitrary closed-solid Boolean or interior manifold solver.
    for plane in volume['planes']:
        ds=[plane_value(plane,p) for p in poly]
        if max(ds)<=1e-8:return [poly]
    remaining=poly;outside=[]
    for plane in volume['planes']:
        if not remaining:break
        ds=[plane_value(plane,p) for p in remaining]
        if min(ds)>=-1e-9:continue
        if max(ds)<=1e-9:outside.append(remaining);remaining=[];break
        fragment=clip3(remaining,tuple(-v for v in plane))
        if fragment:outside.append(fragment)
        remaining=clip3(remaining,plane)
    return outside

def trim_mesh(source,volumes,label):
    result=Mesh(source.p,source.seed);removed=clipped=0
    for i,face in enumerate(source.faces):
        original=[source.vertices[j] for j in face];pieces=[original]
        for vol in volumes:
            pieces=[part for poly in pieces for part in subtract_volume(poly,vol)]
            if not pieces:break
        if not pieces:removed+=1
        elif pieces!=[original]:clipped+=1
        for piece in pieces:result.face(piece,source.names[i],source.materials[i]);result.colors[-1]=source.colors[i]
    result.features=source.features.copy()
    result.trim_stats=dict(module=label,faces_removed=removed,faces_clipped=clipped,cutters=[v['label'] for v in volumes])
    return result

def merge_mesh(dst,src,prefix=''):
    for i,face in enumerate(src.faces):
        dst.face([src.vertices[j] for j in face],prefix+src.names[i],src.materials[i]);dst.colors[-1]=src.colors[i]
    for key,val in src.features.items():dst.features[key]=dst.features.get(key,0)+val

def assemble(m,components):
    # Every module supplies its occupied architectural envelope. Both sides of
    # an interface are clipped, preventing hidden roofs/windows inside towers.
    m.assembly_checks=[]
    for label,mesh,own in components:
        others=[v for name,_,vols in components if name!=label for v in vols]
        cut=trim_mesh(mesh,others,label);merge_mesh(m,cut,label+'/')
        m.assembly_checks.append(cut.trim_stats)
    m.assembly_volumes={label:vols for label,_,vols in components}

def tower_volumes(x,z,w,base,floors,fh,spire,label,crown_style='pyramid'):
    top=base+floors*fh;v=[box_volume(x-w/2,x+w/2,base,top,z-w/2,z+w/2,label+'_body')]
    if spire>0 and crown_style=='openwork':
        v.extend(spire_envelopes(x,top,z,w+.12,spire,8,label))
    elif spire>0:
        roofv=box_volume(x-w/2,x+w/2,top,top+spire,z-w/2,z+w/2,label+'_spire');k=spire/(w/2)
        roofv['planes'] += [(k,-1,0,top+spire-k*x),(-k,-1,0,top+spire+k*x),(0,-1,k,top+spire-k*z),(0,-1,-k,top+spire+k*z)]
        v.append(roofv)
    return v

def capped_gable(m,x,z,w,y,peak,name,normal=-1,rose_hole=True):
    temp=Mesh(m.p,m.seed);gable(temp,0,0,w,y-.18,peak-.18,name,rose_hole)
    if rose_hole:rose(temp,0,y+min(1.3,(peak-y)*.45),-.08,.68,name+'_rose')
    # Original gable extrudes in +Z, so reverse for rear facade.
    for i,f in enumerate(temp.faces):
        pts=[(x+temp.vertices[j][0],temp.vertices[j][1],z-normal*temp.vertices[j][2]) for j in f]
        if normal>0:pts.reverse()
        m.face(pts,temp.names[i],temp.materials[i])
    for key,val in temp.features.items():m.features[key]=m.features.get(key,0)+val

def church(m,p):
    w=p['nave_width'];b=p['bay_length'];d=p['length_bays']*b;h=p['wall_height'];t=p['wall_thickness'];slope=math.tan(math.radians(p['roof_pitch']));over=.22;peak=h+w/2*slope
    aisles=p['plan_layout'] in ('aisles','cross_aisles');crossplan=p['plan_layout'] in ('cross','cross_aisles');aw=b*.65;ah=max(2.6,h*.55);attach=max(ah+.4,h*.72);cw=min(w,d*.45);cz=d-cw/2-max(b,d*.16)
    pieces=[];n=Mesh(p,m.seed)
    # Functional layout is resolved before facades. Cross arms share bearing
    # height; narrower transepts retain a lower ridge instead of taller walls.
    cross_lo,cross_hi=cz-cw/2,cz+cw/2
    def spans(lo,hi,exclusions=()):
        chunks=[(lo,hi)]
        for a,z in exclusions:
            chunks=[q for x,y in chunks for q in [(x,min(y,a)),(max(x,z),y)] if q[1]-q[0]>.01]
        return chunks
    def strip(mesh,x,lo,hi,y,height,sign,label,exclusions=()):
        for k,(a,z) in enumerate(spans(lo,hi,exclusions)):
            span=z-a
            if span<1.4:
                mesh.box(x-sign*t/2,y,(a+z)/2,t,height,span,label+'_solid')
            else:
                rhythm=p['window_pattern'];index=int(label.split('_')[-1]) if label.split('_')[-1].isdigit() else 0
                split=2 if span>=2.8 and height>=2.4 and (rhythm=='paired' or rhythm=='rhythmic' and index%3==1) else 1
                for j in range(split):
                    panel(mesh,(x,y,a+(j+.5)*span/split),(0,0,-sign),(sign,0,0),span/split,height,min(b*.43,span/split*.47),label+'_%d_%d'%(k,j),bottom=.12)
                if p['facade_style']!='lancet':
                    mesh.box(x,y+height*.12,(a+z)/2,.16,.12,span-.1,label+'_sill_course','trim')
    # The clerestory wall starts at its lean-to roof attachment, not behind it.
    for sign in [-1,1]:
        low=attach if aisles else 0
        for i in range(p['length_bays']):
            exclusions=[(cross_lo,cross_hi)] if crossplan else []
            # Single tower stands beside the front bay, leaving the west portal clear.
            if p['tower_layout']=='front_single' and sign==-1:exclusions.append((0,w*.36))
            strip(n,sign*w/2,i*b,(i+1)*b,low,h-low,sign,'nave_window_%d_%d'%(sign,i),exclusions)
        for i in range(p['length_bays']+1):
            if crossplan and cross_lo-.4<i*b<cross_hi+.4:continue
            if aisles:
                n.box(sign*w/2,attach,i*b,.32,h-attach,.44,'clerestory_pier');pinnacle(n,sign*w/2,h,i*b,.6,'clerestory_pinnacle')
            else:buttress(n,sign*(w/2+.16),i*b,h-.5,'nave_pier')
        if p['tower_layout']=='front_single' and sign==-1 and not aisles:
            n.box(sign*(w/2-t/2),0,w*.18,t,h,w*.36,'tower_interface_infill')
        if crossplan:
            n.box(sign*(w/2-t/2),0,cz,t,h,cw,'crossing_interface_infill')
    course_ring(n,0,h-.23,d/2,w,d,.23,.23,'eave_cornice')
    for z,normal in [(0,-1),(d,1)]:
        if normal==-1 and p['facade_style']!='lancet' and h>=6:west_front(n,w,h,p['facade_style'])
        else:panel(n,(0,0,z),(-normal,0,0),(0,0,normal),w,h,2.2 if normal==-1 else w*.4,'nave_end_'+str(normal),normal==-1)
        capped_gable(n,0,z,w,h,peak,'nave_gable',normal)
    # Eave height derived from bearing height: roof(x=wall) == h.
    roof(n,0,d/2,w+2*over,d+2*over,h-over*slope,p['roof_pitch'],'nave_roof')
    if p['porch_depth']>0 and h>=6:
        entry_porch(n,p['porch_depth'],min(w*.30,2.8))
    count=int(d*(.3+p['detail']*.6)) if p['ridge_detail']=='crest' else max(2,p['length_bays']//2)
    for i in range(count):pinnacle(n,0,peak+.07,(i+.5)*d/count,.3+p['detail']*.22,'ridge_crest')
    nv=gable_volume(-w/2,w/2,0,d,h,peak,'z','nave_envelope')
    pieces.append(('nave',n,[nv]+roof_slab_volumes(-w/2-over,w/2+over,-over,d+over,h-over*slope,peak,'z','nave')))
    if aisles:
        for sign in [-1,1]:
            a=Mesh(p,m.seed);outer=sign*(w/2+aw);inner=sign*w/2
            for i in range(p['length_bays']):
                exclusions=[(cross_lo,cross_hi)] if crossplan else []
                if p['tower_layout']=='front_single' and sign==-1:exclusions.append((0,w*.36))
                strip(a,outer,i*b,(i+1)*b,0,ah,sign,'window_%d'%i,exclusions)
            # Single continuous lean-to plane, not independent roof slabs per bay.
            outer_eave=sign*(w/2+aw+.15);outer_y=ah-.15*(attach-ah)/aw
            a.solid([(inner,attach,0),(outer_eave,outer_y,0),(outer_eave,outer_y,d),(inner,attach,d)],(0,-.18,0),'lean_roof','roof')
            for z,normal in [(0,-1),(d,1)]:
                if normal==-1 and p['tower_layout'] in ('front_pair','front_pair_crossing'):
                    a.box(sign*(w/2+aw/2),0,t/2,aw,ah,t,'west_tower_interface')
                else:panel(a,(sign*(w/2+aw/2),0,z),(1,0,0),(0,0,normal),aw,ah,aw*.44,'end_window')
                a.solid([(inner,ah-.18,z),(outer,ah-.18,z),(inner,attach-.18,z)],(0,0,-normal*t),'lean_gable')
            for i in range(p['length_bays']+1):
                z=i*b
                if crossplan and cross_lo-.4<z<cross_hi+.4:continue
                if p['tower_layout']=='front_single' and sign==-1 and z<w*.36+.4:continue
                buttress(a,outer+sign*.18,z,ah+.45,'outer_pier')
                # Curved flying support stays above the lean-to plane.
                pts=[]
                for j in range(9):
                    f=j/8;xx=outer+(inner-outer)*f;yy=ah+.9+(h*.9-ah-.9)*f+.22*math.sin(math.pi*f);pts.append((xx,yy,z))
                for j in range(8):a.beam(pts[j],pts[j+1],.13,'flying_arch')
                a.features['flying_buttresses']+=1
            x0,x1=sorted([inner,outer]);av=box_volume(x0,x1,0,attach,0,d,'aisle_'+str(sign));k=-sign*(attach-ah)/aw;inter=attach-k*inner;av['planes'].append((k,-1,0,inter))
            pieces.append(('aisle_'+str(sign),a,[av]))
    if crossplan:
        tr=Mesh(p,m.seed);half=w/2+aw+b*.45 if aisles else w/2+b;ty=h;trpeak=ty+cw/2*slope
        for z,normal in [(cz-cw/2,-1),(cz+cw/2,1)]:
            for sign in [-1,1]:
                start=w/2+(aw if aisles else 0);span=half-start
                # Only outward-facing arm walls receive windows, above aisle roof.
                panel(tr,(sign*(start+span/2),0,z),(1,0,0),(0,0,normal),span,ty,min(1.,span*.4),'transept_side')
                if aisles:
                    panel(tr,(sign*(w/2+aw/2),attach+.2,z),(1,0,0),(0,0,normal),aw,ty-attach-.2,min(1.,aw*.4),'transept_clerestory',bottom=.05)
        for sign in [-1,1]:
            ex=sign*half;panel(tr,(ex,0,cz),(0,0,-sign),(sign,0,0),cw,ty,cw*.46,'transept_end')
            temp=Mesh(p,m.seed);gable(temp,0,0,cw,ty-.18,trpeak-.18,'transept_gable',False)
            for i,f in enumerate(temp.faces):tr.face([(ex-sign*temp.vertices[j][2],temp.vertices[j][1],cz+sign*temp.vertices[j][0]) for j in f],temp.names[i],temp.materials[i])
            for z in [cz-cw/2,cz+cw/2]:buttress(tr,ex,z,ty-.5,'transept_pier')
        roof(tr,0,cz,2*half+.3,cw+.3,ty-.15*slope,p['roof_pitch'],'transept_roof','x')
        tv=gable_volume(-half,half,cz-cw/2,cz+cw/2,ty,trpeak,'x','transept_envelope')
        pieces.append(('transept',tr,[tv]+roof_slab_volumes(-half-.15,half+.15,cz-cw/2-.15,cz+cw/2+.15,ty-.15*slope,trpeak,'x','transept')))
    tw=w*.36;specs=[]
    # Towers are front attachments with a controlled overlap = wall thickness.
    rear=t+.08;front_z=rear-tw/2
    if p['tower_layout']=='front_single':specs.append(('front_single',-(w/2+(aw if aisles else 0)+tw/2-t),tw/2,tw,0,p['tower_floors']))
    if p['tower_layout'] in ('front_pair','front_pair_crossing'):
        for sign in [-1,1]:specs.append(('front_tower_'+str(sign),sign*(w/2+tw*.1),front_z,tw,0,p['tower_floors']))
    if p['tower_layout']=='front_pair_crossing':specs.append(('crossing',0,cz,cw*.74,h,max(2,p['tower_floors']-1)))
    for label,x,z,ww,base,floors in specs:
        tm=Mesh(p,m.seed);tower(tm,x,z,ww,floors,p['tower_floor_height'],p['spire_height'],label,base,window_floor=peak+.3 if label=='crossing' else 0,obstruction_height=peak+.3)
        pieces.append((label,tm,tower_volumes(x,z,ww,base,floors,p['tower_floor_height'],p['spire_height'],label,p['tower_crown'])))
    assemble(m,pieces)
    # Interior elements are deliberately inside the exterior shell; the exterior
    # envelope audit excludes this explicit group, not arbitrary intrusions.
    floor=Mesh(p,m.seed)
    floor.box(0,0,d/2,w,.12,d,'nave_floor')
    if aisles:
        for sign in [-1,1]:
            floor.box(sign*(w/2+aw/2),0,d/2,aw,.12,d,'aisle_floor')
            for i in range(p['length_bays']):
                exclusions=[(cross_lo,cross_hi)] if crossplan else []
                for k,(a,z) in enumerate(spans(i*b,(i+1)*b,exclusions)):
                    span=z-a
                    if span<1.4:
                        floor.box(sign*(w/2-t/2),.12,(a+z)/2,t,attach-.12,span,'arcade_end_pier')
                    else:
                        panel(floor,(sign*w/2,.12,(a+z)/2),(0,0,-sign),(sign,0,0),span,attach-.12,min(span-.55,3.2),'arcade_%d_%d_%d'%(sign,i,k),True)
    if crossplan:
        for sign in [-1,1]:
            floor.box(sign*((w/2+aw if aisles else w/2)+half)/2,0,cz,half-(w/2+aw if aisles else w/2),.12,cw,'transept_floor')
        if p['tower_layout']=='front_pair_crossing':
            for sx in [-1,1]:
                for sz in [-1,1]:floor.box(sx*cw*.37,.12,cz+sz*cw*.37,.65,h-.12,.65,'crossing_bearing_pier')
    choir_start=cross_hi if crossplan else d-b
    floor.box(0,.12,(choir_start+d)/2,w*.65,.15,d-choir_start,'chancel_dais')
    floor.box(0,.27,d-1.2,min(2.2,w*.35),.95,.8,'altar')
    # Seating leaves a continuous central circulation aisle.
    seating_end=cross_lo-.8 if crossplan else choir_start-.8
    for i in range(max(0,int((seating_end-2)/1.2))):
        for sign in [-1,1]:skytown_prop(floor,'SM_Furniture01_Bench01',(sign*w*.28,.37,2+i*1.2),(w*.28,.50,.40),90,'pew')
    merge_mesh(m,floor,'interior/')
    if p['interior_lighting']:
        for i in range(p['length_bays']):
            for sign in (-1,1):interior_lamp(m,sign*w*.32,min(h-.6,4.2),(i+.5)*p['bay_length'],'nave_lamp_'+str((i,sign)))
    m.spatial_layout=dict(units='metres_before_global_scale',nave=[-w/2,w/2,0,d],entrance=dict(center=[0,0],clear_width=2.2),choir=[choir_start,d],crossing=[cross_lo,cross_hi] if crossplan else None,transept_bearing=h if crossplan else None,nave_bearing=h,transept_ridge=trpeak if crossplan else None,nave_ridge=peak,functions=['nave_seating','central_aisle','chancel_altar']+(['side_aisle_circulation'] if aisles else [])+(['transept_chapels','crossing'] if crossplan else []),assumed=True)
    m.attachment_checks={'nave_bearing_height':h,'nave_roof_at_wall':h,'eave_gap':0.,'tower_rear_plane':rear,'aisle_attachment_height':attach if aisles else None}

def gatehouse(m,p):
    w=p['nave_width']*.6;fh=p['tower_floor_height'];t=p['wall_thickness'];pieces=[]
    center=Mesh(p,m.seed);tower(center,0,w/2,w,p['tower_floors'],fh,p['spire_height'],'gate',passage=True,clock=p['clock_enable'])
    pieces.append(('gate_tower',center,tower_volumes(0,w/2,w,0,p['tower_floors'],fh,p['spire_height'],'gate',p['tower_crown'])))
    wingh=fh*1.2;depth=max(1.4,2*t+.3);length=w*1.4;turretw=w*.45
    for sign in [-1,1]:
        turretx=sign*(w/2+length);z=depth/2;wing=Mesh(p,m.seed)
        start=w/2-t-.05;end=abs(turretx)-turretw/2+t+.05;span=end-start;x=sign*(start+end)/2
        for zz,normal in [(0,-1),(depth,1)]:panel(wing,(x,0,zz),(1,0,0),(0,0,normal),span,wingh,1.3,'curtain_window')
        wing.box(x,wingh-.2,depth/2,span,.25,depth,'walkway_cap')
        for i in range(9):
            for zz in [0,depth]:wing.box(sign*(start+(i+.5)*span/9),wingh,zz,span/16,.6,.42,'merlon')
        low,high=sorted([sign*start,sign*end]);pieces.append(('wing_'+str(sign),wing,[box_volume(low,high,0,wingh,0,depth,'wing_envelope')]))
        tm=Mesh(p,m.seed);tower(tm,turretx,z,turretw,2,fh*.8,0,'terminal_turret')
        pieces.append(('turret_'+str(sign),tm,tower_volumes(turretx,z,turretw,0,2,fh*.8,0,'turret')))
    assemble(m,pieces);m.attachment_checks={'wing_depth':depth,'wing_height':wingh,'terminal_turrets':2,'clock_replaces_window':True}

def townhouse(m,p):
    w=p['nave_width']*.65;d=p['bay_length']*1.4;fh=3.1;h=p['town_floors']*fh;t=p['wall_thickness'];slope=math.tan(math.radians(p['roof_pitch']));peak=h+w/2*slope
    for floor in range(p['town_floors']):
        interior_room(m,0,d/2,w,d,floor*fh,floor,p['town_floors'],fh,'dwelling','town_interior')
        for side in [-1,1]:
            for bay in range(3):panel(m,(side*w/2,floor*fh,(bay+.5)*d/3),(0,0,-side),(side,0,0),d/3,fh,d/5,'town_side_%d_%d_%d'%(floor,side,bay))
        for zz,sgn in [(0,-1),(d,1)]:
            for bay in range(2):panel(m,((bay-.5)*w/2,floor*fh,zz),(1,0,0),(0,0,sgn),w/2,fh,w*.26,'town_front_%d_%d_%d'%(floor,sgn,bay),door=(floor in (0,1) and bay==0 and zz==0))
        course_ring(m,0,floor*fh,d/2,w,d,.16,.175,'town_band')
    # Roof terminates on inner gable plane; stair parapet is supported to wall top.
    for zz in [t/2,d-t/2]:
        steps=12;bw=w/steps
        for i in range(steps):
            x=-w/2+(i+.5)*bw;top=peak-(max(0,abs(x)-bw/2))*slope+.24
            m.box(x,h,zz,bw,top-h,t,'stepped_gable_%d'%i)
            m.box(x,top,zz,bw+.015,.10,t+.07,'stepped_coping','trim')
    for sign in [-1,1]:
        for zz in [0,d]:buttress(m,sign*w/2,zz,h-.5,'town_corner')
    mainroof=Mesh(p,m.seed);roof(mainroof,0,d/2,w+.3,d-2*t,h-.15*slope,p['roof_pitch'],'town_roof')
    dormers=[];cutters=[];audits=[]
    # Dormers on the -X roof, facing the same evaluation camera as the input.
    for i in range(p['dormer_count']):
        zz=(i+1)*d/(p['dormer_count']+1);dw=min(.95,d/(p['dormer_count']+1)*.64);xf=-w/2+.13
        host=lambda x:peak+slope*x
        base=host(xf)-.10;ey=base+1.15;ds=math.tan(math.radians(50));dp=ey+dw/2*ds;back=(dp-peak)/slope+.08
        dm=Mesh(p,m.seed);panel(dm,(xf,base,zz),(0,0,1),(-1,0,0),dw,.97,dw*.45,'dormer_%d_window'%i,bottom=.02)
        # Solid cheek wedges with their bottom cut by the host slope.
        for side in [-1,1]:
            z=zz+side*dw/2;poly=[(xf+t,base-.2,z),(back,base-.2,z),(back,ey-.18,z),(xf+t,ey-.18,z)]
            poly=clip3(poly,(-slope,1,0,-peak+.18))
            if poly:dm.solid(poly,(0,0,-side*.12),'dormer_%d_cheek'%i)
        dm.solid([(xf,ey-.18,zz-dw/2),(xf,ey-.18,zz+dw/2),(xf,dp-.18,zz)],(.12,0,0),'dormer_%d_gable'%i)
        rr=Mesh(p,m.seed);roof(rr,(xf+back)/2,zz,back-xf,dw,ey,50,'dormer_%d_roof'%i,'x')
        under=box_volume(-w/2-.3,w/2+.3,0,peak,-1,d+1,'host_roof_below');under['planes'].append((slope,-1,0,peak-.02))
        clipped=trim_mesh(rr,[under],'dormer_roof');merge_mesh(dm,clipped)
        hole=gable_volume(xf,back,zz-dw/2,zz+dw/2,ey,dp,'x','dormer_%d_opening'%i);cutters.append(hole)
        # Flashing follows intersection line, from cheek edge to dormer ridge.
        for side in [-1,1]:
            edge=(ey-peak)/slope;ridge=(dp-peak)/slope
            dm.beam((edge,ey+.035,zz+side*dw/2),(ridge,dp+.035,zz),.035,'dormer_flashing','metal')
            dm.beam((xf,host(xf)+.035,zz+side*dw/2),(edge,ey+.035,zz+side*dw/2),.035,'cheek_flashing','metal')
        dormers.append(dm);audits.append(dict(id=i,front_x=xf,back_x=back,z=zz,width=dw,host_at_front=host(xf),base=base,roof_eave=ey,roof_peak=dp))
    trimmed=trim_mesh(mainroof,cutters,'main_roof');merge_mesh(m,trimmed)
    for dm in dormers:merge_mesh(m,dm)
    m.features['dormers']=p['dormer_count'];m.assembly_checks=[trimmed.trim_stats];m.assembly_volumes={'dormer_cutters':cutters}
    m.attachment_checks=dict(eave_gap=0.,nave_roof_at_wall=h,nave_bearing_height=h,dormers=audits)
    bw=w+.16
    m.box(0,fh-.25,-.45,bw,.22,.9,'balcony_slab')
    for x in [-w/2,w/2]:balcony_bracket(m,x,fh-.25,.75)
    for i in range(12):m.box((i/11-.5)*bw,fh,-.8,.06,.7,.06,'baluster')
    m.box(0,fh+.7,-.8,bw+.04,.08,.1,'balcony_rail')


def crown(m,x,y,z,w,h,name,style):
    if h<=0:return
    if style!='openwork':
        m.spire(x,y,z,w,h,name+'_roof')
        if style=='canopied':
            for side in range(4):
                a=side*math.pi/2;n=(math.sin(a),0,-math.cos(a));u=(math.cos(a),0,math.sin(a))
                def pt(xx,yy,dd=0):return (x+u[0]*xx+n[0]*(w*.48+dd),y+yy,z+u[2]*xx+n[2]*(w*.48+dd))
                hh=min(h*.42,w*.85);ww=w*.57
                # Gabled canopy, supported at the tower cornice; a blind niche.
                m.solid([pt(-ww/2,.02),pt(ww/2,.02),pt(0,hh)],tuple(-v*w*.25 for v in n),name+'_gable_canopy')
                for sign in [-1,1]:
                    m.beam(pt(sign*ww/2,.04,.035),pt(0,hh+.05,.035),.075,name+'_raking_moulding')
                    m.beam(pt(sign*ww*.3,0,.08),pt(sign*ww*.3,hh*.42,.08),.06,name+'_niche_jamb')
                spring=hh*.34;ow=min(ww*.6,hh*.65)
                for j in range(10):
                    xx=-ow/2+j*ow/10;xx2=xx+ow/10
                    m.beam(pt(xx,arch(xx,ow,spring),.08),pt(xx2,arch(xx2,ow,spring),.08),.035,name+'_blind_arch')
                pinnacle(m,x+n[0]*w*.48,y+hh,z+n[2]*w*.48,.45,name+'_gable_finial')
            # Large protruding ornaments were not approved for the concept.
        return
    enclosed_spire(m,x,y,z,w,h,name,8)


def spire_profile(w,h):
    # A broad weathering shoulder resolves into a narrow, steep roof shaft.
    return [(0,w*.5),(h*.14,w*.34),(h*.24,w*.32),(h,0.)]


def spire_envelopes(x,y,z,w,h,n,label):
    result=[];profile=spire_profile(w,h);c=math.cos(math.pi/n)
    for k,((ya,ra),(yb,rb)) in enumerate(zip(profile,profile[1:])):
        v=box_volume(x-ra,x+ra,y+ya,y+yb,z-ra,z+ra,label+'_roof_'+str(k))
        slope=(rb-ra)*c/(yb-ya)
        for j in range(n):
            a=j*2*math.pi/n;nx,nz=math.sin(a),-math.cos(a)
            v['planes'].append((-nx,slope,-nz,ra*c+nx*x+nz*z-slope*(y+ya)))
        result.append(v)
    return result


def enclosed_spire(m,x,y,z,w,h,name,n=8):
    profile=spire_profile(w,h)
    def ring(yy,r):return [(x+r*math.sin((j-.5)*2*math.pi/n),y+yy,z-r*math.cos((j-.5)*2*math.pi/n)) for j in range(n)]
    rings=[ring(yy,r) for yy,r in profile]
    # The cornice follows the supporting wall, never a square slab on a round tower.
    if n==8:m.box(x,y-.10,z,w-.12,.10,w-.12,name+'_wall_cap','trim')
    else:m.solid(ring(-.10,w*.5),(0,.10,0),name+'_wall_cap','trim')
    for k in range(len(rings)-1):
        for j in range(n):
            jj=(j+1)%n
            m.face([rings[k][j],rings[k+1][j],rings[k+1][jj],rings[k][jj]],name+'_weatherproof_skin','roof')
            m.beam(rings[k][j],rings[k+1][j],.045,name+'_standing_seam','metal')
    # Four solid gablelets are seated on the shoulder, with blind Gothic niches.
    gh=min(h*.23,w*.52);gw=w*.30
    for j in range(4):
        a=j*math.pi/2;nx,nz=math.sin(a),-math.cos(a);ux,uz=math.cos(a),math.sin(a)
        def pt(xx,yy,rr):return (x+ux*xx+nx*rr,y+yy,z+uz*xx+nz*rr)
        front=w*.43;back=w*.25
        m.solid([pt(-gw/2,0,front),pt(gw/2,0,front),pt(0,gh,front)],(-nx*(front-back),0,-nz*(front-back)),name+'_shoulder_gable','stone')
        for sign in [-1,1]:
            m.beam(pt(sign*gw/2,.02,front+.02),pt(0,gh+.02,front+.02),.065,name+'_gable_rake','trim')
        ow=min(gw*.50,gh*.50);spring=gh*.25
        for k in range(10):
            xx=-ow/2+k*ow/10;xx2=xx+ow/10
            m.beam(pt(xx,arch(xx,ow,spring),front+.025),pt(xx2,arch(xx2,ow,spring),front+.025),.035,name+'_blind_tracery','trim')
        for sign in [-1,1]:m.beam(pt(sign*ow/2,.04,front+.025),pt(sign*ow/2,spring,front+.025),.035,name+'_niche_jamb','trim')
    m.beam((x,y+h,z),(x,y+h+.55,z),.045,name+'_finial','metal')
    m.beam((x-.18,y+h+.35,z),(x+.18,y+h+.35,z),.035,name+'_cross','metal')


def entry_porch(m,depth,w):
    # Open-sided porch: one clear entrance and a roof below the west window sill.
    y=2.72;peak=y+w*.25;front=-depth
    for sign in [-1,1]:
        x=sign*(w/2-.16)
        m.box(x,0,front,.26,y,.26,'entry_porch_column','trim')
        m.box(x,0,front,.40,.18,.40,'entry_porch_base')
        m.box(x,y-.15,front,.38,.15,.38,'entry_porch_capital','trim')
        m.beam((x,y-.12,front),(x,y-.12,0),.10,'entry_porch_lintel')
    ow=min(1.8,w-.55);spring=y-ow*.866-.1
    for k in range(16):
        a=-ow/2+k*ow/16;b=a+ow/16
        m.beam((a,arch(a,ow,spring),front-.03),(b,arch(b,ow,spring),front-.03),.085,'entry_porch_arch')
    gable(m,0,front,w,y-.18,peak-.18,'entry_porch_gable',False)
    roof(m,0,-depth/2,w,depth+.10,y,math.degrees(math.atan(.5)),'entry_porch_roof')


def west_front(m,w,h,style):
    clear=w*.66;lower=min(3.25,h*.38);band=.62;upper=h-lower-band;t=m.p['wall_thickness']
    for sign in [-1,1]:m.box(sign*(w+clear)/4,0,t/2,(w-clear)/2,h,t,'west_flank')
    portals=3 if style=='tracery_front' and w>=7 else 1
    for j in range(portals):
        span=clear/portals;x=(j-(portals-1)/2)*span;ow=min(1.4 if portals==3 else 2.1,span*.65)
        panel(m,(x,0,0),(1,0,0),(0,0,-1),span,lower,ow,'west_portal_%d'%j,True)
        spring=min(lower*.36,2.7)
        # Concentric archivolts and clustered shafts create real facade depth.
        for order in range(3):
            radius=ow+order*.19;depth=.10+order*.11
            for sign in [-1,1]:m.beam((x+sign*radius/2,0,-depth),(x+sign*radius/2,spring,-depth),.055,'portal_shaft_%d_%d'%(j,order))
            for k in range(14):
                xx=-radius/2+k*radius/14;xx2=xx+radius/14
                m.beam((x+xx,arch(xx,radius,spring),-depth),(x+xx2,arch(xx2,radius,spring),-depth),.055,'portal_archivolt_%d_%d'%(j,order))
    m.box(0,lower,t/2,clear,band,t,'west_gallery_wall')
    # Blind arcade is an architectural frieze, not a second occupied floor.
    count=max(5,int(clear/.58));span=clear/count
    for j in range(count):
        x=(j+.5)*span-clear/2;ow=span*.68;spring=lower+.15
        for sign in [-1,1]:m.beam((x+sign*ow/2,lower,-.12),(x+sign*ow/2,spring,-.12),.025,'gallery_colonette')
        for k in range(8):
            xx=-ow/2+k*ow/8;xx2=xx+ow/8
            m.beam((x+xx,arch(xx,ow,spring),-.12),(x+xx2,arch(xx2,ow,spring),-.12),.025,'gallery_arch')
    m.box(0,lower+band-.08,-.12,clear+.16,.12,.3,'gallery_cornice','trim')
    if style=='tracery_front':
        panel(m,(0,lower+band,0),(1,0,0),(0,0,-1),clear,upper,clear*.72,'great_west_window',profile='broad')
    else:
        for j in [-1,0,1]:panel(m,(j*clear/3,lower+band,0),(1,0,0),(0,0,-1),clear/3,upper,clear*.19,'west_triplet_'+str(j))
    for sign in [-1,1]:
        xx=sign*(clear/2+.12);m.box(xx,0,-.15,.23,h,.42,'west_clustered_pier','trim')
        for yy in [lower,lower+band,h-.25]:m.box(xx,yy,-.18,.4,.16,.52,'pier_capital','trim')


def polygon_volume(x,z,r,y0,y1,n,label):
    v=box_volume(x-r,x+r,y0,y1,z-r,z+r,label);ap=r*math.cos(math.pi/n)
    for j in range(n):
        a=j*2*math.pi/n;nx,nz=math.sin(a),-math.cos(a)
        v['planes'].append((-nx,0,-nz,ap+nx*x+nz*z))
    return v


def round_tower(m,x,z,r,floors,fh,spire,name,interface_height=0,crown_style='pyramid'):
    n=12;t=m.p['wall_thickness'];ap=r*math.cos(math.pi/n);span=2*r*math.sin(math.pi/n);top=floors*fh
    for f in range(floors):
        if m.p['interior_enable']:
            outer=max(.20,ap-t-.03);hole=min(.85,outer*.60);count=64
            for k in range(count):
                a=k*math.tau/count;b=(k+1)*math.tau/count
                pts=[(x+rr*math.cos(theta),f*fh+.16,z+rr*math.sin(theta)) for rr,theta in ((outer,a),(outer,b),(hole,b),(hole,a))]
                if f==0:pts=pts[:2]+[(x,f*fh+.16,z)]
                m.solid(pts,(0,.10,0),name+'_interior_round_floor','floor')
            if f<floors-1:
                steps=max(16,math.ceil(fh/.18))
                for k in range(steps):
                    a=k*math.tau/steps;b=(k+1)*math.tau/steps;yy=f*fh+.26+(k+1)*fh/steps
                    pts=[(x+rr*math.cos(theta),yy,z+rr*math.sin(theta)) for rr,theta in ((.10,a),(hole-.02,a),(hole-.02,b),(.10,b))]
                    m.solid(pts,(0,-.06,0),name+'_spiral_tread','wood')
            interior_lamp(m,x+outer*.65,f*fh+fh-.55,z,name+'_lamp')
        for j in range(n):
            a=j*2*math.pi/n;normal=(math.sin(a),0,-math.cos(a));u=(math.cos(a),0,math.sin(a));o=(x+normal[0]*ap,f*fh,z+normal[2]*ap)
            # Openings only on free front/rear elevations below adjoining roofs.
            visible=j in (0,6) or f*fh>=interface_height and j%3==0
            if visible and span>.5:
                panel(m,o,u,normal,span,fh,min(.65,span*.5),name+'_f%d_s%d'%(f,j))
            else:
                poly=[tuple(o[k]+u[k]*xx+(yy if k==1 else 0) for k in range(3)) for xx,yy in [(-span/2,0),(span/2,0),(span/2,fh),(-span/2,fh)]]
                m.solid(poly,tuple(-v*t for v in normal),name+'_shell')
            if f>0:
                a0=(j-.5)*2*math.pi/n;a1=(j+.5)*2*math.pi/n
                m.beam((x+(r+.05)*math.sin(a0),f*fh,z-(r+.05)*math.cos(a0)),(x+(r+.05)*math.sin(a1),f*fh,z-(r+.05)*math.cos(a1)),.09,name+'_belt')
    pts=[(x+r*math.sin((j-.5)*2*math.pi/n),0,z-r*math.cos((j-.5)*2*math.pi/n)) for j in range(n)]
    m.solid(pts,(0,.18,0),name+'_plinth')
    volumes=[polygon_volume(x,z,r,0,top,n,name+'_body')]
    if spire>0:
        if crown_style=='openwork':
            enclosed_spire(m,x,top,z,2*r,spire,name+'_spire',n)
            volumes.extend(spire_envelopes(x,top,z,2*r,spire,n,name))
        else:
            apex=(x,top+spire,z)
            for j in range(n):
                a=pts[j];b=pts[(j+1)%n];m.face([(a[0],top,a[2]),apex,(b[0],top,b[2])],name+'_conical_roof','roof')
            v=box_volume(x-r,x+r,top,top+spire,z-r,z+r,name+'_cone')
            for j in range(n):
                a=j*2*math.pi/n;nx,nz=math.sin(a),-math.cos(a);k=ap/spire
                v['planes'].append((-nx,-k,-nz,ap+nx*x+nz*z+k*top))
            volumes.append(v)
    m.features['towers']+=1
    return volumes


def urban_gate(m,p):
    w=p['nave_width']*.9;depth=p['bay_length'];fh=p['tower_floor_height'];t=p['wall_thickness'];h=fh*2.2;pass_h=fh*1.35;slope=math.tan(math.radians(p['roof_pitch']));peak=h+depth/2*slope
    g=Mesh(p,m.seed)
    interior_room(g,0,depth/2,w,depth,pass_h,0,1,h-pass_h,'workshop','gate_upper_interior')
    interior_lamp(g,-w*.32,pass_h-.5,depth/2,'passage_lamp')
    for z,normal in [(0,-1),(depth,1)]:
        panel(g,(0,0,z),(1,0,0),(0,0,normal),w,pass_h,min(3.8,w*.48),'city_passage_'+str(normal),True)
        if normal==-1 and p['clock_enable']:
            g.box(0,pass_h,t/2,w,h-pass_h,t,'clock_storey')
            radius=min(w*.24,(h-pass_h)*.4);cy=pass_h+(h-pass_h)/2
            rose(g,0,cy,-.18,radius,'astronomical_clock',True)
            for j in range(24):
                a=j*math.pi/12;rr=radius*1.12
                g.beam((rr*math.cos(a),cy+rr*math.sin(a),-.22),((rr+.12)*math.cos(a),cy+(rr+.12)*math.sin(a),-.22),.025,'clock_ray','metal')
            for sign in [-1,1]:
                g.beam((sign*(radius+.35),pass_h,-.28),(sign*(radius+.35),h-.1,-.28),.09,'clock_pilaster','metal')
                g.beam((sign*(radius+.35),h-.1,-.28),(0,peak-.1,-.28),.07,'clock_pediment','metal')
        else:panel(g,(0,pass_h,z),(1,0,0),(0,0,normal),w,h-pass_h,w*.3,'gate_upper_'+str(normal))
        # Roof ridge runs along X; front/rear walls stop at bearing h.
    for sign in [-1,1]:
        x=sign*w/2;g.box(x-sign*t/2,0,depth/2,t,h,depth,'gate_side')
        g.solid([(x,h-.18,0),(x,h-.18,depth),(x,peak-.18,depth/2)],(-sign*t,0,0),'gate_end_gable')
    roof(g,0,depth/2,w+.3,depth+.3,h-.15*slope,p['roof_pitch'],'city_gate_roof','x')
    volumes=[gable_volume(-w/2,w/2,0,depth,h,peak,'x','city_gate')]+roof_slab_volumes(-w/2-.15,w/2+.15,-.15,depth+.15,h-.15*slope,peak,'x','city_gate')
    pieces=[('city_gate',g,volumes)]
    radius=w*.25;leftx=-w/2-radius+t;left=Mesh(p,m.seed)
    vols=round_tower(left,leftx,depth/2,radius,p['tower_floors']+1,fh,p['spire_height'],'watchtower',interface_height=peak,crown_style=p['tower_crown'])
    pieces.append(('watchtower',left,vols))
    small=w*.115;right=Mesh(p,m.seed);rightx=w/2+small-t
    vols=round_tower(right,rightx,depth/2,small,3,h/3,max(2,p['spire_height']*.4),'stair_turret',interface_height=peak)
    pieces.append(('stair_turret',right,vols));assemble(m,pieces)
    m.spatial_layout={'functions':['through_passage','clock_storey','watchtower','stair_turret'],'passage_width':min(3.8,w*.48),'assumed':True,'interior_stairs':'spiral_in_towers'}


def urban_building(m,p):
    """Street-facing housing and civic masses with deliberately different plans."""
    w=p['nave_width'];d=p['urban_depth'];fh=p['floor_height'];floors=p['town_floors']
    h=fh*floors;t=p['wall_thickness'];bays=p['front_bays'];program=p['urban_program']
    slope=math.tan(math.radians(p['roof_pitch']));plan=p['urban_plan']
    if plan=='deep':d*=1.2
    front_depth=d if plan in ('compact','deep') else min(5.5,d*.44)
    masses=[(0.,0.,w,front_depth,'street')]
    if plan=='corner':masses.append((w*.30,front_depth,w*.40,d-front_depth,'corner_wing'))
    elif plan=='courtyard':
        for sign in (-1,1):masses.append((sign*w*.36,front_depth,w*.28,d-front_depth,'court_wing_'+str(sign)))
    def hip_roof(x,z,ww,dd,eave,label):
        reach=min(ww,dd)/2;peak=eave+reach*slope
        c=[(x-ww/2,eave,z),(x+ww/2,eave,z),(x+ww/2,eave,z+dd),(x-ww/2,eave,z+dd)]
        if dd>=ww:
            a=(x,peak,z+reach);b=(x,peak,z+dd-reach)
            polys=[[c[0],c[1],a],[c[1],c[2],b,a],[c[2],c[3],b],[c[3],c[0],a,b]]
        else:
            a=(x-ww/2+reach,peak,z+dd/2);b=(x+ww/2-reach,peak,z+dd/2)
            polys=[[c[0],c[1],b,a],[c[1],c[2],b],[c[2],c[3],a,b],[c[3],c[0],a]]
        for poly in polys:m.solid(poly,(0,-.16,0),label,'roof')
    for x,z,ww,dd,label in masses:
        front_count=bays if label=='street' else max(1,round(ww/2.6))
        side_count=max(2,round(dd/2.7))
        m.box(x,0,z+dd/2,ww+.24,.18,dd+.24,label+'_foundation')
        for floor in range(floors):
            yy=floor*fh
            for zz,sgn in [(z,-1),(z+dd,1)]:
                for bay in range(front_count):
                    span=ww/front_count;xx=x+(bay-(front_count-1)/2)*span
                    entrance=(floor==0 or (floor==1 and p['balcony_enable'])) and label=='street' and sgn==-1 and bay==front_count//2
                    arcade=program=='market' and floor==0 and sgn==-1
                    opening=min(1.65,span*.60)
                    if program=='shop' and floor==0:opening=min(2.,span*.70)
                    if arcade:opening=span*.78
                    tag=label+('_market_arcade' if arcade else '_door' if entrance else '_window')+f'_{floor}_{sgn}_{bay}'
                    panel(m,(xx,yy,zz),(1,0,0),(0,0,sgn),span,fh,opening,tag,entrance or arcade,bottom=.05)
            for sign in (-1,1):
                for bay in range(side_count):
                    span=dd/side_count;zz=z+(bay+.5)*span
                    panel(m,(x+sign*ww/2,yy,zz),(0,0,-sign),(sign,0,0),span,fh,min(1.3,span*.50),label+f'_side_{floor}_{sign}_{bay}',bottom=.10)
            if p['interior_enable']:interior_room(m,x,z+dd/2,ww,dd,yy,floor,floors,fh,program,label+'_interior')
            else:m.box(x,yy+.16,z+dd/2,ww-2*t,.13,dd-2*t,label+'_floor_'+str(floor),'floor')
            if floor>0 or p['detail']>=.40:
                course_ring(m,x,yy-.02,z+dd/2,ww,dd,.14,.12,label+'_floor_course')
        for sign in (-1,1):
            for zz in (z,z+dd):
                if p['detail']>=.65:buttress(m,x+sign*ww/2,zz,h-.5,label+'_corner_buttress')
                else:m.box(x+sign*ww/2,0,zz,.26,h,.32,label+'_corner_quoin','trim')
        course_ring(m,x,h-.15,z+dd/2,ww,dd,.22,.17,label+'_eave_cornice')
        if p['urban_roof']=='hip':hip_roof(x,z-.18,ww+.36,dd+.36,h-.18*slope,label+'_hip_roof')
        else:
            peak=h+ww/2*slope
            roof(m,x,z+dd/2,ww+.36,max(.5,dd-2*t),h-.18*slope,p['roof_pitch'],label+'_roof')
            for zz,normal in [(z,-1),(z+dd,1)]:
                if p['urban_roof']=='stepped':
                    steps=10;bw=ww/steps
                    for i in range(steps):
                        xx=x-ww/2+(i+.5)*bw;top=peak-max(0,abs(xx-x)-bw/2)*slope+.15
                        m.box(xx,h,zz,bw,top-h,t,label+'_stepped_gable')
                        m.box(xx,top,zz,bw+.02,.10,t+.08,label+'_stepped_coping','trim')
                else:
                    m.solid([(x-ww/2,h,zz),(x+ww/2,h,zz),(x,peak,zz)],(0,0,-normal*t),label+'_gable')
                    for sign in (-1,1):m.beam((x+sign*ww/2,h+.03,zz),(x,peak+.03,zz),.065,label+'_gable_rake')
        if program in ('market','guildhall') and label=='street':
            for bay in range(front_count):
                xx=x+(bay-(front_count-1)/2)*ww/front_count
                skytown_prop(m,'SM_Furniture01_Table01',(xx,.60,z+dd*.48),(min(1.5,ww/front_count*.65),.80,.8),90,'market_counter')
                skytown_prop(m,'SM_FruitCrate01',(xx,.20+.80+.14,z+dd*.48),(.48,.28,.40),name='market_produce')
    if p['balcony_enable'] and floors>1:
        bw=min(w*.66,5.2) if bays>2 else w+.16;y=fh+.02
        piers=[-w/2+i*w/bays for i in range(bays+1)]
        supports=[xx for xx in piers if abs(xx)<=bw/2-.03]
        if len(supports)<2:
            bearing=min(abs(xx) for xx in piers if abs(xx)>1e-6)
            bw=max(bw,2*bearing+.24)
            supports=[xx for xx in piers if abs(xx)<=bw/2-.03]
        assert len(supports)>=2,'Balcony requires at least two masonry-pier bearings'
        m.box(0,y-.17,-.55,bw,.20,1.20,'urban_balcony','trim')
        for i in range(max(5,int(bw/.32))):
            xx=(i/(max(5,int(bw/.32))-1)-.5)*bw
            m.beam((xx,y,-1.05),(xx,y+.85,-1.05),.032,'balcony_baluster','metal')
        m.beam((-bw/2,y+.85,-1.05),(bw/2,y+.85,-1.05),.055,'balcony_rail','metal')
        for x in supports:balcony_bracket(m,x,y-.17,.95)
    for i in range(p['chimney_count']):
        xx=(-.25 if i%2==0 else .25)*w;zz=front_depth*(.35+.20*(i//2))
        roof_y=h+(max(0.,min(w/2-abs(xx),zz,front_depth-zz))*slope if p['urban_roof']=='hip' else (w/2-abs(xx))*slope)
        m.box(xx,roof_y-.35,zz,.55,1.25,.65,'chimney_shaft')
        m.box(xx,roof_y+.78,zz,.72,.18,.82,'chimney_cap','trim')
    if p['clock_enable']:
        rose(m,0,h+min(1.3,w*slope*.25),-.20,min(1.,w*.08),'civic_clock',True)
    if p['porch_depth']>0 and program in ('inn','manor','guildhall','townhall','library','bathhouse'):
        entry_porch(m,p['porch_depth'],min(w*.40,3.4))
    m.spatial_layout={'functions':[program,'street_entrance','upper_floors' if floors>1 else 'single_floor']+(['rear_courtyard'] if plan=='courtyard' else []),'plan':plan,'floors':floors,'masses':len(masses),'assumed':True}
    m.attachment_checks={'roof_bearing':h,'eave_gap':0.,'floor_height':fh,'street_front_z':0.,'courtyard_clear_width':w*.44 if plan=='courtyard' else None}


def generate(config=None):
    c=normalize(config);p=c['parameters'];m=Mesh(p,c['seeds']['decoration'])
    # Layout seed drives reproducible small stone colour variations through stable IDs as well.
    m.seed=(c['seeds']['layout']*1000003+c['seeds']['decoration'])%2147483647
    typ=p['building_type']
    if typ=='church':church(m,p)
    elif typ=='gatehouse':
        if p['gate_composition']=='urban_asymmetric':urban_gate(m,p)
        else:gatehouse(m,p)
    elif typ=='townhouse':townhouse(m,p)
    elif typ in ('residence','public'):urban_building(m,p)
    else:tower(m,0,0,p['nave_width']*.45,p['tower_floors'],p['tower_floor_height'],p['spire_height'],'tower',clock=p['clock_enable'],window_floor=0)
    if p['gold_accents']:
        for i,mat in enumerate(m.materials):
            if mat in ('trim','metal'):m.materials[i]='gold';m.colors[i]=(.82,.54,.15)
    seen=set();keep=[]
    for i,face in enumerate(m.faces):
        key=tuple(sorted(tuple(round(x,8) for x in m.vertices[j]) for j in face))
        if key not in seen:seen.add(key);keep.append(i)
    m.faces=[m.faces[i] for i in keep];m.names=[m.names[i] for i in keep];m.materials=[m.materials[i] for i in keep];m.colors=[m.colors[i] for i in keep]
    used=sorted(set(j for f in m.faces for j in f));remap={j:i for i,j in enumerate(used)};m.vertices=[m.vertices[j] for j in used];m.faces=[[remap[j] for j in f] for f in m.faces]
    sc=p['global_scale'];m.vertices=[tuple(v*sc for v in pnt) for pnt in m.vertices]
    return m

def mesh_hash(m):return hashlib.sha256(json.dumps([m.vertices,m.faces,m.names,m.materials,m.colors],separators=(',',':')).encode()).hexdigest()
def write_obj(m,path):
    path=pathlib.Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('w') as f:
        f.write('# Gothic procedural core '+VERSION+'\n')
        for v in m.vertices:f.write('v %.8f %.8f %.8f\n'%v)
        for face,name,mat in zip(m.faces,m.names,m.materials):
            f.write('g '+name+'\nusemtl '+mat+'\nf '+' '.join(str(i+1) for i in face)+'\n')

def fill_hou(geo,m):
    import hou
    geo.clear();pts=geo.createPoints(m.vertices)
    attrs={k:geo.addAttrib(hou.attribType.Prim,k,default) for k,default in [('name',''),('material_id',''),('Cd',(1.,1.,1.))]}
    uv=geo.addAttrib(hou.attribType.Vertex,'uv',(0.,0.,0.));normal=geo.addAttrib(hou.attribType.Vertex,'N',(0.,1.,0.))
    groups={s:geo.createPrimGroup(s) for s in sorted(set(m.materials))}
    for idx,face in enumerate(m.faces):
        poly=geo.createPolygon();coords=[m.vertices[i] for i in face];n=unit(cross(sub(coords[1],coords[0]),sub(coords[2],coords[0])));drop=max(range(3),key=lambda j:abs(n[j]));axes=[j for j in range(3) if j!=drop]
        # Houdini's clockwise polygon convention is opposite to the mesh core.
        # Reverse the vertex order so its geometric normal agrees with N.
        for pi,xyz in zip(reversed(face),reversed(coords)):
            v=poly.addVertex(pts[pi]);v.setAttribValue(uv,(xyz[axes[0]],xyz[axes[1]],0.));v.setAttribValue(normal,n)
        poly.setAttribValue(attrs['name'],m.names[idx]);poly.setAttribValue(attrs['material_id'],m.materials[idx]);poly.setAttribValue(attrs['Cd'],m.colors[idx]);groups[m.materials[idx]].add(poly)

def cook_embedded():
    import hou
    node=hou.pwd();parent=node.parent();p={}
    for k,s in SCHEMA.items():
        v=parent.parm(k).evalAsString() if s['type']=='string' else parent.parm(k).eval()
        p[k]=bool(v) if s['type']=='boolean' else v
    c={'parameters':p,'seeds':{k:parent.parm('seed_'+k).evalAsInt() for k in ('layout','decoration')}}
    fill_hou(node.geometry(),generate(c))

def embedded_source():
    # Embed the complete stdlib geometry implementation; no filesystem import at cook.
    source=pathlib.Path(__file__).read_text(encoding='utf-8')
    source=source.split('\n# INSTALLER_BOUNDARY')[0]
    code=source+'\ncook_embedded()\n';compile(code,'embedded_gothic','exec');return code

# INSTALLER_BOUNDARY

def apply_config(asset_node,config):
    c=normalize(config)
    for k,v in c['parameters'].items():
        parm=asset_node.parm(k)
        if parm is None:raise RuntimeError('Missing parm '+k)
        parm.set(int(v) if isinstance(v,bool) else v)
    for k,v in c['seeds'].items():asset_node.parm('seed_'+k).set(v)
    return asset_node

def build(config=None,parent_path='/obj',asset_name='gothic_building'):
    import hou
    import native_network as native
    from production_pipeline import configure
    configure()
    c=normalize(config);parent=hou.node(parent_path)
    if parent is None:raise ValueError('Parent does not exist: '+parent_path)
    if parent.childTypeCategory()!=hou.objNodeTypeCategory():raise ValueError('Use an OBJ network parent')
    obj=parent.node(asset_name)
    if obj is not None and obj.userData('owner')!=OWNER:raise ValueError('Name belongs to user node; choose another asset_name')
    if obj is None:
        obj=parent.createNode('geo',asset_name,run_init_scripts=False);obj.setUserData('owner',OWNER)
    subnet=obj.node('generator')
    if subnet is None:subnet=obj.createNode('subnet','generator');subnet.setUserData('owner',OWNER)
    if subnet.userData('owner')!=OWNER:raise ValueError('generator is not owned')
    if subnet.type().definition():
        apply_config(subnet,c);subnet.hdaModule().rebuild(subnet);return subnet
    subnet.setParmTemplateGroup(native.interface(c));apply_config(subnet,c)
    old=subnet.node('mesh_core')
    if old is not None:old.destroy()
    native.rebuild(subnet)
    subnet.setDisplayFlag(True);subnet.setRenderFlag(True);subnet.layoutChildren();obj.layoutChildren()
    return subnet

def export_asset(asset_node,output_path):
    import hou
    license_name=str(hou.licenseCategory()).lower()
    ext='.hdanc' if 'apprentice' in license_name else '.hdalc' if 'indie' in license_name else '.hda'
    path=pathlib.Path(output_path).with_suffix(ext)
    if path.exists():raise FileExistsError('Refusing to overwrite '+str(path))
    path.parent.mkdir(parents=True,exist_ok=True)
    if not asset_node.canCreateDigitalAsset():raise RuntimeError('Node cannot be converted to digital asset')
    # Spare parameters survive on the source node but are not automatically
    # part of a new HDA definition. Publish the interface for fresh instances.
    templates=asset_node.parmTemplateGroup()
    asset=asset_node.createDigitalAsset(name='quynh::gothic_building::'+'.'.join(VERSION.split('.')[:2]),hda_file_name=str(path),description='Gothic Building',min_num_inputs=0,max_num_inputs=0)
    asset.type().definition().setParmTemplateGroup(templates)
    from production_pipeline import embedded
    asset.type().definition().addSection('PythonModule',embedded())
    asset.type().definition().addSection('EditableNodes','construction MANUAL_EDIT final_boundary_alignment')
    return asset

def main():
    a=argparse.ArgumentParser();a.add_argument('--dataset');a.add_argument('--output-dir',default='output');a.add_argument('--export-hda',action='store_true');args=a.parse_args()
    root=pathlib.Path(__file__).resolve().parent;out=pathlib.Path(args.output_dir);out=out if out.is_absolute() else root/out;out.mkdir(parents=True,exist_ok=True)
    path=pathlib.Path(args.dataset) if args.dataset else None
    c=json.loads((path if path.is_absolute() else root/path).read_text()) if path else None
    node=build(c);node.cook(force=True)
    if node.errors():raise RuntimeError(str(node.errors()))
    node.geometry().saveToFile(str(out/'gothic_building.bgeo.sc'))
    if args.export_hda:export_asset(node,out/'gothic_building.hda')
if __name__=='__main__':main()
