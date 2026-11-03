from pathlib import Path
import json
import collections
import numpy as np
import hou
from hda_builder.evidence import ROOT,write_json

obj=hou.node('/obj').createNode('geo','diagnostic',run_init_scripts=False)
for kind in ('clean','polydoctor','uvtexture','polybevel::3.0'):
    choices=[k for k in hou.sopNodeTypeCategory().nodeTypes() if k==kind or k.split('::')[0]==kind]
    print('CAPABILITY',kind,choices,flush=True)
    if not choices:continue
    n=obj.createNode(choices[-1])
    write_json(ROOT/f'results/capability_{kind.split("::")[0]}.json',{'type':n.type().name(),'parameters':{p.name():{'label':p.description(),'value':str(p.eval()),'menu':list(p.menuItems()) if p.parmTemplate().type()==hou.parmTemplateType.Menu else []} for p in n.parms()}})
g=hou.Geometry();g.loadFromFile(str(ROOT/'geometry/before/baseline.bgeo.sc'))
xyz=np.asarray(g.pointFloatAttribValues('P')).reshape(-1,3);seen={};issues=[]
for p in g.prims():
    ids=[v.point().number() for v in p.vertices()];pts=xyz[ids]
    area=np.linalg.norm(np.cross(pts,np.roll(pts,-1,axis=0)).sum(axis=0))*.5
    key=tuple(sorted(map(tuple,np.round(pts,5))))
    previous=seen.get(key)
    row={'prim':p.number(),'name':p.attribValue('name'),'material':p.attribValue('material_family'),'area':float(area)}
    if area<1e-10:issues.append(dict(row,issue='degenerate',positions=pts.tolist()))
    if previous:issues.append(dict(row,issue='duplicate',previous=previous))
    seen[key]=row
write_json(ROOT/'results/baseline_defects.json',issues)
print(collections.Counter((r['issue'],r['name'].split('/')[0]) for r in issues),flush=True)
# Open every provided reference and retain a labeled contact sheet for visual inspection.
from PIL import Image,ImageDraw
manifest=json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
refs=[r for r in manifest['references'] if r['role']=='variant']
for group in range(3):
    subset=refs[group*5:(group+1)*5]
    sheet=Image.new('RGB',(1500,1100),(225,220,209))
    for i,r in enumerate(subset):
        im=Image.open(ROOT/r['path']).convert('RGB');im.thumbnail((500,500))
        sheet.paste(im,((i%3)*500,(i//3)*550));ImageDraw.Draw(sheet).text(((i%3)*500+10,(i//3)*550+505),r['reference_id'],fill=(20,20,20))
    sheet.save(ROOT/f'references/review_group_{group+1}.png')
