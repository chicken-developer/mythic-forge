"""Optional native facade kits for reference-specific architectural features."""
from pathlib import Path
import math
import hou
VEX=Path(__file__).resolve().parents[1]/'sources/vex'

def create(parent,typ,name,input_node=None,**parms):
    n=parent.node(name) or parent.createNode(typ,name)
    if input_node is not None:n.setInput(0,input_node)
    for key,val in parms.items():
        if n.parm(key) is None:raise ValueError((typ,key))
        n.parm(key).set(val)
    n.setUserData('medieval_layout_owner','reference_hero_modules')
    return n

def tag(parent,n,name,material):
    return create(parent,'attribwrangle',name+'_metadata',n,**{'class':1,'snippet':f's@name="{name}";s@material_family="{material}";s@component="facade_details";'})

def box(parent,name,size,center,material='wood',rotation=None):
    n=create(parent,'box',name);n.parmTuple('size').set(size);n.parmTuple('t').set(center)
    if rotation:n.parmTuple('r').set(rotation)
    return tag(parent,n,name,material)

def tube(parent,name,radius,height,center,material='metal',axis='y',scale=1.,cols=16):
    n=create(parent,'tube',name,type='poly',orient=axis,cap=1,rad1=radius*scale,rad2=radius,height=height,radscale=1.,cols=cols)
    n.parmTuple('t').set(center)
    return tag(parent,n,name,material)

def boolean(parent,name,a,b,operation):
    n=create(parent,'boolean::2.0',name,a,booleanop=operation);n.setInput(1,b);return n

def merge(parent,name,inputs):
    n=create(parent,'merge',name)
    for i,v in enumerate(inputs):n.setInput(i,v)
    return n

def beam(parent,name,a,b,width,material='wood'):
    mid=tuple((x+y)*.5 for x,y in zip(a,b));delta=hou.Vector3(b)-hou.Vector3(a)
    n=box(parent,name,(width,delta.length(),width),mid,material)
    source=n.inputs()[0]
    source.parmTuple('r').set(hou.Vector3(0,1,0).matrixToRotateTo(delta).extractRotates())
    return n

def copy(parent,name,prototype,points,attributes):
    n=create(parent,'copytopoints::2.0',name,prototype,pack=0,useimplicitn=0,targetattribs=1,
             useapply1=1,applyto1=0,applymethod1=0,applyattribs1=attributes)
    n.setInput(1,points);return n

def placements(parent,name,inlet,kind):
    n=create(parent,'attribwrangle',name,inlet,**{'class':0,'snippet':(VEX/'hero_placements.vfl').read_text()})
    if n.parm('kind') is None:n.addSpareParmTuple(hou.StringParmTemplate('kind','Module kind',1,default_value=(kind,)))
    n.parm('kind').set(kind);return n

