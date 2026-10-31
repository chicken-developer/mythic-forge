"""Publish reviewed source changes and one self-contained HIP/HDA per family."""
from pathlib import Path
import argparse
import datetime
import difflib
import hashlib
import json
import shutil
import sys
import hou
from build_detail_assets import load,P,R

def main():
    ap=argparse.ArgumentParser();ap.add_argument('family',choices=['Medieval','Gothic']);args=ap.parse_args()
    family=args.family;B,N,canonical,presets=load(family)
    stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    backup=R/'SourceBackups'/family/stamp;backup.mkdir(parents=True,exist_ok=False)
    code=P/'SourceAssets/Houdini'/family
    names=['build_hda.py'] if family=='Medieval' else ['build_hda.py','native_network.py','production_geometry.py']
    for relative in names+[family.lower()+'_building.hip','asset/'+family.lower()+'_building.hda']:
        source=canonical/relative;target=backup/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    for name in names:
        before=(canonical/name).read_text(encoding='utf-8');after=(code/name).read_text(encoding='utf-8')
        (backup/(name+'.diff')).write_text(''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='before/'+name,tofile='after/'+name)),encoding='utf-8')
        shutil.copy2(code/name,canonical/name)
    if family=='Medieval':
        hou.hipFile.load(str(canonical/'medieval_building.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
        obj=hou.node('/obj/medieval_building');asset=obj.node('generator');asset.allowEditingOfContents()
        group=asset.parmTemplateGroup()
        if not group.find('mesh_quality'):
            group.insertBefore('detail_level',hou.StringParmTemplate('mesh_quality','Mesh Quality',1,default_value=('normal',),menu_items=['low','normal','high'],menu_labels=['Low / faceted','Normal / 8 mm curves','High / 2 mm curves']))
        if not group.find('interior_lighting'):
            group.insertAfter('interior_enable',hou.ToggleParmTemplate('interior_lighting','Interior Lighting Fixtures',default_value=True))
        menu=group.find('building_preset')
        menu.setScriptCallback("import json\nn=kwargs['node']\nquality=n.evalParm('mesh_quality')\nc=json.loads(n.type().definition().sections()['Presets.json'].contents())[n.evalParm('building_preset')]\nn.setParms(dict(c['parameters'],layout_seed=c['seeds']['layout'],decoration_seed=c['seeds']['decoration']))\nn.parm('mesh_quality').set(quality)")
        group.replace('building_preset',menu);asset.setParmTemplateGroup(group)
        asset.node('construct_geometry').parm('python').set(B._source()+'\n_cook()\n')
        B.apply_config(asset,presets['01_townhouse_base']);asset.parm('building_preset').set('01_townhouse_base')
        asset.cook(force=True);assert not asset.errors(),asset.errors()
        definition=asset.type().definition();definition.addSection('Presets.json',json.dumps(presets))
        definition.addSection('ParameterSchema.json',json.dumps(B.SCHEMA))
        definition.addSection('CatalogDetailVersion',B.VERSION)
        definition.updateFromNode(asset);definition.setParmTemplateGroup(group)
        definition.copyToHDAFile(str(code/'medieval_building.hda'))
    else:
        obj=hou.node('/obj').createNode('geo','gothic_building_v2_1',run_init_scripts=False)
        asset=obj.createNode('subnet','generator');first=presets['variant_01'];cfg=B.normalize(first)
        asset.setParmTemplateGroup(N.interface(cfg));asset.setParms({k:int(v) if isinstance(v,bool) else v for k,v in cfg['parameters'].items()})
        asset.setParms({'seed_'+k:v for k,v in first['seeds'].items()});asset.parm('building_preset').set('variant_01');N.rebuild(asset)
        asset.cook(force=True);assert not asset.errors(),asset.errors()
        group=asset.parmTemplateGroup()
        asset=asset.createDigitalAsset(name='quynh::gothic_building::3.0',hda_file_name=str(backup/'published_gothic.hda'),description='Gothic Catalog / Interior and Curved Profiles',min_num_inputs=0,max_num_inputs=0)
        definition=asset.type().definition();definition.setParmTemplateGroup(group)
        embedded=(code/'native_network.py').read_text(encoding='utf-8')+'\nCORE_SOURCE='+repr(N.CORE_SOURCE)+'\nPRODUCTION_SOURCE='+repr(N.PRODUCTION_SOURCE)+'\nPRESETS=json.loads('+repr(json.dumps(N.PRESETS))+')\n'
        definition.addSection('PythonModule',embedded);definition.addSection('EditableNodes','construction MANUAL_EDIT final_boundary_alignment')
        definition.addSection('CatalogDetailVersion',B.VERSION)
        definition.copyToHDAFile(str(code/'gothic_building.hda'))
    target=canonical/'asset'/(family.lower()+'_building.hda')
    definition.copyToHDAFile(str(target));hou.hda.installFile(str(target));definitions=hou.hda.definitionsInFile(str(target));definitions[0].setIsPreferred(True)
    # Keep the scene portable, including its current geometry and generator code.
    definition.copyToHDAFile('Embedded')
    for item in hou.hda.definitionsInFile('Embedded'):
        if item.nodeTypeName()==asset.type().name():item.setIsPreferred(True)
    hou.hscript('otconfig -s 1');asset.setCurrent(True,clear_all_selected=True)
    hou.hipFile.save(str(canonical/(family.lower()+'_building.hip')))
    # Use the exact same verified binary at both delivery paths.
    shutil.copy2(code/target.name,target)
    # The schema and manifest are visible authoring documentation as well as HDA sections.
    (canonical/'parameter_schema.json').write_text(json.dumps(B.SCHEMA,indent=2),encoding='utf-8')
    report={'status':'passed','family':family,'version':B.VERSION,'runtime':hou.applicationVersionString(),
        'hda':str(target),'repository_hda':str(code/target.name),'hip':str(canonical/(family.lower()+'_building.hip')),
        'type':asset.type().name(),'presets':len(presets),'backup':str(backup),'hda_sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
    (R/(family.lower()+'_source_publish.json')).write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__':main()
