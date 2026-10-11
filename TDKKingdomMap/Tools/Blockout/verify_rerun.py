"""Compare real Unreal execution receipts before/after the same-input rerun."""
import json
from pathlib import Path
base=Path(__file__).resolve().parents[2]/'Design/Blockout/Generated'
before=json.loads((base/'rerun_before.json').read_text())
after=json.loads((base/'unreal_execution_report.json').read_text())
checks={
 'same_input':before['fingerprint']==after['fingerprint'],
 'same_owned_actor_count':before['owned_actor_count']==after['owned_actor_count'],
 'same_actor_transforms':before['actor_transform_md5']==after['actor_transform_md5'],
 'no_duplicate_ids':not after['duplicate_ids'],
 'manual_count_unchanged':before['manual_actor_count']==after['manual_actor_count'],
 'manual_probe_survived':before['manual_probe_present'] and after['manual_probe_present'],
 'all_previous_owned_actors_replaced':after['replaced_owned_actors']==before['owned_actor_count'],
 'level_saved':after['level_saved'],
 'actual_collision_passed':after['collision_status']=='pass',
 'actual_dimensions_passed':after['dimensions_status']=='pass'
}
result=dict(status='pass' if all(checks.values()) else 'fail',checks=checks,
            fingerprint=after['fingerprint'],owned_actor_count=after['owned_actor_count'],
            manual_actor_count=after['manual_actor_count'],actor_transform_md5=after['actor_transform_md5'],
            evidence='Two actual Unreal generate commandlet runs; reloaded saved level; one untagged manual sentinel preserved')
(base/'rerun_validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if result['status']=='pass' else 1)
