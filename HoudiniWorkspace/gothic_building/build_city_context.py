"""Native SOP street furniture and wall kit for the sample Gothic city."""
from pathlib import Path
import collections
import hashlib
import json
import math
import hou
import native_network as N

ROOT=Path(__file__).parent;REPO=next(p for p in (ROOT.parents[1],ROOT.parents[1]/'mythic-forge') if (p/'TDKProps/TDKProps.uproject').is_file());OUT=REPO/'TDKProps/SourceAssets/Gothic/Context';OUT.mkdir(parents=True,exist_ok=True)
exec((ROOT/'production_geometry.py').read_text(encoding='utf-8'),N.__dict__)

def round_loft(parent,name,levels,material='stone',sides=48):
    sections=[[(radius*math.cos(i*2*math.pi/sides),y,radius*math.sin(i*2*math.pi/sides)) for i in range(sides)] for y,radius in levels]
    source=N.p_stash(parent,name+'_profiles',N.p_poly(parent,sections,True))
    skin=parent.createNode('skin',name+'_loft');skin.setInput(0,source);N.set_menu(skin,'surftype','quads')
    cap=parent.createNode('polycap',name+'_caps');cap.setInput(0,skin)
    return N.p_mark(parent,N.p_bevel(parent,cap,.005,name+'_bevel'),name,material)

def box(parent,name,loc,size,material):
    x,y,z=loc;w,h,d=size
    return N.p_mark(parent,N.p_bevel(parent,N.p_box(parent,x,y,z,w,h,d,name,material),min(.008,w*.08,h*.08,d*.08),name+'_edge'),name,material)

