"""Concept 03: Central Fountain. Run in the already-open Maya session."""
import maya.cmds as c
import maya.api.OpenMaya as om
import math, random, os, json, traceback, time
from collections import defaultdict

OUT = 'E:/Repos/mythic-forge/MayaWorkspace'
random.seed(3003)
T = math.tau
ROOT = None
GROUPS = {}
MATS = {}
SGS = {}
START = time.time()

def progress(stage):
    with open(OUT+'/build_status.json','w') as f:
        json.dump({'stage':stage,'seconds':round(time.time()-START,1)},f)

def material(name, color, rough=.7, metal=0, transmission=0, emission=0):
    mat = c.shadingNode('aiStandardSurface',asShader=True,name='M_'+name)
    sg = c.sets(renderable=True,noSurfaceShader=True,empty=True,name=mat+'_SG')
    c.connectAttr(mat+'.outColor',sg+'.surfaceShader',force=True)
    for attr,val in [('base',.85),('specularRoughness',rough),('metalness',metal),('transmission',transmission),('emission',emission)]:
        c.setAttr(mat+'.'+attr,val)
    c.setAttr(mat+'.baseColor',*color,type='double3')
    c.setAttr(mat+'.emissionColor',*color,type='double3')
    if transmission:
        c.setAttr(mat+'.specularIOR',1.333)
        c.setAttr(mat+'.thinWalled',1)
    MATS[name], SGS[name] = mat,sg
    return mat

def assign(node, mat):
    c.sets(node,e=True,forceElement=SGS[mat])

def group(name,parent=None):
    return c.group(empty=True,name=name,parent=parent or ROOT)

def mesh(name, verts, faces, mat, parent, smooth=False):
    if not verts: return None
    unique=[];lookup={};remap=[]
    for p in verts:
        key=tuple(round(q,6) for q in p)
        if key not in lookup:lookup[key]=len(unique);unique.append(p)
        remap.append(lookup[key])
    clean=[]
    for f in faces:
        f=list(dict.fromkeys(remap[i] for i in f))
        if len(f)>=3:clean.append(f)
    verts,faces=unique,clean
    fn=om.MFnMesh()
    fn.create([om.MPoint(*p) for p in verts],[len(f) for f in faces],[i for f in faces for i in f])
    trans=om.MFnDagNode(fn.parent(0)).fullPathName()
    trans=c.rename(trans,name)
    trans=c.parent(trans,parent)[0]
    assign(trans,mat)
    # Independent projected face UVs, in physical metre scale.
    u,v=[],[]
    for face in faces:
        a,b,d=[om.MVector(*verts[i]) for i in face[:3]]
        n=(b-a)^(d-a)
        axis=max(range(3),key=lambda j:abs(n[j]))
        for i in face:
            p=verts[i]
            uv=(p[2],p[1]) if axis==0 else ((p[0],p[2]) if axis==1 else (p[0],p[1]))
            u.append(uv[0]/100.); v.append(uv[1]/100.)
    fn.setUVs(u,v)
    fn.assignUVs([len(f) for f in faces],list(range(len(u))))
    if smooth:
        for e in range(fn.numEdges): fn.setEdgeSmoothing(e,True)
        fn.cleanupEdgeSmoothing()
    return trans

class Batch:
    def __init__(self): self.data=defaultdict(lambda:[[],[]])
    def add(self,mat,verts,faces):
        vs,fs=self.data[mat]; off=len(vs)
        vs.extend(verts); fs.extend([[i+off for i in f] for f in faces])
    def finish(self,name,parent,smooth=False):
        return [mesh(name+'_'+mat,vs,fs,mat,parent,smooth) for mat,(vs,fs) in self.data.items() if vs]

def bevel(obj, width=1.5):
    try:
        c.polyBevel3(obj,offset=width,segments=1,ch=False,mergeVertices=True)
        c.polySoftEdge(obj,angle=35,ch=False)
    except Exception as exc:
        print('Bevel skipped:',obj,str(exc)[:100])
    return obj

def cube(name,pos,size,mat,parent,bev=1.5,ry=0):
    obj=c.polyCube(name=name,w=size[0],h=size[1],d=size[2],ch=False)[0]
    c.xform(obj,translation=pos,rotation=(0,ry,0))
    obj=c.parent(obj,parent)[0]; assign(obj,mat)
    if bev: bevel(obj,bev)
    c.makeIdentity(obj,apply=True,t=True,r=True,s=True,n=0)
    return obj

def radial(r,a,y=0): return (r*math.sin(a),y,r*math.cos(a))

def transform_point(p,a,origin):
    x,y,z=p; sa,ca=math.sin(a),math.cos(a)
    return (origin[0]+x*ca+z*sa,origin[1]+y,origin[2]-x*sa+z*ca)

def lathe_segment(name,profile,a0,a1,mat,parent,steps=3,bev=0,jitter=0):
    n=len(profile); verts=[]; faces=[]
    for i in range(steps+1):
        a=a0+(a1-a0)*i/steps
        for r,y in profile:
            verts.append((r*math.sin(a),y+(random.uniform(-jitter,jitter) if jitter and r>.001 else 0),r*math.cos(a)))
    for i in range(steps):
        for j in range(n): faces.append([i*n+j,(i+1)*n+j,(i+1)*n+(j+1)%n,i*n+(j+1)%n])
    if abs((a1-a0)-T)>.0001:
        faces.append(list(range(n)))
        faces.append([steps*n+j for j in reversed(range(n))])
    obj=mesh(name,verts,faces,mat,parent)
    if bev: bevel(obj,bev)
    return obj

