"""Update the existing native function graph, retaining its public contracts."""
from pathlib import Path
import collections
import hashlib
import json
import shutil
import time
import hou
from render_review import plain_materials
import hero_modules

ROOT=Path(__file__).resolve().parents[1]
VEX=ROOT/'sources/vex'
VERSION='2026.11-medieval-layout-2'

def spare(node,template,expression=None):
    if not node.parm(template.name()):
        group=node.parmTemplateGroup();group.append(template);node.setParmTemplateGroup(group)
    if expression is not None:node.parm(template.name()).setExpression(expression,hou.exprLanguage.Hscript)

def string_arg(node,name,default,source):
    spare(node,hou.StringParmTemplate(name,name.replace('_',' ').title(),1,default_value=(default,)),f'chs("../CTRL_BUILDING/{source}")')

def float_arg(node,name,default,source):
    spare(node,hou.FloatParmTemplate(name,name.replace('_',' ').title(),1,default_value=(default,)),f'ch("../CTRL_BUILDING/{source}")')

def wrangle(parent,name,source,input_node=None,runover=0):
    node=parent.node(name) or parent.createNode('attribwrangle',name)
    node.setParms({'class':runover,'snippet':source})
    if input_node:node.setInput(0,input_node)
    node.setUserData('medieval_layout_owner',VERSION)
    return node

def source(node,filename):node.parm('snippet').set((VEX/filename).read_text(encoding='utf-8'))

def copy_attributes(node,attributes):
    node.setParms({'useimplicitn':0,'targetattribs':1,'useapply1':1,'applyto1':0,'applymethod1':0,'applyattribs1':attributes})

