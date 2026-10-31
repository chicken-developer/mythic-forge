"""Native-SOP facade quality study: legacy / game / high-detail.

Run with Houdini 22 hython. Units metres, Y up, front -Z.
Curve samples and section profiles are embedded in Stash nodes; all sweeps,
booleans, bevels, caps and normal generation remain editable native SOPs.
The canonical building HIP and the ten building assets are not overwritten.
"""
from pathlib import Path
import collections
import hashlib
import json
import math
import sys
import time
import hou

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/'HoudiniWorkspace/gothic_building'
HERE=Path(__file__).resolve().parent
EXPORT=ROOT/'TDKProps/SourceAssets/Gothic/FacadeReview'
sys.path.insert(0,str(SOURCE))
import build_hda as B
import native_network as N

WIDTH=4.0
HEIGHT=6.4
OPENING=1.65
SILL=.45+HEIGHT*.16
SPRING=HEIGHT*.61
CONCEPT_REVISION={
    'basis':['HoudiniWorkspace/gothic_building/references/master.png',
             'HoudiniWorkspace/gothic_building/references/preset_01.png'],
    'removed':['oversized curved leaf ornaments','diamond-shaped tip','rounded column shaft'],
    'retained':'Pointed pinnacle, lancet recesses, stepped rectangular buttress and stone moldings',
    'high_rule':'Same components and design as Game; finer curve sampling and bevel tessellation only',
}

def note(n,text):
    n.setComment(text);n.setGenericFlag(hou.nodeFlag.DisplayComment,True)
    return n

def menu(n,key,token):
    N.set_menu(n,key,token)

def stash(parent,name,geometry):
    n=parent.createNode('stash',name);n.parm('stash').set(geometry);return n

def poly_geometry(parent,polylines,closed=False):
    g=N.geometry64(parent)
    for line in polylines:
        p=g.createPolygon(is_closed=closed)
        for pt in g.createPoints(line):p.addVertex(pt)
    return g

def arch_path(width,spring,sill,error,center=0.):
    """Equal-angle circle sampling gives a bounded chord error, including apex."""
    count=max(8,math.ceil((math.pi/3)/(2*math.acos(max(-1.,1-error/width)))))
    r=width/2
    path=[(center-r,sill),(center-r,spring)]
    # Left jamb -> apex -> right jamb. X increases continuously over the arch.
    for i in range(1,count+1):
        a=i*math.pi/(3*count)
        path.append((center+r-width*math.cos(a),spring+width*math.sin(a)))
    for i in range(1,count+1):
        a=(count-i)*math.pi/(3*count)
        path.append((center-r+width*math.cos(a),spring+width*math.sin(a)))
    path.append((center+r,sill))
    return path,count

def opening_polygon(width,spring,sill,error,center=0.):
    path,_=arch_path(width,spring,sill,error,center)
    return list(reversed(path))

def solid(parent,polygon,vector,name,index=0):
    return N.source_branch(parent,[{'kind':'solid','points':polygon,'vector':vector,
        'name':name,'material':'stone','seed':71}],'',index)

def rect_solid(parent,name,x,y,z,w,h,d,index=0):
    return solid(parent,[(x-w/2,y,z),(x+w/2,y,z),(x+w/2,y+h,z),(x-w/2,y+h,z)],(0,0,d),name,index)

def boolean(parent,name,a,b):
    n=parent.createNode('boolean::2.0',name);n.setInput(0,a);n.setInput(1,b)
    menu(n,'booleanop','subtract');menu(n,'subtractchoices','aminusb')
    n.parm('removeinlinepoints').set(1);n.parm('correctnormals').set(1)
    return n

def merge(parent,name,items):
    n=parent.createNode('merge',name)
    for i,item in enumerate(items):n.setInput(i,item)
    return n

def mark(parent,name,source,part):
    return N.wrangle(parent,name,source,'s@name='+json.dumps(part)+'; s@part='+json.dumps(part)+'; s@material_id="clay"; @Cd=set(.5,.5,.5);')

