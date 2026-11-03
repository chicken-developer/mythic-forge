import json,collections
import build_hda as B
from hda_builder.evidence import ROOT,write_json
b=B.build();rows=[]
for ident in ('08_normal_house_b','10_normal_house_c_canopy','11_artisan_house'):
    B.apply_config(b,json.loads((ROOT/'datasets'/(ident+'.json')).read_text(encoding='utf-8')))
    row={'id':ident,'nodes':{}}
    for module in ('DETAILS/site','OPENINGS/attachments'):
        for name in ('canopy_members_only','canopy_wall_contact','canopy_roof_domain','canopy_tile_placements','copy_canopy_tiles'):
            g=b.node(module+'/'+name).geometry()
            row['nodes'][module+'/'+name]={'points':len(g.points()),'prims':len(g.prims()),'vertex_counts':dict(collections.Counter(len(p.vertices()) for p in g.prims())),'materials':dict(collections.Counter(g.primStringAttribValues('material_family'))) if g.findPrimAttrib('material_family') else {}}
    rows.append(row)
write_json(ROOT/'results/canopy_probe.json',rows)
print(json.dumps(rows),flush=True)