def ring_course(name,ri,ro,y0,y1,count,parent,offset=0,gaps=False,cap=False):
    if ri==0:
        # A solid un-bevelled core avoids acute, pinched bevels at the axis.
        # The visible masonry stays an annular course with well-spaced corners.
        core=ro*.36
        lathe_segment(name+'_SolidCore',[(0,y0),(core,y0),(core,y1-.12),(0,y1-.12)],0,T,'cap0' if cap else 'stone2',parent,steps=64)
        ri=ro*.35
    for i in range(count):
        a=(i+offset)*T/count
        if gaps and min(abs((a-k*math.pi/2+math.pi)%T-math.pi) for k in range(4)) < math.radians(10): continue
        da=T/count*.487
        lathe_segment('%s_%02d'%(name,i+1),[(ri,y0),(ro,y0),(ro,y1),(ri,y1)],a-da,a+da,
                      random.choice(['cap0','cap1','cap2'] if cap else ['stone0','stone1','stone2','stone3','stone4','stone5']),
                      parent,steps=3,bev=1.2 if cap else 1.5,jitter=.5)

def extrude_poly(name,poly,zback,zfront,mat,parent,bev=0):
    n=len(poly)
    vs=[(x,y,zfront) for x,y in poly]+[(x,y,zback) for x,y in poly]
    fs=[list(range(n)),list(reversed(range(n,2*n)))]
    fs += [[i,n+i,n+(i+1)%n,(i+1)%n] for i in range(n)]
    obj=mesh(name,vs,fs,mat,parent)
    if bev: bevel(obj,bev)
    return obj

def tube(batch,points,radius,mat,sides=5):
    vs=[]; fs=[]
    for i,p in enumerate(points):
        tangent=om.MVector(*points[min(i+1,len(points)-1)])-om.MVector(*points[max(0,i-1)])
        tangent.normalize()
        axis=om.MVector(0,1,0) if abs(tangent.y)<.9 else om.MVector(1,0,0)
        u=(tangent^axis).normal(); v=(tangent^u).normal()
        rr=radius*(1-.25*i/max(1,len(points)-1))
        for j in range(sides):
            q=om.MVector(*p)+rr*(u*math.cos(T*j/sides)+v*math.sin(T*j/sides))
            vs.append(tuple(q))
    for i in range(len(points)-1):
        for j in range(sides): fs.append([i*sides+j,i*sides+(j+1)%sides,(i+1)*sides+(j+1)%sides,(i+1)*sides+j])
    fs.extend([list(reversed(range(sides))),[(len(points)-1)*sides+j for j in range(sides)]])
    batch.add(mat,vs,fs)

def sphere(batch,pos,scale,mat,segments=8,rings=5):
    vs=[]; fs=[]
    for i in range(rings+1):
        a=math.pi*i/rings
        for j in range(segments):
            b=T*j/segments
            vs.append((pos[0]+scale[0]*math.sin(a)*math.cos(b),pos[1]+scale[1]*math.cos(a),pos[2]+scale[2]*math.sin(a)*math.sin(b)))
    for i in range(rings):
        for j in range(segments): fs.append([i*segments+j,i*segments+(j+1)%segments,(i+1)*segments+(j+1)%segments,(i+1)*segments+j])
    batch.add(mat,vs,fs)

def leaf(batch,base,tip,width,mat,curl=6):
    b=om.MVector(*base); t=om.MVector(*tip); d=t-b
    side=(d^om.MVector(0,1,0))
    if side.length()<.01: side=om.MVector(1,0,0)
    side.normalize()
    vs=[]
    for k in range(5):
        f=k/4.; center=b+d*f+om.MVector(0,math.sin(f*math.pi)*curl,0)
        w=math.sin(math.pi*f)**.8*width/2
        for q in [-1,0,1]:
            p=center+side*(w*q)+om.MVector(0,(1-abs(q))*w*.23,0)
            vs.append(tuple(p))
    fs=[]
    for k in range(4):
        for q in range(2): fs.append([k*3+q,k*3+q+1,(k+1)*3+q+1,(k+1)*3+q])
    batch.add(mat,vs,fs)

def ribbon(batch,points,width,mat,horizontal=False):
    vs=[];fs=[]
    for i,p in enumerate(points):
        if horizontal:
            prev=points[max(0,i-1)]; nex=points[min(len(points)-1,i+1)]
            dx,dz=nex[0]-prev[0],nex[2]-prev[2]; l=math.hypot(dx,dz) or 1
            off=(-dz/l*width/2,0,dx/l*width/2)
        else: off=(width/2,0,0)
        vs.extend([tuple(p[j]-off[j] for j in range(3)),tuple(p[j]+off[j] for j in range(3))])
        if i: fs.append([2*i-2,2*i-1,2*i+1,2*i])
    batch.add(mat,vs,fs)

