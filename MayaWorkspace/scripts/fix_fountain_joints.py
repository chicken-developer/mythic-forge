"""Revision 003: fitted architectural joints and close-fitting halo masonry.
Execute inside the open v002 Maya scene. Modeling, normals and UVs only.
"""
import maya.cmds as c
import maya.api.OpenMaya as om
import math, json, time, traceback, os, re

OUT='E:/Repos/mythic-forge/MayaWorkspace'
DEST=OUT+'/Fountain_Concept03_Modeling_v003.ma'
STAGE='_Fountain_JointFix_STAGE'
START=time.time()
SOURCE=open(OUT+'/scripts/modeling_fountain.py',encoding='utf-8').read()
BASE={}
exec(compile(SOURCE[:SOURCE.rfind('\ntry:\n    main()')],'fountain_modeling_helpers','exec'),BASE)
NEW=[]
OLD=[]
BEVELS={}
JOINTS=[]

def status(stage):
    with open(OUT+'/joints_status.json','w') as f:json.dump({'stage':stage,'seconds':round(time.time()-START,1)},f)

def mesh(name,verts,faces):
    return BASE['mesh'](name,verts,faces,STAGE)

def fn(obj):return BASE['fn_mesh'](obj)

def finish(obj,width=1):
    BASE['finish_edges'](obj,width,2)
    BEVELS[obj]=width
    return obj

def sweep(name,profile,a0,a1,steps=14):
    verts=[];faces=[];n=len(profile)
    for i in range(steps+1):
        a=a0+(a1-a0)*i/steps
        verts.extend(BASE['radial'](r,a,y) for r,y in profile)
    for i in range(steps):
        for j in range(n):faces.append([i*n+j,(i+1)*n+j,(i+1)*n+(j+1)%n,i*n+(j+1)%n])
    faces.extend([list(range(n)),[steps*n+j for j in reversed(range(n))]])
    return mesh(name,verts,faces)

def rail_volume(name,side,angle):
    # The flat upper return stays at coping height instead of cutting through it.
    profile=[(490,0),(715,0),(715,36),(550,178),(490,178)]
    n=len(profile)
    verts=[BASE['world']((xx,y,r),angle) for xx in [side*103-16,side*103+16] for r,y in profile]
    faces=[list(range(n)),list(reversed(range(n,2*n)))]
    faces.extend([[i,n+i,n+(i+1)%n,(i+1)%n] for i in range(n)])
    return mesh(name,verts,faces)

def post_volume(name,angle):
    origin=BASE['radial'](527,angle)
    verts=[BASE['world']((x,y,z),angle,origin) for y in [0,183] for x,z in [(-74,74),(74,74),(74,-74),(-74,-74)]]
    faces=[[3,2,1,0],[4,5,6,7]]+[[i,(i+1)%4,4+(i+1)%4,4+i] for i in range(4)]
    return mesh(name,verts,faces)

