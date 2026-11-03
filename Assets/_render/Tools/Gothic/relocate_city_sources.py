"""Repair export destinations after HoudiniWorkspace moved beside mythic-forge."""
from pathlib import Path
import ast
import json
import os
import sys

REPO=Path(__file__).resolve().parents[4]
SOURCE=REPO/'Assets/hda_gothic_building'
assert (SOURCE/'gothic_building.hip').is_file()
replacement="REPO=next(p for p in Path(__file__).resolve().parents if (p/'Assets/_render/TDKProps.uproject').is_file())"
for name in ('production_pipeline.py','build_city_context.py'):
    path=SOURCE/name;text=path.read_text(encoding='utf-8')
    if replacement not in text:
        assert 'REPO=ROOT.parents[1]' in text
        text=text.replace('REPO=ROOT.parents[1]',replacement,1)
    if name=='production_pipeline.py':
        if '\nimport os\n' not in text:text=text.replace('\nimport math\n','\nimport math\nimport os\n',1)
        text=text.replace("'source_hip':'Assets/hda_gothic_building/gothic_building.hip'", "'source_hip':os.path.relpath(ROOT/'gothic_building.hip',REPO).replace(chr(92),'/')")
    ast.parse(text);path.write_text(text,encoding='utf-8')
for path in (REPO/'Assets/_render/SourceAssets/Gothic/mesh_manifest.json',SOURCE/'reports/production_validation.json'):
    data=json.loads(path.read_text());data['source_hip']=os.path.relpath(SOURCE/'gothic_building.hip',REPO).replace(chr(92),'/')
    path.write_text(json.dumps(data,indent=2),encoding='utf-8')
sys.path.insert(0,str(SOURCE))
import production_pipeline as pipeline
assert pipeline.REPO.resolve()==REPO
assert pipeline.EXPORT.resolve()==(REPO/'Assets/_render/SourceAssets/Gothic').resolve()
print('SOURCE_PATHS_VERIFIED',SOURCE,pipeline.EXPORT)