def apply_updates(b):
    c=b.node('CTRL_BUILDING')
    group=c.parmTemplateGroup()
    for key,maximum in [('width_m',14.),('depth_m',12.),('roof_height_m',5.),('door_width_m',3.2),('door_height_m',3.2)]:
        t=group.find(key);t.setMaxValue(maximum);group.replace(key,t)
    c.setParmTemplateGroup(group)
    group=c.parmTemplateGroup();t=group.find('palette')
    t.setMenuItems(('storybook_red','slate_blue'));t.setMenuLabels(('Storybook Red','Slate Blue'))
    group.replace('palette',t);c.setParmTemplateGroup(group)
    # Numeric ranges are part of each typed local argument as well as the UI.
    for function in (n for n in b.children() if n.type().name()=='subnet'):
        group=function.parmTemplateGroup();changed=False
        for key,maximum in [('width_m',14.),('depth_m',12.),('door_width_m',3.2),('door_height_m',3.2)]:
            t=group.find(key)
            if t and isinstance(t,hou.FloatParmTemplate):t.setMaxValue(maximum);group.replace(key,t);changed=True
        if changed:function.setParmTemplateGroup(group)
    for name,label,items,default in [
        ('roof_axis','Roof ridge axis',('z','x'),1),
        ('window_shape','Window shape',('rectangular','arched'),0)]:
        spare(c,hou.MenuParmTemplate(name,label,items,menu_labels=items,default_value=default))
    spare(c,hou.FloatParmTemplate('awning_span_ratio','Awning facade span',1,default_value=(.78,),min=.25,max=.95))
    spare(c,hou.IntParmTemplate('seed_decoration','Decoration seed',1,default_value=(17,)))
    layout=b.node('F_FACADE_LAYOUT')
    for name,val in [('frame_width_m',.09),('floor_thickness_m',.12)]:float_arg(layout,name,val,name)
    spare(layout,hou.ToggleParmTemplate('shutters_enable','Shutters',True),'ch("../CTRL_BUILDING/shutters_enable")')
    bays=layout.node('FACADE_BAYS');s=bays.parm('snippet').eval()
    if 'bay_count' not in s:
        s=s.replace('string row=rows[min(floor,len(rows)-1)];int n=max(1,len(row));',
                    'string row=rows[min(floor,len(rows)-1)];int n=1;foreach(string r;rows)n=max(n,len(r));')
        s=s.replace('string opening=row[j:j+1];','string opening=j<len(row)?row[j:j+1]:".";')
        s=s.replace('opening=j<requested?"W":".";if(side==0&&floor==0&&j==n/2)opening="D";',
                    'opening=".";int first=(n-requested)/2; if(j>=first&&j<first+requested)opening="W";if(side==0&&floor==0&&j==n/2)opening="D";')
        s=s.replace('setprimattrib(0,"bay_width",p,length);','setprimattrib(0,"bay_count",p,n);setprimattrib(0,"bay_width",p,length);')
        s=s.replace('itoa(j);','itoa(round?ringindex:j);')
        bays.parm('snippet').set(s)
    source(layout.node('FACADE_OPENING_POINTS'),'opening_points.vfl')
    details=b.node('F_FACADE_DETAILS')
    source(details.node('TIMBER_PLACEMENT'),'timber_placement.vfl')
    source(details.node('TIMBER_BRACE_CURVES'),'timber_braces.vfl')
    source(details.node('FACADE_DETAIL_MODULE/CLASSIFY_MODULES_AND_OVERRIDE_SLOTS'),'facade_classify.vfl')
    hero_modules.build(details.node('FACADE_DETAIL_MODULE'))
    # Preserve the existing independent roof_plan output, adding only a local argument.
    plan=b.node('F_PLAN');string_arg(plan,'roof_axis','x','roof_axis')
    rp=plan.node('ROOF_PLANS');s=rp.parm('snippet').eval()
    if 'roof_axis' not in s:
        s+='\nif(i@floor_i==i@floor_count-1 && !i@is_round && s@role=="main" && chs("../roof_axis")=="x"){float w=f@width;f@width=f@depth;f@depth=w;f@rot_y+=90.;}\n'
        rp.parm('snippet').set(s)
    # Rectangular casement windows and matching rectangular wall cutters.
    kits=b.node('F_OPENING_KITS');string_arg(kits,'window_shape','rectangular','window_shape')
    aperture=kits.node('WINDOW_MODULE/clear_aperture_profile');s=aperture.parm('snippet').eval()
    if 'window_shape' not in s:
        s=s.replace('vector outline[]=arch_outline(w,h,rise,0.);',
                    'vector outline[]=arch_outline(w,h,rise,0.);\nif(chs("../../window_shape")=="rectangular")outline=array(set(-w*.5,-h*.5,0),set(w*.5,-h*.5,0),set(w*.5,h*.5,0),set(-w*.5,h*.5,0));')
        aperture.parm('snippet').set(s)
    frame=kits.node('WINDOW_MODULE/frame_arch_profiles');s=frame.parm('snippet').eval()
    if 'window_shape' not in s:
        s=s.replace('append(outer,q);','if(chs("../../window_shape")=="rectangular")q.y=p.y+(p.y>0?f:-f);\n    append(outer,q);')
        frame.parm('snippet').set(s)
    # Native copies of interior framing use the declared bay geometry input.
    interior=b.node('F_INTERIOR');interior.setInput(1,layout,0)
    interior.parm('label2').set('visible_facade_bays')
    inlet=interior.node('IN_visible_facade_bays') or interior.createNode('null','IN_visible_facade_bays')
    inlet.setInput(0,interior.indirectInputs()[1])
    data=wrangle(interior,'INTERIOR_WALL_FRAME_DATA',(VEX/'interior_frame.vfl').read_text(),inlet)
    copy=interior.node('COPY_INTERIOR_WALL_FRAMES') or interior.createNode('copytopoints::2.0','COPY_INTERIOR_WALL_FRAMES')
    copy.setInput(0,interior.node('unit_member'));copy.setInput(1,data);copy.parm('pack').set(0)
    copy_attributes(copy,'part_name part_material part_color role role_id floor_i')
    meta=wrangle(interior,'INTERIOR_FRAME_METADATA',interior.node('member_materials_and_names').parm('snippet').eval(),copy,1)
    merge=interior.node('merge_interior_structure');merge.setInput(2,meta)
    floors=interior.node('OUT_FLOORS')
    floor_meta=wrangle(interior,'FLOOR_EXTRUSION_METADATA','''s@material_family="wood";v@Cd=set(.27,.19,.12);
if(s@name==""){int pr;vector uv;xyzdist(1,@P,pr,uv);s@name=prim(1,"name",pr)+"/edge";}
s@component="interior";''',interior.node('extrude_floor_thickness'),1)
    floor_meta.setInput(1,interior.node('floor_and_spiral_profiles'));floors.setInput(0,floor_meta)
    # A second explicit input gives the exterior function the actual roof datum.
    details.setInput(1,plan,3);details.parm('label2').set('roof_plans')
    for name,default in [('roof_shape','gable')]:string_arg(details,name,default,name)
    spare(details,hou.ToggleParmTemplate('attic_window_enable','Attic window',False),'ch("../CTRL_BUILDING/attic_window_enable")')
    roofin=details.node('IN_roof_plans') or details.createNode('null','IN_roof_plans');roofin.setInput(0,details.indirectInputs()[1])
    data=wrangle(details,'GABLE_TRUSS_DATA',(VEX/'gable_frame.vfl').read_text(),roofin)
    copy=details.node('COPY_GABLE_TRUSSES') or details.createNode('copytopoints::2.0','COPY_GABLE_TRUSSES')
    copy.setInput(0,details.node('TIMBER_UNIT'));copy.setInput(1,data);copy.parm('pack').set(0)
    copy_attributes(copy,'name')
    details.node('MERGE_TIMBER').setInput(2,copy)
    # Awning width follows the facade proportion instead of covering one random bay.
    site=b.node('F_SITE');float_arg(site,'awning_span_ratio',.78,'awning_span_ratio')
    for name in ('awning_and_canopy_profiles','entry_and_canopy_placements'):
        node=site.node(name);s=node.parm('snippet').eval().replace('w/bays','w*ch("../awning_span_ratio")')
        node.parm('snippet').set(s)
    finish=b.node('F_FINISH')
    finish.node('BIND_MATERIALS').parm('snippet').set('s@shop_materialpath=chs("../material_root")+"/"+s@material_family;')
    float_arg(finish,'seed_decoration',17,'seed_decoration')
    names=finish.node('FINAL_COMPONENT_NAMES');s=names.parm('snippet').eval()
    if 'seed_decoration' not in s:
        s+='\nfloat shade=fit01(rand(random_shash(s@name)+chi("../seed_decoration")),.94,1.04);v@Cd*=shade;\n'
    if 'MATERIAL_PALETTE' not in s:
        s+='''
// MATERIAL_PALETTE: the same constant base colors are used by previews and UE.
vector color=set(.42,.32,.22);string family=s@material_family;
if(family=="plaster")color=set(.79,.66,.45);
else if(family=="wood")color=set(.22,.10,.041);
else if(family=="stone")color=set(.45,.43,.37);
else if(family=="tile"||family=="roof")color=chs("../palette")=="slate_blue"?set(.15,.27,.42):set(.54,.19,.075);
else if(family=="glass")color=set(.92,.51,.13);
else if(family=="teal")color=set(.12,.32,.37);
else if(family=="canvas")color=set(.73,.52,.28);
else if(family=="metal")color=set(.13,.15,.16);
else if(family=="rock")color=set(.42,.43,.34);
else if(family=="light")color=set(1.,.67,.25);
else if(family=="leaf")color=set(.22,.36,.08);
else if(family=="flower")color=set(.70,.35,.22);
else if(family=="sail")color=set(.78,.73,.57);
v@Cd=color*shade;
'''
    names.parm('snippet').set(s)
    plain_materials(b,False)
    for mat in b.node('materials').children():
        if mat.parm('basecolor_usePointColor'):mat.parm('basecolor_usePointColor').set(0)
        if mat.name() in ('roof','tile'):
            for p,blue,red in zip(mat.parmTuple('basecolor'),(.15,.27,.42),(.54,.19,.075)):
                p.setExpression(f'if(strcmp(chs("../../CTRL_BUILDING/palette"),"slate_blue")==0,{blue},{red})',hou.exprLanguage.Hscript)
    b.setUserData('medieval_layout_version',VERSION)
    b.setDisplayFlag(True)
    return b

