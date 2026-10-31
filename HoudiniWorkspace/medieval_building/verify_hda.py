"""Fresh-process verification; imports no authoring generator. Run with hython."""
from pathlib import Path
import argparse,json,time

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('asset',type=Path);ap.add_argument('--preset',default='01_townhouse_base');ap.add_argument('--report',type=Path,required=True);args=ap.parse_args()
    if args.report.exists():raise SystemExit('Report already exists; use a new path.')
    import hou
    path=args.asset.resolve();hou.hda.installFile(str(path));definitions=hou.hda.definitionsInFile(str(path))
    if len(definitions)!=1:raise RuntimeError('Expected exactly one HDA definition')
    d=definitions[0];presets=json.loads(d.sections()['Presets.json'].contents());cfg=presets[args.preset]
    obj=hou.node('/obj').createNode('geo','verify_storybook',run_init_scripts=False);node=obj.createNode(d.nodeTypeName(),'building')
    node.setParms(dict(cfg['parameters'],layout_seed=cfg['seeds']['layout'],decoration_seed=cfg['seeds']['decoration']))
    start=time.perf_counter();node.cook(force=True)
    if node.errors():raise RuntimeError('\n'.join(node.errors()))
    geo=node.geometry()
    if not geo.prims():raise RuntimeError('Empty geometry')
    result=dict(status='passed',runtime=hou.applicationVersionString(),asset=str(path),preset=args.preset,points=len(geo.points()),primitives=len(geo.prims()),geometry_sha256=geo.attribValue('geometry_sha256'),elapsed_seconds=time.perf_counter()-start,authoring_generator_imported=False)
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
if __name__=='__main__':main()