class Facade:
    def __init__(self,label,high=False):
        self.obj=hou.node('/obj').createNode('geo',label,run_init_scripts=False)
        self.high=high;self.error=.00025 if high else .0012
        self.divisions=6 if high else 3
        self.parts=[];self.arc_errors=[]
        self.obj.addSpareParmTuple(hou.FloatParmTemplate('bevel_scale','Bevel scale',1,default_value=(1.,),min=.25,max=2.))
        self.obj.addSpareParmTuple(hou.IntParmTemplate('bevel_divisions','Bevel divisions',1,default_value=(self.divisions,),min=2,max=8))
        self.obj.addSpareParmTuple(hou.FloatParmTemplate('smooth_angle','Smooth angle',1,default_value=(48.,),min=20,max=80))
        self.obj.setComment('Facade prototype revised against references/master.png and preset_01.png.\nNo curved leaf ornaments, diamond tip or round column shaft.\nHigh and Game share the same design; High refines curve and bevel sampling.\nBevel Scale, Bevel Divisions and Smooth Angle are live.')

    def group(self,name):
        n=self.obj.createNode('subnet',name);n.setColor(hou.Color((.32,.46,.55)))
        return n

    def finish_part(self,parent,source,name,bevel=.008):
        if bevel:
            b=parent.createNode('polybevel::3.0','architectural_edge_bevel')
            b.setInput(0,source);menu(b,'grouptype','edges')
            b.parm('ignoreflatedges').set(1);b.parm('flatangle').set(12.)
            b.parm('offset').setExpression(str(bevel)+'*ch("../../bevel_scale")')
            b.parm('divisions').setExpression('ch("../../bevel_divisions")')
            menu(b,'filletshape','round');b.parm('detectcollisions').set(1)
            b.parm('useedgefilletgroup').set(1);b.parm('edgeprims').set('bevel_faces')
            note(b,'Real geometric fillet; flat curve facets are excluded. Units metres.')
            source=b
        out=mark(parent,'PART_ID',source,name)
        out.setDisplayFlag(True);out.setRenderFlag(True)
        parent.layoutChildren();self.parts.append(parent)
        return parent

    def box(self,name,x,y,z,w,h,d,bevel=.012):
        p=self.group(name);n=rect_solid(p,name,x,y,z,w,h,d)
        return self.finish_part(p,n,name,bevel)

    def sweep(self,name,path,profile,closed=False,bevel=.003):
        p=self.group(name)
        spine=stash(p,'EDIT_BACKBONE',poly_geometry(p,[path],closed))
        section=stash(p,'EDIT_SECTION',poly_geometry(p,[[(x,y,0.) for x,y in profile]],True))
        sw=p.createNode('sweep::2.0','continuous_profile_sweep')
        sw.setInput(0,spine);sw.setInput(1,section)
        menu(sw,'surfaceshape','input');menu(sw,'surfacetype','quads')
        menu(sw,'upvectortype','z');sw.parm('upvectoratstart').set(0)
        sw.parm('stretcharoundturns').set(1);sw.parm('maxstretcharoundturns').set(3.)
        menu(sw,'endcaptype','none' if closed else 'single')
        note(sw,'A shared mesh is swept over the complete backbone.\nMitered bends replace independent capped box segments.')
        reverse=p.createNode('reverse','OUTWARD_SWEEP_WINDING');reverse.setInput(0,sw)
        return self.finish_part(p,reverse,name,bevel)

    def loft(self,name,sections,bevel=.008):
        p=self.group(name)
        profiles=stash(p,'EDIT_SECTION_RINGS',poly_geometry(p,sections,True))
        skin=p.createNode('skin','loft_profile');skin.setInput(0,profiles)
        menu(skin,'surftype','quads')
        cap=p.createNode('polycap','close_ends');cap.setInput(0,skin)
        return self.finish_part(p,cap,name,bevel)

    def build_wall(self):
        p=self.group('01_wall_with_true_opening')
        wall=rect_solid(p,'wall',0,0,0,WIDTH,HEIGHT,.34)
        contour=opening_polygon(OPENING,SPRING,SILL,self.error)
        cutter=solid(p,[(x,y,-.6) for x,y in contour],(0,0,1.1),'opening',1)
        carved=boolean(p,'cut_full_depth_opening',wall,cutter)
        self.finish_part(p,carved,'wall_shell',.012)

    def build_arch(self):
        path,count=arch_path(OPENING,SPRING,SILL,self.error)
        self.arc_errors.append({'name':'main_arch','segments_per_half':count,'maximum_chord_error_m':OPENING*(1-math.cos(math.pi/(6*count)))})
        profile=[(-.015,.035),(.035,.035),(.035,-.035),(.090,-.035),(.110,-.075),
            (.110,-.115),(.145,-.140),(.185,-.140),(.220,-.110),(.240,-.055),
            (.290,-.055),(.290,.020),(.330,.020),(.330,.085),(-.015,.085)]
        self.sweep('02_continuous_archivolt',[(x,y,-.045) for x,y in path],profile,bevel=.004)
        # The sill runs under both jambs; a sloping top and drip nose resolve rainwater.
        self.profile_beam('03_sloping_sill',SILL-.15,WIDTH*.62,
            [(-.10,.06),(.04,.06),(.10,-.16),(.04,-.38),(-.04,-.40),(-.10,-.32)],.009)

    def profile_beam(self,name,y,width,profile,bevel=.006):
        p=self.group(name)
        contour=[(-width/2,y+yy,zz) for yy,zz in profile]
        n=solid(p,contour,(width,0,0),name)
        return self.finish_part(p,n,name,bevel)

    def build_cornices(self):
        # Concave/convex section tiers form one continuous solid, with no stacked boxes.
        self.profile_beam('04_plinth_moulding',.18,4.28,
            [(-.18,.06),(.13,.06),(.17,-.12),(.13,-.27),(.06,-.31),(.00,-.36),(-.09,-.36),(-.18,-.28)],.014)
        self.profile_beam('05_stringcourse',.82,4.20,
            [(-.09,.045),(.09,.045),(.12,-.11),(.07,-.18),(.03,-.22),(-.04,-.22),(-.09,-.16)],.007)
        self.profile_beam('06_crowning_cornice',6.30,4.42,
            [(-.16,.10),(.28,.10),(.29,-.16),(.24,-.31),(.18,-.38),(.10,-.38),
             (.10,-.25),(.05,-.25),(-.02,-.16),(-.10,-.13),(-.16,-.08)],.011)

    @staticmethod
    def chamfer_ring(x,y,z,rx,rz,chamfer=.22):
        c=chamfer
        return [(x+sx*rx,y,z+sz*rz) for sx,sz in [(-1,-1+c),(-1+c,-1),(1-c,-1),(1,-1+c),(1,1-c),(1-c,1),(-1+c,1),(-1,1-c)]]

    def stack(self,name,x,z,levels,bevel=.008):
        return self.loft(name,[self.chamfer_ring(x,y,z,rx,rz) for y,rx,rz in levels],bevel)

    def build_pier(self,x,index):
        z=-.10
        self.stack(f'10_{index}_pier_base',x,z,[(0.,.40,.43),(.14,.40,.43),(.22,.33,.36),(.40,.33,.36),(.54,.27,.30),(.68,.27,.30)],.013)
        # Concept buttresses have flat, stepped masonry faces rather than round columns.
        self.stack(f'11_{index}_stepped_buttress_shaft',x,z,
            [(.64,.24,.28),(3.28,.24,.28),(3.40,.22,.25),(6.16,.22,.25)],.006)
        self.stack(f'12_{index}_pier_capital',x,z,[(6.10,.27,.30),(6.20,.27,.30),(6.27,.34,.37),(6.36,.38,.41),(6.46,.38,.41),(6.52,.32,.35)],.009)
        # The neck passes through the cornice and seats the turret on a genuine bearing.
        self.stack(f'13_{index}_pinnacle_seat',x,z,[(6.46,.27,.30),(6.70,.27,.30),(6.78,.34,.37),(6.88,.34,.37)],.009)
        p=self.group(f'14_{index}_carved_pinnacle_lantern')
        body=rect_solid(p,'lantern',x,6.84,z-.26,.52,.65,.52)
        cutters=[]
        opening=opening_polygon(.22,7.14,6.96,self.error*.5)
        for j in range(4):
            a=j*math.pi/2
            def rot(xx,zz):return (x+xx*math.cos(a)+zz*math.sin(a),z-xx*math.sin(a)+zz*math.cos(a))
            pts=[]
            for xx,yy in opening:
                xx1,zz1=rot(xx,-.33);pts.append((xx1,yy,zz1))
            vector=(math.sin(a)*.14,0.,math.cos(a)*.14)
            cutters.append(solid(p,pts,vector,'blind_niche_'+str(j),j+1))
        cut=boolean(p,'recess_four_blind_niches',body,merge(p,'niche_cutters',cutters))
        self.finish_part(p,cut,'carved_lantern',.004)
        self.stack(f'15_{index}_canopy_cornice',x,z,[(7.44,.27,.27),(7.51,.34,.34),(7.59,.35,.35),(7.64,.29,.29)],.008)
        # A clean straight taper replaces invented leaf ornaments and the diamond cap.
        # The small closed tip preserves the shared overall height without a new finial.
        self.stack(f'16_{index}_pointed_spire',x,z,
            [(7.59,.29,.29),(7.64,.29,.29),(8.78,.006,.006)],.002)

    def build_tracery(self):
        # Two continuous lancets and a roundel, all seated in the opening.
        start=len(self.parts)
        profile=[(.035*math.cos(i*2*math.pi/(16 if self.high else 10)),.035*math.sin(i*2*math.pi/(16 if self.high else 10))) for i in range(16 if self.high else 10)]
        for i,center in enumerate((-.39,.39)):
            path,_=arch_path(.70,3.68,SILL,self.error*.65,center)
            self.sweep('20_lancet_'+str(i),[(x,y,.06) for x,y in path],profile,bevel=0.)
        self.box('21_central_mullion',0,SILL,.015,.16,3.72-SILL,.10,.008)
        count=80 if self.high else 48
        roundel_y=3.68+math.sqrt((.70+.267)**2-.74**2)
        circular=[(.267*math.cos(i*2*math.pi/count),roundel_y+.267*math.sin(i*2*math.pi/count),.06) for i in range(count)]
        self.sweep('22_tracery_roundel',circular,profile,True,0.)
        # Union the tangent stone ribs and their common mullion into one closed piece.
        ribs=self.parts[start:];self.parts=self.parts[:start]
        p=self.group('24_connected_stone_tracery')
        for i,rib in enumerate(ribs):p.setInput(i,rib)
        inputs=p.indirectInputs();joined=inputs[0]
        for i,rib in enumerate(inputs[1:len(ribs)]):
            b=p.createNode('boolean::2.0','union_rib_'+str(i))
            b.setInput(0,joined);b.setInput(1,rib);menu(b,'booleanop','union')
            b.parm('correctnormals').set(1);b.parm('removeinlinepoints').set(1);joined=b
        self.finish_part(p,joined,'connected_tracery',0.)
        # Real recessed glazing volume, rather than a plane over the front trim.
        p=self.group('23_recessed_glazing')
        contour=opening_polygon(OPENING-.012,SPRING,SILL+.01,self.error)
        n=solid(p,[(x,y,.27) for x,y in contour],(0,0,.012),'glazing')
        self.finish_part(p,n,'glazing',0.)

    def build(self):
        self.build_wall();self.build_arch();self.build_cornices()
        for i,x in enumerate((-1.72,1.72)):self.build_pier(x,str(i))
        self.build_tracery()
        merged=merge(self.obj,'ASSEMBLE_FINISHED_MODULES',self.parts)
        # Each component is already closed. Keep intentional architectural interfaces.
        normals=self.obj.createNode('normal','WEIGHTED_VERTEX_NORMALS')
        normals.setInput(0,merged);menu(normals,'type','typevertex');menu(normals,'method','area')
        normals.parm('cuspangle').setExpression('ch("../smooth_angle")')
        note(normals,'Area-weighted normals after all booleans and bevels.\n48 degrees smooths bevel rows and curve facets while preserving profile corners.')
        uv=N.wrangle(self.obj,'metric_planar_uvs',normals,'vector p=point(0,"P",@ptnum); vector n=abs(prim_normal(0,@primnum,.5,.5)); if(n.x>=n.y && n.x>=n.z) v@uv=set(p.z,p.y,0)/2.; else if(n.y>=n.z) v@uv=set(p.x,p.z,0)/2.; else v@uv=set(p.x,p.y,0)/2.;',owner='vertex')
        tri=self.obj.createNode('divide','TRIANGULATE_FOR_EXPORT');tri.setInput(0,uv)
        tri.parm('convex').set(1);tri.parm('usemaxsides').set(1);tri.parm('numsides').set(3)
        out=self.obj.createNode('null','OUT_HIGH' if self.high else 'OUT_GAME')
        out.setInput(0,tri);out.setDisplayFlag(True);out.setRenderFlag(True)
        self.obj.layoutChildren();return out

