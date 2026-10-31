"""Promote all verified Gothic staging exports together, preserving the prior files."""
from pathlib import Path
import json
import shutil

P=Path(__file__).resolve().parents[2];R=P/'Saved/PropsDetail';root=P.parent.resolve()
records=R/'Gothic/normal';staging=(P/'SourceAssets/Gothic/RoundFix').resolve()
destination=(R/'CorrectedGothicFBX').resolve();backup=R/'BeforeRoundFixMeshes';backup.mkdir(parents=True,exist_ok=True)
assert staging.is_relative_to(root) and destination.is_relative_to(root)
rows=[json.loads(p.read_text()) for p in sorted(records.glob('variant_*.json'))]
assert len(rows)==28 and all(r['generator_version']=='3.1.1-catalog-round-fix' for r in rows)
for row in rows:
    source=(P.parent/row['file']).resolve();assert source.is_relative_to(staging)
    target=(P/'SourceAssets/Gothic/Meshes'/source.name).resolve();assert target.is_relative_to(root)
    if target.exists():shutil.copy2(target,backup/target.name)
    shutil.copy2(source,target);row['file']=target.relative_to(P.parent).as_posix()
    (records/(row['id']+'.json')).write_text(json.dumps(row,indent=2))
manifest=P/'SourceAssets/Gothic/mesh_manifest.json';data=json.loads(manifest.read_text())
data.update(status='passed',version='3.1.1-catalog-round-fix',variants=rows)
manifest.write_text(json.dumps(data,indent=2))
assert not destination.exists()
shutil.move(str(staging),str(destination))
print('PROMOTED',len(rows))
