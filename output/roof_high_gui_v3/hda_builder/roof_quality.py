"""Native roof envelope/junction and LOW/HIGH factories used by one shared HDA."""
import hou
from .registry import ROOT,inventory
from .graph_core import create_node,publish_outputs,make_subnet,fix_helper_interfaces

def node(parent,kind,name,source=None,index=0,**params):
    n=create_node(parent,kind,name,parent.userData('module_id'))
    if source:n.setInput(0,source,index)
    for key,value in params.items():
        p=n.parm(key)
        if p is None:raise ValueError((n.path(),key))
        p.set(value)
    return n

def wrangle(parent,name,code,source,cls=1):
    return node(parent,'attribwrangle',name,source,**{'class':cls,'snippet':code})

def inlet(parent,name,index):
    n=node(parent,'null','IN_'+name);n.setInput(0,parent.indirectInputs()[index]);return n

def bind(module,stage,ctrl,key):
    from .corrections import local_arg
    local_arg(module,stage,ctrl,key)

def build_custom(spec,stage,building,library):
    ident=spec['module_id'];name=spec['path'].split('/')[-1]
    n=make_subnet(stage,name,list(spec['inputs']),spec['outputs'],library,'mythic::medieval_module_'+ident.replace('.','_')+'::3.0',ident)
    if ident=='roof.patch_surfaces':ends=patch_surfaces(n,stage,building)
    elif ident=='roof.junctions':ends=junctions(n)
    elif ident=='output.detail_levels':ends=detail_levels(n,stage,building.node('CTRL_BUILDING'))
    else:raise ValueError('Unknown module factory '+ident)
    return n,publish_outputs(n,spec['outputs'],[(x,0) for x in ends],ident)

def patch_surfaces(module,stage,building):
    source=building.node('F_ROOF');ctrl=building.node('CTRL_BUILDING')
    names=['IN_roof_plans','ROOF_SECTION_PROFILES','GABLE_ROOF_SECTIONS','CONE_ROOF_SECTIONS',
           'SKIN_GABLE_ROOFS','SKIN_CONE_ROOFS','HIPPED_ROOF_MODULE','ROOF_SURFACE',
           'DETECT_ROOF_WINDING','ORIENT_ROOF_SURFACE','INWARD_ROOF_PROFILES','ROOF_THICKNESS']
    original={n:source.node(n) for n in names}
    edges={name:[(c.inputIndex(),c.inputNode().name(),c.outputIndex()) for c in n.inputConnections() if c.inputNode() is not None] for name,n in original.items()}
    moved=dict(zip(names,hou.moveNodesTo(tuple(original.values()),module)))
    for name,n in moved.items():
        for idx,up,out in edges[name]:
            if up in moved:n.setInput(idx,moved[up],out)
    moved['IN_roof_plans'].setInput(0,module.indirectInputs()[0])
    for key in ('roof_shape','tile_size_m','mesh_quality'):bind(module,stage,ctrl,key)
    code=inventory()['F_ROOF/ROOF_SECTION_PROFILES']['parameters']['snippet']
    for flag in (0,1):
        marker=f'setprimattrib(0,"profile_round",p,{flag});'
        code=code.replace(marker,marker+'setprimattrib(0,"roof_role",p,role);')
    code=code.replace('setpointattrib(0,"roof_role",pt,role);','setpointattrib(0,"roof_role",pt,role);setpointattrib(0,"roof_patch_id",pt,"roof/"+itoa(role));')
    moved['ROOF_SECTION_PROFILES'].parm('snippet').set(code)
    # A single Skin over main+wing curves creates a spurious bridge. Partition roles first.
    for family in ('GABLE','CONE'):
        filtered=moved[family+'_ROOF_SECTIONS'];skin=moved['SKIN_'+family+'_ROOFS']
        main=node(module,'blast',family+'_MAIN_ONLY',filtered,group='@roof_role=0',grouptype='prims',negate=1)
        secondary=node(module,'blast',family+'_SECONDARY_ONLY',filtered,group='@roof_role=1',grouptype='prims',negate=1)
        skin.setInput(0,main)
        other=hou.copyNodesTo((skin,),module)[0];other.setName('SKIN_'+family+'_SECONDARY');other.setInput(0,secondary)
        moved['ROOF_SURFACE'].setNextInput(other)
    fix_helper_interfaces(module,'F_ROOF')
    raw=wrangle(module,'canonical_patch_identity','i@roof_role=point(0,"roof_role",primpoint(0,@primnum,0));s@roof_patch_id="roof/"+itoa(i@roof_role);',moved['ORIENT_ROOF_SURFACE'])
    return [raw]

