"""Save the exact current catalog parameters and seeds for manual HDA authoring."""
from pathlib import Path
import hashlib
import json

P=Path(__file__).resolve().parents[2]
for family in ('Medieval','Gothic'):
    source=P/'SourceAssets/Houdini'/family/'Editor'/(family.lower()+'_building_editable.hda')
    rows=json.loads((P/'SourceAssets'/family/'mesh_manifest.json').read_text())['variants']
    folder=source.parent.parent/'Args';folder.mkdir(parents=True,exist_ok=True)
    entries=[]
    for row in rows:
        parameters=dict(row['parameters'])
        if family=='Medieval':
            parameters.update(layout_seed=row['seeds']['layout'],decoration_seed=row['seeds']['decoration'],mesh_quality='normal',layout_input_enable=False)
        else:
            parameters.update({'seed_'+key:value for key,value in row['seeds'].items()})
            parameters.update(mesh_quality='game',auto_rebuild=False)
        parameters['building_preset']=row['id']
        args={'family':family,'id':row['id'],'label':row['label'],'generator_version':row['generator_version'],
              'hda':'../Editor/'+source.name,'hda_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
              'houdini_asset':'/Game/'+family+'/Houdini/'+source.stem,
              'houdini_parameters':parameters,'seeds':row['seeds'],
              'reference_blueprint':'/Game/'+family+'/Blueprints/BP_'+family+'_'+row['id'],
              'map':'/Game/'+family+'/Maps/L_'+family+'_Catalog',
              'source_origin_m':row['source_origin_m'],
              'bake_folder':'/Game/'+family+'/ManualBakes/'+row['id'],
              'auto_bake':False}
        path=folder/(row['id']+'.json');path.write_text(json.dumps(args,ensure_ascii=False,indent=2),encoding='utf-8')
        entries.append({'id':row['id'],'args':path.relative_to(P).as_posix()})
    (folder/'index.json').write_text(json.dumps({'family':family,'entries':entries},indent=2),encoding='utf-8')
    print('ARGS',family,len(entries))
