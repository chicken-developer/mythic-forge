"""Syntax, original-data integrity and official skill validators; never geometry QA."""
from pathlib import Path
import ast
import argparse
import json
import subprocess
import sys
import hashlib
from hda_builder.evidence import ROOT,write_json,sha

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--require-images',action='store_true');args=parser.parse_args()
    files=[p for p in ROOT.rglob('*.py') if 'tooling' not in p.parts and '__pycache__' not in p.parts]
    errors=[]
    for p in files:
        try:ast.parse(p.read_text(encoding='utf-8'),filename=str(p))
        except SyntaxError as exc:errors.append(str(exc))
    repo=ROOT.parents[1]
    sources=json.loads((ROOT/'results/source_fingerprints.json').read_text(encoding='utf-8'))
    changed=[p for p,digest in sources.items() if not (repo/p).exists() or sha(repo/p)!=digest]
    mismatched=[p.name for p in (ROOT/'datasets').glob('*.json') if sha(p)!=sha(repo/'Assets/hda_medieval_building/datasets'/p.name)]
    commands=[['tooling/design-hda-building/scripts/validate_module_graph.py','module_graph.json']]
    commands += [['tooling/design-hda-building/scripts/validate_dataset.py',f'manifest_group_{i}.json']+(['--require-images'] if args.require_images else []) for i in range(1,4)]
    validators=[]
    for command in commands:
        command=[str(ROOT/p) if not p.startswith('--') else p for p in command]
        result=subprocess.run([sys.executable]+command,capture_output=True,text=True,encoding='utf-8',errors='replace')
        validators.append({'args':command,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
    from hda_builder.corrections import EXTENSIONS
    write_json(ROOT/'extension_schema.json',{'schema_version':1,'config_key':'extensions','parameters':EXTENSIONS,
                                          'migration':'Original parameters and datasets remain byte-identical; optional added controls use embedded UI defaults.'})
    result={'status':'pass' if not errors and not changed and not mismatched and all(v['exit_code']==0 for v in validators) else 'fail',
            'python_files_compiled':len(files),'syntax_errors':errors,'original_source_files_checked':len(sources),
            'changed_originals':changed,'dataset_copy_mismatches':mismatched,'validators':validators,
            'require_images':args.require_images,'scope':'Static only; Houdini cook, rendering, portability and Unreal status have separate evidence.'}
    write_json(ROOT/'results/static_checks.json',result)
    print('STATIC',result['status'],'files',len(files),'originals_unchanged',not changed,flush=True)
    for v in validators:print(v['exit_code'],v['stdout'].strip(),v['stderr'].strip(),flush=True)
    return result['status']!='pass'

if __name__=='__main__':raise SystemExit(main())