def build_materials():
    for i,col in enumerate([(.51,.43,.35),(.59,.50,.40),(.64,.54,.43),(.56,.47,.39),(.67,.57,.46),(.61,.50,.40)]): material('stone'+str(i),col,.78)
    for i,col in enumerate([(.72,.62,.49),(.77,.67,.54),(.68,.58,.47)]): material('cap'+str(i),col,.7)
    material('mortar',(.24,.245,.22),.95)
    material('innerStone',(.34,.38,.34),.8)
    material('gold',(.9,.47,.085),.28,.72)
    material('goldLight',(1.,.69,.19),.2,.55,emission=.06)
    material('crystalGold',(.96,.46,.045),.22,.45,emission=.28)
    material('crystalLight',(1.,.79,.29),.23,.3,emission=.35)
    material('crystalShade',(.61,.24,.025),.25,.65)
    material('water',(.075,.32,.43),.17,.12,transmission=.15)
    material('fall',(.25,.55,.69),.16,.08,transmission=.3)
    material('fallLight',(.52,.75,.83),.19,.02,transmission=.18)
    material('caustic',(.31,.67,.77),.25,emission=.12)
    material('foam',(.79,.91,.91),.43,emission=.08)
    material('cloth',(.38,.085,.036),.95)
    material('clothDark',(.21,.043,.022),.95)
    material('soil',(.105,.12,.055),1)
    for i,col in enumerate([(.18,.25,.035),(.29,.37,.045),(.4,.45,.055),(.48,.51,.085),(.24,.33,.055)]): material('leaf'+str(i),col,.85)
    material('stem',(.22,.29,.06),.85)
    material('flowerWhite',(.88,.85,.72),.55)
    material('flowerLilac',(.55,.47,.73),.55)
    material('flowerPink',(.78,.58,.66),.55)
    material('pollen',(.94,.64,.12),.6)
    material('moss',(.22,.29,.055),1)
    material('mossLight',(.34,.38,.07),1)
    material('crack',(.21,.19,.135),1)
    material('ground',(.12,.15,.19),.88)
    # Fine procedural relief remains self-contained in the .ma.
    noise=c.shadingNode('noise',asTexture=True,name='Stone_FineGrain')
    c.setAttr(noise+'.frequency',35)
    bump=c.shadingNode('bump2d',asUtility=True,name='Stone_SubtleRelief')
    c.setAttr(bump+'.bumpDepth',.025)
    c.connectAttr(noise+'.outAlpha',bump+'.bumpValue')
    for key,mat in MATS.items():
        if key.startswith(('stone','cap')): c.connectAttr(bump+'.outNormal',mat+'.normalCamera')

def build_architecture():
    g=GROUPS['Architecture']
    ring_course('FoundationBlock',0,563,0,27,64,g,cap=True)
    # Recessed stone lining and the visible courses of the circular retaining wall.
    lathe_segment('BasinFloor',[(0,29),(500,29),(500,43),(0,43)],0,T,'innerStone',g,steps=128)
    lathe_segment('BasinInnerLining',[(487,42),(505,42),(505,153),(487,153)],0,T,'innerStone',g,steps=128)
    for row in range(3): ring_course('PoolWall_Course%d'%(row+1),497,550,29+row*41,68+row*41,64,g,offset=.5*(row%2),gaps=True)
    ring_course('PoolCoping',480,565,153,177,64,g,gaps=True,cap=True)
    for k in range(4):
        a=k*math.pi/2
        stairs=group('Staircase_%s'%['South','East','North','West'][k],g)
        for step in range(6):
            rr=687-step*29; height=(step+1)*26.5
            for block in range(3):
                pos=transform_point(((block-1)*57.8,height/2,rr),a,(0,0,0))
                cube('Step_%02d_Stone_%d'%(step+1,block+1),pos,(56.6,height,29.2),random.choice(['cap0','cap1','cap2']),stairs,1.3,math.degrees(a))
        for side in [-1,1]:
            poly=[(520,0),(711,0),(711,37),(520,185)]
            # Local x in poly is radial; extrusion is across stair width.
            n=len(poly);vs=[]
            for xx in [side*105-13,side*105+13]:
                vs.extend([transform_point((xx,yy,rr),a,(0,0,0)) for rr,yy in poly])
            fs=[list(range(n)),list(reversed(range(n,2*n)))]+[[i,n+i,n+(i+1)%n,(i+1)%n] for i in range(n)]
            rail=mesh('Stair_StoneStringer',vs,fs,'stone2',stairs);bevel(rail,2)
            for j in range(6):
                ro=711-j*191/6;ri=ro-191/6+1.2
                yo=37+(711-ro)*148/191;yi=37+(711-ri)*148/191
                profile=[(ri,yi),(ro,yo),(ro,yo+11),(ri,yi+11)]
                vs=[transform_point((xx,yy,rr),a,(0,0,0)) for xx in [side*105-16,side*105+16] for rr,yy in profile]
                fs=[list(range(4)),list(reversed(range(4,8)))]+[[i,4+i,4+(i+1)%4,(i+1)%4] for i in range(4)]
                obj=mesh('StringerCap',vs,fs,'cap1',stairs);bevel(obj,1.2)
    progress('architecture')