def build(module):
    parent=module.node('REFERENCE_HERO_MODULES') or module.createNode('subnet','REFERENCE_HERO_MODULES')
    parent.setInput(0,module.node('CLASSIFY_MODULES_AND_OVERRIDE_SLOTS'))
    inlet=create(parent,'null','IN_classified_bays');inlet.setInput(0,parent.indirectInputs()[0])
    # An actual hollow furnace: a native union arch cutter subtracts its mouth.
    body=box(parent,'forge_outer',(2.3,2.7,1.45),(0,1.35,-.70),'stone')
    lower=box(parent,'furnace_cutter_lower',(1.6,1.55,2.2),(0,.79,-.70),'stone')
    arch=tube(parent,'furnace_cutter_arch',.80,2.2,(0,1.55,-.70),'stone','z',cols=20)
    cutter=boolean(parent,'UNION_FURNACE_APERTURE',lower,arch,'union')
    furnace=tag(parent,boolean(parent,'CUT_FURNACE_MOUTH',body,cutter,'subtract'),'forge_masonry','stone')
    flue_outer=tube(parent,'flue_outer',.54,6.60,(0,6.00,-.70),'stone',scale=.90,cols=20)
    flue_inner=tube(parent,'flue_inner',.39,6.90,(0,6.00,-.70),'stone',scale=.90,cols=20)
    flue=tag(parent,boolean(parent,'HOLLOW_INDUSTRIAL_FLUE',flue_outer,flue_inner,'subtract'),'forge_flue','stone')
    parts=[furnace,flue,box(parent,'forge_back',(1.58,2.30,.06),(0,1.20,-.025),'metal'),
           box(parent,'forge_hearth',(1.58,.12,.85),(0,.16,-.78),'light')]
    for j,y in enumerate((3.0,5.5,8.95)):
        a=tube(parent,'flue_band_outer_'+str(j),.60,.14,(0,y,-.70),'metal',cols=20)
        b=tube(parent,'flue_band_inner_'+str(j),.46,.25,(0,y,-.70),'metal',cols=20)
        parts.append(tag(parent,boolean(parent,'HOLLOW_FLUE_BAND_'+str(j),a,b,'subtract'),'flue_band_'+str(j),'metal'))
    for j in range(10):
        angle=math.pi*(j+.5)/10
        parts.append(box(parent,'arch_stone_'+str(j),(.265,.23,.14),(.94*math.cos(angle),1.55+.94*math.sin(angle),-1.49),'stone',(0,0,math.degrees(angle)-90)))
    for side in (-1,1):
        for j in range(4):parts.append(box(parent,f'jamb_stone_{side}_{j}',(.26,.32,.14),(side*.94,.25+j*.36,-1.49),'stone'))
    forge=merge(parent,'FORGE_KIT',parts)
    forgecopy=copy(parent,'COPY_FORGE_KIT',forge,placements(parent,'FORGE_DATA',inlet,'forge'),'module_prefix')
    # Timber jib, grounded wall brace, rope and ring; the warehouse adds a crate.
    hoist=merge(parent,'HOIST_KIT',[
        box(parent,'hoist_jib',(.22,.24,2.25),(0,0,-1.00)),
        box(parent,'hoist_wall_post',(.24,1.45,.24),(0,-.60,-.035)),
        beam(parent,'hoist_knee',(0,-1.15,-.04),(0,-.10,-1.42),.15),
        tube(parent,'hoist_rope',.025,1.15,(0,-.65,-1.90),'wood',cols=8)])
    ring=create(parent,'torus','hoist_ring',type='poly',orient='z',radx=.14,rady=.025,rows=8,cols=16,ty=-1.30,tz=-1.90)
    hoist=merge(parent,'HOIST_WITH_RING',[hoist,tag(parent,ring,'hoist_ring','metal')])
    hoistcopy=copy(parent,'COPY_HOIST_KIT',hoist,placements(parent,'HOIST_DATA',inlet,'hoist'),'module_prefix')
    crate_parts=[box(parent,'hoist_crate_body',(.72,.70,.72),(0,-1.72,-1.90))]
    for side in (-1,1):
        crate_parts.append(box(parent,'crate_rail_'+str(side),(.76,.09,.09),(0,-1.72+side*.25,-2.28)))
    crate_parts.append(beam(parent,'crate_diagonal_a',(-.29,-1.97,-2.285),(.29,-1.47,-2.285),.07))
    crate_parts.append(beam(parent,'crate_diagonal_b',(.29,-1.97,-2.29),(-.29,-1.47,-2.29),.07))
    cratecopy=copy(parent,'COPY_HANGING_CRATE',merge(parent,'CRATE_KIT',crate_parts),placements(parent,'CRATE_DATA',inlet,'crate'),'module_prefix')
    allkits=merge(parent,'MERGE_INSTANCED_KITS',[forgecopy,hoistcopy,cratecopy])
    names=create(parent,'attribwrangle','INSTANCE_NAMES',allkits,**{'class':1,'snippet':'s@name="facade/"+point(0,"module_prefix",primpoint(0,@primnum,0))+"/"+s@name;'})
    # Square course-built corner turrets use the actual facade top datum.
    guild=create(parent,'attribwrangle','GUILD_PIER_DATA',inlet,**{'class':0,'snippet':(VEX/'guild_piers.vfl').read_text()})
    if guild.parm('caps') is None:guild.addSpareParmTuple(hou.ToggleParmTemplate('caps','Caps',False))
    guild.parm('caps').set(0)
    caps=create(parent,'attribwrangle','GUILD_CAP_DATA',inlet,**{'class':0,'snippet':(VEX/'guild_piers.vfl').read_text()})
    if caps.parm('caps') is None:caps.addSpareParmTuple(hou.ToggleParmTemplate('caps','Caps',True))
    caps.parm('caps').set(1)
    unit=create(parent,'box','GUILD_UNIT_BOX')
    pyramid=create(parent,'tube','SQUARE_PYRAMID',type='poly',orient='y',cap=1,rad1=.01767767,rad2=.70710678,radscale=1.,height=1.,cols=4,ry=45.)
    piers=copy(parent,'COPY_GUILD_STONE',unit,guild,'part_name part_material part_color')
    roofs=copy(parent,'COPY_GUILD_CAPS',pyramid,caps,'part_name part_material part_color')
    guildmerge=merge(parent,'MERGE_GUILD_PARTS',[piers,roofs])
    guildmeta=create(parent,'attribwrangle','GUILD_METADATA',guildmerge,**{'class':1,'snippet':'''int pt=primpoint(0,@primnum,0);
s@name=point(0,"part_name",pt);s@material_family=point(0,"part_material",pt);v@Cd=point(0,"part_color",pt);s@component="facade_details";'''})
    result=merge(parent,'MERGE_HERO_GEOMETRY',[names,guildmeta])
    out=create(parent,'output','OUT_hero_geometry',result,outputidx=0)
    out.setDisplayFlag(True);out.setRenderFlag(True)
    complete=merge(module,'MERGE_FACADE_AND_HERO',[module.node('COPY_FACADE_PARTS'),parent])
    module.node('FACADE_VERTEX_NORMALS').setInput(0,complete)
    return parent
