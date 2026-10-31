"""Promote completed native exports only after Unreal releases the prior batch."""
from pathlib import Path
import hashlib
import json
import shutil
from production_pipeline import ROOT,REPO,EXPORT,OUT,configure,collect

state=json.loads((REPO/'TDKProps/Saved/GothicReview/City/asset_import.json').read_text())
assert state['status']=='passed','Unreal is still reading the previous FBX batch'
records=configure()
for r in records[1:]:
    path=OUT/'records'/(r['variant_id']+'_game.json');row=json.loads(path.read_text())
    assert '/_final_stage/' in row['file'],r['variant_id']
    source=REPO/row['file'];assert hashlib.sha256(source.read_bytes()).hexdigest()==row['fbx_sha256']
    target=EXPORT/'Meshes'/source.name
    assert source.resolve().is_relative_to(EXPORT.resolve()) and target.resolve().parent==(EXPORT/'Meshes').resolve()
    source.replace(target)
    row['file']=target.relative_to(REPO).as_posix();path.write_text(json.dumps(row,indent=2),encoding='utf-8')
collect(records)
print('FINAL_EXPORTS_PROMOTED',flush=True)
