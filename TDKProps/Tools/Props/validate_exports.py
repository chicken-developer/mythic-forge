"""Validate every deliverable file, preset, quality level and source checksum."""
from pathlib import Path
import hashlib
import json

P=Path(__file__).resolve().parents[2];OUT=P/'RenderTests/PropsDetail';OUT.mkdir(parents=True,exist_ok=True)
report={'status':'running','exports':[],'quality_comparison':[]}
data={}
for family,qualities,count in [('Medieval',('low','normal','high'),24),('Gothic',('normal',),28)]:
    for quality in qualities:
        path=P/'SourceAssets'/family/('mesh_manifest.json' if quality=='normal' else 'mesh_manifest_'+quality+'.json')
        manifest=json.loads(path.read_text());rows=manifest['variants'];assert len(rows)==count
        assert len({r['id'] for r in rows})==count
        assert manifest['status']=='passed'
        for row in rows:
            file=P.parent/row['file'];assert file.is_file()
            assert hashlib.sha256(file.read_bytes()).hexdigest()==row['fbx_sha256'],str(file)
            assert row['invalid_normals']==0 and row['nonfinite_attributes']==0
            assert row['interior_triangles']>0 and row['interior_lights']
            assert row['quality']==quality
        data[(family,quality)]={r['id']:r for r in rows}
        report['exports'].append({'family':family,'quality':quality,'presets':len(rows),'triangles':sum(r['triangles'] for r in rows),'interior_lights':sum(len(r['interior_lights']) for r in rows),'checksum_mismatches':0,'invalid_normals':0})
for ident,normal in data[('Medieval','normal')].items():
    low=data[('Medieval','low')][ident];high=data[('Medieval','high')][ident]
    assert low['triangles']<normal['triangles']<high['triangles'],(ident,low['triangles'],normal['triangles'],high['triangles'])
    assert low['seeds']==normal['seeds']==high['seeds']
    report['quality_comparison'].append({'id':ident,**{q:data[('Medieval',q)][ident]['triangles'] for q in ('low','normal','high')}})
for family in ('Medieval','Gothic'):
    published=json.loads((P/'Saved/PropsDetail'/(family.lower()+'_source_publish.json')).read_text())
    actual=hashlib.sha256(Path(published['hda']).read_bytes()).hexdigest()
    assert actual==published['hda_sha256']==hashlib.sha256(Path(published['repository_hda']).read_bytes()).hexdigest()
report['status']='passed';(OUT/'exports.json').write_text(json.dumps(report,indent=2));print(json.dumps(report['exports'],indent=2))
