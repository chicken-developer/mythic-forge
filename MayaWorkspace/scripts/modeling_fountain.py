"""Simplified fountain: polygon modeling, bevels, normals and a packed UV atlas.

Run in Maya's Python Script Editor. No materials, textures, lights or render setup.
The earlier detailed asset remains saved as Fountain_Concept03_v001.ma.
"""
import maya.cmds as cmds
import maya.api.OpenMaya as om
import math, json, os, time, traceback

OUT = 'E:/Repos/mythic-forge/MayaWorkspace'
SCENE = OUT + '/Fountain_Concept03_Modeling_v002.ma'
TAU = math.tau
OBJECTS = []
BEVELS = {}
GROUPS = {}
START = time.time()

def status(stage):
    with open(OUT+'/modeling_status.json','w') as f:
        json.dump({'stage':stage,'seconds':round(time.time()-START,1)},f)

def mesh(name, vertices, faces, parent):
    unique=[]; lookup={}; remap=[]
    for p in vertices:
        key=tuple(round(v,6) for v in p)
        if key not in lookup:
            lookup[key]=len(unique); unique.append(p)
        remap.append(lookup[key])
    clean=[]
    for f in faces:
        ids=list(dict.fromkeys(remap[i] for i in f))
        if len(ids)>=3: clean.append(ids)
    fn=om.MFnMesh()
    fn.create([om.MPoint(*p) for p in unique], [len(f) for f in clean], [i for f in clean for i in f])
    obj=cmds.rename(om.MFnDagNode(fn.parent(0)).fullPathName(),name)
    obj=cmds.parent(obj,parent)[0]
    cmds.sets(obj,e=True,forceElement='initialShadingGroup')
    OBJECTS.append(obj)
    return obj

def fn_mesh(obj):
    selection=om.MSelectionList()
    selection.add(cmds.listRelatives(obj,shapes=True,fullPath=True,type='mesh')[0])
    return om.MFnMesh(selection.getDagPath(0))

def stitch_duplicate_edges(obj):
    """Consolidate duplicated edge records left by bevel at a stair grid junction."""
    fn=fn_mesh(obj);it=om.MItMeshEdge(fn.dagPath());seen=set();duplicate=False
    while not it.isDone():
        pair=tuple(sorted([it.vertexId(0),it.vertexId(1)]))
        if pair in seen:duplicate=True
        seen.add(pair);it.next()
    if not duplicate:return False
    points=fn.getPoints();counts,ids=fn.getVertices();old_shape=fn.fullPathName()
    selection=om.MSelectionList();selection.add(obj);transform=selection.getDagPath(0).node()
    new_fn=om.MFnMesh();new_fn.create(points,counts,ids,parent=transform)
    cmds.delete(old_shape)
    cmds.rename(new_fn.fullPathName(),obj.split('|')[-1]+'Shape')
    cmds.sets(obj,e=True,forceElement='initialShadingGroup')
    return True

def close_bevel_corners(obj):
    fn=fn_mesh(obj);it=om.MItMeshEdge(fn.dagPath());borders=[]
    while not it.isDone():
        if len(it.getConnectedFaces())==1:borders.append('%s.e[%d]'%(obj,it.index()))
        it.next()
    if not borders:return False
    cmds.polyCloseBorder(borders,constructionHistory=False)
    return True

def finish_edges(obj,width=1.2,segments=2):
    """Bevel only actual corners; preserve the radial quads of curved surfaces."""
    fn=fn_mesh(obj)
    edges=om.MItMeshEdge(fn.dagPath())
    corners=[]
    while not edges.isDone():
        connected=edges.getConnectedFaces()
        if len(connected)==2:
            n1=fn.getPolygonNormal(connected[0]);n2=fn.getPolygonNormal(connected[1])
            angle=math.degrees(math.acos(max(-1.,min(1.,n1*n2))))
            if angle>32:
                corners.append('%s.e[%d]'%(obj,edges.index()))
        edges.next()
    if corners:
        cmds.polyBevel3(corners,offset=width,offsetAsFraction=False,segments=segments,
                       autoFit=True,mergeVertices=True,mergeVertexTolerance=.001,
                       mitering=0,constructionHistory=False)
        BEVELS[obj]={'width_cm':width,'segments':segments,'base_corner_edges':len(corners)}
    stitch_duplicate_edges(obj)
    close_bevel_corners(obj)
    cmds.polySoftEdge(obj,angle=30,constructionHistory=False)
    return obj