def junctions(module):
    raw=inlet(module,'patches',0)
    volumes=[]
    for role in (0,1):
        selected=node(module,'blast','roof_role_'+str(role),raw,group='@roof_role='+str(role),grouptype='prims',negate=1)
        reverse=node(module,'reverse','orient_volume_'+str(role),selected)
        direction=wrangle(module,'volume_direction_'+str(role),'v@extrude_direction={0,-1,0};',reverse,2)
        volume=node(module,'polyextrude::2.0','closed_roof_patch_volumes' if role==0 else 'closed_secondary_roof_volume',direction,
                    splittype='components',extrusionmode='pointnormal',ptnormalsrc='attribute',ptnormalattrib='extrude_direction',dist=30.,outputback=1,outputfront=1,outputside=1)
        volumes.append(volume)
    union=node(module,'boolean::2.0','resolve_roof_volume_union',volumes[0],booleanop='union',resolvea=1,resolveb=1,mergenbrs=0);union.setInput(1,volumes[1])
    triangles=node(module,'divide','convex_roof_fragments',union,usemaxsides=1,numsides=3)
    mask=wrangle(module,'classify_upper_envelope','''int pr;vector uv,hit;float distance=xyzdist(1,@P,pr,uv);
int above=intersect(1,@P+set(0,50,0),set(0,-100,0),hit,uv);
i@group_roof_upper_envelope=abs(prim_normal(0,@primnum,.5,.5).y)>.02&&distance<1e-4&&above>=0&&abs(hit.y-@P.y)<2e-4;
i@group_flip_roof_top=prim_normal(0,@primnum,.5,.5).y<0;''',triangles)
    mask.setInput(1,raw)
    top=node(module,'blast','retain_visible_roof_domain',mask,group='roof_upper_envelope',grouptype='prims',negate=1)
    top=node(module,'reverse','outward_upper_envelope',top,group='flip_roof_top')
    top=wrangle(module,'restore_resolved_patch_ids','int pr;vector uv;f@roof_surface_error=xyzdist(1,@P,pr,uv);s@roof_patch_id=prim(1,"roof_patch_id",pr);i@roof_role=prim(1,"roof_role",pr);',top)
    top.setInput(1,raw)
    reverse_top=node(module,'reverse','roof_shell_orientation',top)
    down=wrangle(module,'roof_shell_thickness_direction','v@extrude_direction={0,-1,0};',reverse_top,2)
    interior=node(module,'polyextrude::2.0','closed_resolved_roof_interior',down,
               splittype='components',extrusionmode='pointnormal',ptnormalsrc='attribute',ptnormalattrib='extrude_direction',dist=30.,outputback=1,outputfront=1,outputside=1)
    shell=node(module,'polyextrude::2.0','continuous_roof_thickness',down,
               splittype='components',extrusionmode='pointnormal',ptnormalsrc='attribute',ptnormalattrib='extrude_direction',dist=.13,outputback=1,outputfront=1,outputside=1)
    shell=wrangle(module,'roof_domain_material','int pr;vector uv;xyzdist(1,@P,pr,uv);s@roof_patch_id=prim(1,"roof_patch_id",pr);s@material_family="tile";v@Cd={.45,.22,.09};s@name="roof_shell/"+s@roof_patch_id;',shell)
    shell.setInput(1,top)
    guides=wrangle(module,'ridge_valley_guides',(ROOT/'vex/roof_junction_guides.vfl').read_text(encoding='utf-8'),top,0)
    trim=node(module,'polywire','ridge_valley_flashing',guides,radius=.022,div=6)
    return [top,interior,shell,trim]