def schema(c):
    params={}
    for p in c.parms():
        if p.name() in ('copyinput','cacheinput','seed_decoration') or p.parmTemplate().type()==hou.parmTemplateType.FolderSet:continue
        t=p.parmTemplate();v=p.eval()
        if isinstance(t,hou.ToggleParmTemplate):item={'type':'boolean','default':bool(v)}
        elif isinstance(t,hou.MenuParmTemplate):item={'type':'string','default':p.evalAsString(),'enum':list(p.menuItems())}
        elif isinstance(t,hou.StringParmTemplate):
            item={'type':'string','default':v}
            if t.menuItems():item['enum']=list(t.menuItems())
        elif isinstance(t,hou.IntParmTemplate):item={'type':'integer','default':v}
        elif isinstance(t,hou.FloatParmTemplate):item={'type':'number','default':v}
        else:continue
        params[p.name()]=item
    return {'schema_version':1,'parameters':params,'seed_mapping':{'decoration':'seed_decoration'},
            'constraints':['One ground-floor entrance D on front facade.', 'Pattern rows contain only W D . and have equal bay counts per elevation.',
                           'All full shutter envelopes fit between structural posts.','Rectangular gable and L-shape variants retain native roof junctions.']}

def main():
    path=ROOT/'medieval_building.hip';backup=ROOT/'backup/before_layout_20261102'
    backup.mkdir(parents=True,exist_ok=True)
    if not (backup/path.name).exists():shutil.copy2(path,backup/path.name)
    hou.hipFile.load(str(path),suppress_save_prompt=True,ignore_load_warnings=True)
    b=apply_updates(hou.node('/obj/medieval_building'))
    c=b.node('CTRL_BUILDING')
    defaults={'width_m':8.2,'depth_m':6.4,'floor_count':2,'floor_height_m':2.65,
              'front_bays':'WDW/W.W','right_bays':'W.W/W.W','rear_bays':'W.W/W.W','left_bays':'W.W/W.W',
              'upper_overhang_m':.16,'roof_height_m':2.15,'eave_curve':.14,'roof_axis':'x',
              'attic_window_enable':False,'chimney_count':1,'facade_style':'B','timber_pattern':'curved_brace',
              'window_width_m':.95,'window_height_m':1.25,'awning_enable':False,'entrance_canopy':True,
              'shutters_enable':True,'ground_slope_preset':'flat'}
    c.setParms({k:int(v) if isinstance(v,bool) else v for k,v in defaults.items()})
    start=time.monotonic();geo=b.node('OUT_BUILDING').geometry()
    errors=[(n.path(),n.errors()) for n in b.allSubChildren() if n.errors()]
    if errors:raise RuntimeError(errors)
    record={'version':VERSION,'houdini':hou.applicationVersionString(),'seconds':time.monotonic()-start,
            'points':len(geo.points()),'primitives':len(geo.prims()),'bounds':list(geo.boundingBox().sizevec()),
            'sop_types':dict(collections.Counter(n.type().name() for n in b.allSubChildren())),
            'python_sops':[n.path() for n in b.allSubChildren() if n.type().name()=='python'],'errors':errors}
    (ROOT/'parameter_schema.json').write_text(json.dumps(schema(c),indent=2),encoding='utf-8')
    (ROOT/'review_20261102/update_graph.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
    hou.hipFile.save(str(path))
    print(json.dumps(record),flush=True)

if __name__=='__main__':main()