def build_upper():
    g=GROUPS['UpperBasin']
    ring_course('CentralFoot',0,183,89,118,24,g,cap=True)
    ring_course('CentralPlinth',0,171,119,161,24,g)
    ring_course('PlinthCap',0,177,162,178,24,g,cap=True)
    for row in range(3): ring_course('Drum_Course%d'%row,0,123,179+row*40,217+row*40,18,g,offset=(row%2)*.5)
    # Splayed stone brackets carry the wide upper bowl.
    for i in range(12):
        a=i*T/12; da=T/12*.47
        lathe_segment('Bowl_Corbel_%02d'%i,[(107,255),(130,255),(163,281),(211,305),(212,318),(107,318)],a-da,a+da,random.choice(['stone0','stone1','stone2']),g,steps=3,bev=1.8)
    ring_course('UpperBowl_LowerBand',120,242,304,326,32,g,cap=True)
    ring_course('UpperBowl_StoneWall',222,250,327,358,32,g)
    ring_course('UpperBowl_Coping',222,257,359,376,32,g,cap=True)
    lathe_segment('UpperBasinLining',[(0,326),(235,326),(235,337),(0,337)],0,T,'innerStone',g,steps=96)
    for idx,(r,y0,y1) in enumerate([(104,366,390),(85,391,415),(60,416,438)]):
        ring_course('SculpturePlinth_%d'%idx,0,r,y0,y1,16,g,cap=True)
    progress('upper basin')

def build_sculpture():
    g=GROUPS['HaloCrystal']; details=Batch()
    # The two separate, open ends are a defining feature of the concept.
    arcs=[(-76,73,7),(108,255,7)]
    index=0
    for start,end,count in arcs:
        for k in range(count):
            index+=1
            a0=math.radians(start+(end-start)*k/count+.65)
            a1=math.radians(start+(end-start)*(k+1)/count-.65)
            ro=165+random.uniform(-2.8,2.8); ri=119+random.uniform(-2,2)
            poly=[]
            for j in range(4):
                a=a0+(a1-a0)*j/3; poly.append((ro*math.cos(a),552+ro*math.sin(a)))
            for j in reversed(range(4)):
                a=a0+(a1-a0)*j/3; poly.append((ri*math.cos(a),552+ri*math.sin(a)))
            obj=extrude_poly('Halo_AncientStone_%02d'%index,poly,-26,29,random.choice(['stone1','stone2','stone3','cap0']),g,2.1)
            # Small surface fractures branch from stone joints.
            mid=(a0+a1)/2; rr=random.uniform(135,155)
            p=(rr*math.cos(mid),552+rr*math.sin(mid),29.7)
            pts=[p,(p[0]+random.uniform(-8,8),p[1]-7,30),(p[0]+random.uniform(-10,10),p[1]-15,30)]
            tube(details,pts,.55,'crack',4)
            if k in [0,1,4,6]:
                pts=[(p[0]-5,p[1]+8,30.3),(p[0]-3,p[1]+3,30.3),(p[0]-6,p[1]-2,30.3)]
                tube(details,pts,1.7,'moss',5)
    outer=[(0,655),(-89,540),(0,423),(89,540)]
    inner=[(0,617),(-56,540),(0,462),(56,540)]
    # Four individually bevelled frame members leave the centre open.
    for i in range(4):
        j=(i+1)%4
        poly=[outer[i],outer[j],inner[j],inner[i]]
        extrude_poly('Diamond_StoneFrame_%d'%i,poly,6,59,'cap1' if i in [0,3] else 'stone3',g,2)
    outer_gold=[(0,620),(-58,540),(0,459),(58,540)]
    inner_gold=[(0,608),(-48,540),(0,473),(48,540)]
    for i in range(4):
        j=(i+1)%4
        extrude_poly('Diamond_GoldBezel_%d'%i,[outer_gold[i],outer_gold[j],inner_gold[j],inner_gold[i]],57,62,'gold',g,.6)
    ring=[(0,608,61),(-48,540,61),(0,473,61),(48,540,61)]
    peak=(0,541,99);back=(0,541,31)
    for i in range(4):
        j=(i+1)%4
        mat=['crystalLight','crystalGold','crystalShade','goldLight'][i]
        mesh('Crystal_Facet_%d'%i,[ring[i],ring[j],peak],[[0,1,2]],mat,g)
        mesh('Crystal_BackFacet_%d'%i,[ring[j],ring[i],back],[[0,1,2]],'crystalGold',g)
    mesh('Crystal_LuminousSpine',[(0,608,61),(-4,541,100),(0,473,61),(4,541,100)],[[0,1,2],[0,2,3]],'crystalLight',g)
    details.finish('Halo_Patina',g)
    progress('halo and crystal')