def legacy():
    obj=hou.node('/obj').createNode('geo','01_LEGACY_FROM_V2_2',run_init_scripts=False)
    p=B.BASELINE['parameters'].copy();p['facade_style']='lancet'
    m=B.Mesh(p,101)
    B.panel(m,(0,0,0),(1,0,0),(0,0,-1),WIDTH,HEIGHT,OPENING,'review_window')
    for i,x in enumerate((-1.72,1.72)):
        m.box(x,0,-.1,.38,6.50,.52,'legacy_pier_'+str(i))
        B.pinnacle(m,x,6.65,-.1,1.78,'legacy_pinnacle_'+str(i))
    for y in (.78,6.30):m.box(0,y,-.10,4.42,.24,.55,'legacy_cornice')
    g=hou.Geometry();B.fill_hou(g,m)
    base=stash(obj,'ORIGINAL_GENERATOR_MESH',g)
    fuse=obj.createNode('fuse::2.0','weld_exact_profile_vertices');fuse.setInput(0,base);fuse.parm('tol3d').set(1e-6)
    normal=obj.createNode('normal','ORIGINAL_FLAT_NORMALS');normal.setInput(0,fuse);normal.parm('cuspangle').set(0.)
    tri=obj.createNode('divide','TRIANGULATE');tri.setInput(0,normal)
    out=obj.createNode('null','OUT_LEGACY');out.setInput(0,tri);out.setDisplayFlag(True);out.setRenderFlag(True)
    obj.layoutChildren();return obj,out

