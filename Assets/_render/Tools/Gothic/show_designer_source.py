"""Open a delivered material graph without replacing the loaded package."""
import sd
from sd.api.sbs.sdsbscompgraph import SDSBSCompGraph
app=sd.getContext().getSDApplication();pm=app.getPackageMgr()
p=pm.getUserPackageFromFilePath('E:/Repos/mythic-forge/Assets/texture_gothic/Gothic_Stylized.sbs')
assert p
g=next(r for r in p.getChildrenResources(True) if isinstance(r,SDSBSCompGraph) and r.getIdentifier()=='Wall_Plaster')
app.getUIMgr().openResourceInEditor(g)
app.getUIMgr().setExplorerSelection(g)
