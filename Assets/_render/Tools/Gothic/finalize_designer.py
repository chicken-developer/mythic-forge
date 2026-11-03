"""Add readable graph frames and open the delivered source in Designer."""
from pathlib import Path
import sd,json
from sd.api.sdgraphobjectframe import SDGraphObjectFrame
from sd.api.sdbasetypes import float2,ColorRGBA
from sd.api.sbs.sdsbscompgraph import SDSBSCompGraph
SRC=(Path(__file__).resolve().parents[4] / 'Assets/texture_gothic')
app=sd.getContext().getSDApplication();pm=app.getPackageMgr()
for old in list(pm.getUserPackages()):
    if Path(old.getFilePath()).name=='Gothic_Stylized.sbs':pm.unloadUserPackage(old)
p=pm.loadUserPackage(str(SRC/'Gothic_Stylized.sbs'),True)
graphs={r.getIdentifier():r for r in p.getChildrenResources(True) if isinstance(r,SDSBSCompGraph)}
assert len(graphs)==10,list(graphs)
specs=json.loads((SRC/'material_manifest.json').read_text())
for spec in specs['graphs']:
    g=graphs[spec['name']]
    for o in list(g.getGraphObjects()):
        if isinstance(o,SDGraphObjectFrame) and o.getTitle().startswith('GOTHIC |'):g.deleteGraphObject(o)
    for title,pos,size,color in [
        ('GOTHIC | 01 - Patterns and surface wear',(-1140,-180),(1250,1600),(.19,.26,.29,1.)),
        ('GOTHIC | 02 - PBR response / DirectX / ORM',(160,-180),(770,1280),(.28,.25,.19,1.)),
        ('GOTHIC | 03 - Export 2048 x 2048',(970,-180),(300,1790),(.20,.29,.25,1.))]:
        frame=SDGraphObjectFrame.sNew(g);frame.setTitle(title);frame.setPosition(float2(*pos));frame.setSize(float2(*size));frame.setColor(ColorRGBA(*color))
pm.savePackage(p)
g=graphs['Wall_Plaster']
app.getUIMgr().setExplorerSelection(g)
app.getUIMgr().openResourceInEditor(g)
(SRC/'build_error.txt').unlink(missing_ok=True)