def audit(node,strict=True):
    g=node.geometry();edges=collections.Counter();zero=0;normals=0
    neighbors=collections.defaultdict(set);volumes={};bad_parts=collections.Counter()
    for p in g.prims():
        pts=[v.point().number() for v in p.vertices()]
        for a,b in zip(pts,pts[1:]+pts[:1]):
            edges[tuple(sorted((a,b)))]+=1;neighbors[a].add(b);neighbors[b].add(a)
        a,b,c=[v.point().position() for v in p.vertices()]
        volumes[p.number()]=a.dot(b.cross(c))/6
        if p.intrinsicValue('measuredarea')<1e-12:
            zero+=1;bad_parts[p.attribValue('name')]+=1
        for v in p.vertices():
            n=v.attribValue('N')
            if abs(sum(a*a for a in n)-1.)>.005:normals+=1
    result={'points':len(g.points()),'triangles':len(g.prims()),'boundary_edges':sum(c==1 for c in edges.values()),'nonmanifold_edges':sum(c>2 for c in edges.values()),'degenerate_triangles':zero,'invalid_vertex_normals':normals,'bounds_m':[list(g.boundingBox().minvec()),list(g.boundingBox().maxvec())]}
    shell_of={};shells=[]
    for pt in g.points():
        first=pt.number()
        if first in shell_of:continue
        index=len(shells);shells.append(0.);queue=[first];shell_of[first]=index
        while queue:
            for other in neighbors[queue.pop()]:
                if other not in shell_of:shell_of[other]=index;queue.append(other)
    for p in g.prims():shells[shell_of[p.points()[0].number()]]+=volumes[p.number()]
    # Houdini outward polygon winding has negative signed volume in this convention.
    result['closed_shells']=len(shells)
    result['inverted_or_zero_volume_shells']=sum(v>=-1e-12 for v in shells)
    if strict:
        if zero or normals:print('DEGENERATE_PARTS',dict(bad_parts),flush=True)
        assert not result['boundary_edges'] and not result['nonmanifold_edges'],result
        assert not zero and not normals,result
        assert not result['inverted_or_zero_volume_shells'],result
    return result

