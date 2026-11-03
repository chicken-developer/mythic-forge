"""Evaluate baseline and ALL preserved records with real Houdini cooks."""
import argparse
import json
import shutil
import sys
import traceback
import hashlib
import hou
import build_hda as B
from hda_builder.evidence import ROOT,write_json,sha,cook_record,geometry_metrics
from hda_builder.graph_audit import audit

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--ids',nargs='*');ap.add_argument('--render',action='store_true');args=ap.parse_args()
    manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    records=[json.loads((ROOT/p).read_text(encoding='utf-8')) for p in ['datasets/baseline.json']+manifest['datasets']]
    if args.ids:records=[r for r in records if r['variant_id'] in args.ids]
    building=B.build();graph=audit(building)
    generator_files=[ROOT/'build_hda.py',ROOT/'parameter_schema.json',ROOT/'results/source_inventory.json',ROOT/'scenes/source_snapshot.hip']+list((ROOT/'hda_builder').rglob('*.py'))+list((ROOT/'vex').glob('*.vfl'))
    generator_tree_hash=hashlib.sha256(json.dumps({p.relative_to(ROOT).as_posix():sha(p) for p in sorted(generator_files)},sort_keys=True).encode()).hexdigest()
    print('GRAPH',graph['graph_modularity'],flush=True)
    rows=[]
    for record in records:
        ident=record['variant_id']
        try:
            B.apply_config(building,record)
            row=cook_record(building,building.node('OUT_BUILDING'),record,f'geometry/after/{ident}.bgeo.sc')
            before=json.loads((ROOT/f'results/before/{ident}.json').read_text(encoding='utf-8'))
            comparable=['position_hash','topology_hash','name_hash','uv_hash','material_hash']
            row['before_after']={key:row['metrics'][key]==before['metrics'][key] for key in comparable}
            row['geometry_parity']='pass' if all(row['before_after'].values()) else 'fail'
            old=hou.Geometry();old.loadFromFile(str(ROOT/f'geometry/before/{ident}.bgeo.sc'))
            old_metrics=geometry_metrics(old)
            row['intentional_corrections']=['Remove coincident duplicate faces and zero-area polygons; no cross-part welding.',
                                            'Apply isometric UV coordinates to sloped faces; retain axis-aligned trim orientation.']
            m=row['metrics']
            row['valid_surface_parity']=m['valid_unique_surface_hash']==old_metrics['valid_unique_surface_hash']
            row['dimensions_preserved']=all(abs(a-b)<1e-5 for a,b in zip(m['dimensions'],old_metrics['dimensions']))
            row['geometry_acceptance']='pass' if row['cook_status']=='pass' and row['valid_surface_parity'] and row['dimensions_preserved'] and not m['degenerate_polygons'] and not m['exact_duplicate_polygons'] and m['uv_finite'] and m['normals_finite'] else 'fail'
            pure=json.loads((ROOT/f'results/pure_refactor/records/{ident}.json').read_text(encoding='utf-8'))
            row['pure_refactor_parity']=pure['geometry_parity']
            from hda_builder.corrections import EXTENSIONS
            row['effective_extensions']={key:building.parm(key).eval() for key in EXTENSIONS}
            row['generator_tree_sha256']=generator_tree_hash
            all_ids=set(s['module_id'] for s in __import__('hda_builder.registry',fromlist=['contracts']).contracts()['modules'])
            producer_ids=set(building.geometry().primStringAttribValues('module_id'))
            row['producer_module_audit']={'module_ids':sorted(producer_ids),'unknown_ids':sorted(producer_ids-all_ids),'status':'pass' if producer_ids and not (producer_ids-all_ids) and 'output.game_export' not in producer_ids else 'fail'}
            if row['producer_module_audit']['status']!='pass':row['geometry_acceptance']='fail'
            from check_regressions import opening_relations
            relations=opening_relations(building)
            row['opening_relation_audit']={'tested':len(relations),'blocked':[r['opening_id'] for r in relations if r['ray_blocked_by_wall']],
                                            'missing_wall_ids':[r['opening_id'] for r in relations if not r['wall_id']],
                                            'openings':relations}
            row['opening_relation_audit']['status']='pass' if relations and not row['opening_relation_audit']['blocked'] and not row['opening_relation_audit']['missing_wall_ids'] else 'fail'
            if row['opening_relation_audit']['status']!='pass':row['geometry_acceptance']='fail'
            row['generator_sha256']=sha(ROOT/'build_hda.py');row['hda_sha256']=sha(ROOT/'asset/medieval_building_modular.hda')
            row['dataset_sha256']=sha(ROOT/f'datasets/{ident}.json')
            row['stages']={'code_generated':'pass','static_checked':'pending_static','houdini_cooked':row['cook_status'],
                           'geometry_exported':'pass','rendered':'not_run','hda_packaged':'pending_portability'}
            row['technical_issues']={k:row['metrics'][k] for k in ('degenerate_polygons','exact_duplicate_polygons') if row['metrics'][k]}
            row['visual_fidelity']='pending_render_review'
            row['structural_qa']='see opening_relation_audit; roof/support visual inspection recorded separately'
            (ROOT/'geometry').mkdir(exist_ok=True)
            shutil.copy2(ROOT/row['geometry'], ROOT/record['outputs']['geometry'])
            scene=ROOT/f'scenes/variants/{ident}.hip';scene.parent.mkdir(exist_ok=True)
            hou.hipFile.save(str(scene));row['scene']=str(scene.relative_to(ROOT)).replace('\\','/')
            if ident=='baseline':shutil.copy2(scene,ROOT/'scenes/medieval_building_modular_v2.hip')
            write_json(ROOT/f'results/after/{ident}.json',row);write_json(ROOT/record['outputs']['result'],row)
            print('AFTER',ident,'cook',row['cook_status'],'acceptance',row['geometry_acceptance'],'surface_parity',row['valid_surface_parity'],row['technical_issues'],flush=True)
        except Exception as exc:
            row={'variant_id':ident,'cook_status':'fail','exception':str(exc),'traceback':traceback.format_exc()}
            write_json(ROOT/f'results/after/{ident}.json',row);print('FAIL',ident,str(exc),flush=True)
        rows.append({'variant_id':ident,'cook_status':row['cook_status'],'geometry_parity':row.get('geometry_parity','not_run'),
                     'geometry_acceptance':row.get('geometry_acceptance','not_run')})
        write_json(ROOT/'results/evaluation_summary.json',{'houdini':hou.applicationVersionString(),'graph_modularity':graph['graph_modularity'],'records':rows})
    if args.render:
        import subprocess
        subprocess.run([str(__import__('pathlib').Path(hou.expandString('$HFS'))/'bin/hython.exe'),str(ROOT/'run_render_batch.py'),'--phase','after'],check=True)
    return int(graph['graph_modularity']!='pass' or any(r['cook_status']!='pass' or r['geometry_acceptance']!='pass' for r in rows))

if __name__=='__main__':sys.exit(main())