def world(p,angle=0,origin=(0,0,0)):
    x,y,z=p;sa,ca=math.sin(angle),math.cos(angle)
    return (origin[0]+x*ca+z*sa,origin[1]+y,origin[2]-x*sa+z*ca)

def radial(r,a,y=0):
    return (r*math.sin(a),y,r*math.cos(a))

def closed_ring(name,profile,a0,a1,steps,parent,width=1.2):
    """A closed radial cross-section swept through a circular arc."""
    count=len(profile);vertices=[];faces=[]
    full=abs(a1-a0-TAU)<1e-6
    stations=steps if full else steps+1
    for i in range(stations):
        a=a0+(a1-a0)*i/steps
        vertices.extend(radial(r,a,y) for r,y in profile)
    for i in range(steps):
        j=(i+1)%stations
        for k in range(count):
            faces.append([i*count+k,j*count+k,j*count+(k+1)%count,i*count+(k+1)%count])
    if not full:
        faces.extend([list(range(count)),[steps*count+k for k in reversed(range(count))]])
    return finish_edges(mesh(name,vertices,faces,parent),width)

def capped_lathe(name,profile,parent,segments=48,width=1.2):
    """Quads along the profile with flat, planar polygon caps."""
    vertices=[];faces=[];count=len(profile)
    for i in range(segments):
        vertices.extend(radial(r,TAU*i/segments,y) for r,y in profile)
    for i in range(segments):
        j=(i+1)%segments
        for k in range(count-1):
            faces.append([i*count+k,j*count+k,j*count+k+1,i*count+k+1])
    faces.append([i*count for i in reversed(range(segments))])
    faces.append([i*count+count-1 for i in range(segments)])
    return finish_edges(mesh(name,vertices,faces,parent),width)

def square_post(name,profile,angle,origin,parent):
    vertices=[];faces=[]
    for half,y in profile:
        vertices.extend(world((x,y,z),angle,origin) for x,z in [(-half,half),(half,half),(half,-half),(-half,-half)])
    for j in range(len(profile)-1):
        for i in range(4):
            faces.append([j*4+i,j*4+(i+1)%4,(j+1)*4+(i+1)%4,(j+1)*4+i])
    faces.extend([list(reversed(range(4))),[(len(profile)-1)*4+i for i in range(4)]])
    return finish_edges(mesh(name,vertices,faces,parent),1.4)

def staircase(name,angle,parent):
    vertices=[];faces=[]
    # Shared, welded quads on the side walls, treads, risers and underside.
    def p(side,i,j): return world((side*87,j*26.5,715-i*30),angle)
    def face(points):
        off=len(vertices);vertices.extend(points);faces.append(list(range(off,off+len(points))))
    for i in range(6):
        for side in [-1,1]:
            for j in range(i+1):
                points=[p(side,i,j),p(side,i+1,j),p(side,i+1,j+1),p(side,i,j+1)]
                face(points if side==1 else points[::-1])
        face([p(-1,i,i+1),p(1,i,i+1),p(1,i+1,i+1),p(-1,i+1,i+1)])
        face([p(-1,i,i),p(1,i,i),p(1,i,i+1),p(-1,i,i+1)])
        face([p(-1,i,0),p(-1,i+1,0),p(1,i+1,0),p(1,i,0)])
    for j in range(6):
        face([p(-1,6,j),p(-1,6,j+1),p(1,6,j+1),p(1,6,j)])
    return finish_edges(mesh(name,vertices,faces,parent),.9)

