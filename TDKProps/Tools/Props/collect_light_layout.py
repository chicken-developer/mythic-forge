"""Collect connected-globe lighting evidence without altering any mesh binaries."""
from pathlib import Path
import json
P=Path(__file__).resolve().parents[2];R=P/'Saved/PropsDetail';OUT=P/'RenderTests/PropsDetail'
audits=[json.loads(p.read_text()) for p in sorted((R/'LightAudit').glob('variant_*.json'))]
assert len(audits)==28 and all(a['status']=='passed' for a in audits)
rows=[json.loads((R/'Gothic/normal'/(a['id']+'.json')).read_text()) for a in audits]
for row,audit in zip(rows,audits):
    assert len(row['interior_lights'])==audit['actual_lights']
    assert row['triangles']==audit['triangles_unchanged']
manifest=P/'SourceAssets/Gothic/mesh_manifest.json';data=json.loads(manifest.read_text());data['variants']=rows;data['light_layout_version']=2
manifest.write_text(json.dumps(data,indent=2))
report={'status':'passed','method':'connected primitive components; independent of repeated names','fbx_geometry_unchanged':True,'presets':28,'lights':sum(a['actual_lights'] for a in audits),'records':audits}
(OUT/'gothic_light_layout.json').write_text(json.dumps(report,indent=2))
proof_path=OUT/'gothic_portable_hda.json';proof=json.loads(proof_path.read_text());assert proof['status']=='passed'
byid={a['id']:a for a in audits}
for record in proof['records']:
    record.setdefault('named_light_groups',record['lights']);record['lights']=byid[record['id']]['actual_lights'];record['connected_light_sources_checked']=True
proof['lighting_evidence']='gothic_light_layout.json';proof_path.write_text(json.dumps(proof,indent=2))
print('CONNECTED_LIGHT_LAYOUT',report['presets'],report['lights'])