def architecture():
    # Chamfer the swept profile before joining volumes. This keeps the fillet
    # loops regular and avoids offsetting complex boolean corner n-gons.
    profile=[(497,28),(549,28),(549,156.2),(549.8,157),
             (563.2,157),(564,157.8),(564,177.2),(563.2,178),
             (480.8,178),(480,177.2),(480,157.8),(480.8,157),
             (496.2,157),(497,156.2)]
    # Solve one quarter, then repeat its welded topology around the basin.
    # This also keeps all four junctions geometrically identical.
    a=math.pi
    wall=sweep('JointWall_TMP',profile,a+math.radians(10),a+math.radians(80))
    rail1=rail_volume('JointRailA_TMP',1,a)
    rail2=rail_volume('JointRailB_TMP',-1,a+math.pi/2)
    pier=post_volume('JointPier_TMP',a+math.pi/4)
    finish(rail1,.8);finish(rail2,.8);finish(pier,1.0)
    canonical=c.polyCBoolOp(wall,rail1,rail2,pier,operation=1,useCarveBooleans=True,
                           constructionHistory=False,mergeUVSets=1,name='JointQuarter_Canonical_TMP')[0]
    canonical=c.parent(canonical,STAGE)[0]
    c.sets(canonical,e=True,forceElement='initialShadingGroup')
    c.polyMergeVertex(canonical,distance=.001,constructionHistory=False)
    c.polySoftEdge(canonical,angle=30,constructionHistory=False)
    c.xform(canonical,worldSpace=True,pivots=(0,0,0))
    for k in range(4):
        result=c.duplicate(canonical,name='Basin_Quarter_%02d_JFIX'%(k+1),returnRootsOnly=True)[0]
        c.rotate(0,k*90-180,0,result,relative=True,worldSpace=True)
        c.makeIdentity(result,apply=True,translate=True,rotate=True,scale=True)
        NEW.append((result,'Foundation_GRP','Basin_Quarter_%02d'%(k+1)))
        for prefix in ['Pool_Wall','Pool_Rim','Corner_Pier']:
            OLD.append('%s_%02d'%(prefix,k+1))
        OLD.extend(['StairRail_%02d_R'%(k+1),'StairRail_%02d_L'%((k+1)%4+1)])
    c.delete(canonical)
    status('four continuous wall, coping and rail joints')

def stairs():
    for k in range(4):
        angle=k*math.pi/2;verts=[];faces=[]
        def p(side,i,j):return BASE['world']((side*87,j*178/6,715-i*220/6),angle)
        def face(points):
            off=len(verts);verts.extend(points);faces.append(list(range(off,off+len(points))))
        for i in range(6):
            for side in [-1,1]:
                for j in range(i+1):
                    q=[p(side,i,j),p(side,i+1,j),p(side,i+1,j+1),p(side,i,j+1)]
                    face(q if side==1 else q[::-1])
            face([p(-1,i,i+1),p(1,i,i+1),p(1,i+1,i+1),p(-1,i+1,i+1)])
            face([p(-1,i,i),p(1,i,i),p(1,i,i+1),p(-1,i,i+1)])
            face([p(-1,i,0),p(-1,i+1,0),p(1,i+1,0),p(1,i,0)])
        for j in range(6):face([p(-1,6,j),p(-1,6,j+1),p(1,6,j+1),p(1,6,j)])
        name='Staircase_%02d'%(k+1)
        obj=finish(mesh(name+'_JFIX',verts,faces),.8)
        NEW.append((obj,'Stairs_GRP',name));OLD.append(name)
    status('stair landings aligned to coping and side rails')

def halo():
    # Adjacent stones derive from the same cut planes and contour stations.
    # Only a small mortar-width separation and a light chamfer remain.
    arcs=[('Right',[-76,-56,-32,-8,14,38,57,73],
           [0,3,-4,5,-3,4,-2,0],[0,-2,3,-4,2,-3,3,0],
           [165,164,167,176,167,164,165,167]),
          ('Left',[108,128,152,176,197,218,238,255],
           [0,-3,4,-5,3,-4,2,0],[0,3,-2,4,-3,3,-2,0],
           [166,164,166,175,166,164,165,165])]
    for side,angles,inner_offsets,outer_offsets,outer_radii in arcs:
        for i in range(7):
            name='Halo_%s_%02d'%(side,i+1);verts=[];faces=[];steps=3
            for j in range(steps+1):
                t=j/steps
                # Shared endpoints give each pair a matching slanted joint.
                ai=math.radians(angles[i]+inner_offsets[i])*(1-t)+math.radians(angles[i+1]+inner_offsets[i+1])*t
                ao=math.radians(angles[i]+outer_offsets[i])*(1-t)+math.radians(angles[i+1]+outer_offsets[i+1])*t
                cut=.0012*(1-2*t)
                ai+=cut;ao+=cut
                ri=(119+(i%3-1)*.7)*(1-t)+(119+((i+1)%3-1)*.7)*t
                ro=outer_radii[i]*(1-t)+outer_radii[i+1]*t+.65*math.sin(math.pi*t)
                inn=(ri*math.cos(ai),552+ri*math.sin(ai))
                outer=(ro*math.cos(ao),552+ro*math.sin(ao))
                verts.extend([(inn[0],inn[1],-26),(outer[0],outer[1],-26),(outer[0],outer[1],29),(inn[0],inn[1],29)])
            for j in range(steps):
                for p in range(4):faces.append([j*4+p,(j+1)*4+p,(j+1)*4+(p+1)%4,j*4+(p+1)%4])
            faces.extend([list(range(4)),[steps*4+j for j in reversed(range(4))]])
            obj=finish(mesh(name+'_JFIX',verts,faces),.65)
            NEW.append((obj,'Sculpture_GRP',name));OLD.append(name)
            if i<6:JOINTS.append({'side':side,'joint':i+1,'separation_cm':round(2*.0012*142,3)})
    status('halo stones with shared slanted joints and reduced gaps')

