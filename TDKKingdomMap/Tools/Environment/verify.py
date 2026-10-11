"""Compare saved Unreal manifests and source hashes; this does not claim interactive playtesting."""
import hashlib
import json
import pathlib
import sys

ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'Design/Environment/Generated'
def read(name):return json.loads((OUT/name).read_text(encoding='utf-8-sig'))
data=read('environment_compiled.json');natural=set(data['natural_proxy_ids'])
baseline=read('baseline_manifest.json');after=read('trial_after_manifest.json' if '--trial' in sys.argv else 'after_manifest.json')
def protected(manifest):
    result={}
    for a in manifest['actors']:
        ids=[t.removeprefix('ThanhA.Id.') for t in a['tags'] if t.startswith('ThanhA.Id.')]
        if not any('mesh' in c for c in a['components']) or any(x in natural for x in ids):continue
        result[a['name']]=a
    return result
b=protected(baseline);a=protected(after);errors=[]
for k,v in b.items():
    if a.get(k)!=v:errors.append('Protected actor changed: '+k)
hashes=read('pack_hashes_before.json')
for path,expected in hashes.items():
    f=ROOT/path
    if not f.exists() or hashlib.sha256(f.read_bytes()).hexdigest()!=expected:errors.append('Package source changed: '+path)
run=read('trial_execution.json' if '--trial' in sys.argv else 'execution.json')
if run['height_before_md5']!=run['height_after_md5'] or run['height_after_md5']!=data['heightmap_md5']:errors.append('Landscape changed')
if run['rejected_instances']:errors.append('Unresolved instance placement')
if len({x['id'] for x in data['instances']})!=len(data['instances']):errors.append('Duplicate placement ID')
performance=read('performance.json') if (OUT/'performance.json').exists() else {}
walk=read('walk_validation.json') if (OUT/'walk_validation.json').exists() else {}
report=dict(status='fail' if errors else 'pass',errors=errors,protected_static_mesh_actors=len(b),pack_files_unchanged=len(hashes),heightmap_md5=run['height_after_md5'],fingerprint=data['fingerprint'],interactive_playtest=False,
            standalone_auto_walk_passed=walk.get('environment_fingerprint')==data['fingerprint'] and walk.get('status')=='pass',
            fps_measured=performance.get('environment_fingerprint')==data['fingerprint'] and performance.get('sampled_frames',0)>0 and performance.get('width',0)>0,
            performance_report='performance.json' if performance else None)
name='trial_preservation.json' if '--trial' in sys.argv else 'preservation.json'
(OUT/name).write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report));sys.exit(bool(errors))