def detail_levels(module,stage,ctrl):
    src=inlet(module,'building',0)
    for key in ('preview_quality','bevel_width_m'):bind(module,stage,ctrl,key)
    masks=wrangle(module,'exact_face_cleanup_masks',(ROOT/'vex/mark_duplicate_degenerate.vfl').read_text(encoding='utf-8'),src,0)
    masks.parm('vex_precision').set('64')
    low=node(module,'blast','LOW_VALIDATED',masks,group='qa_remove_degenerate qa_remove_duplicate',grouptype='prims',negate=0)
    eligible=wrangle(module,'high_semantic_members','''int eligible=s@material_family=="wood"||s@material_family=="stone"||s@material_family=="metal"||s@material_family=="tile";
if(re_match(".*(roof_shell|mullion|transom|strap|hinge|baluster|leaf|plant|shutter|glazing|flashing).*",s@name))eligible=0;
i@group_high_members=eligible;
''',low)
    scales=wrangle(module,'local_feature_bevel_limits','''int near[]=neighbours(0,@ptnum);int faces[]=pointprims(0,@ptnum);
int valid=len(near)==3&&len(faces)==3;float edge=1e9;vector dirs[];
foreach(int pt;near){vector d=point(0,"P",pt)-@P;edge=min(edge,length(d));append(dirs,normalize(d));}
foreach(int pr;faces)if(primvertexcount(0,pr)!=4||!inprimgroup(0,"high_members",pr))valid=0;
for(int i=0;i<len(dirs);i++)for(int j=i+1;j<len(dirs);j++)if(abs(dot(dirs[i],dirs[j]))>.005)valid=0;
i@high_box_corner=valid&&edge>1e-5;
float base=min(ch("../bevel_width_m"),.025);f@high_offset_scale=min(1.,max(0.,edge*.12/max(base,1e-6)));''',eligible,2)
    boxes=wrangle(module,'bevel_only_orthogonal_members','int valid=1;foreach(int pt;primpoints(0,@primnum))if(!point(0,"high_box_corner",pt))valid=0;i@group_high_members=valid;',scales)
    high=node(module,'polybevel::3.0','HIGH_FEATURE_BEVEL',boxes,group='high_members',grouptype='prims',
              offset=.012,divisions=3,ignoreflatedges=1,detectcollisions=1,limit='individually',useoffsetscale='byattrib',pointscaleattr='high_offset_scale')
    high.parm('offset').setExpression('min(ch("../bevel_width_m"),0.025)',hou.exprLanguage.Hscript)
    restore=wrangle(module,'restore_high_semantics','''if(s@name==""||s@material_family==""||s@module_id==""){
int pr;vector uv;xyzdist(1,@P,pr,uv);
s@name=prim(1,"name",pr);s@material_family=prim(1,"material_family",pr);s@module_id=prim(1,"module_id",pr);s@part_role=prim(1,"part_role",pr);s@shop_materialpath=prim(1,"shop_materialpath",pr);v@Cd=prim(1,"Cd",pr);
}i@high_detail_geometry=1;''',high);restore.setInput(1,low)
    checked=wrangle(module,'high_exact_face_cleanup',(ROOT/'vex/mark_duplicate_degenerate.vfl').read_text(encoding='utf-8'),restore,0)
    checked.parm('vex_precision').set('64')
    high_valid=node(module,'blast','HIGH_VALIDATED',checked,group='qa_remove_degenerate qa_remove_duplicate',grouptype='prims',negate=0)
    normals=node(module,'normal','HIGH_NORMALS',high_valid)
    selected=node(module,'switch','DISPLAY_LOW_OR_HIGH',low);selected.setInput(1,normals)
    selected.parm('input').setExpression('if(strcmp(chs("../preview_quality"),"high")==0,1,0)',hou.exprLanguage.Hscript)
    return [low,normals,selected]

