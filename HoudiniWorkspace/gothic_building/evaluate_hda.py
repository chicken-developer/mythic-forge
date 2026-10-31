"""Batch: Python --offline; hython default cooks + exports. --render requires numpy/Pillow."""
import argparse,json,time,sys,hashlib,math,traceback,platform
from pathlib import Path
import build_hda as B

def metrics(m):
    v=m.vertices;seen=set();duplicate=0;degenerate=0
    for face in m.faces:
        key=tuple(sorted(tuple(round(x,8) for x in v[i]) for i in face))
        duplicate+=key in seen;seen.add(key)
        n=B.cross(B.sub(v[face[1]],v[face[0]]),B.sub(v[face[2]],v[face[0]]));degenerate+=sum(x*x for x in n)<1e-18
    return dict(points=len(v),primitives=len(m.faces),bounds_min=[min(x[i] for x in v) for i in range(3)],bounds_max=[max(x[i] for x in v) for i in range(3)],finite=all(math.isfinite(x) for p in v for x in p),duplicate_faces=duplicate,degenerate_faces=degenerate,mesh_sha256=B.mesh_hash(m))
def render_one(root,out,record,mesh,camera,reference):
    from render_geometry import render,compare
    hero=out/record['outputs']['hero']
    render(mesh,hero,camera,record['variant_id']+' | '+record['label'])
    compare(root/reference,hero,out/record['outputs']['comparison'],record['variant_id']+' | '+record['label'])
    return True

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--manifest',default='manifest.json');parser.add_argument('--output-dir',default='.');parser.add_argument('--offline',action='store_true');parser.add_argument('--render',action='store_true');parser.add_argument('--render-workers',type=int,default=1);args=parser.parse_args()
    if args.render_workers<1 or args.render_workers>4:parser.error('--render-workers must be 1..4')
    if not args.offline and args.render_workers!=1:parser.error('Parallel offline renderer requires --offline')
    root=Path(__file__).resolve().parent;manifest=Path(args.manifest);manifest=manifest if manifest.is_absolute() else root/manifest;data=json.loads(manifest.read_text());root=manifest.parent
    out=Path(args.output_dir);out=out if out.is_absolute() else root/out;out.mkdir(parents=True,exist_ok=True)
    records=[json.loads((root/p).read_text()) for p in [data['baseline']]+data['datasets']+data.get('supplemental_sweeps',[])]
    refs={r['reference_id']:r for r in data['references']+data.get('supplemental_references',[])};sha=hashlib.sha256((root/'build_hda.py').read_bytes()).hexdigest();meshlist=[];results=[];failed=False
    compile((root/'build_hda.py').read_text(),'build_hda.py','exec');B.embedded_source()
    for record in records:
        id=record['variant_id'];st=time.perf_counter();res=dict(variant_id=id,effective_config=B.normalize(record),generator_sha256=sha,runtime=dict(python=platform.python_version(),houdini=None),stages=dict(code_generated=True,static_checked=True,houdini_cooked='not_run',geometry_exported=False,rendered=False,hda_packaged='not_run'),failures=[],image_provenance='offline_geometry_preview',houdini_cook_seconds=None)
        try:
            mesh=B.generate(record);res['metrics']=metrics(mesh);res['core_build_seconds']=time.perf_counter()-st;res['spatial_layout']=getattr(mesh,'spatial_layout',None);res['observed_features']=mesh.features;res['feature_count_scope']='constructed before interface clipping';res['deterministic']=B.mesh_hash(B.generate(record))==res['metrics']['mesh_sha256']
            from assembly_validation import check
            res['assembly_audit']=check(mesh,record)
            if res['assembly_audit']['residual_intrusions']:raise ValueError('Residual surface in another assembly envelope: '+str(res['assembly_audit']['examples']))
            if not res['metrics']['finite'] or res['metrics']['degenerate_faces']:raise ValueError('Nonfinite or degenerate geometry')
            path=out/record['outputs']['geometry'];B.write_obj(mesh,path)
            # Verify serialization count, finite data and connectivity bounds.
            vv=ff=0
            for line in path.read_text().splitlines():
                if line.startswith('v '):vv+=1;assert all(math.isfinite(float(x)) for x in line.split()[1:])
                elif line.startswith('f '):ff+=1;assert all(1<=int(x)<=len(mesh.vertices) for x in line.split()[1:])
            assert vv==len(mesh.vertices) and ff==len(mesh.faces)
            res['stages']['geometry_exported']=True;res['geometry_origin']='pure_python_core_obj'
            if not args.offline:
                import hou
                start=time.perf_counter();node=B.build(record);node.cook(force=True);res['runtime']['houdini']=hou.applicationVersionString();res['houdini_cook_seconds']=time.perf_counter()-start
                res['houdini_errors']=list(node.errors());res['houdini_warnings']=list(node.warnings())
                if node.errors():raise RuntimeError(str(node.errors()))
                geo=node.geometry();geo.saveToFile(str(path.with_suffix('.bgeo.sc')))
                assert len(geo.points())==len(mesh.vertices) and len(geo.prims())==len(mesh.faces)
                res['stages']['houdini_cooked']=True
                # Exact topology/coordinates are checked before offline rendering of the core.
                assert all(sum((a-b)**2 for a,b in zip(point.position(),v))<1e-8 for point,v in zip(geo.points(),mesh.vertices))
                res['geometry_origin']='houdini_cook_verified_core'
            meshlist.append(mesh)
        except Exception as exc:
            failed=True;res['failures'].append(str(exc));meshlist.append(None);traceback.print_exc()
        results.append(res);print(id,res.get('metrics',{}).get('primitives'), 'failed' if res['failures'] else 'core_ok',flush=True)
    if args.render:
        from render_geometry import frame,render,compare
        camera=frame([m for m in meshlist if m]);data['camera']=camera
        from concurrent.futures import ProcessPoolExecutor
        import multiprocessing
        if args.render_workers>1:
            with ProcessPoolExecutor(max_workers=args.render_workers,mp_context=multiprocessing.get_context('spawn')) as pool:
                jobs=[(record,res,pool.submit(render_one,root,out,record,mesh,camera,refs[record['reference_ids'][0]]['path'])) for record,mesh,res in zip(records,meshlist,results) if mesh is not None]
                for record,res,future in jobs:
                    try:
                        future.result();res['stages']['rendered']=True;res['camera']=camera;print('rendered',record['variant_id'],flush=True)
                    except Exception as exc:failed=True;res['failures'].append('render: '+str(exc))
        else:
            for record,mesh,res in zip(records,meshlist,results):
                if mesh is None:continue
                try:
                    render_one(root,out,record,mesh,camera,refs[record['reference_ids'][0]]['path']);res['stages']['rendered']=True;res['camera']=camera;print('rendered',record['variant_id'],flush=True)
                except Exception as exc:failed=True;res['failures'].append('render: '+str(exc))
    for record,res in zip(records,results):
        path=out/record['outputs']['result'];path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(res,indent=2))
    (out/'results'/'batch_summary.json').write_text(json.dumps(dict(records=len(results),failures=sum(bool(r['failures']) for r in results),results=[dict(variant_id=r['variant_id'],stages=r['stages']) for r in results],camera=data['camera']),indent=2))
    return 1 if failed else 0
if __name__=='__main__':raise SystemExit(main())