def stair_rail(name,side,angle,parent):
    profile=[(525,0),(715,0),(715,37),(525,185)]
    vertices=[world((xx,y,r),angle) for xx in [side*104-14,side*104+14] for r,y in profile]
    faces=[list(range(4)),list(reversed(range(4,8)))]
    faces.extend([[i,4+i,4+(i+1)%4,(i+1)%4] for i in range(4)])
    return finish_edges(mesh(name,vertices,faces,parent),1.3)

def halo_stone(name,a0,a1,parent):
    profile=[(119,-26),(165,-26),(165,29),(119,29)]
    vertices=[];faces=[];steps=2
    for i in range(steps+1):
        a=a0+(a1-a0)*i/steps
        vertices.extend((r*math.cos(a),552+r*math.sin(a),z) for r,z in profile)
    for i in range(steps):
        for j in range(4):faces.append([i*4+j,(i+1)*4+j,(i+1)*4+(j+1)%4,i*4+(j+1)%4])
    faces.extend([list(range(4)),[steps*4+j for j in reversed(range(4))]])
    return finish_edges(mesh(name,vertices,faces,parent),1.5)

def diamond_frame(parent):
    outer=[(0,655),(-89,540),(0,423),(89,540)]
    inner=[(0,617),(-56,540),(0,462),(56,540)]
    vertices=[];faces=[]
    for z in [59,6]:
        vertices.extend((x,y,z) for x,y in outer)
        vertices.extend((x,y,z) for x,y in inner)
    for i in range(4):
        j=(i+1)%4
        faces.extend([[i,j,4+j,4+i],[8+i,12+i,12+j,8+j],
                      [i,8+i,8+j,j],[4+i,4+j,12+j,12+i]])
    return finish_edges(mesh('Diamond_StoneFrame',vertices,faces,parent),1.2)

def build():
    foundation=GROUPS['Foundation'];stairs=GROUPS['Stairs'];upper=GROUPS['CentralBody'];sculpture=GROUPS['Sculpture']
    capped_lathe('Foundation_Slab',[(561,0),(561,28)],foundation,64,1.4)
    capped_lathe('Pool_Floor',[(499,28),(499,44)],foundation,64,.7)
    for k in range(4):
        a0=math.radians(k*90+10);a1=math.radians(k*90+80)
        closed_ring('Pool_Wall_%02d'%(k+1),[(497,28),(549,28),(549,157),(497,157)],a0,a1,14,foundation,1.3)
        closed_ring('Pool_Rim_%02d'%(k+1),[(480,157),(564,157),(564,178),(480,178)],a0,a1,14,foundation,1.2)
        a=k*math.pi/2
        staircase('Staircase_%02d'%(k+1),a,stairs)
        stair_rail('StairRail_%02d_L'%(k+1),-1,a,stairs)
        stair_rail('StairRail_%02d_R'%(k+1),1,a,stairs)
        a=math.radians(45+90*k)
        square_post('Corner_Pier_%02d'%(k+1),[(74,0),(74,183)],a,radial(527,a),foundation)
    status('main basin and staircases')
    capped_lathe('Central_Base',[(181,85),(181,113),(165,113),(165,162),(176,162),(176,179)],upper,48,1.3)
    capped_lathe('Central_Column',[(116,178),(123,185),(123,270),(157,292),(204,308),(230,314)],upper,48,1.4)
    capped_lathe('Upper_Bowl',[(225,307),(248,327),(248,359),(257,359),(257,376),(222,376),(222,338)],upper,64,1.3)
    capped_lathe('Sculpture_Base',[(104,338),(104,389),(85,389),(85,415),(60,415),(60,438)],upper,48,1.2)
    for side,(start,end) in enumerate([(-76,73),(108,255)]):
        for i in range(7):
            a0=math.radians(start+(end-start)*i/7+.5)
            a1=math.radians(start+(end-start)*(i+1)/7-.5)
            halo_stone('Halo_%s_%02d'%('Right' if side==0 else 'Left',i+1),a0,a1,sculpture)
    diamond_frame(sculpture)
    vertices=[(0,608,61),(-48,540,61),(0,473,61),(48,540,61),(0,541,98),(0,541,31)]
    faces=[]
    for i in range(4):faces.extend([[i,(i+1)%4,4],[(i+1)%4,i,5]])
    crystal=mesh('Central_Crystal',vertices,faces,sculpture)
    cmds.polySoftEdge(crystal,angle=0,constructionHistory=False)
    status('main sculpture and edge bevels')

