"""Pure core validation; does not certify HOM, building codes or HDA portability."""
import copy,json,hashlib
from pathlib import Path
import build_hda as B
from evaluate_hda import metrics
r=Path(__file__).resolve().parent;rows=[]
B.embedded_source()
for mode in ['min','max']:
 c=copy.deepcopy(B.BASELINE)
 for k,s in B.SCHEMA.items():
  if mode in s:c['parameters'][k]=s[mode]
 m=B.generate(c);q=metrics(m);assert q['finite'] and q['degenerate_faces']==0 and q['duplicate_faces']==0
 rows.append(dict(case='numeric_'+mode,primitives=len(m.faces),passed=True))
for name,patch in [('unsupported_gate',dict(building_type='church',gate_composition='urban_asymmetric')),('low_openwork',dict(tower_crown='openwork',tower_floors=2)),('unknown_menu',dict(facade_style='unknown'))]:
 c=copy.deepcopy(B.BASELINE);c['parameters'].update(patch)
 try:B.normalize(c)
 except ValueError:rows.append(dict(case=name,rejected=True))
 else:raise AssertionError(name)
c=copy.deepcopy(B.BASELINE)
for k in B.LEGACY_DEFAULTS:c['parameters'].pop(k)
assert set(B.normalize(c)['parameters'])==set(B.SCHEMA);rows.append(dict(case='legacy_parameter_migration',passed=True))
man=json.loads((r/'manifest.json').read_text());records={json.loads((r/p).read_text())['variant_id']:json.loads((r/p).read_text()) for p in man['supplemental_sweeps']}
for c in records.values():
 if c['comparison_kind']=='sweep':
  base=records[c['baseline_id']];actual=[k for k in c['parameters'] if c['parameters'][k]!=base['parameters'][k]];assert actual==c['changed_parameters'],(c['variant_id'],actual)
(r/'results').mkdir(exist_ok=True);(r/'results/control_validation.json').write_text(json.dumps(dict(generator_sha256=hashlib.sha256((r/'build_hda.py').read_bytes()).hexdigest(),checks=rows,supplemental_sweeps=21),indent=2));print('PASS: controls, boundaries, migration, sweep mappings')
