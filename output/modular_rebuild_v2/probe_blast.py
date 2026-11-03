import json
import hou
import build_hda as B
from hda_builder.evidence import ROOT,geometry_metrics,write_json
cfg=json.loads((ROOT/'datasets/09_normal_house_c.json').read_text(encoding='utf-8'))
b=B.build(cfg)
old=hou.Geometry();old.loadFromFile(str(ROOT/'geometry/before/09_normal_house_c.bgeo.sc'))
before=geometry_metrics(old)
rows=[]
for path in ('OUTPUT/game_export/IN_building','OUTPUT/game_export/classify_exact_face_defects','OUTPUT/game_export/remove_classified_face_defects','OUTPUT/game_export/remove_orphan_points_only'):
    node=b.node(path);m=geometry_metrics(node.geometry());rows.append({'path':path,'metrics':m})
    print(path,m['points'],m['primitives'],m['exact_duplicate_polygons'],m['valid_unique_surface_hash']==before['valid_unique_surface_hash'],flush=True)
    g=node.geometry();print('GROUPS',[(x.name(),len(x.prims())) for x in g.primGroups() if 'qa_remove' in x.name()],
                           [(a.name(),g.attribValue(a)) for a in g.globalAttribs() if a.name().startswith('qa_')],flush=True)
write_json(ROOT/'results/blast_cleanup_probe.json',rows)
holder=hou.node('/obj').createNode('geo','EXPLICIT_PRIMITIVE_TEST',run_init_scripts=False)
stash=holder.createNode('stash');stash.parm('stash').set(b.node('OUTPUT/game_export/classify_exact_face_defects').geometry())
blast=holder.createNode('blast');blast.setInput(0,stash);print('GROUP_MENU',blast.parm('grouptype').menuItems(),flush=True)
blast.parm('group').set('qa_remove_degenerate qa_remove_duplicate');blast.parm('grouptype').set('prims')
m=geometry_metrics(blast.geometry());print('EXPLICIT_PRIMS',m['primitives'],m['valid_unique_surface_hash']==before['valid_unique_surface_hash'],flush=True)