def main():
    EXPORT.mkdir(parents=True,exist_ok=True);HERE.mkdir(parents=True,exist_ok=True)
    source_hash=hashlib.sha256((SOURCE/'gothic_building.hip').read_bytes()).hexdigest()
    old,oldout=legacy()
    game=Facade('02_REVISED_GAME');gameout=game.build()
    high=Facade('03_HIGH_DETAIL_SOURCE',True);highout=high.build()
    # Only these review objects exist in the new scene.
    records=[]
    rop=hou.node('/out').createNode('filmboxfbx','EXPORT_REVIEW_FBX')
    rop.parm('trange').set(0)
    for label,obj,out in [('Legacy',old,oldout),('Game',game.obj,gameout),('High',high.obj,highout)]:
        started=time.time();out.cook(force=True)
        errors=[(n.path(),list(n.errors())) for n in obj.allSubChildren() if n.errors()]
        assert not errors,errors
        stats=audit(out,label!='Legacy')
        cm=obj.createNode('xform','CENTIMETRES_EXPORT_ONLY');cm.setInput(0,out);cm.parm('scale').set(100.)
        cm.setDisplayFlag(True);cm.setRenderFlag(True)
        path=EXPORT/('SM_FacadeReview_'+label+'.fbx')
        rop.parm('startnode').set(obj.path());rop.parm('sopoutput').set(str(path));rop.render()
        out.setDisplayFlag(True);out.setRenderFlag(True)
        stats.update({'name':label,'file':path.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'seconds':round(time.time()-started,2),'sop_nodes':len(obj.allSubChildren()),'sweep_nodes':len([n for n in obj.allSubChildren() if n.type().name().startswith('sweep')]),'bevel_nodes':len([n for n in obj.allSubChildren() if n.type().name().startswith('polybevel')]),'python_sop_nodes':len([n for n in obj.allSubChildren() if n.type().name().startswith('python')])})
        if label!='Legacy':
            stats['components']=sorted(set(out.geometry().primStringAttribValues('part')))
            assert not any(any(forbidden in part for forbidden in ['crocket','finial','clustered_pier']) for part in stats['components'])
        records.append(stats);print(json.dumps(stats),flush=True)
    assert records[1]['components']==records[2]['components'],'High introduced a different design'
    old.parm('tx').set(-2.9);game.obj.parm('tx').set(2.9)
    high.obj.parm('tx').set(2.9);high.obj.setDisplayFlag(False)
    game.obj.setSelected(True,clear_all_selected=True)
    report={'status':'passed','houdini':hou.applicationVersionString(),'source_hip_sha256':source_hash,'review_only':True,'units':'metres in HIP, centimetres in FBX','variants':records,'game_arch_sampling':game.arc_errors,'high_arch_sampling':high.arc_errors,'concept_revision':CONCEPT_REVISION}
    (EXPORT/'facade_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (HERE/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    hou.hipFile.save(str(HERE/'gothic_facade_review.hip'))
    assert hashlib.sha256((SOURCE/'gothic_building.hip').read_bytes()).hexdigest()==source_hash
    print('FACADE_REVIEW_COMPLETE',flush=True)

if __name__=='__main__':main()
