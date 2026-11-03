"""Measure fabric projection and support recook against actual balcony geometry."""
import copy,json
import build_hda as B
from hda_builder.evidence import ROOT,write_json

def front(g,pattern=None):
    faces=[p for p in g.prims() if pattern is None or pattern in p.attribValue('name')]
    return min((pt.position()[2] for p in faces for pt in p.points()),default=None)

b=B.build();rows=[]
for file in sorted((ROOT/'datasets').glob('*.json')):
    cfg=json.loads(file.read_text(encoding='utf-8'))
    if not cfg['parameters']['awning_enable']:continue
    B.apply_config(b,cfg);g=b.node('DETAILS/site').geometry();balcony=b.node('DETAILS/balcony').geometry()
    canvas=front(g,'awning_canvas');posts=front(g,'awning_post');edge=front(balcony)
    projection=edge-canvas if edge is not None and canvas is not None else None
    rows.append({'variant_id':cfg['variant_id'],'canvas_front_z':canvas,'support_front_z':posts,'balcony_front_z':edge,
                 'projection_beyond_balcony_m':projection,'status':'pass' if canvas is not None and posts is not None and (projection is None or projection>=.44) else 'fail'})
cfg=json.loads((ROOT/'datasets/10_normal_house_c_canopy.json').read_text(encoding='utf-8'))
B.apply_config(b,cfg);g=b.node('DETAILS/site').geometry();canvas0=front(g,'awning_canvas');posts0=front(g,'awning_post')
cfg['parameters']['balcony_depth_m']+=.35;B.apply_config(b,cfg);g=b.node('DETAILS/site').geometry();canvas1=front(g,'awning_canvas');posts1=front(g,'awning_post')
test={'test':'balcony_depth_updates_canopy_and_supports','canvas_shift_m':canvas0-canvas1,'support_shift_m':posts0-posts1,
      'status':'pass' if abs(canvas0-canvas1-.35)<1e-4 and abs(posts0-posts1-.35)<1e-4 else 'fail'}
result={'status':'pass' if test['status']=='pass' and all(r['status']=='pass' for r in rows) else 'fail',
        'rows':rows,'regression':test,'scope':'Actual geometry projection beyond balcony; not a structural load/collision certificate.'}
write_json(ROOT/'results/canopy_clearance.json',result);print(json.dumps(result),flush=True)
raise SystemExit(result['status']!='pass')
