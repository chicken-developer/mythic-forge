"""Create a review ZIP and verify its CRC; never touch authoring sources."""
from pathlib import Path
import hashlib
import json
import zipfile
import os

ROOT=Path(__file__).resolve().parent
target=ROOT.parent/'modular_rebuild_v2.zip'
temporary=ROOT.parent/'modular_rebuild_v2.zip.tmp'
files=[]
for path in ROOT.rglob('*'):
    if not path.is_file():continue
    relative=path.relative_to(ROOT).as_posix()
    if '__pycache__' in path.parts or 'backup' in path.parts:continue
    if relative.startswith('results/portable/'):continue
    if relative.endswith(('debug_failed_build.hip','ports_probe.hda')):continue
    if relative.startswith('results/pure_refactor/') and path.suffix in ('.hda','.hip'):continue
    files.append(path)
manifest={'files':[{'path':p.relative_to(ROOT).as_posix(),'size_bytes':p.stat().st_size,
                    'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)],
          'omitted':'__pycache__, auto-backups, relocated test duplicate and failed/probe scene binaries; diagnostic JSON/logs and pure-refactor parity records retained.'}
manifest_path=ROOT/'package_files.json';manifest_path.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
files=[p for p in files if p!=manifest_path]+[manifest_path]
with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
    for path in sorted(files):archive.write(path,'modular_rebuild_v2/'+path.relative_to(ROOT).as_posix())
with zipfile.ZipFile(temporary) as archive:
    broken=archive.testzip()
    if broken:raise RuntimeError('ZIP CRC failed: '+broken)
os.replace(temporary,target)
digest=hashlib.sha256(target.read_bytes()).hexdigest()
(ROOT.parent/'modular_rebuild_v2.sha256').write_text(digest+'  '+target.name+'\n',encoding='utf-8')
print(json.dumps({'archive':str(target),'files':len(files),'size_bytes':target.stat().st_size,'sha256':digest,'crc':'pass'}),flush=True)
