"""Actual HOM connectivity audit, independent of geometry assertions."""
import collections
import re
import hou
from .registry import ROOT, OWNER, contracts, validate
from .evidence import write_json

def audit(building):
    doc=contracts();order=validate(doc)
    specs={s['module_id']:s for s in doc['modules']}
    migration=__import__('json').loads((ROOT/'results/migration.json').read_text(encoding='utf-8'))
    violations=[];modules=[];stage_records=[]
    for spec in doc['modules']:
        n=building.node(spec['path'])
        if n is None:violations.append('Missing module '+spec['path']);continue
        if n.childTypeCategory()!=hou.sopNodeTypeCategory():violations.append('Not a real SOP module '+n.path())
        if n.userData('generator_owner')!=OWNER:violations.append('Missing ownership '+n.path())
        debug=building.node(spec['debug_endpoint'])
        if debug is None:violations.append('Missing debug endpoint '+spec['debug_endpoint'])
        actual=[]
        for c in n.inputConnections():
            source=c.inputNode()
            indirect=c.subnetIndirectInput() if hasattr(c,'subnetIndirectInput') else None
            actual.append({'input':c.inputIndex(),'source':building.relativePathTo(source) if source else None,
                           'source_output':c.outputIndex(),'indirect_input':indirect.number() if indirect else None})
        expected=[]
        for i,(port,target) in enumerate(spec['inputs'].items()):
            producer,output=target.split(':');p=specs[producer]
            if len(spec.get('source_members',[]))==1:
                indices=[r['input'] for r in spec['routes'] if r['port']==port]
            else:indices=[i]
            for index in indices:
                if p['stage']==spec['stage']:
                    desired={'input':index,'source':p['path'],'source_output':p['outputs'].index(output),'indirect_input':None}
                else:
                    desired={'input':index,'source':p['stage'],'source_output':migration['stage_outputs'][p['stage']][target],
                             'indirect_input':migration['stage_inputs'][spec['stage']][target]}
                expected.append(desired)
        for desired in expected:
            if desired not in actual:violations.append({'module':spec['path'],'expected_wire':desired,'actual':actual})
        modules.append({'module_id':spec['module_id'],'path':n.path(),'type':n.type().name(),'parent':n.parent().path(),
                        'owner':n.userData('generator_owner'),'actual_input_wires':actual,'expected_input_wires':expected,
                        'outputs':spec['outputs'],'debug_endpoint':debug.path() if debug else None,
                        'direct_children':len(n.children()),'descendants':len(n.allSubChildren()),
                        'node_types':dict(collections.Counter(x.type().name() for x in n.allSubChildren()))})
    for name,ports in migration['stage_inputs'].items():
        stage=building.node(name);actual=[]
        for c in stage.inputConnections():
            source=c.inputNode()
            actual.append((c.inputIndex(),source.name() if source else None,c.outputIndex()))
        expected=[]
        for target,index in ports.items():
            producer=specs[target.split(':')[0]]['stage']
            expected.append((index,producer,migration['stage_outputs'][producer][target]))
        for wire in expected:
            if wire not in actual:violations.append({'stage':name,'missing_stage_wire':wire,'actual':actual})
        if stage.node('OUT_DEBUG') is None:violations.append('No stage debug '+name)
        stage_records.append({'name':name,'path':stage.path(),'type':stage.type().name(),
                              'input_count':len(ports),'actual_inputs':actual,'expected_inputs':expected,
                              'published_outputs':migration['stage_outputs'][name],
                              'max_inputs':stage.type().maxNumInputs(),'max_outputs':stage.type().maxNumOutputs()})
    children=building.allSubChildren()
    python=[n.path() for n in children if n.type().name()=='python']
    if python:violations.append({'python_sops':python})
    object_merges=[]
    for n in children:
        if n.type().name()=='object_merge':
            row={'path':n.path(),'path_expression':n.parm('objpath1').rawValue()}
            object_merges.append(row)
            if n.name()!='OPTIONAL_BLOCKOUT_INPUT':violations.append({'hidden_geometry_dependency':row})
    root_scattered=[n.name() for n in building.children() if n.name() not in set(migration['stage_inputs'])|{'CTRL_BUILDING','materials','OUT_BUILDING','PUBLIC_BUILDING_OUTPUT'}]
    if root_scattered:violations.append({'unregistered_root_nodes':root_scattered})
    reach=set()
    def visit(ident):
        if ident in reach:return
        reach.add(ident)
        for dep in specs[ident]['depends_on']:visit(dep)
    visit(doc['final_module'])
    result={'houdini_version':hou.applicationVersionString(),'license':str(hou.licenseCategory()),
            'graph_modularity':'pass' if not violations else 'fail','violations':violations,
            'module_count':len(modules),'stages':stage_records,'modules':modules,
            'node_types':dict(collections.Counter(n.type().name() for n in children)),
            'python_sops':python,'wrangles':[n.path() for n in children if n.type().name()=='attribwrangle'],
            'object_merges':object_merges,'root_children':[n.name() for n in building.children()],
            'final_output':building.node('OUT_BUILDING').path(),'all_declared_modules_reach_output':len(reach)==len(specs),
            'source_function_count':18,'network_capture':'not_run; headless HOM audit, no synthetic screenshot',
            'parameter_sweeps':'results/regressions.json'}
    write_json(ROOT/'graph_audit.json',result)
    return result
