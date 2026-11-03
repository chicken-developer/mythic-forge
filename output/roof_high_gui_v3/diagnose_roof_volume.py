import json
import hou
import build_hda as B
from hda_builder.evidence import ROOT,write_json
b=B.build(json.loads((ROOT/'datasets/06_tavern.json').read_text(encoding='utf-8')))
rows=[]
for path in ('ROOF/patch_surfaces/OUT_raw_surfaces','ROOF/ridge_valley_solver/closed_roof_patch_volumes','ROOF/ridge_valley_solver/resolve_roof_volume_union',
             'ROOF/ridge_valley_solver/retain_visible_roof_domain','ROOF/ridge_valley_solver/restore_resolved_patch_ids','ROOF/ridge_valley_solver/OUT_roof_shell',
             'ROOF/dormers/matched_dormer_roof_volume','ROOF/dormers/COPY_DORMER_CUTTERS',
             'ROOF/surfaces_tiles/TILE_MATERIAL','ROOF/surfaces_tiles/clip_tiles_to_roof_envelope','ROOF/surfaces_tiles/TRIM_TILES_AGAINST_BODY','ROOF/surfaces_tiles/OUT_roof_tiles'):
    n=b.node(path);g=n.geometry();ys=[v.point().position().y() for p in g.prims() for v in p.vertices()]
    normals=[p.normal().y() for p in g.prims() if p.type()==hou.primType.Polygon]
    row={'path':path,'prims':g.intrinsicValue('primitivecount'),'y_referenced':[min(ys),max(ys)] if ys else [],
         'upward':sum(n>.02 for n in normals),'downward':sum(n<-.02 for n in normals),'bounds':list(g.boundingBox().sizevec())}
    for sign,label in [(1,'up_y'),(-1,'down_y')]:
        yy=[sum(v.point().position()[1] for v in p.vertices())/max(1,len(p.vertices())) for p in g.prims() if p.type()==hou.primType.Polygon and p.normal().y()*sign>.02]
        row[label]=[min(yy),max(yy)] if yy else []
    if g.findPrimAttrib('roof_surface_error'):row['max_error']=max(g.primFloatAttribValues('roof_surface_error'),default=0)
    if n.type().name()=='boolean::2.0':row['parms']={k:n.parm(k).evalAsString() for k in ('booleanop','asurface','bsurface','windingop','adepth1','adepth2')}
    rows.append(row);print(json.dumps(row),flush=True)
write_json(ROOT/'results/volume_diagnostic.json',rows)