def finish_modules(building,modules,stages):
    roof=modules['roof.surfaces_tiles']
    resolved=inlet(roof,'resolved_shell',3);volume=inlet(roof,'roof_interior',4);raw=inlet(roof,'raw_surfaces',5)
    old=roof.node('EXTRUDE_ROOF_THICKNESS');old.setInput(0,resolved);old.bypass(True)
    candidates=roof.node('ROOF_TILE_PLACEMENT');candidates.setInput(0,raw)
    code=candidates.parm('snippet').eval().replace('int pt=addpoint(0,pos+normal*.035);',
        'int pt=addpoint(0,pos+normal*.035);setpointattrib(0,"roof_patch_id",pt,string(prim(0,"roof_patch_id",src)));')
    candidates.parm('snippet').set(code)
    visible=wrangle(roof,'tile_candidates_on_resolved_domain','''vector p,uv;int hit=intersect(1,@P+set(0,20,0),set(0,-40,0),p,uv);
if(hit<0||p.y>@P.y+.012||string(prim(1,"roof_patch_id",hit))!=s@roof_patch_id)removepoint(0,@ptnum);''',candidates,2);visible.setInput(1,resolved)
    roof.node('COPY_ROOF_TILES').setInput(1,visible)
    roof.node('COPY_ROOF_TILES').setParms({'useimplicitn':0,'targetattribs':1,'useapply1':1,'applyto1':0,'applymethod1':0,'applyattribs1':'name roof_patch_id material_family Cd'})
    material=roof.node('TILE_MATERIAL')
    material.parm('snippet').set(material.parm('snippet').eval()+'\ns@roof_patch_id=point(0,"roof_patch_id",primpoint(0,@primnum,0));s@name=point(0,"name",primpoint(0,@primnum,0));')
    inset=node(roof,'xform','occlusion_tolerance',volume,ty=-.003)
    clipped=node(roof,'boolean::2.0','clip_tiles_to_roof_envelope',roof.node('TILE_MATERIAL'),booleanop='subtract');clipped.setInput(1,inset)
    roof.node('TRIM_TILES_AGAINST_BODY').setInput(0,clipped)
    # Dormer fronts sit on the low/front sample; rear tails die into the same roof volume.
    dormers=modules['roof.dormers'];prototype=dormers.node('DORMER_MODULE')
    for key,value in [('dormer_width',1.4),('dormer_wall_h',1.0),('dormer_roof_h',.42),('dormer_proto_depth',2.6)]:
        group=dormers.parmTemplateGroup();group.append(hou.FloatParmTemplate(key,key,1,default_value=(value,)));dormers.setParmTemplateGroup(group)
    for target,source in [('width','dormer_width'),('wall_h','dormer_wall_h'),('roof_h','dormer_roof_h'),('depth','dormer_proto_depth')]:
        prototype.parm(target).setExpression('ch("../'+source+'")',hou.exprLanguage.Hscript)
    bind(dormers,stages['ROOF'],building.node('CTRL_BUILDING'),'tile_size_m')
    group=prototype.parmTemplateGroup();group.append(hou.FloatParmTemplate('tile_size_m','Tile Size',1,default_value=(.32,)));prototype.setParmTemplateGroup(group)
    prototype.parm('tile_size_m').setExpression('ch("../tile_size_m")',hou.exprLanguage.Hscript)
    code=inventory()['F_ROOF/ROOF_TILE_PLACEMENT']['parameters']['snippet'].replace('"tile_"','"dormer_tile_"')
    points=wrangle(prototype,'dormer_tile_placements',code,prototype.node('pitched_roof_profiles'),0)
    unit=node(prototype,'box','dormer_tile_unit')
    tiles=node(prototype,'copytopoints::2.0','copy_dormer_tiles',unit);tiles.setInput(1,points)
    tiles.setParms({'useimplicitn':0,'targetattribs':1,'useapply1':1,'applyto1':0,'applymethod1':0,'applyattribs1':'name material_family Cd'})
    tiles=wrangle(prototype,'dormer_tile_material','s@name=point(0,"name",primpoint(0,@primnum,0));s@material_family="tile";v@Cd={.76,.24,.105};',tiles)
    prototype.node('merge_dormer_kit').setNextInput(tiles)
    placement=dormers.node('DORMER_PLACEMENT_DATA');placement.parm('snippet').set((ROOT/'vex/dormer_host_placement.vfl').read_text(encoding='utf-8'))
    profile=wrangle(dormers,'matched_dormer_aperture_profile','''float hw=ch("../dormer_width")*.5-.09,h=ch("../dormer_wall_h"),r=ch("../dormer_roof_h");
vector p[]=array(set(-hw,-.12,-.5),set(hw,-.12,-.5),set(hw,h-.04,-.5),set(0,h+r-.04,-.5),set(-hw,h-.04,-.5));
int ids[];foreach(vector pos;p){int pt=addpoint(0,pos);append(ids,pt);setpointattrib(0,"extrude_direction",pt,set(0,0,1));}
int pr=addprim(0,"poly",ids);s@name="matched_dormer_aperture";''',None,0)
    matched=node(dormers,'polyextrude::2.0','matched_dormer_roof_volume',profile,
                 splittype='components',extrusionmode='pointnormal',ptnormalsrc='attribute',ptnormalattrib='extrude_direction',dist=1.,outputback=1,outputfront=1,outputside=1)
    matched=node(dormers,'reverse','outward_dormer_aperture',matched)
    dormers.node('COPY_DORMER_CUTTERS').setInput(0,matched)
    dormers.node('DORMER_ROOF_CUTTER').setComment('Legacy tall box; retained for diagnostic comparison, not used for cuts.')
    dormers.node('DORMER_CUTTER_TRANSFORMS').parm('snippet').set('matrix3 m=ident();scale(m,set(1.,1.,max(.1,f@dormer_depth-.18)));rotate(m,radians(f@dormer_rotation),{0,1,0});3@transform=m;')
    dormer_volume=inlet(dormers,'roof_interior',3);inlet(dormers,'roof_host',4)
    trim=node(dormers,'boolean::2.0','trim_dormers_into_host_roof',dormers.node('COPY_DORMER_MODULES'),booleanop='subtract');trim.setInput(1,dormer_volume)
    dormers.node('OUT_dormer_geometry').setInput(0,trim)
    # Remove buried ridge/eave boards from secondary roofs using the same solved volume.
    rooftrim=modules['roof.trim'];inside=inlet(rooftrim,'roof_interior',1)
    output=next(n for n in rooftrim.children() if n.type().name()=='output' and n.parm('outputidx').eval()==0)
    oldsource=output.inputs()[0];offset=node(rooftrim,'xform','trim_envelope_tolerance',inside,ty=-.02)
    cut=node(rooftrim,'boolean::2.0','remove_buried_roof_trim',oldsource,booleanop='subtract');cut.setInput(1,offset);output.setInput(0,cut)
    # Canopy and lean-to roofs are attached to the wall datum, with real tiles.
    site=modules['details.site']
    overhead=inlet(site,'overhead_access',2)
    reach='''float canopy_reach(float d;vector center;float angle){
float front=-d*.5-.015;float a=radians(angle);
for(int pt=0;pt<npoints(1);pt++){vector p=point(1,"P",pt)-center;front=min(front,sin(a)*p.x+cos(a)*p.z);}
return max(1.45,-d*.5-.015-front+.45);
}\n'''
    for child in ('awning_and_canopy_profiles','entry_and_canopy_placements'):
        n=site.node(child);p=n.parm('snippet')
        code=p.eval().replace('-d*.5-.12','-d*.5-.015').replace('depth=1.45;','depth=canopy_reach(d,center,angle);')
        p.set(reach+code);n.setInput(1,overhead)
    add_canopy_tiles(site,stages['DETAILS'],building.node('CTRL_BUILDING'),1,
                     're_match(".*(awning|entrance_canopy|canopy_brace).*",s@name)',
                     's@material_family=="roof"')
    add_canopy_tiles(modules['openings.attachments'],stages['OPENINGS'],building.node('CTRL_BUILDING'),4,
                     're_match("attachments/main/.*(roof|canopy).*",s@name)',
                     're_match("attachments/main/.*(roof|canopy).*",s@name)')