def build_water():
    g=GROUPS['Water']; caustics=Batch(); foam=Batch()
    for title,ri,ro,level,nr,nt in [('LowerPool',174,493,108,16,160),('UpperPool',103,237,368,10,128)]:
        vs=[];fs=[]
        for i in range(nr+1):
            r=ri+(ro-ri)*i/nr
            for j in range(nt):
                a=T*j/nt
                y=level+.65*math.sin(a*11+r*.13)+.4*math.cos(a*17-r*.1)
                vs.append(radial(r,a,y))
        for i in range(nr):
            for j in range(nt):fs.append([i*nt+j,(i+1)*nt+j,(i+1)*nt+(j+1)%nt,i*nt+(j+1)%nt])
        mesh(title+'_WaterSurface',vs,fs,'water',g,True)
        # Soft, irregular cellular caustics over the blue pool surface.
        spacing=35 if title=='LowerPool' else 26
        for ix in range(-16,17):
            for iz in range(-16,17):
                x=ix*spacing*1.5;z=(iz+(ix%2)*.5)*spacing*1.732
                rad=math.hypot(x,z)
                if not ri+25<rad<ro-22:continue
                pts=[]
                for j in range(7):
                    a=T*(j%6)/6
                    rr=spacing*random.uniform(.63,.95)
                    pts.append((x+rr*math.cos(a),level+1.25,z+rr*math.sin(a)))
                # Broken edges keep the pattern from becoming a rigid grid.
                if random.random()<.82:ribbon(caustics,pts[:random.choice([4,5,7])],random.uniform(.55,1.3),'caustic',True)
        # Ripples along the inner and outer edges.
        for rr in [ri+6,ri+13,ro-8]:
            for k in range(7):
                a0=random.random()*T;length=random.uniform(.16,.5)
                pts=[radial(rr+1.5*math.sin(j*.9),a0+length*j/16,level+1.5) for j in range(17)]
                ribbon(caustics,pts,1.,'caustic',True)
    for k in range(8):
        a=k*T/8
        width=44 if k%2==0 else 25
        local=[];fs=[];rows=32;cols=10
        def point(t,q):
            # A short overflow lip curls smoothly into a falling sheet.
            r=230+60*(1-math.exp(-t*4.2))
            y=369-260*(t**1.48)
            xx=q*width*(1+.17*t)+math.sin(t*19+q*5+k)*1.4*t
            rr=r+math.sin(t*17+q*8)*1.5*t
            return transform_point((xx,y,rr),a,(0,0,0))
        for i in range(rows+1):
            for j in range(cols+1):local.append(point(i/rows,j/cols-.5))
        for i in range(rows):
            for j in range(cols):
                s=i*(cols+1)+j;fs.append([s,s+cols+1,s+cols+2,s+1])
        mesh('Waterfall_%02d_Sheet'%(k+1),local,fs,'fall',g,True)
        # Individually tapered pale folds run down the sheet.
        streaks=Batch()
        for j in range(9):
            q=(j/8-.5)*.94
            vv=[];ff=[]
            for i in range(rows+1):
                t=i/rows;w=(.45+random.random()*.9)/width
                p1=point(t,q-w);p2=point(t,q+w)
                p1=tuple(p1[n]+(math.sin(a)*.7 if n==0 else math.cos(a)*.7 if n==2 else 0) for n in range(3))
                p2=tuple(p2[n]+(math.sin(a)*.7 if n==0 else math.cos(a)*.7 if n==2 else 0) for n in range(3))
                vv.extend([p1,p2])
                if i:ff.append([2*i-2,2*i,2*i+1,2*i-1])
            streaks.add('foam' if j in [0,8] else 'fallLight',vv,ff)
        streaks.finish('Waterfall_%02d_Folds'%(k+1),g,True)
        center=radial(290,a,110)
        for ripple in range(4):
            rr=13+ripple*12
            pts=[]
            for j in range(65):
                ang=j*T/64;rad=rr*(1+.12*math.sin(ang*7+k))
                pts.append((center[0]+rad*math.cos(ang),110.3+random.random()*.8,center[2]+rad*.7*math.sin(ang)))
            ribbon(foam,pts,2.9-ripple*.5,'foam' if ripple<2 else 'caustic',True)
        for j in range(22):
            ang=random.random()*T;rr=random.uniform(3,31)
            p=(center[0]+rr*math.cos(ang),110+random.uniform(0,3),center[2]+rr*.7*math.sin(ang))
            sphere(foam,p,(random.uniform(2,7),random.uniform(.5,2),random.uniform(2,5)),'foam',7,3)
        for j in range(11):
            ang=random.random()*T;rr=random.uniform(8,28)
            x,z=center[0]+rr*math.cos(ang),center[2]+rr*math.sin(ang)
            sphere(foam,(x,random.uniform(115,136),z),(1.1,random.uniform(1.5,4.5),1.1),'foam',6,4)
        for j in range(4):
            ang=random.random()*T;h=random.uniform(12,26);dist=random.uniform(20,35)
            pts=[(center[0]+dist*math.cos(ang)*t/8,111+h*math.sin(math.pi*t/8),center[2]+dist*math.sin(ang)*t/8) for t in range(9)]
            tube(foam,pts,.9,'foam',4)
    caustics.finish('Pool_CausticLines',g)
    foam.finish('Water_SplashAndFoam',g,True)
    progress('waterfalls and pools')

