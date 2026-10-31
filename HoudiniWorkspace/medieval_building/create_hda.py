"""Run with hython. --check-only also works with normal Python (stdlib only)."""
from pathlib import Path
import argparse,datetime,hashlib,json,sys,time,traceback
import build_hda as B
ROOT=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--preset',default='01_townhouse_base')
    ap.add_argument('--output-dir',type=Path)
    ap.add_argument('--all-presets',action='store_true')
    ap.add_argument('--check-only',action='store_true')
    args=ap.parse_args()
    presets={p.stem:B.normalize(json.loads(p.read_text())) for p in sorted((ROOT/'assets/datasets').glob('*.json'))}
    # A new packaged instance has no blockout input yet. Raw dataset files stay unchanged.
    for config in presets.values():config['parameters']['layout_input_enable']=False
    if args.preset not in presets:ap.error('Unknown preset: '+args.preset)
    compile(B._source()+'\n_cook()\n','<embedded_generator>','exec')
    if args.check_only:
        print(json.dumps({'status':'static_checks_passed','preset_count':len(presets),'generator':B.VERSION,'houdini_cooked':False},indent=2));return
    try:import hou
    except ImportError:raise SystemExit('Run this script with Houdini hython, or use --check-only with normal Python.')
    if hou.isUIAvailable():raise SystemExit('Use a separate hython process; this launcher saves its own scene.')
    if presets[args.preset]['parameters']['layout_input_enable']:
        raise SystemExit('Initial packaging needs a parameter-driven preset; connect blockout input afterward.')
    out=(args.output_dir or ROOT/'output'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S')).resolve()
    out.mkdir(parents=True,exist_ok=False)
    report={'runtime':hou.applicationVersionString(),'license':str(hou.licenseCategory()),'generator_sha256':hashlib.sha256((ROOT/'build_hda.py').read_bytes()).hexdigest(),'records':[],'status':'running'}
    try:
        node=B.build(presets[args.preset],asset_name='storybook_interior_handoff')
        asset,hda=B.export_asset(node,out/'Storybook_Building.hda')
        definition=asset.type().definition()
        definition.addSection('Presets.json',json.dumps(presets))
        definition.addSection('HandoffNotes.txt','Geometry generator v5.1. Native shaders, collision and Unreal bindings require validation. Textures remain in package assets/textures.')
        report.update(asset_file=hda,type_name=asset.type().name(),hda_packaged=True)
        chosen=presets if args.all_presets else {args.preset:presets[args.preset]}
        failed=False
        for key,cfg in chosen.items():
            t=time.perf_counter();record={'preset':key}
            try:
                B.apply_config(asset,cfg);asset.cook(force=True)
                errors=asset.errors()
                child=asset.node('construct_geometry')
                if child:errors+=child.errors()
                if errors:raise RuntimeError('\n'.join(errors))
                geo=asset.geometry()
                if not geo.points() or not geo.prims():raise RuntimeError('Empty geometry')
                geo.saveToFile(str(out/(key+'.bgeo.sc')))
                record.update(status='passed',points=len(geo.points()),primitives=len(geo.prims()),geometry_sha256=geo.attribValue('geometry_sha256'),warnings=list(asset.warnings()))
            except Exception as e:record.update(status='failed',error=str(e));failed=True
            record['cook_and_export_seconds']=time.perf_counter()-t;report['records'].append(record)
        B.apply_config(asset,presets[args.preset]);asset.cook(force=True)
        suffix={'.hda':'.hip','.hdalc':'.hiplc','.hdanc':'.hipnc'}[Path(hda).suffix]
        scene=out/('Storybook_Building'+suffix);hou.hipFile.save(str(scene));report['scene_file']=str(scene)
        report['status']='failed' if failed else 'passed'
        report['fresh_process_portability']='pending: run verify_hda.py'
        if failed:raise RuntimeError('One or more preset cooks failed; inspect build_report.json')
    except Exception:
        report['status']='failed';report['traceback']=traceback.format_exc();raise
    finally:(out/'build_report.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
