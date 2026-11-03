"""Scoped repairs after the measured lossless migration; no Python mesh cooks."""
import hou
from .registry import ROOT
from .graph_core import create_node,stage_parameter,tag

EXTENSIONS={
 'preview_quality':{'type':'string','default':'high','enum':['low','high']},
 'uv_tile_scale':{'type':'number','default':1.0,'min':.05,'max':20.0},
 'uv_local_projection':{'type':'boolean','default':True},
 'window_move_id':{'type':'string','default':''},
 'window_offset_u_m':{'type':'number','default':0.0,'min':-1.0,'max':1.0},
}

def add_controls(ctrl):
    group=ctrl.parmTemplateGroup()
    for name,spec in EXTENSIONS.items():
        if group.find(name):continue
        if spec['type']=='number':
            parm=hou.FloatParmTemplate(name,name.replace('_',' ').title(),1,default_value=(spec['default'],),min=spec['min'],max=spec['max'],min_is_strict=True,max_is_strict=True)
        elif spec['type']=='boolean':parm=hou.ToggleParmTemplate(name,name.replace('_',' ').title(),spec['default'])
        else:
            parm=hou.StringParmTemplate(name,name.replace('_',' ').title(),1,default_value=(spec['default'],))
            if 'enum' in spec:parm.setMenuItems(spec['enum']);parm.setMenuLabels([s.upper() for s in spec['enum']])
        group.append(parm)
    ctrl.setParmTemplateGroup(group)

def local_arg(node,stage,ctrl,key):
    stage_parameter(stage,ctrl,key)
    group=node.parmTemplateGroup()
    if group.find(key) is None:
        group.append(ctrl.parm(key).parmTemplate());node.setParmTemplateGroup(group)
    p=node.parm(key);p.deleteAllKeyframes()
    fn='chs' if p.parmTemplate().type()==hou.parmTemplateType.String else 'ch'
    p.setExpression(f'{fn}("{node.relativePathTo(stage)}/{key}")',hou.exprLanguage.Hscript)

def apply(building,modules,stages):
    ctrl=building.node('CTRL_BUILDING')
    layout=modules['shell.facade_layout']
    for key in ('window_move_id','window_offset_u_m'):local_arg(layout,stages['SHELL'],ctrl,key)
    # Stable host IDs and window-local offset travel with the canonical visible bay stream.
    bays=layout.node('FACADE_BAYS');code=bays.parm('snippet').eval()
    code=code.replace('setprimattrib(0,"bay_id",p,key);',
        'setprimattrib(0,"wall_id",p,role+"/"+itoa(floor)+"/"+itoa(side));\n'
        '   float offset=(opening=="W" && key==chs("../window_move_id"))?ch("../window_offset_u_m"):0.;\n'
        '   setprimattrib(0,"opening_offset_u_m",p,offset);\n'
        '   setprimattrib(0,"bay_id",p,key);')
    bays.parm('snippet').set(code)
    openings=layout.node('FACADE_OPENING_POINTS');code=openings.parm('snippet').eval()
    code=code.replace('c.y=bottom+ph*sy*.5;c+=n*.015;',
        'float offset=prim(0,"opening_offset_u_m",src);\n'
        ' if(abs(offset)+(envelope*sx)*.5>bay*.5-.10)error("Window outside host bay: %s",string(prim(0,"bay_id",src)));\n'
        ' c.y=bottom+ph*sy*.5;c+=n*.015+u*offset;')
    code=code.replace('setpointattrib(0,"opening_kind",pt,kind);',
        'setpointattrib(0,"opening_kind",pt,kind);\n setpointattrib(0,"wall_id",pt,string(prim(0,"wall_id",src)));')
    openings.parm('snippet').set(code)
    attic=layout.node('ATTIC_OPENING_POINTS')
    attic.parm('snippet').set(attic.parm('snippet').eval().replace(
        'setpointattrib(0,"opening_id",pt,"attic");',
        'setpointattrib(0,"opening_id",pt,"attic");setpointattrib(0,"wall_id",pt,"main/attic/front");'))
    facade=modules['details.facade'].node('FACADE_DETAIL_MODULE/FUNCTIONAL_MODULE_POINTS')
    code=facade.parm('snippet').eval()
    code=code.replace('center.y=y;', 'center.y=y;\n    if(prim(0,"opening",pr)=="W")center+=u*float(prim(0,"opening_offset_u_m",pr));')
    facade.parm('snippet').set(code)
    finish=modules['uv_material.surfacing'].node('F_FINISH')
    for key in ('uv_tile_scale','uv_local_projection'):local_arg(finish,stages['UV_MATERIAL'],ctrl,key)
    uv=finish.node('METRIC_UV');code=uv.parm('snippet').eval()
    code+='''
// Isometric local projection on sloped faces; axis-aligned legacy trim orientation stays intact.
vector normal=normalize(prim_normal(0,@primnum,.5,.5));
if(chi("../uv_local_projection") && abs(normal.y)>.001 && abs(normal.y)<.999){
 vector u=normalize(cross(set(0,1,0),normal));vector v=normalize(cross(normal,u));
 v@uv=set(dot(@P,u),dot(@P,v),0);
}
v@uv*=ch("../uv_tile_scale");
'''
    uv.parm('snippet').set(code)
    from .roof_quality import finish_modules
    finish_modules(building,modules,stages)
    output=modules['output.game_export']
    metadata=output.node('export_semantic_metadata')
    metadata.parm('snippet').set(metadata.parm('snippet').eval()+'\nint pt=primpoint(0,@primnum,0);\nif(s@wall_id=="")s@wall_id=point(0,"wall_id",pt);\nif(s@roof_patch_id=="")s@roof_patch_id=point(0,"roof_patch_id",pt);\n')
