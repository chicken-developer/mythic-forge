"""Cook/export actual native LOW and HIGH. No image rendering in this runner."""
from pathlib import Path
import collections,hashlib,json,time
import numpy as np
import hou
import build_hda as B
from hda_builder.evidence import ROOT,write_json,sha,node_messages
from hda_builder.graph_audit import audit

def metrics(g,qa):
    xyz=np.asarray(g.pointFloatAttribValues('P'),dtype=np.float64)
    uv=np.asarray(g.vertexFloatAttribValues('uv') if g.findVertexAttrib('uv') else g.pointFloatAttribValues('uv') if g.findPointAttrib('uv') else [],dtype=np.float64)
    ns=g.vertexFloatAttribValues('N') if g.findVertexAttrib('N') else g.pointFloatAttribValues('N') if g.findPointAttrib('N') else []
    qa[0].parm('stash').set(g);checked=qa[1].geometry()
    count=lambda name:len(checked.findPrimGroup(name).prims()) if checked.findPrimGroup(name) else 0
    return {'points':g.intrinsicValue('pointcount'),'primitives':g.intrinsicValue('primitivecount'),
            'dimensions':list(g.boundingBox().sizevec()),'bounds_min':list(g.boundingBox().minvec()),'bounds_max':list(g.boundingBox().maxvec()),
            'finite_positions':bool(np.isfinite(xyz).all()),'uv_present':bool(uv.size),'uv_finite':bool(uv.size and np.isfinite(uv).all()),
            'normals_present':bool(ns),'normals_finite':bool(ns and np.isfinite(ns).all()),
            'position_hash':hashlib.sha256(xyz.tobytes()).hexdigest(),
            'uv_hash':hashlib.sha256(uv.tobytes()).hexdigest(),
            'exact_duplicate_polygons':count('qa_remove_duplicate'),'degenerate_polygons':count('qa_remove_degenerate'),
            'material_families':dict(collections.Counter(g.primStringAttribValues('material_family')))}

