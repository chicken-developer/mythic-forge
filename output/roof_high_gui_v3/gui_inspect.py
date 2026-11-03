import hou
from pathlib import Path
import json
from PySide6 import QtWidgets
ROOT=Path(__file__).resolve().parent
hou.hipFile.load(str(ROOT.parent/'modular_rebuild_v2/scenes/variants/06_tavern.hip'),suppress_save_prompt=True,ignore_load_warnings=True)
b=hou.node('/obj/medieval_modular_v2/BUILDING')
viewer=hou.ui.paneTabOfType(hou.paneTabType.SceneViewer)
viewer.setPwd(b.parent());b.setCurrent(True,clear_all_selected=True)
viewer.curViewport().frameBoundingBox(b.geometry().boundingBox())
hou.ui.setStatusMessage('V2 BEFORE | 06_tavern | inspecting dormer / wing roofs')
result={'ui':hou.isUIAvailable(),'node':b.path(),'viewport':viewer.name(),
        'viewer_methods':[n for n in dir(viewer) if any(k in n.lower() for k in ('qt','camera','display','viewport'))],
        'viewport_methods':[n for n in dir(viewer.curViewport()) if any(k in n.lower() for k in ('camera','view','save'))],
        'controls':{k:b.parm(k).eval() for k in ('detail_level','mesh_quality','footprint','roof_axis','dormer_count')},
        'bounds':list(b.geometry().boundingBox().sizevec()),'primitives':b.geometry().intrinsicValue('primitivecount')}