def diamond():
    outer=[(0,655),(-89,540),(0,423),(89,540)]
    inner=[(0,617),(-56,540),(0,462),(56,540)]
    def scale(poly,s):return [(x*s,540+(y-540)*s) for x,y in poly]
    loops=[(scale(outer,.984),59),(outer,57.8),(outer,7.2),(scale(outer,.984),6),
           (scale(inner,1.023),6),(inner,7.2),(inner,57.8),(scale(inner,1.023),59)]
    verts=[(x,y,z) for poly,z in loops for x,y in poly];faces=[]
    for j in range(8):
        nj=(j+1)%8
        for i in range(4):faces.append([j*4+i,nj*4+i,nj*4+(i+1)%4,j*4+(i+1)%4])
    obj=mesh('Diamond_StoneFrame_JFIX',verts,faces)
    c.polySoftEdge(obj,angle=30,constructionHistory=False)
    NEW.append((obj,'Sculpture_GRP','Diamond_StoneFrame'));OLD.append('Diamond_StoneFrame')
    status('continuous bevel loops on the diamond frame')

def seat_pedestals():
    for name,profile in [
        ('Central_Base',[(181,43),(181,113),(165,113),(165,162),(176,162),(176,179)]),
        ('Sculpture_Base',[(104,337),(104,389),(85,389),(85,415),(60,415),(60,438)])]:
        obj=BASE['capped_lathe'](name+'_JFIX',profile,STAGE,48,1.0)
        NEW.append((obj,'CentralBody_GRP',name));OLD.append(name)

def mesh_checks(objects,uv=False):
    problems=[];faces=0;vertices=0;volumes={}
    for obj in objects:
        f=fn(obj);faces+=f.numPolygons;vertices+=f.numVertices
        if any(not math.isfinite(v) for p in f.getPoints() for v in (p.x,p.y,p.z)):problems.append((obj,'nonfinite vertex'))
        for flag in ['nonManifoldVertices','nonManifoldEdges','laminaFaces']:
            if c.polyInfo(f.fullPathName(),**{flag:True}):problems.append((obj,flag))
        it=om.MItMeshEdge(f.dagPath())
        while not it.isDone():
            if len(it.getConnectedFaces())!=2:problems.append((obj,'boundary edge',it.index()))
            it.next()
        it=om.MItMeshPolygon(f.dagPath())
        while not it.isDone():
            if it.getArea()<1e-7:problems.append((obj,'zero-area face',it.index()))
            it.next()
        pts=f.getPoints();tri_counts,tri_ids=f.getTriangles();volume=0.
        for j in range(0,len(tri_ids),3):
            a,b,d=[om.MVector(pts[tri_ids[j+k]]) for k in range(3)]
            volume+=a*(b^d)/6
        volumes[obj]=round(volume,3)
        if volume<=0:problems.append((obj,'inward or inconsistent face winding',volume))
        if uv:
            counts,ids=f.getAssignedUVs();u,v=f.getUVs()
            if len(counts)!=f.numPolygons or any(n!=f.polygonVertexCount(i) for i,n in enumerate(counts)):problems.append((obj,'incomplete UVs'))
            if not ids or min(ids)<0 or max(ids)>=len(u):problems.append((obj,'invalid UV index'))
            if any(not math.isfinite(x) or x<-.0001 or x>1.0001 for x in list(u)+list(v)):problems.append((obj,'UV outside 0-1'))
    return {'meshes':len(objects),'faces':faces,'vertices':vertices,'problems':problems,'signed_volumes_cm3':volumes}