def main():
    b=B.build();graph=audit(b)
    qaobj=hou.node('/obj').createNode('geo','QA_ONLY',run_init_scripts=False);qaobj.setDisplayFlag(False)
    stash=qaobj.createNode('stash','actual_geometry');mask=qaobj.createNode('attribwrangle','exact_face_audit');mask.setInput(0,stash)
    mask.parm('class').set(0);mask.parm('vex_precision').set('64');mask.parm('snippet').set((ROOT/'vex/mark_duplicate_degenerate.vfl').read_text(encoding='utf-8'))
    results=[]
    files=[ROOT/'datasets/baseline.json']+sorted(p for p in (ROOT/'datasets').glob('*.json') if p.stem!='baseline')
    for file in files:
        start=time.perf_counter();cfg=json.loads(file.read_text(encoding='utf-8'));B.apply_config(b,cfg)
        row={'variant_id':cfg['variant_id'],'config':str(file.relative_to(ROOT)).replace('\\','/'),'levels':{}}
        for quality in ('low','high'):
            output=b.node('OUTPUT/detail_levels/OUT_'+quality);output.cook(force=True);g=output.geometry()
            data=metrics(g,(stash,mask));path=ROOT/'geometry'/(file.stem+'_'+quality+'.bgeo.sc');g.saveToFile(str(path))
            data.update({'file':str(path.relative_to(ROOT)).replace('\\','/'),'sha256':sha(path)})
            row['levels'][quality]=data
        raw=b.node('ROOF/patch_surfaces/OUT_raw_surfaces').geometry();top=b.node('ROOF/ridge_valley_solver/OUT_roof_top').geometry()
        visible=b.node('ROOF/surfaces_tiles/tile_candidates_on_resolved_domain').geometry()
        placement=b.node('ROOF/dormers/DORMER_PLACEMENT_DATA').geometry()
        dormers=[{key:p.attribValue(key) for key in ('roof_patch_id','dormer_depth','base_y','front_host_y','ridge_clearance')} for p in placement.points()]
        row['roof']={'raw_faces':len(raw.prims()),'top_faces':len(top.prims()),'top_dimensions':list(top.boundingBox().sizevec()),
                     'max_source_surface_error_m':max(top.primFloatAttribValues('roof_surface_error'),default=0),
                     'raw_faces_mixing_roles':sum(len({p.attribValue('roof_role') for p in face.points()})>1 for face in raw.prims()),
                     'raw_patch_ids':dict(collections.Counter(raw.primStringAttribValues('roof_patch_id'))),
                     'visible_tile_points':dict(collections.Counter(visible.pointStringAttribValues('roof_patch_id'))),
                     'dormers':dormers}
        canopy_domains={};canopy_tiles={}
        for path in ('DETAILS/site','OPENINGS/attachments'):
            canopy_domains[path]=len(b.node(path+'/canopy_roof_domain').geometry().prims())
            canopy_tiles[path]=len(b.node(path+'/copy_canopy_tiles').geometry().prims())
        row['canopy']={'roof_faces':canopy_domains,'tile_faces':canopy_tiles}
        row.update(node_messages(b));low,high=row['levels']['low'],row['levels']['high']
        row['high_to_low_primitive_ratio']=high['primitives']/low['primitives']
        roofok=row['roof']['top_faces']>0 and row['roof']['max_source_surface_error_m']<1e-4 and not row['roof']['raw_faces_mixing_roles']
        roofok=roofok and all(d['ridge_clearance']>.08 and .079<d['front_host_y']-d['base_y']<.081 and d['dormer_depth']>.1 for d in dormers)
        roofok=roofok and all(row['roof']['visible_tile_points'].get(k,0)>0 for k in row['roof']['raw_patch_ids'])
        row['checks']={'roof_domain':roofok,'high_is_distinct_geometry':high['primitives']>low['primitives']*2,
                       'solid_canopy_has_tiles':all(not count or canopy_tiles[key]>0 for key,count in canopy_domains.items()),
                       'bounds_preserved_in_high':bool(np.allclose(low['dimensions'],high['dimensions'],atol=.001)),
                       'finite_geometry_uv_normals':all(all(d[k] for k in ('finite_positions','uv_present','uv_finite','normals_present','normals_finite')) for d in (low,high)),
                       'no_exact_defective_faces':all(not(d['exact_duplicate_polygons'] or d['degenerate_polygons']) for d in (low,high)),
                       'no_cook_errors':not row['errors']}
        row['status']='pass' if all(row['checks'].values()) else 'fail';row['seconds']=time.perf_counter()-start
        b.parm('preview_quality').set('high');b.setDisplayFlag(True);b.setCurrent(True,clear_all_selected=True)
        for obj in hou.node('/obj').children():
            if obj.type().name()=='geo':obj.setDisplayFlag(obj==b.parent())
        scene=ROOT/'scenes'/(file.stem+'.hip');hou.hipFile.save(str(scene));row['scene']=str(scene.relative_to(ROOT)).replace('\\','/')
        results.append(row);write_json(ROOT/'results/variants.json',results)
        print(file.stem,row['status'],low['primitives'],high['primitives'],row['checks'],flush=True)
    qaobj.destroy();B.apply_config(b,json.loads(files[0].read_text(encoding='utf-8')))
    hou.hipFile.save(str(ROOT/'scenes/medieval_building_roof_high_v3.hip'))
    summary={'houdini':hou.applicationVersionString(),'license':str(hou.licenseCategory()),'variant_count':15,'baseline_count':1,
             'geometry_exports':len(results)*2,'graph_status':graph['graph_modularity'],'status':'pass' if all(r['status']=='pass' for r in results) and graph['graph_modularity']=='pass' else 'fail',
             'failed':[{k:r[k] for k in ('variant_id','checks')} for r in results if r['status']!='pass'],
             'screenshots':'separate GUI pass; this runner never renders images'}
    write_json(ROOT/'results/geometry_summary.json',summary);print(summary,flush=True)
    return summary['status']!='pass'

if __name__=='__main__':raise SystemExit(main())