def make_uvs():
    if not cmds.pluginInfo('Unfold3D',query=True,loaded=True):cmds.loadPlugin('Unfold3D',quiet=True)
    for i,obj in enumerate(OBJECTS):
        cmds.polyAutoProjection(obj,planes=6,optimize=1,layout=0,scaleMode=0,
                               projectBothDirections=False,constructionHistory=False)
        cmds.select(obj+'.map[*]',replace=True)
        cmds.u3dUnfold(iterations=2,pack=False,borderintersection=True,
                       triangleflip=True,mapsize=2048,roomspace=0)
        if i%12==0:status('UV unfold %d/%d'%(i+1,len(OBJECTS)))
    cmds.select([obj+'.map[*]' for obj in OBJECTS],replace=True)
    cmds.u3dLayout(multiObject=True,resolution=2048,mutations=1,
                  preScaleMode=1,preRotateMode=1,layoutScaleMode=2,
                  shellSpacing=8./2048,tileMargin=8./2048,
                  packBox=(0,1,0,1),rotateMin=0,rotateMax=360,rotateStep=90)
    cmds.select(clear=True)
    status('UV atlas packed into 0-1')

def modeling_view():
    # Use Maya's ordinary perspective camera, with no presentation scene.
    cmds.xform('persp',translation=(1050,1040,1800))
    locator=cmds.spaceLocator()[0];cmds.xform(locator,translation=(0,335,0))
    constraint=cmds.aimConstraint(locator,'persp',aimVector=(0,0,-1),upVector=(0,1,0),worldUpType='scene')[0]
    cmds.delete(constraint,locator)
    cmds.setAttr('perspShape.nearClipPlane',1)
    cmds.setAttr('perspShape.farClipPlane',100000)
    cmds.setAttr('perspShape.focalLength',50)
    for panel in cmds.getPanel(type='modelPanel'):
        cmds.modelEditor(panel,edit=True,unlockMainConnection=True,mainListConnection='',viewSelected=False)
        cmds.modelEditor(panel,edit=True,displayAppearance='smoothShaded',displayTextures=False,
                         displayLights='default',twoSidedLighting=True,wireframeOnShaded=False,
                         grid=True,cameras=False,lights=False,locators=False,selectionHiliteDisplay=True)
    cmds.modelPanel('modelPanel4',edit=True,camera='persp')
    cmds.setFocus('modelPanel4')
    cmds.select(clear=True)