def uv_changes(objects):
    for i,obj in enumerate(objects):
        c.polyAutoProjection(obj,planes=6,optimize=0,layout=0,scaleMode=0,projectBothDirections=False,constructionHistory=False)
        # Cut the few UV fan junctions left by automatic projection at miters.
        for _ in range(4):
            pinches=c.polyInfo(obj,nonManifoldUVs=True) or []
            if not pinches:break
            edges=c.polyListComponentConversion(pinches,fromUV=True,toEdge=True)
            c.polyMapCut(edges,constructionHistory=False)
        c.select(obj+'.map[*]',replace=True)
        c.u3dUnfold(iterations=2,pack=False,borderintersection=True,triangleflip=True,mapsize=2048,roomspace=0)
        if i%8==0:status('UV unfold %d/%d'%(i+1,len(objects)))

def commit():
    # The untouched meshes retain their geometry and unwraps.
    for name in dict.fromkeys(OLD):
        if c.objExists(name):c.delete(name)
    for obj,parent,name in NEW:
        obj=c.parent(obj,parent)[0]
        obj=c.rename(obj,name)
        c.makeIdentity(obj,apply=True,translate=True,rotate=True,scale=True)
        c.xform(obj,centerPivots=True)
    if c.objExists(STAGE):c.delete(STAGE)
    shapes=c.listRelatives('Fountain_Model_GRP',allDescendents=True,type='mesh',fullPath=True)
    objects=[c.listRelatives(sh,parent=True,fullPath=True)[0] for sh in shapes]
    pack_uv(objects)
    c.delete(objects,constructionHistory=True)
    return objects

def pack_uv(objects):
    for obj in objects:
        for _ in range(4):
            pinches=c.polyInfo(obj,nonManifoldUVs=True) or []
            if not pinches:break
            edges=c.polyListComponentConversion(pinches,fromUV=True,toEdge=True)
            c.polyMapCut(edges,constructionHistory=False)
        f=fn(obj);counts,ids=f.getAssignedUVs();u,v=f.getUVs()
        used=sorted(set(ids))
        if len(used)!=len(u):
            remap={old:new for new,old in enumerate(used)}
            new_ids=[remap[i] for i in ids]
            new_u=[u[i] for i in used];new_v=[v[i] for i in used]
            f.clearUVs();f.setUVs(new_u,new_v);f.assignUVs(counts,new_ids)
    layout_rectangles(objects)

