"""Public HOM builder for the preserved Medieval functions and modular DAG."""
from pathlib import Path
import argparse
import json
import sys
import hou
_PACKAGE_ROOT=Path(__file__).resolve().parent
if str(_PACKAGE_ROOT) not in sys.path:sys.path.insert(0,str(_PACKAGE_ROOT))
from hda_builder.registry import ROOT, OWNER, VERSION, TYPE
from hda_builder.schema import normalize
from hda_builder.evidence import write_json

def apply_config(asset_node, config=None):
    cfg=normalize(config)
    node=asset_node if asset_node.parm('footprint') else asset_node.node('BUILDING')
    if node is None:raise ValueError('Expected Medieval HDA or owned holder')
    values=dict(cfg['parameters'],seed_decoration=cfg['seeds']['decoration'])
    from hda_builder.corrections import EXTENSIONS
    values.update({key:cfg.get('extensions',{}).get(key,spec['default']) for key,spec in EXTENSIONS.items()})
    missing=[key for key in values if node.parm(key) is None]
    if missing:raise ValueError({'missing_public_parameters':missing})
    for key,value in values.items():node.parm(key).set(int(value) if isinstance(value,bool) else value)
    return node

def build(config=None,parent_path='/obj',asset_name='medieval_modular_v2'):
    cfg=normalize(config)
    parent=hou.node(parent_path)
    existing=parent.node(asset_name)
    if existing:
        target=existing.node('BUILDING') if existing.childTypeCategory()==hou.sopNodeTypeCategory() and existing.parm('footprint') is None else existing
        if target is None or target.userData('generator_owner')!=OWNER:raise ValueError('Refusing to update artist-owned node: '+existing.path())
        return apply_config(target,cfg)
    holder=parent.createNode('geo',asset_name,run_init_scripts=False) if parent.childTypeCategory()==hou.objNodeTypeCategory() else parent
    package=ROOT/'asset/medieval_building_modular.hda'
    if package.exists():
        hou.hda.installFile(str(package))
        building=holder.createNode(TYPE,'BUILDING');building.setUserData('generator_owner',OWNER)
    else:
        from hda_builder.migrate import build_graph
        template=hou.node('/obj/medieval_building')
        if template is None:
            hou.hipFile.merge(str(ROOT/'scenes/source_snapshot.hip'),node_pattern='/obj/medieval_building',overwrite_on_conflict=False)
            template=hou.node('/obj/medieval_building')
        building=build_graph(template,holder,'BUILDING')
    return apply_config(building,cfg)

def export_asset(asset_node,output_path):
    path=Path(output_path).resolve();path.parent.mkdir(parents=True,exist_ok=True)
    suffix={hou.licenseCategoryType.Commercial:'.hda',hou.licenseCategoryType.Indie:'.hdalc',hou.licenseCategoryType.Apprentice:'.hdanc'}.get(hou.licenseCategory())
    if suffix is None or path.suffix!=suffix:raise ValueError('License requires '+str(suffix))
    group=asset_node.parmTemplateGroup()
    values={p.name():p.eval() for p in asset_node.parms() if not isinstance(p.parmTemplate(),hou.FolderSetParmTemplate)}
    if asset_node.type().definition() is None:
        asset_node=asset_node.createDigitalAsset(name=TYPE,hda_file_name=str(path),
                         description='Medieval Building - Modular Hybrid SOP v2',min_num_inputs=0,max_num_inputs=0,create_backup=False)
    definition=asset_node.type().definition()
    definition.setParmTemplateGroup(group)
    for key,value in values.items():
        if asset_node.parm(key):asset_node.parm(key).set(value)
    definition.updateFromNode(asset_node)
    for name,file in [('ModuleGraph.json','module_graph.json'),('ParameterSchema.json','parameter_schema.json'),('Baseline.json','datasets/baseline.json')]:
        definition.addSection(name,(ROOT/file).read_text(encoding='utf-8'))
    definition.addSection('BuildVersion',VERSION)
    definition.save(str(path))
    library=ROOT/'asset/medieval_modular_modules.hda'
    if library.exists():
        for nested in hou.hda.definitionsInFile(str(library)):nested.copyToHDAFile(str(path))
    return path

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--dataset',type=Path);ap.add_argument('--package',action='store_true');ap.add_argument('--rebuild',action='store_true')
    args=ap.parse_args()
    if args.rebuild:
        # Explicit graph rebuild uses the snapshot, never an artist's open GUI session.
        from hda_builder.migrate import build_graph
        hou.hipFile.load(str(ROOT/'scenes/source_snapshot.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
        holder=hou.node('/obj').createNode('geo','medieval_modular_v2',run_init_scripts=False)
        node=build_graph(hou.node('/obj/medieval_building'),holder,'BUILDING')
    else:node=build()
    cfg=json.loads(args.dataset.read_text(encoding='utf-8')) if args.dataset else None
    apply_config(node,cfg)
    try:node.cook(force=True)
    except hou.Error:
        from hda_builder.evidence import node_messages
        write_json(ROOT/'results/build_errors.json',node_messages(node))
        print([(n.path(),n.parm('outputidx').rawValue(),n.parm('outputidx').eval()) for n in node.allSubChildren() if n.type().name()=='output' and '/kits/' in n.path()],flush=True)
        hou.hipFile.save(str(ROOT/'scenes/debug_failed_build.hip'))
        raise
    print('COOK',len(node.geometry().points()),len(node.geometry().prims()),flush=True)
    if args.package:print('ASSET',export_asset(node,ROOT/'asset/medieval_building_modular.hda'),flush=True)
    hou.hipFile.save(str(ROOT/'scenes/medieval_building_modular_v2.hip'))
    print('SAVED',flush=True)

if __name__=='__main__':main()
