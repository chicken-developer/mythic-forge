"""Cook every original config without saving any changes to authoring files."""
from pathlib import Path
import sys
import json
import importlib.util
import shutil
import traceback
import hou
from hda_builder.evidence import ROOT, write_json, sha, cook_record

REPO = ROOT.parents[1]
SOURCE = REPO / 'Assets/hda_medieval_building'

def main():
    for folder in ('datasets','references','results/before','geometry/before','scenes','asset','renders','comparisons'):
        (ROOT/folder).mkdir(parents=True, exist_ok=True)
    for folder in ('datasets','references'):
        for p in (SOURCE/folder).iterdir():
            if p.is_file(): shutil.copy2(p, ROOT/folder/p.name)
    for name in ('parameter_schema.json','manifest.json','manifest_group_1.json','manifest_group_2.json','manifest_group_3.json'):
        shutil.copy2(SOURCE/name, ROOT/name)
    shutil.copy2(SOURCE/'medieval_building.hip', ROOT/'scenes/source_snapshot.hip')
    spec=importlib.util.spec_from_file_location('original_api', SOURCE/'build_hda.py')
    api=importlib.util.module_from_spec(spec); spec.loader.exec_module(api)
    manifest=json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8'))
    paths=['datasets/baseline.json']+manifest['datasets']
    fingerprints={str(p.relative_to(REPO)).replace('\\','/'):sha(p) for p in
                  [SOURCE/'medieval_building.hip', SOURCE/'build_hda.py', SOURCE/'asset/medieval_house_layout.hda', SOURCE/'parameter_schema.json', SOURCE/'manifest.json']+
                  list((SOURCE/'datasets').glob('*.json'))+list((SOURCE/'sources').rglob('*.*'))}
    write_json(ROOT/'results/source_fingerprints.json', fingerprints)
    hou.hipFile.load(str(SOURCE/'medieval_building.hip'), suppress_save_prompt=True, ignore_load_warnings=True)
    b=hou.node('/obj/medieval_building')
    print('RUNTIME', hou.applicationVersionString(), hou.licenseCategory(), flush=True)
    print('SUBNET_TYPE', hou.sopNodeTypeCategory().nodeTypes()['subnet'].maxNumInputs(), flush=True)
    rows=[]
    for path in paths:
        record=json.loads((SOURCE/path).read_text(encoding='utf-8')); ident=record['variant_id']
        try:
            api.apply_config(b, record)
            row=cook_record(b, b.node('OUT_BUILDING'), record, f'geometry/before/{ident}.bgeo.sc')
            row['source_hip_sha256']=fingerprints['Assets/hda_medieval_building/medieval_building.hip']
            write_json(ROOT/f'results/before/{ident}.json',row)
            print('BASELINE',ident,row['cook_status'],row['metrics']['points'],round(row['cook_seconds'],3),flush=True)
        except Exception as e:
            row={'variant_id':ident,'cook_status':'fail','exception':str(e),'traceback':traceback.format_exc()}
            write_json(ROOT/f'results/before/{ident}.json', row)
            print('FAIL',ident,str(e),flush=True)
        rows.append({'variant_id':ident,'status':row['cook_status']})
        write_json(ROOT/'results/baseline_summary.json', {'houdini':hou.applicationVersionString(),'records':rows})
    return int(any(r['status']!='pass' for r in rows))

if __name__=='__main__': sys.exit(main())