def layout_rectangles(objects):
    """Conservative shell layout: uniform density, quarter turns, padded 0-1.

    Bounding rectangles keep even very thin bevel charts safely separated.
    Unfolded shell shapes and seams are retained.
    """
    charts=[];records={};padding=4/2048.
    for obj in objects:
        f=fn(obj);u,v=f.getUVs();u=list(u);v=list(v)
        shell_count,shell_ids=f.getUvShellsIds()
        counts,uv_ids=f.getAssignedUVs();vertex_counts,vertex_ids=f.getVertices()
        tri_counts,tri_ids=f.getTriangles();points=f.getPoints()
        members={};areas3={};areas2={}
        for i,shell in enumerate(shell_ids):members.setdefault(shell,[]).append(i)
        offset=0;tri_offset=0
        poly=om.MItMeshPolygon(f.dagPath())
        for face_id,count in enumerate(counts):
            uv_row=list(uv_ids[offset:offset+count]);vertices=list(vertex_ids[offset:offset+count]);offset+=count
            shell=shell_ids[uv_row[0]];mapping=dict(zip(vertices,uv_row))
            poly.setIndex(face_id);areas3[shell]=areas3.get(shell,0)+poly.getArea()
            area=0.
            for _ in range(tri_counts[face_id]):
                a,b,d=[mapping[tri_ids[tri_offset+j]] for j in range(3)];tri_offset+=3
                area+=abs((u[b]-u[a])*(v[d]-v[a])-(v[b]-v[a])*(u[d]-u[a]))/2
            areas2[shell]=areas2.get(shell,0)+area
        records[obj]=(f,u,v)
        for shell,indices in members.items():
            min_u=min(u[i] for i in indices);max_u=max(u[i] for i in indices)
            min_v=min(v[i] for i in indices);max_v=max(v[i] for i in indices)
            density=math.sqrt(max(areas3.get(shell,0),1e-10)/max(areas2.get(shell,0),1e-10))
            width=(max_u-min_u)*density;height=(max_v-min_v)*density
            rotate=height>width
            charts.append({'object':obj,'indices':indices,'u0':min_u,'v0':min_v,'raw_w':max_u-min_u,
                           'density':density,'w':height if rotate else width,'h':width if rotate else height,'rotate':rotate})
    charts.sort(key=lambda row:(-row['h'],-row['w']))
    def arrange(scale):
        x=0.;y=0.;row_height=0.;placements=[]
        for row in charts:
            w=row['w']*scale+2*padding;h=row['h']*scale+2*padding
            if w>1 or h>1:return None
            if x+w>1+1e-12:x=0.;y+=row_height;row_height=0.
            if y+h>1+1e-12:return None
            placements.append((x+padding,y+padding));x+=w;row_height=max(row_height,h)
        return placements
    lo=0.;hi=1/max(max(row['w'],row['h'],1e-8) for row in charts)
    for _ in range(42):
        mid=(lo+hi)/2
        if arrange(mid) is None:hi=mid
        else:lo=mid
    placements=arrange(lo*.99999)
    if placements is None:raise RuntimeError('UV shell rectangles could not be packed.')
    scale=lo*.99999
    for row,(x,y) in zip(charts,placements):
        f,u,v=records[row['object']]
        for i in row['indices']:
            du=u[i]-row['u0'];dv=v[i]-row['v0']
            if row['rotate']:du,dv=dv,row['raw_w']-du
            u[i]=x+du*row['density']*scale;v[i]=y+dv*row['density']*scale
    for f,u,v in records.values():f.setUVs(u,v);f.updateSurface()
    c.select(clear=True)