def build_gardens():
    garden=GROUPS['Gardens']
    for k in range(4):
        a=math.radians(45+90*k); origin=radial(527,a)
        g=group('FlowerPlanter_%02d'%(k+1),garden)
        def box(n,y,s,m,b=2):return cube(n,(origin[0],y,origin[2]),s,m,g,b,math.degrees(a))
        box('Planter_Foot',20,(161,40,161),'stone3')
        box('Planter_BaseMoulding',44,(146,13,146),'cap0')
        box('Planter_Shaft',111,(131,122,131),'stone2')
        # Inset panels and raised borders on all four sides.
        for j in range(4):
            b=a+j*math.pi/2
            for xx in [-51,51]:
                cube('Planter_PanelStile',transform_point((xx,111,66.4),b,origin),(10,103,6),'cap0',g,1,math.degrees(b))
            for yy in [61,160]:
                cube('Planter_PanelRail',transform_point((0,yy,67),b,origin),(104,9,6),'cap0',g,1,math.degrees(b))
        box('Planter_Neck',174,(143,17,143),'stone1')
        box('Planter_Crown',192,(166,23,166),'cap1')
        box('Planter_DarkSoil',205,(137,7,137),'soil',1)
        plants=Batch()
        # Layered, broad pointed leaves reproduce the low, spreading foliage.
        for layer,num in [(0,42),(1,32),(2,23)]:
            for j in range(num):
                ang=random.random()*T;rb=random.uniform(8,49)
                base=(origin[0]+rb*math.cos(ang),208+random.uniform(0,12),origin[2]+rb*math.sin(ang))
                reach=random.uniform(40,83)*(1-layer*.16)
                tip=(base[0]+reach*math.cos(ang),base[1]+random.uniform(1,23)+layer*12,base[2]+reach*math.sin(ang))
                leaf(plants,base,tip,random.uniform(15,26),random.choice(['leaf0','leaf1','leaf2','leaf3','leaf4']),random.uniform(8,17))
        # Delicate white and lavender five-petal flowers on curved stems.
        for j in range(42):
            ang=random.random()*T;r=random.uniform(7,52)
            x,z=origin[0]+r*math.cos(ang),origin[2]+r*math.sin(ang)
            yy=random.uniform(251,302)
            top=(x+random.uniform(-10,10),yy,z+random.uniform(-10,10))
            tube(plants,[(x,213,z),(x+3,(yy+213)/2,z-2),top],.65,'stem',5)
            for s in [-1,1]:
                base=(x,yy-25,z)
                leaf(plants,base,(x+s*random.uniform(10,20),yy-17,z+10),6,'leaf1',1)
            flowerMat=random.choices(['flowerWhite','flowerLilac','flowerPink'],[.63,.29,.08])[0]
            radius=random.uniform(6,9.3)
            for petal in range(5):
                b=T*petal/5
                # Petals face upward, with a lifted tip and a little overlap.
                base=(top[0]+math.cos(b)*.9,yy,top[2]+math.sin(b)*.9)
                tip=(top[0]+math.cos(b)*radius,yy+random.uniform(2,5),top[2]+math.sin(b)*radius)
                leaf(plants,base,tip,random.uniform(3.5,5.8),flowerMat,1.8)
            sphere(plants,(top[0],yy+1,top[2]),(1.9,1.5,1.9),'pollen',7,4)
        for j in range(9):
            ang=random.random()*T;rr=random.uniform(15,48)
            x,z=origin[0]+rr*math.cos(ang),origin[2]+rr*math.sin(ang)
            yy=random.uniform(265,297)
            tube(plants,[(x,217,z),(x+4,yy,z+3)],.7,'stem')
            for h in range(4):
                b=h*2.4
                sphere(plants,(x+4+2*math.cos(b),yy+h*3,z+3+2*math.sin(b)),(2.4,4.1,2.4),'flowerLilac' if j%2 else 'flowerWhite',6,4)
        # Ivy trails over the front and sides of each stone planter.
        for j in range(10):
            b=a+random.uniform(-1.4,1.4)
            length=random.uniform(52,137)
            pts=[]
            for h in range(10):
                rr=79+4*math.sin(h*.6);yy=211-length*h/9
                pts.append((origin[0]+rr*math.sin(b)+4*math.sin(h),yy,origin[2]+rr*math.cos(b)))
            tube(plants,pts,.85,'stem')
            for h in range(1,10):
                p=pts[h];sgn=(-1)**h
                tip=(p[0]+math.cos(b)*sgn*13+math.sin(b)*4,p[1]-10,p[2]-math.sin(b)*sgn*13+math.cos(b)*4)
                leaf(plants,p,tip,random.uniform(7,12),random.choice(['leaf0','leaf1','leaf2']),2)
        plants.finish('Planter_%02d_Botanical'%(k+1),g)
    progress('flower planters')

def banner_poly(batch,poly,z,mat,a,origin):
    vs=[transform_point((x,y,z),a,origin) for x,y in poly]
    batch.add(mat,vs,[list(range(len(poly)))])

