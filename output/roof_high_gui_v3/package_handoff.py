"""Package the final native screenshots and validated HDA; verify ZIP CRC."""
from pathlib import Path
import hashlib,json,zipfile,os
ROOT=Path(__file__).resolve().parent
summary=json.loads((ROOT/'results/final_summary.json').read_text(encoding='utf-8'))
assert summary['status']=='pass'
target=ROOT.parent/'roof_high_gui_v3.zip';temporary=target.with_suffix('.zip.tmp')
excluded_parts={'__pycache__','backup','gui_prefs','gui_commands','gui_results','portable'}
diagnostics={'probe_canopies.py','probe_native.py','probe_roofs.py','diagnose_roof_volume.py','gui_diagnostics.py','gui_inspect.py','gui_visibility.py','inspect_canvas.py'}
result_files={'variants.json','geometry_summary.json','regressions.json','canopy_clearance.json','portability.json','static_checks.json','source_fingerprints.json','source_inventory.json','migration.json','clean_scenes.json','gui_capture_all.json','gui_capture_extra.json','gui_ready.json','publication.json','repo_path_audit.json','visual_review.json','final_summary.json','native_capabilities.json','camera.json'}
files=[]
captures=json.loads((ROOT/'results/gui_capture_all.json').read_text(encoding='utf-8'))+json.loads((ROOT/'results/gui_capture_extra.json').read_text(encoding='utf-8'))
screens={r['image'] for r in captures}
screens.update(p.replace('_gui.png','_gui_viewport.png') for p in list(screens) if (ROOT/p.replace('_gui.png','_gui_viewport.png')).exists())
screens.update('screenshots/final/review_sheet_'+str(i)+'.png' for i in range(1,5))
for path in ROOT.rglob('*'):
    if not path.is_file() or excluded_parts.intersection(path.relative_to(ROOT).parts):continue
    rel=path.relative_to(ROOT).as_posix()
    if path.name in diagnostics or path.name=='package_files.json':continue
    if rel.startswith('screenshots/') and not rel.startswith('screenshots/final/'):continue
    if rel.startswith('screenshots/final/') and rel not in screens:continue
    if rel.startswith('scenes/') and path.name.startswith('debug_'):continue
    if rel.startswith('results/') and path.name not in result_files and not path.stem in {p.stem for p in (ROOT/'datasets').glob('*.json')}:continue
    files.append(path)
manifest={'files':[{'path':p.relative_to(ROOT).as_posix(),'size_bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)],
          'omitted':'Auto-backups, temporary GUI queues/preferences/PIDs, relocated duplicate HDA, failed/prototype screenshots/scenes, diagnostic scripts/logs. Canonical QA reports and source snapshot retained.'}
mf=ROOT/'package_files.json';mf.write_text(json.dumps(manifest,indent=2),encoding='utf-8');files.append(mf)
with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
    for path in sorted(files):archive.write(path,'roof_high_gui_v3/'+path.relative_to(ROOT).as_posix())
with zipfile.ZipFile(temporary) as archive:
    broken=archive.testzip()
    if broken:raise RuntimeError('ZIP CRC failed: '+broken)
os.replace(temporary,target);digest=hashlib.sha256(target.read_bytes()).hexdigest()
(ROOT.parent/'roof_high_gui_v3.sha256').write_text(digest+'  '+target.name+'\n',encoding='utf-8')
print(json.dumps({'archive':str(target),'files':len(files),'size_bytes':target.stat().st_size,'sha256':digest,'crc':'pass'}),flush=True)
