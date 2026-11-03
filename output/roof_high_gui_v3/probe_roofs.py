import json
import collections
import hou
import build_hda as B
from hda_builder.evidence import ROOT,write_json

b=B.build();results=[]
for ident in ('06_tavern','08_normal_house_b','11_artisan_house'):
    cfg=json.loads((ROOT/f'datasets/{ident}.json').read_text(encoding='utf-8'));B.apply_config(b,cfg)
    row={'variant_id':ident,'nodes':{}}
    for path in ('ROOF/patch_surfaces/OUT_raw_surfaces','ROOF/ridge_valley_solver/OUT_roof_top','ROOF/ridge_valley_solver/OUT_roof_shell',
                 'ROOF/surfaces_tiles/ROOF_TILE_PLACEMENT','ROOF/surfaces_tiles/tile_candidates_on_resolved_domain',
                 'ROOF/surfaces_tiles/TILE_MATERIAL','ROOF/surfaces_tiles/clip_tiles_to_roof_envelope',
                 'ROOF/surfaces_tiles/TRIM_TILES_AGAINST_BODY','ROOF/surfaces_tiles/OUT_roof_tiles',
                 'ROOF/dormers/DORMER_PLACEMENT_DATA','OUTPUT/detail_levels/OUT_low','OUTPUT/detail_levels/OUT_high'):
        n=b.node(path);g=n.geometry()
        data={'points':g.intrinsicValue('pointcount'),'primitives':g.intrinsicValue('primitivecount'),'bounds':list(g.boundingBox().sizevec())}
        if g.findPrimAttrib('roof_patch_id'):data['primitive_roof_ids']=dict(collections.Counter(g.primStringAttribValues('roof_patch_id')))
        if g.findPointAttrib('roof_patch_id'):data['point_roof_ids']=dict(collections.Counter(g.pointStringAttribValues('roof_patch_id')))
        row['nodes'][path]=data
    results.append(row);print(ident,json.dumps(row),flush=True)
write_json(ROOT/'results/roof_probe.json',results)
