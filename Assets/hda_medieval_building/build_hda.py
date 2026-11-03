"""Public loader for the preserved native HIP / portable HDA and full configs."""
from pathlib import Path
import argparse
import json
import math
import hou

ROOT=Path(__file__).resolve().parent
VERSION='2026.11-medieval-layout-2'
TYPE='mythic::medieval_house::3.0'

def normalize(config=None):
    if config is None:config=json.loads((ROOT/'datasets/baseline.json').read_text(encoding='utf-8'))
    schema=json.loads((ROOT/'parameter_schema.json').read_text(encoding='utf-8'))['parameters']
    p=config['parameters']
    if set(p)!=set(schema):raise ValueError({'missing':sorted(set(schema)-set(p)),'unknown':sorted(set(p)-set(schema))})
    for key,info in schema.items():
        v=p[key];typ=info['type']
        if typ=='number' and (type(v) not in (int,float) or not math.isfinite(v)):raise ValueError(key)
        if typ=='integer' and type(v) is not int:raise ValueError(key)
        if typ=='boolean' and type(v) is not bool:raise ValueError(key)
        if typ=='string' and not isinstance(v,str):raise ValueError(key)
        if 'enum' in info and v not in info['enum']:raise ValueError((key,v,info['enum']))
    for key in ('front_bays','right_bays','rear_bays','left_bays'):
        rows=p[key].split('/')
        if not all(rows) or any(set(r)-set('WD.') for r in rows) or len({len(r) for r in rows})!=1:raise ValueError('Invalid aligned bay pattern: '+key)
    if p['front_bays'].split('/')[0].count('D')!=1:raise ValueError('Require one ground-floor front entrance D')
    if not 3.5<=p['width_m']<=14 or not 3.5<=p['depth_m']<=12:raise ValueError('Footprint outside supported bake range')
    if not 1<=p['floor_count']<=3:raise ValueError('floor_count outside 1..3')
    if p['balcony_layout']!='none' and p['floor_count']<2:raise ValueError('Balcony requires upper floor')
    if p['balcony_layout']!='none' and not any('D' in r for r in p['front_bays'].split('/')[1:]):raise ValueError('Balcony requires an upper access door D')
    if p['roof_shape']=='conical' and p['footprint']!='round':raise ValueError('Conical roof requires round footprint')
    if config.get('seeds',{}).keys()!={'decoration'} or type(config['seeds']['decoration']) is not int:raise ValueError('Require integer seeds.decoration')
    json.loads(p['facade_overrides'])
    return config

def apply_config(asset_node,config):
    config=normalize(config)
    ctrl=asset_node if asset_node.parm('footprint') is not None else asset_node.node('CTRL_BUILDING')
    p=dict(config['parameters'],seed_decoration=config['seeds']['decoration'])
    for key,value in p.items():
        parm=ctrl.parm(key)
        if parm is None:raise ValueError('Missing actual control: '+key)
        parm.set(int(value) if isinstance(value,bool) else value)
    for key,value in p.items():
        parm=ctrl.parm(key);actual=parm.evalAsString() if isinstance(value,str) else parm.eval()
        if isinstance(value,(int,float)) and not isinstance(value,bool):
            if abs(actual-value)>1e-5:raise ValueError(f'Control clamped: {key}: {value} -> {actual}')
        elif actual!=value:raise ValueError((key,value,actual))
    return asset_node

def build(config=None,parent_path='/obj',asset_name='medieval_building'):
    config=normalize(config)
    node=hou.node(parent_path+'/'+asset_name)
    if node is None:
        asset=ROOT/'asset/medieval_house_layout.hda'
        if asset.exists():
            hou.hda.installFile(str(asset));parent=hou.node(parent_path)
            if parent.childTypeCategory()==hou.objNodeTypeCategory():parent=parent.createNode('geo',asset_name,run_init_scripts=False)
            node=parent.createNode(TYPE,'BUILDING')
        else:
            if parent_path!='/obj':raise ValueError('Unpackaged HIP fallback requires /obj')
            hou.hipFile.merge(str(ROOT/'medieval_building.hip'),node_pattern='/obj/medieval_building*',overwrite_on_conflict=False)
            node=hou.node('/obj/medieval_building');node.setName(asset_name)
    apply_config(node,config)
    return node

def export_asset(asset_node,output_path):
    # Copy the inspected graph into an SOP asset; the source object stays editable.
    holder=hou.node('/obj').createNode('geo','MEDIEVAL_PACKAGE',run_init_scripts=False)
    subnet=holder.createNode('subnet','medieval_house')
    hou.copyNodesTo(asset_node.children(),subnet)
    ctrl=subnet.node('CTRL_BUILDING');group=ctrl.parmTemplateGroup()
    for key in ('copyinput','cacheinput'):group.remove(key)
    subnet.setParmTemplateGroup(group)
    values={}
    for p in ctrl.parms():
        if p.name() in ('copyinput','cacheinput') or isinstance(p.parmTemplate(),hou.FolderSetParmTemplate):continue
        target=subnet.parm(p.name())
        if target:
            values[p.name()]=p.eval()
            target.set(p.eval())
            expression=f'chs("../{p.name()}")' if p.parmTemplate().type()==hou.parmTemplateType.String else f'ch("../{p.name()}")'
            p.setExpression(expression,hou.exprLanguage.Hscript)
    output=subnet.createNode('output','OUTPUT_BUILDING');output.setInput(0,subnet.node('OUT_BUILDING'));output.parm('outputidx').set(0)
    path=Path(output_path);path.parent.mkdir(parents=True,exist_ok=True)
    hda=subnet.createDigitalAsset(name=TYPE,hda_file_name=str(path),description='Medieval House - aligned facade and framing',min_num_inputs=0,max_num_inputs=0)
    definition=hda.type().definition()
    definition.setParmTemplateGroup(group)
    hda.setParms(values)
    definition.updateFromNode(hda)
    definition.addSection('ParameterSchema.json',(ROOT/'parameter_schema.json').read_text(encoding='utf-8'))
    definition.addSection('Baseline.json',(ROOT/'datasets/baseline.json').read_text(encoding='utf-8'))
    definition.addSection('BuildVersion',VERSION)
    definition.save(str(path))
    holder.destroy()
    return path

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--dataset',type=Path);ap.add_argument('--package',action='store_true');args=ap.parse_args()
    config=json.loads(args.dataset.read_text(encoding='utf-8')) if args.dataset else None
    if args.package:
        hou.hipFile.load(str(ROOT/'medieval_building.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
        node=hou.node('/obj/medieval_building');apply_config(node,config)
        print(export_asset(node,ROOT/'asset/medieval_house_layout.hda'))
    else:node=build(config);node.cook(force=True)

if __name__=='__main__':main()
