"""Compose the preserved native functions through declared stage/module ports."""
import json
import hou
from .registry import ROOT, OWNER, VERSION, TYPE, contracts, validate, inventory
from .graph_core import (tag,create_node,make_subnet,publish_outputs,bind_original,
                         fix_helper_interfaces,save_definition)
from .evidence import write_json

def build_graph(template, parent, name):
    document=contracts();order=validate(document)
    write_json(ROOT/'module_graph.json',document)
    rows=inventory();specs={s['module_id']:s for s in document['modules']}
    library=ROOT/'asset/medieval_modular_modules.hda';library.parent.mkdir(exist_ok=True)
    building=create_node(parent,'subnet',name,'building')
    tag(building,'building')
    hou.copyNodesTo(template.children(),building)
    ctrl=building.node('CTRL_BUILDING')
    from .corrections import add_controls
    add_controls(ctrl)
    group=ctrl.parmTemplateGroup()
    for key in ('copyinput','cacheinput'):
        if group.find(key):group.remove(key)
    building.setParmTemplateGroup(group)
    for p in ctrl.parms():
        target=building.parm(p.name())
        if target is None:continue
        if isinstance(p.parmTemplate(),hou.FolderSetParmTemplate):continue
        target.set(p.eval())
        fn='chs' if p.parmTemplate().type()==hou.parmTemplateType.String else 'ch'
        p.setExpression(f'{fn}("../{p.name()}")',hou.exprLanguage.Hscript)
    stages={};stage_inputs={};stage_outputs={}
    for spec in document['modules']:
        stage=spec['stage'];stage_inputs.setdefault(stage,{})
        stage_outputs.setdefault(stage,{})
        for port in spec['outputs']:
            key=spec['module_id']+':'+port
            stage_outputs[stage][key]=len(stage_outputs[stage])
        for target in spec['inputs'].values():
            producer=target.split(':')[0]
            if specs[producer]['stage']!=stage and target not in stage_inputs[stage]:
                stage_inputs[stage][target]=len(stage_inputs[stage])
    for stage in stage_inputs:
        stages[stage]=make_subnet(building,stage,list(stage_inputs[stage]),list(stage_outputs[stage]),library,
                                  'mythic::medieval_stage_'+stage.lower()+'::2.0','stage.'+stage.lower())
    modules={};port_handles={};external_nodes={};changes=[];migration={}
    for ident in order:
        spec=specs[ident];stage=stages[spec['stage']]
        members=spec.get('source_members',[])
        if len(members)==1:
            node=hou.moveNodesTo((building.node(members[0]),),stage)[0]
            node.setName(spec['path'].split('/')[-1]);tag(node,ident)
            original_nodes={members[0]:node}
            # Preserve every original output index; debug is separate.
            debug=create_node(node,'null','OUT_DEBUG',ident)
            outs=sorted([n for n in node.children() if n.type().name()=='output'],key=lambda n:n.parm('outputidx').eval())
            debug.setInput(0,outs[0]);debug.setDisplayFlag(True)
            handles={port:(node,i) for i,port in enumerate(spec['outputs'])}
        elif members:
            node=make_subnet(stage,spec['path'].split('/')[-1],list(spec['inputs']),spec['outputs'],library,
                             'mythic::medieval_module_'+ident.replace('.','_')+'::2.0',ident)
            moved=hou.moveNodesTo(tuple(building.node(n) for n in members),node)
            original_nodes=dict(zip(members,moved))
            # Restore exact internal cut edges after movement.
            for old,n in original_nodes.items():
                for c in rows[old]['inputs']:
                    if c['source'] in original_nodes:n.setInput(c['input'],original_nodes[c['source']],c['output'])
            handles=publish_outputs(node,spec['outputs'],[(original_nodes[spec['source_output']],i) for i in range(len(spec['outputs']))],ident)
        else:
            node=make_subnet(stage,'game_export',list(spec['inputs']),spec['outputs'],library,
                             'mythic::medieval_module_game_export::2.0',ident)
            original_nodes={}
            incoming=create_node(node,'null','IN_building',ident);incoming.setInput(0,node.indirectInputs()[0])
            metadata=create_node(node,'attribwrangle','export_semantic_metadata',ident);metadata.setInput(0,incoming)
            metadata.parm('class').set(1)
            metadata.parm('snippet').set('if(s@module_id=="")s@module_id="output.game_export";\nif(s@part_role=="")s@part_role="unclassified";')
            handles=publish_outputs(node,['geometry'],[(metadata,0)],ident)
        modules[ident]=node;port_handles.update({ident+':'+port:handle for port,handle in handles.items()})
        external_nodes[ident]=original_nodes
        for old,n in original_nodes.items():
            migration[old]=building.relativePathTo(n)
            # Functions keep typed local args; caller bindings are promoted to stage UI.
            bind_original(n,rows[old]['parameters'],stage,ctrl)
            if old.startswith('F_'):
                changes.extend(fix_helper_interfaces(n,old))
            for child in [n]+list(n.allSubChildren()):
                if child.userData('generator_owner') is None:tag(child,ident)
        node.setComment(spec['responsibility']);node.setGenericFlag(hou.nodeFlag.DisplayComment,True)
    # Module connectors route only through published stage ports.
    for ident in order:
        spec=specs[ident];node=modules[ident];stage=stages[spec['stage']]
        for i,(port,target) in enumerate(spec['inputs'].items()):
            producer=target.split(':')[0]
            upstream=port_handles[target] if specs[producer]['stage']==spec['stage'] else (stage.indirectInputs()[stage_inputs[spec['stage']][target]],0)
            if len(spec.get('source_members',[]))==1:
                for route in spec['routes']:
                    if route['port']==port:node.setInput(route['input'],*upstream)
            else:
                node.setInput(i,*upstream)
                if spec.get('source_members'):
                    for route in spec['routes']:
                        if route['port']==port:external_nodes[ident][route['member']].setInput(route['input'],node.indirectInputs()[i])
    # Publish all function streams, including debug/secondary outputs, in stable order.
    stage_handles={}
    for stage_name,stage in stages.items():
        labels=[key.split(':')[0].replace('.','_')+'_'+key.split(':')[1] for key in stage_outputs[stage_name]]
        handles=publish_outputs(stage,labels,[port_handles[key] for key in stage_outputs[stage_name]],'stage.'+stage_name.lower())
        stage_handles.update({key:(stage,idx) for key,idx in stage_outputs[stage_name].items()})
    for stage_name,stage in stages.items():
        for target,index in stage_inputs[stage_name].items():stage.setInput(index,*stage_handles[target])
        stage.layoutChildren()
    final=building.node('OUT_BUILDING')
    final.setInput(0,*stage_handles['output.game_export:geometry']);tag(final,'output.game_export')
    final.setDisplayFlag(True);final.setRenderFlag(True)
    asset_output=create_node(building,'output','PUBLIC_BUILDING_OUTPUT','output.game_export')
    asset_output.setInput(0,final);asset_output.parm('outputidx').set(0)
    from .corrections import apply as apply_corrections
    apply_corrections(building,modules,stages)
    for out in building.allSubChildren():
        idx=out.userData('declared_output_index')
        if idx is not None:
            out.parm('outputidx').deleteAllKeyframes();out.parm('outputidx').set(int(idx))
    # Save dependency definitions after all ports and caller parameter bindings exist.
    for ident in reversed(order):
        n=modules[ident];n.layoutChildren();save_definition(n)
    for stage in stages.values():save_definition(stage)
    building.layoutChildren()
    write_json(ROOT/'results/migration.json',{'public_parameter_migration':'identity; no original dataset edits',
                'source_paths_to_new_paths':migration,'legacy_object_merges_replaced_by_connectors':changes,
                'stage_inputs':stage_inputs,'stage_outputs':stage_outputs})
    return building
