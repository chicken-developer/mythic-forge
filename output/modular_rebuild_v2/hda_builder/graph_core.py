"""HOM factories for real, lossless subnet connectors and owned graph updates."""
import re
import json
import hou
from .registry import ROOT, OWNER, VERSION

def tag(node, module_id):
    node.setUserData('generator_owner',OWNER)
    node.setUserData('generator_version',VERSION)
    node.setUserData('module_id',module_id)
    return node

def create_node(parent, kind, name, module_id):
    return tag(parent.createNode(kind,name,run_init_scripts=False),module_id)

def make_subnet(parent, name, inputs, outputs, library, type_name, module_id):
    node=create_node(parent,'subnet',name,module_id)
    if len(inputs)>4 or len(outputs)>4:
        node=node.createDigitalAsset(name=type_name,hda_file_name=str(library),
                                     min_num_inputs=0,max_num_inputs=max(len(inputs),1),
                                     description=name.replace('_',' ').title(),create_backup=False)
        node.allowEditingOfContents()
        node.type().definition().setMaxNumOutputs(max(1,len(outputs)))
        tag(node,module_id)
    for i,label in enumerate(inputs):
        p=node.parm(f'label{i+1}')
        if p:p.set(label)
    return node

def publish_outputs(node, outputs, source_nodes, module_id):
    handles={}
    for i,(port,(source,index)) in enumerate(zip(outputs,source_nodes)):
        # Stage boundaries publish producer identity; surfacing/output preserve it.
        if module_id.startswith('stage.'):
            producer=source.userData('module_id')
            if producer and not producer.startswith(('uv_material.','output.')):
                stamp=create_node(node,'attribwrangle','IDENTITY_'+port,module_id)
                stamp.setInput(0,source,index);stamp.parm('class').set(1)
                family=producer.split('.')[0]
                role={'shell':'shell','layout':'guide','roof':'roof','details':producer.split('.')[-1],
                      'openings':{'windows':'window','doors':'door','wall_cuts':'wall','attachments':'attachment','kits':'opening_kit'}.get(producer.split('.')[-1],'opening')}.get(family,family)
                if 'tiles' in port:role='roof_tile'
                if 'structural_timber' in port:role='structural_timber'
                stamp.parm('snippet').set('s@module_id="'+producer+'";s@part_role="'+role+'";')
                source,index=stamp,0
        out=create_node(node,'output','OUT_'+port,module_id)
        out.setInput(0,source,index);out.parm('outputidx').deleteAllKeyframes();out.parm('outputidx').set(i)
        out.setUserData('declared_output_index',str(i))
        handles[port]=(node,i)
    debug=create_node(node,'null','OUT_DEBUG',module_id)
    debug.setInput(0,source_nodes[0][0],source_nodes[0][1])
    debug.setDisplayFlag(True);debug.setRenderFlag(True)
    return handles

def stage_parameter(stage, ctrl, key):
    if stage.parm(key):return
    group=stage.parmTemplateGroup()
    group.append(ctrl.parm(key).parmTemplate())
    stage.setParmTemplateGroup(group)
    p=stage.parm(key)
    fn='chs' if p.parmTemplate().type()==hou.parmTemplateType.String else 'ch'
    p.setExpression(f'{fn}("{stage.relativePathTo(ctrl)}/{key}")',hou.exprLanguage.Hscript)

def bind_original(node, original_parameters, stage, ctrl):
    for key,value in original_parameters.items():
        p=node.parm(key)
        if p is None or not isinstance(value,str):continue
        matches=re.findall(r'CTRL_BUILDING/([a-z0-9_]+)',value)
        if matches:
            for arg in matches:stage_parameter(stage,ctrl,arg)
            expr=re.sub(r'(?:\.\./)+CTRL_BUILDING/([a-z0-9_]+)',
                        lambda m:node.relativePathTo(stage)+'/'+m.group(1),value)
            p.deleteAllKeyframes()
            p.setExpression(expr,hou.exprLanguage.Hscript)
        elif key=='material_root' and value.startswith('opfullpath'):
            p.deleteAllKeyframes()
            p.setExpression(f'opfullpath("{node.relativePathTo(ctrl.parent().node("materials"))}")',hou.exprLanguage.Hscript)

def fix_helper_interfaces(module, original_name):
    """Replace the five legacy internal Object Merge paths with helper ports."""
    replacements=[]
    if original_name=='F_OPENING_KITS':
        for family in ('window','door'):
            helper=module.node(family.upper()+'_MODULE')
            endpoint=helper.node('OUT_APERTURE')
            existing=[n for n in helper.children() if n.type().name()=='output']
            out=create_node(helper,'output','PUBLIC_APERTURE',module.userData('module_id'))
            out.setInput(0,endpoint);out.parm('outputidx').deleteAllKeyframes();out.parm('outputidx').set(1)
            out.setUserData('declared_output_index','1')
            merge=module.node('RETURN_'+family+'_aperture')
            new=merge.changeNodeType('null',keep_parms=False)
            new.setInput(0,helper,1);tag(new,module.userData('module_id'))
            replacements.append(new.path())
    if original_name=='F_DORMERS':
        helper=module.node('DORMER_MODULE')
        for i,(old,stream) in enumerate((('reused_window_kit','IN_window_geometry'),('shared_window_aperture','IN_window_aperture'))):
            helper.setInput(i,module.node(stream))
            n=helper.node(old).changeNodeType('null',keep_parms=False)
            n.setInput(0,helper.indirectInputs()[i]);tag(n,module.userData('module_id'))
            replacements.append(n.path())
    if original_name=='F_ROOF':
        helper=module.node('HIPPED_ROOF_MODULE');helper.setInput(0,module.node('IN_roof_plans'))
        n=helper.node('live_roof_plans').changeNodeType('null',keep_parms=False)
        n.setInput(0,helper.indirectInputs()[0]);tag(n,module.userData('module_id'))
        replacements.append(n.path())
    return replacements

def save_definition(node):
    definition=node.type().definition()
    if definition and definition.libraryFilePath().replace('\\','/').startswith(str(ROOT).replace('\\','/')):
        definition.updateFromNode(node)

