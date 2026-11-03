"""Read-only inventory of actual workspace assets; reference kits are not targets."""
from pathlib import Path
import collections
import json
import os
import hou
from hda_builder.evidence import ROOT,write_json,sha

def main():
    repo=ROOT.parents[1];assets=repo/'Assets'
    extensions={'.hip','.hiplc','.hipnc','.hda','.hdalc','.hdanc','.py','.vfl','.json','.fbx','.obj','.bgeo','.sc','.png','.jpg','.jpeg','.zip','.exr'}
    excluded={'.git','__pycache__','Plugins','Binaries','Intermediate','Saved','DerivedDataCache','.vs','node_modules'}
    counts=collections.Counter();paths=[];libraries=[];named=[]
    for directory,dirs,files in os.walk(repo):
        dirs[:]=[d for d in dirs if d not in excluded and not (Path(directory)==repo and d=='output')]
        for filename in files:
            p=Path(directory)/filename
            if p.suffix.lower() not in extensions:continue
            relative=p.relative_to(repo).as_posix();counts[p.suffix.lower()]+=1
            row={'path':relative,'size_bytes':p.stat().st_size,'extension':p.suffix.lower()}
            if relative.startswith('Assets/hda_medieval_building/backup/'):row['role']='historical_backup'
            elif relative.startswith('Assets/hda_samples/'):row['role']='reference_training_kit_not_target'
            elif relative.startswith('Assets/_render/'):row['role']='unreal_review_or_retired_export_not_target'
            elif relative.startswith('Assets/hda_medieval_building/'):row['role']='target_authoring_family'
            elif relative.startswith('Assets/hda_gothic_building/'):row['role']='other_authoring_family_inventory_only'
            else:row['role']='other_workspace_content'
            if p.suffix.lower() in {'.hip','.hiplc','.hipnc','.hda','.hdalc','.hdanc','.zip'}:
                row['sha256']=sha(p)
            if p.suffix.lower() in {'.hda','.hdalc','.hdanc'}:
                try:row['definitions']=[{'type':d.nodeTypeName(),'description':d.description(),'min_inputs':d.minNumInputs(),'max_inputs':d.maxNumInputs(),'max_outputs':d.maxNumOutputs()} for d in hou.hda.definitionsInFile(str(p))]
                except hou.Error as exc:row['definition_read_error']=str(exc)
                libraries.append(row)
            paths.append(row)
            if any(s in filename.lower() for s in ('gothic_building_final','golem_hearth_forge','storybook')):named.append(relative)
    active=assets/'hda_medieval_building'
    manifest=json.loads((active/'manifest.json').read_text(encoding='utf-8'))
    schema=json.loads((active/'parameter_schema.json').read_text(encoding='utf-8'))
    records=[]
    for p in manifest['datasets']:
        cfg=json.loads((active/p).read_text(encoding='utf-8'))
        records.append({'variant_id':cfg['variant_id'],'original_dataset':'Assets/hda_medieval_building/'+p,
                        'dataset':p,'sha256':sha(active/p),'seeds':cfg['seeds'],'reference_ids':cfg['reference_ids'],
                        'expected_features':cfg['expected_features'],'dependencies':cfg['module_dependencies'],
                        'old_type':'mythic::medieval_house::3.0','new_type':'mythic::medieval_house::4.0',
                        'new_hda':'asset/medieval_building_modular.hda','scene':f'scenes/variants/{cfg["variant_id"]}.hip',
                        'geometry':cfg['outputs']['geometry'],'hero':cfg['outputs']['hero'],'comparison':cfg['outputs']['comparison']})
    args=list((assets/'_render/SourceAssets/Houdini/Medieval/Args').glob('*.json'))
    families=[{'family':'Medieval current','source':'Assets/hda_medieval_building/medieval_building.hip',
               'old_hda':'Assets/hda_medieval_building/asset/medieval_house_layout.hda','old_type':'mythic::medieval_house::3.0',
               'mode':'refactor_existing','dataset_count':len(records),'baseline_count':1,'public_parameter_count':len(schema['parameters']),
               'seeds':'seeds.decoration -> seed_decoration','reference_count':len(manifest['references'])-1},
              {'family':'Gothic current','source':'Assets/hda_gothic_building/gothic_building.hip',
               'mode':'inventory_only_outside_selected_HDA','current_datasets':len(list((assets/'hda_gothic_building').glob('datasets/*.json'))),
               'note':'Current authoring folder has no builder/dataset/HDA; retired review exports are not source replacements.'},
              {'family':'Legacy Medieval review','source':'Assets/_render/SourceAssets/Houdini/Medieval',
               'mode':'retired_review_do_not_restore','argument_files':[p.relative_to(repo).as_posix() for p in args],
               'note':'Outside the selected authoring HIP; repository instructions retire the old full-Python generators.'},
              {'family':'SideFX house/window/stair reference kits','source':'Assets/hda_samples',
               'mode':'reference_dependencies_only','note':'Training HIP/HDA examples, not user Medieval building datasets.'}]
    report={'scope':'User-selected Assets/hda_medieval_building/medieval_building.hip; recursive discovery does not authorize replacing other families.',
            'houdini':hou.applicationVersionString(),'file_counts':dict(counts),'files':paths,'hda_libraries':libraries,
            'families':families,'named_family_matches':named,'missing_named_families':[s for s in ('Gothic_Building_Final','Golem_Hearth_Forge','Storybook') if not any(s.lower() in p.lower() for p in named)],
            'target_records':records,'skill_archive_sha256':sha(assets/'skills/design-hda-building-modular-houdini22.zip')}
    write_json(ROOT/'results/workspace_inventory.json',report)
    write_json(ROOT/'variant_mapping.json',{'schema_version':1,'records':records})
    print('INVENTORY',dict(counts),'target_records',len(records),flush=True)

if __name__=='__main__':main()