def validate():
    problems=[];face_count=0;vertex_count=0;shell_count=0;uv_min=[float('inf')]*2;uv_max=[float('-inf')]*2
    triangles=0;quads=0;ngons=0
    for obj in OBJECTS:
        fn=fn_mesh(obj);points=fn.getPoints()
        if cmds.polyInfo(fn.fullPathName(),nonManifoldVertices=True):problems.append((obj,'nonmanifold vertices'))
        if cmds.polyInfo(fn.fullPathName(),laminaFaces=True):problems.append((obj,'lamina faces'))
        face_count+=fn.numPolygons;vertex_count+=fn.numVertices
        if any(not all(math.isfinite(v) for v in (p.x,p.y,p.z)) for p in points):problems.append((obj,'nonfinite vertices'))
        edge_iter=om.MItMeshEdge(fn.dagPath())
        while not edge_iter.isDone():
            if len(edge_iter.getConnectedFaces())!=2:problems.append((obj,'open or nonmanifold edge',edge_iter.index()))
            edge_iter.next()
        poly=om.MItMeshPolygon(fn.dagPath())
        while not poly.isDone():
            if poly.getArea()<1e-7:problems.append((obj,'zero-area face',poly.index()))
            n=poly.polygonVertexCount()
            triangles+=n==3;quads+=n==4;ngons+=n>4
            poly.next()
        counts,ids=fn.getAssignedUVs();uu,vv=fn.getUVs()
        if len(counts)!=fn.numPolygons or any(n!=fn.polygonVertexCount(i) for i,n in enumerate(counts)):
            problems.append((obj,'incomplete UV assignment'))
        if not ids or min(ids)<0 or max(ids)>=len(uu):problems.append((obj,'invalid UV indices'))
        if any(not math.isfinite(x) for x in list(uu)+list(vv)):problems.append((obj,'nonfinite UVs'))
        if uu and vv:
            for i,values in enumerate([uu,vv]):uv_min[i]=min(uv_min[i],min(values));uv_max[i]=max(uv_max[i],max(values))
        shells,shell_ids=fn.getUvShellsIds();shell_count+=shells
        if cmds.listConnections(fn.fullPathName()+'.inMesh',source=True,destination=False):problems.append((obj,'construction history'))
    if any(x<-.0001 for x in uv_min) or any(x>1.0001 for x in uv_max):problems.append(('atlas','outside 0-1'))
    bounds=cmds.exactWorldBoundingBox(OBJECTS)
    report={'scene':SCENE,'meshes':len(OBJECTS),'faces':face_count,'vertices':vertex_count,
            'triangles':triangles,'quads':quads,'planar_cap_ngons':ngons,
            'previous_meshes':916,'previous_faces':91623,
            'UV':{'set':'map1','method':'automatic seams, Unfold3D, multi-object 0-1 layout',
                  'shells':shell_count,'minimum':uv_min,'maximum':uv_max,'padding':8,'reference_resolution':2048},
            'beveled_meshes':len(BEVELS),'bevel_segments':2,
            'materials':cmds.ls(materials=True),'file_textures':cmds.ls(type='file'),
            'lights':cmds.ls(lights=True),'cameras':cmds.ls(type='camera'),
            'dimensions_m':[(bounds[i+3]-bounds[i])/100 for i in range(3)],'problems':problems}
    if problems:
        with open(OUT+'/Fountain_Concept03_Modeling_Report.json','w') as f:json.dump(report,f,indent=2)
        raise RuntimeError('Model validation: '+str(problems[:6]))
    return report

def main():
    # Refuse to discard unrecorded edits. The detailed v001 is already saved.
    if cmds.file(query=True,modified=True):
        raise RuntimeError('The current Maya scene has unsaved edits; save those before rebuilding.')
    current=cmds.file(query=True,sceneName=True).replace('\\','/')
    allowed=['',OUT+'/Fountain_Concept03_v001.ma',SCENE]
    if current not in allowed:raise RuntimeError('Unexpected open scene: '+current)
    cmds.file(new=True,force=True)
    cmds.currentUnit(linear='cm')
    cmds.connectAttr('lambert1.outColor','initialShadingGroup.surfaceShader',force=True)
    cmds.refresh(suspend=True)
    try:
        root=cmds.group(empty=True,name='Fountain_Model_GRP')
        for key in ['Foundation','Stairs','CentralBody','Sculpture']:
            GROUPS[key]=cmds.group(empty=True,name=key+'_GRP',parent=root)
        status('modeling main forms')
        build()
        cmds.delete(OBJECTS,constructionHistory=True)
        for obj in OBJECTS:
            cmds.makeIdentity(obj,apply=True,translate=True,rotate=True,scale=True)
            cmds.xform(obj,centerPivots=True)
        make_uvs()
        cmds.delete(OBJECTS,constructionHistory=True)
        modeling_view()
        report=validate()
        cmds.file(rename=SCENE)
        cmds.file(save=True,type='mayaAscii',force=True)
        report['saved']=True;report['seconds']=round(time.time()-START,1)
        with open(OUT+'/Fountain_Concept03_Modeling_Report.json','w') as f:json.dump(report,f,indent=2)
        status('saved modeling scene')
    finally:
        cmds.refresh(suspend=False)
        cmds.refresh(force=True)

try:
    main()
except Exception:
    with open(OUT+'/modeling_error.txt','w') as f:f.write(traceback.format_exc())
    status('error')
    raise