def resolve_uv_overlaps(objects):
    repaired=0
    for attempt in range(3):
        overlaps=c.polyUVOverlap([obj+'.f[*]' for obj in objects],overlappingComponents=True) or []
        if not overlaps:return repaired
        if attempt==1:
            # Only tessellate the few planar cap n-gons whose UV fans fold.
            groups={}
            for component in overlaps:groups.setdefault(component.split('.f[')[0],[]).append(component)
            for components in groups.values():c.polyTriangulate(components,constructionHistory=False)
            overlaps=c.polyUVOverlap([obj+'.f[*]' for obj in objects],overlappingComponents=True) or []
            if not overlaps:return repaired
        affected={}
        for face in c.ls(overlaps,flatten=True):
            match=re.match(r'(.+)\.f\[(\d+)\]$',face)
            if match:affected.setdefault(match.group(1),set()).add(int(match.group(2)))
        for shape,face_ids in affected.items():
            selection=om.MSelectionList();selection.add(shape)
            dag=selection.getDagPath(0)
            if dag.apiType()!=om.MFn.kMesh:dag.extendToShape()
            f=om.MFnMesh(dag);counts,ids=f.getAssignedUVs();counts=list(counts);ids=list(ids)
            u,v=f.getUVs();u=list(u);v=list(v);points=f.getPoints();offsets=[];offset=0
            for n in counts:offsets.append(offset);offset+=n
            for face_id in face_ids:
                normal=f.getPolygonNormal(face_id).normal()
                axis=om.MVector(1,0,0) if abs(normal.x)<.9 else om.MVector(0,0,1)
                axis_u=(axis-normal*(axis*normal)).normal();axis_v=(normal^axis_u).normal()
                verts=f.getPolygonVertices(face_id)
                for j,vert in enumerate(verts):
                    point=om.MVector(points[vert]);index=len(u)
                    u.append((point*axis_u)/100);v.append((point*axis_v)/100)
                    ids[offsets[face_id]+j]=index
                repaired+=1
            f.setUVs(u,v);f.assignUVs(counts,ids);f.updateSurface()
        pack_uv(objects)
        status('UV seam cleanup %d'%(attempt+1))
    return repaired

def main():
    if not c.objExists('Fountain_Model_GRP'):raise RuntimeError('Expected the fountain modeling scene to be open.')
    if c.objExists(STAGE):raise RuntimeError('Joint-fix staging geometry already exists; inspect it before restarting.')
    # Save unrecorded user edits as a separate recovery copy before replacing parts.
    if c.file(q=True,modified=True):
        backup=OUT+'/Fountain_before_joint_fix_%s.ma'%time.strftime('%Y%m%d_%H%M%S')
        c.file(backup,exportAll=True,type='mayaAscii',force=True)
    c.group(empty=True,name=STAGE,parent='Fountain_Model_GRP')
    c.group(empty=True,name='_JointStageAnchor',parent=STAGE)
    c.setAttr(STAGE+'.visibility',0)
    c.refresh(suspend=True)
    try:
        architecture();stairs();halo();diamond();seat_pedestals()
        objects=[row[0] for row in NEW]
        report=mesh_checks(objects)
        if report['problems']:
            with open(OUT+'/joints_stage_report.json','w') as f:json.dump(report,f,indent=2)
            raise RuntimeError('Staging topology: '+str(report['problems'][:8]))
        uv_changes(objects)
        objects=commit()
        repaired_uv_faces=resolve_uv_overlaps(objects)
        report=mesh_checks(objects,uv=True)
        overlaps=c.polyUVOverlap([obj+'.f[*]' for obj in objects],overlappingComponents=True) or []
        report['overlapping_uv_faces']=overlaps
        if report['problems'] or overlaps:raise RuntimeError('Final topology or UV issues: '+str(report['problems'][:5])+str(overlaps[:5]))
        report.update({'scene':DEST,'halo_shared_joints':JOINTS,'repaired_uv_faces':repaired_uv_faces,
                       'architecture':'four united wall/coping/pier/rail sections; level stair landings',
                       'base_seating':'central base extends down to the lower pool floor',
                       'materials':'Maya default lambert1 only','file_textures':c.ls(type='file'),
                       'lights':c.ls(lights=True),'seconds':round(time.time()-START,1)})
        c.select(clear=True)
        c.file(rename=DEST);c.file(save=True,type='mayaAscii',force=True)
        report['saved']=True
        with open(OUT+'/Fountain_Concept03_Joints_Report.json','w') as f:json.dump(report,f,indent=2)
        status('saved v003')
    finally:
        c.refresh(suspend=False);c.refresh(force=True)

if __name__=='__main__':
    try:main()
    except:
        with open(OUT+'/joints_error.txt','w') as f:f.write(traceback.format_exc())
        status('error')
        raise