def build_banners():
    allg=GROUPS['Banners']
    emblem=[(0,1),(-.17,.59),(-.1,.26),(-.31,.43),(-.41,.72),(-.63,.46),(-.5,.06),(-.13,-.19),(-.11,-.52),(0,-.73),(.11,-.52),(.13,-.19),(.5,.06),(.63,.46),(.41,.72),(.31,.43),(.1,.26),(.17,.59)]
    for k in range(4):
        a=math.radians(19+90*k);origin=radial(535,a)
        g=group('HeraldicPier_%02d'%(k+1),allg)
        for name,y,size,mat in [('Foot',18,(122,36,117),'stone3'),('Shaft',101,(105,130,100),'stone2'),('Neck',166,(113,13,112),'cap0'),('Crown',182,(132,22,127),'cap1')]:
            cube('BannerPier_'+name,(origin[0],y,origin[2]),size,mat,g,1.7,math.degrees(a))
        b=Batch()
        shield=[(-39,168),(-37,64),(0,36),(37,64),(39,168)]
        banner_poly(b,shield,53,'gold',a,origin)
        cloth=[(-34,162),(-32,67),(0,43),(32,67),(34,162)]
        banner_poly(b,cloth,54.1,'cloth',a,origin)
        banner_poly(b,[(-31,160),(-29,71),(-20,62),(-24,161)],54.5,'clothDark',a,origin)
        emblem_poly=[(x*29,109+y*39) for x,y in emblem]
        banner_poly(b,emblem_poly,55.5,'goldLight',a,origin)
        banner_poly(b,[(0,153),(-3,119),(0,108),(3,119)],56,'gold',a,origin)
        # Gold hem piping follows the pointed banner silhouette.
        pts=[transform_point((x,y,54.6),a,origin) for x,y in shield+[shield[0]]]
        tube(b,pts,.85,'goldLight',5)
        rod=[transform_point((x,171,55),a,origin) for x in [-44,44]]
        tube(b,rod,1.6,'gold',8)
        for xx in [-42,42]:sphere(b,transform_point((xx,171,55),a,origin),(2.5,2.5,2.5),'gold',8,5)
        b.finish('Banner_%02d_ClothAndInsignia'%(k+1),g)
    progress('heraldic banners')

def build_patina():
    b=Batch();g=GROUPS['Details']
    # Angular olive patches sit on coping joints and drip over the stonework.
    for i in range(55):
        a=random.random()*T
        if min(abs((a-k*math.pi/2+math.pi)%T-math.pi) for k in range(4))<.18:continue
        rr=random.uniform(495,550);wide=random.uniform(3,10)
        pts=[radial(rr,a-wide/rr,177.8),radial(rr+random.uniform(12,30),a-wide/rr,177.8),radial(min(562,rr+25),a+wide/rr,177.8),radial(rr-9,a+wide*.6/rr,177.8)]
        b.add('mossLight' if i%3==0 else 'moss',pts,[[0,1,2],[0,2,3]])
        if i%2==0:
            h=random.uniform(18,68);r=550.8
            points=[radial(r,a,152),radial(r+.2,a+.006,148-h*.25),radial(r+.3,a-.004,148-h*.6),radial(r+.3,a+.002,148-h)]
            tube(b,points,random.uniform(1.7,3.2),'moss',4)
    for radius,y,drop in [(251,355,32),(174,158,32),(122,283,82),(103,390,22)]:
        for j in range(16):
            a=random.random()*T
            points=[radial(radius+.8,a,y),radial(radius+.9,a+.012,y-drop*.3),radial(radius+1,a+.003,y-drop*random.uniform(.6,1))]
            tube(b,points,random.uniform(1.4,3.5),'moss' if j%2 else 'mossLight',5)
    for j in range(80):
        a=random.random()*T;y=random.uniform(45,145)
        if min(abs((a-k*math.pi/2+math.pi)%T-math.pi) for k in range(4))<.2:continue
        points=[radial(551,a,y),radial(551.4,a+.01,y-5),radial(551.4,a+.006,y-13)]
        tube(b,points,.55,'crack',4)
    b.finish('Stone_MossAndHairlineCracks',g)
    progress('stone patina')

def aim(obj,point):
    con=c.aimConstraint(point,obj,aimVector=(0,0,-1),upVector=(0,1,0),worldUpType='scene')[0]
    c.delete(con)