def make(name):
    obj=hou.node('/obj').createNode('geo','City_'+name,run_init_scripts=False);parts=[]
    if name=='Lamp':
        parts.append(round_loft(obj,'iron_post',[(0,.20),(.13,.20),(.23,.12),(.36,.09),(3.2,.055),(3.28,.13),(3.36,.13)],'metal',24))
        parts.append(box(obj,'lantern_glass',(0,3.35,0),(.34,.56,.34),'lampglass'))
        for x in (-.20,.20):
            for z in (-.20,.20):parts.append(box(obj,'lantern_corner',(x,3.32,z),(.045,.65,.045),'metal'))
        parts.append(round_loft(obj,'lantern_cap',[(3.9,.34),(3.96,.34),(4.16,.035)],'metal',4))
    elif name=='Bench':
        for z in (-.23,-.07,.09,.25):parts.append(box(obj,'seat_slat',(0,.46,z),(1.9,.065,.125),'wood'))
        for y in (.68,.88,1.08):parts.append(box(obj,'back_slat',(0,y,.31),(1.9,.14,.065),'wood'))
        for x in (-.69,.69):
            for z in (-.18,.24):parts.append(box(obj,'bench_leg',(x,0,z),(.075,.48,.075),'metal'))
            parts.append(box(obj,'back_support',(x,.42,.32),(.065,.82,.07),'metal'))
    elif name=='Fountain':
        parts.append(round_loft(obj,'basin_base',[(0,3.2),(.18,3.2),(.24,3.05)],'stone'))
        outer=round_loft(obj,'basin_outer',[(.20,3.02),(.30,3.15),(.82,3.15),(.91,3.25)],'stone')
        inner=round_loft(obj,'basin_cutter',[(.32,2.74),(1.2,2.74)],'stone')
        parts.append(N.p_mark(obj,N.p_boolean(obj,outer,inner,'hollow_basin'),'fountain_basin','stone'))
        parts.append(round_loft(obj,'still_water',[(.58,2.72),(.595,2.72)],'water'))
        parts.append(round_loft(obj,'center_pedestal',[(.30,.64),(.48,.64),(.60,.42),(1.20,.31),(1.44,.63),(1.57,1.02),(1.70,1.02),(1.85,.22),(2.45,.16),(2.52,.26)],'stone'))
    elif name=='Planter':
        parts.append(N.p_stack(obj,[(0,.75,.75),(.12,.75,.75),(.23,.66,.66),(.55,.66,.66),(.65,.77,.77)],'stone_planter',bevel=.012))
        parts.append(box(obj,'soil',(0,.64,0),(1.20,.035,1.20),'soil'))
    elif name=='Cypress':
        parts.append(round_loft(obj,'tree_trunk',[(0,.15),(2.4,.10),(4.7,.045)],'wood',20))
        rings=[];sides=40
        for j in range(22):
            f=j/21;y=.8+5.8*f
            radius=max(.025,(.20+1.05*math.sin(math.pi*min(.95,f*.9))**.75)*(1-f)**.55)
            rings.append([(radius*(1+.08*math.sin(i*1.7+j*.9))*math.cos(i*2*math.pi/sides),y,radius*(1+.08*math.sin(i*1.7+j*.9))*math.sin(i*2*math.pi/sides)) for i in range(sides)])
        stash=N.p_stash(obj,'leaf_canopy_profiles',N.p_poly(obj,rings,True))
        skin=obj.createNode('skin','cypress_canopy');skin.setInput(0,stash);N.set_menu(skin,'surftype','quads')
        cap=obj.createNode('polycap','canopy_caps');cap.setInput(0,skin)
        parts.append(N.p_mark(obj,cap,'cypress_canopy','foliage'))
    elif name=='Wall':
        parts.append(box(obj,'wall_foundation',(0,0,0),(8.,.35,1.4),'stone'))
        parts.append(box(obj,'wall_body',(0,.32,0),(8.,3.60,1.05),'stone'))
        parts.append(box(obj,'wall_coping',(0,3.82,0),(8.,.24,1.3),'trim'))
        for x in (-3.2,-1.6,0,1.6,3.2):parts.append(box(obj,'merlon',(x,4.02,0),(.86,.65,1.1),'stone'))
    merged=N.merge_nodes(obj,'ASSEMBLE_CONTEXT',parts)
    normals=obj.createNode('normal','area_weighted_normals');normals.setInput(0,merged);N.set_menu(normals,'type','typevertex');N.set_menu(normals,'method','area');normals.parm('cuspangle').set(48.)
    uv=N.wrangle(obj,'metric_uv',normals,'vector p=point(0,"P",@ptnum);vector n=abs(prim_normal(0,@primnum,.5,.5));if(n.y>=n.x && n.y>=n.z)v@uv=set(p.x,p.z,0)/2;else if(n.x>=n.z)v@uv=set(p.z,p.y,0)/2;else v@uv=set(p.x,p.y,0)/2;',owner='vertex')
    tri=obj.createNode('divide','triangulate');tri.setInput(0,uv);tri.parm('convex').set(1);tri.parm('usemaxsides').set(1);tri.parm('numsides').set(3)
    output=obj.createNode('null','OUT_CONTEXT');output.setInput(0,tri);output.setDisplayFlag(True);output.setRenderFlag(True);obj.layoutChildren()
    return obj,output

def main():
    records=[];mat=hou.node('/mat')
    rop=hou.node('/out').createNode('filmboxfbx','context_export');rop.parm('trange').set(0)
    for i,name in enumerate(['Lamp','Bench','Fountain','Planter','Cypress','Wall']):
        obj,node=make(name);g=node.geometry()
        assert not node.errors(),node.errors()
        families=collections.Counter(g.primStringAttribValues('material_id'))
        for family in families:
            if mat.node('Gothic_'+family) is None:mat.createNode('principledshader::2.0','Gothic_'+family)
        bind=N.wrangle(obj,'fbx_materials',node,'s@shop_materialpath="/mat/Gothic_"+s@material_id;')
        cm=obj.createNode('xform','centimetres_export');cm.setInput(0,bind);cm.parm('scale').set(100.);cm.setDisplayFlag(True);cm.setRenderFlag(True)
        target=OUT/('SM_City_'+name+'.fbx');rop.parm('startnode').set(obj.path());rop.parm('sopoutput').set(str(target));rop.render()
        node.setDisplayFlag(True);node.setRenderFlag(True);obj.parm('tx').set(i*8.)
        row={'id':name,'file':target.relative_to(REPO).as_posix(),'triangles':len(g.prims()),'materials':dict(families),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
        records.append(row);print('CONTEXT',name,len(g.prims()),flush=True)
    (OUT/'context_manifest.json').write_text(json.dumps({'version':'3.0','props':records},indent=2),encoding='utf-8')
    hou.hipFile.save(str(ROOT/'gothic_city_context.hip'))

if __name__=='__main__':main()