def add_canopy_tiles(module,stage,ctrl,body_input,member_expression,roof_expression):
    bind(module,stage,ctrl,'tile_size_m')
    output=next(n for n in module.children() if n.type().name()=='output' and n.parm('outputidx').eval()==0)
    source=output.inputs()[0];body=inlet(module,'solid_body',body_input)
    mask=wrangle(module,'classify_canopy_members','i@group_canopy_members='+member_expression+';',source)
    members=node(module,'blast','canopy_members_only',mask,group='canopy_members',grouptype='prims',negate=1)
    rest=node(module,'blast','other_architecture',mask,group='canopy_members',grouptype='prims',negate=0)
    clipped=node(module,'boolean::2.0','canopy_wall_contact',members,booleanop='subtract');clipped.setInput(1,body)
    # Tile the original quad domains; clipping first turns the quads into ngons.
    # The copied tiles are clipped to the wall afterwards with the canopy solid.
    topmask=wrangle(module,'canopy_upper_faces','i@group_canopy_top=('+roof_expression+')&&prim_normal(0,@primnum,.5,.5).y>.2;',members)
    top=node(module,'blast','canopy_roof_domain',topmask,group='canopy_top',grouptype='prims',negate=1)
    code=inventory()['F_ROOF/ROOF_TILE_PLACEMENT']['parameters']['snippet']
    code=code.replace('"tile_"','"canopy_tile_"').replace('int pt=addpoint(0,pos+normal*.035);','int pt=addpoint(0,pos+normal*.035);setpointattrib(0,"roof_patch_id",pt,"canopy/main");')
    placements=wrangle(module,'canopy_tile_placements',code,top,0)
    unit=node(module,'box','canopy_tile_unit')
    copies=node(module,'copytopoints::2.0','copy_canopy_tiles',unit);copies.setInput(1,placements)
    copies.setParms({'useimplicitn':0,'targetattribs':1,'useapply1':1,'applyto1':0,'applymethod1':0,'applyattribs1':'name roof_patch_id material_family Cd'})
    material=wrangle(module,'canopy_tile_material','s@material_family="tile";s@name=point(0,"name",primpoint(0,@primnum,0));v@Cd={.76,.24,.105};',copies)
    tileclip=node(module,'boolean::2.0','canopy_tiles_wall_contact',material,booleanop='subtract');tileclip.setInput(1,body)
    merged=node(module,'merge','canopy_with_supported_architecture',rest);merged.setNextInput(clipped);merged.setNextInput(tileclip)
    output.setInput(0,merged)