def build_presentation():
    g=GROUPS['Presentation']
    floor=cube('Studio_Ground',(0,-13,0),(20000,18,20000),'ground',g,0)
    cam,shape=c.camera(name='Fountain_Beauty_CAM')
    c.parent(cam,g);c.setAttr(shape+'.orthographic',1);c.setAttr(shape+'.orthographicWidth',1730)
    c.setAttr(shape+'.nearClipPlane',5);c.setAttr(shape+'.farClipPlane',30000)
    c.setAttr(shape+'.focalLength',50)
    c.xform(cam,translation=(650,1250,2000))
    target=c.spaceLocator(name='CameraAim_TEMP')[0];c.xform(target,translation=(0,335,0));aim(cam,target);c.delete(target)
    c.setAttr(shape+'.renderable',1)
    for shape0 in c.ls(type='camera'):
        if shape0!=shape:c.setAttr(shape0+'.renderable',0)
    for title,loc,intensity,col,angle in [('Key',(-700,1300,900),3.1,(1.,.83,.65),7),('Fill',(950,650,400),.85,(.64,.77,1.),12),('Rim',(200,1100,-950),1.7,(.86,.92,1.),5)]:
        light=c.directionalLight(name='Studio_'+title,intensity=intensity,rgb=col)
        tr=light if c.nodeType(light)=='transform' else c.listRelatives(light,parent=True)[0]
        sh=c.listRelatives(tr,shapes=True)[0]
        tr=c.parent(tr,g)[0];c.xform(tr,translation=loc)
        target=c.spaceLocator()[0];c.xform(target,translation=(0,200,0));aim(tr,target);c.delete(target)
        if c.attributeQuery('aiAngle',node=sh,exists=True):c.setAttr(sh+'.aiAngle',angle)
    light=c.shadingNode('aiSkyDomeLight',asLight=True,name='Studio_Ambient')
    tr=light if c.nodeType(light)=='transform' else c.listRelatives(light,parent=True)[0]
    dome=c.listRelatives(tr,shapes=True)[0]
    c.parent(tr,g)
    c.setAttr(dome+'.color',.66,.76,.91,type='double3');c.setAttr(dome+'.intensity',.4)
    if c.attributeQuery('camera',node=dome,exists=True):c.setAttr(dome+'.camera',0)
    import mtoa.core
    mtoa.core.createOptions()
    c.setAttr('defaultRenderGlobals.currentRenderer','arnold',type='string')
    c.setAttr('defaultRenderGlobals.imageFilePrefix',OUT+'/renders/Fountain_Concept03_Beauty',type='string')
    c.setAttr('defaultResolution.width',1500);c.setAttr('defaultResolution.height',1500)
    c.setAttr('defaultResolution.deviceAspectRatio',1)
    c.setAttr('defaultArnoldRenderOptions.AASamples',4)
    c.setAttr('defaultArnoldRenderOptions.GIDiffuseSamples',2)
    c.setAttr('defaultArnoldRenderOptions.GISpecularSamples',2)
    c.setAttr('defaultArnoldRenderOptions.GITransmissionSamples',2)
    c.setAttr('defaultArnoldRenderOptions.GIDiffuseDepth',2)
    c.setAttr('defaultArnoldDriver.aiTranslator','png',type='string')
    for panel in c.getPanel(type='modelPanel'):
        c.modelEditor(panel,e=True,displayAppearance='smoothShaded',displayTextures=True,displayLights='default',twoSidedLighting=True,grid=False,cameras=False,lights=False,joints=False,locators=False,selectionHiliteDisplay=False)
        try:
            c.modelEditor(panel,e=True,rendererName='vp2Renderer')
        except:pass
    for attr,val in [('ssaoEnable',1),('multiSampleEnable',1),('multiSampleCount',8)]:
        try:c.setAttr('hardwareRenderingGlobals.'+attr,val)
        except:pass
    c.displayRGBColor('background',.095,.12,.16)
    c.displayRGBColor('backgroundTop',.095,.12,.16)
    c.displayRGBColor('backgroundBottom',.095,.12,.16)
    c.modelPanel('modelPanel4',edit=True,camera=cam)
    c.select(clear=True)
    c.setAttr(floor+'.primaryVisibility',1)
    progress('presentation')

def main():
    global ROOT,GROUPS
    if c.objExists('Fountain_Concept03_GRP'):
        raise RuntimeError('An existing Fountain_Concept03_GRP is present; refusing to replace it.')
    c.undoInfo(openChunk=True,chunkName='Build Concept03 Fountain')
    c.refresh(suspend=True)
    try:
        c.currentUnit(linear='cm')
        ROOT=c.group(empty=True,name='Fountain_Concept03_GRP')
        c.addAttr(ROOT,longName='referenceImage',dataType='string');c.setAttr(ROOT+'.referenceImage',OUT+'/ConceptArt_03_Foutain_v1.png',type='string')
        c.addAttr(ROOT,longName='assetNotes',dataType='string');c.setAttr(ROOT+'.assetNotes','Central Fountain | concept 03 | centimetres | editable modular stonework, four stairways, four flower planters, open stone halo and golden crystal | static water meshes',type='string')
        for i,key in enumerate(['Architecture','UpperBasin','HaloCrystal','Water','Gardens','Banners','Details','Presentation']):GROUPS[key]=group('G%02d_%s_GRP'%(i+1,key))
        build_materials()
        build_architecture()
        build_upper()
        build_sculpture()
        build_water()
        build_gardens()
        build_banners()
        build_patina()
        build_presentation()
        c.delete(ROOT,constructionHistory=True)
        os.makedirs(OUT+'/renders',exist_ok=True)
        c.file(rename=OUT+'/Fountain_Concept03_v001.ma')
        c.file(save=True,type='mayaAscii',force=True)
        meshes=c.listRelatives(ROOT,allDescendents=True,type='mesh',fullPath=True) or []
        report={'scene':c.file(q=True,sn=True),'meshes':len(meshes),'faces':sum(c.polyEvaluate(n,face=True) for n in meshes),'vertices':sum(c.polyEvaluate(n,vertex=True) for n in meshes),'materials':len(MATS),'seconds':round(time.time()-START,1)}
        with open(OUT+'/fountain_report.json','w') as f:json.dump(report,f,indent=2)
        progress('saved')
    except:
        with open(OUT+'/build_error.txt','w') as f:f.write(traceback.format_exc())
        progress('error')
        raise
    finally:
        c.refresh(suspend=False)
        c.undoInfo(closeChunk=True)
        c.refresh(force=True)

main()
