from pathlib import Path
import json
import math
import hashlib
import numpy as np
import hou
from PySide6 import QtWidgets,QtCore
ROOT=Path(__file__).resolve().parent
window=hou.qt.mainWindow()
if window.isMinimized():window.showNormal()
window.raise_();window.activateWindow();QtWidgets.QApplication.processEvents()
old_network=hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
if old_network and old_network.pane().isMaximized():old_network.pane().setIsMaximized(False)
scene=ROOT/job.get('scene','scenes/medieval_building_roof_high_v3.hip')
current_hda=ROOT/'asset/medieval_building_modular.hda'
asset_hash=hashlib.sha256(current_hda.read_bytes()).hexdigest()
if not job.get('asset_path') and getattr(hou.session,'_tdk_review_hda_hash',None)!=asset_hash:
    hou.hda.installFile(str(current_hda));hou.hda.reloadFile(str(current_hda))
    for definition in hou.hda.definitionsInFile(str(current_hda)):definition.setIsPreferred(True)
    hou.session._tdk_review_hda_hash=asset_hash
hou.hipFile.load(str(scene),suppress_save_prompt=True,ignore_load_warnings=True)
b=hou.node(job.get('asset_path','/obj/medieval_roof_high_v3/BUILDING'))
if b is None:raise RuntimeError('Missing review HDA')
record=json.loads((ROOT/'datasets'/(job.get('dataset','baseline')+'.json')).read_text(encoding='utf-8'))
for k,v in dict(record['parameters'],seed_decoration=record['seeds']['decoration']).items():b.parm(k).set(int(v) if isinstance(v,bool) else v)
if b.parm('preview_quality'):b.parm('preview_quality').set(job.get('quality','high'))
for obj in hou.node('/obj').children():
    if obj.type().name()=='geo':obj.setDisplayFlag(obj==b.parent())
b.setDisplayFlag(True);b.cook(force=True)
g=b.geometry()
position_hash=hashlib.sha256(np.asarray(g.pointFloatAttribValues('P'),dtype=np.float64).tobytes()).hexdigest()
if not job.get('asset_path'):
    rows=json.loads((ROOT/'results/variants.json').read_text(encoding='utf-8'))
    expected=next(row for row in rows if row['variant_id']==record['variant_id'])['levels'][job.get('quality','high')]
    if position_hash!=expected['position_hash']:raise RuntimeError('GUI geometry does not match the validated LOW/HIGH export')
target=hou.Vector3(job.get('target',(1.,4.,0.)))
view=job.get('view','hero')
delta=hou.Vector3(job.get('delta',(17.,11.,-21.) if view=='hero' else (-17.,11.,21.) if view=='rear' else (0.,30.,0.001)))
eye=target+delta;direction=target-eye
rot=(math.degrees(math.atan2(direction[1],math.hypot(direction[0],direction[2]))),math.degrees(math.atan2(-direction[0],-direction[2])),0.)
cam=hou.node('/obj').createNode('cam','GUI_REVIEW_CAMERA');cam.parmTuple('t').set(eye);cam.parmTuple('r').set(rot)
cam.parm('projection').set('ortho');cam.parm('orthowidth').set(job.get('width',21.))
viewer=hou.ui.paneTabOfType(hou.paneTabType.SceneViewer);viewer.setPwd(b.parent());viewer.curViewport().setCamera(cam)
viewer.curViewport().settings().setDisplayOrthoGrid(False)
viewer.curViewport().settings().setAmbientOcclusion(True)
b.setCurrent(True,clear_all_selected=True)
network=hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
if network:
    destination=b.node('OUTPUT/detail_levels') if job.get('network')=='detail_levels' else b if job.get('network') else b.parent()
    network.setPwd(destination)
    if job.get('network'):
        if job.get('network_full'):network.pane().setIsMaximized(True);QtWidgets.QApplication.processEvents()
        nodes=destination.children();positions=[n.position() for n in nodes]
        bounds=hou.BoundingRect(hou.Vector2(min(p[0] for p in positions)-1,min(p[1] for p in positions)-1),hou.Vector2(max(p[0] for p in positions)+5,max(p[1] for p in positions)+2))
        network.setVisibleBounds(bounds)
    else:network.homeToSelection()
if job.get('wire'):
    viewer.curViewport().settings().displaySet(hou.displaySetType.DisplayModel).setShadedMode(hou.glShadingType.SmoothWire)
else:
    viewer.curViewport().settings().displaySet(hou.displaySetType.DisplayModel).setShadedMode(hou.glShadingType.Smooth)
pane=viewer.qtWindow()
previous=getattr(hou.session,'_tdk_quality_label',None)
if previous:previous.deleteLater()
label=QtWidgets.QLabel(pane);label.setText(record['variant_id']+'  |  '+job.get('label',job.get('quality','high').upper())+'  |  '+str(g.intrinsicValue('primitivecount'))+' primitives  |  LIVE HOUDINI GUI')
label.setStyleSheet('background-color:rgba(20,20,20,190);color:white;font:12px;padding:7px;');label.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents)
label.adjustSize();label.move(55,55);label.show();hou.session._tdk_quality_label=label
hou.ui.setStatusMessage(label.text())
if job.get('save_scene'):
    hou.hipFile.save(str(ROOT/job['save_scene']))
definition=b.type().definition()
definition_path=Path(definition.libraryFilePath())
published_hash=asset_hash if not job.get('asset_path') else hashlib.sha256(definition_path.read_bytes()).hexdigest() if definition_path.is_file() else None
result={'variant_id':record['variant_id'],'quality':b.parm('preview_quality').evalAsString() if b.parm('preview_quality') else 'legacy_low',
        'points':g.intrinsicValue('pointcount'),'primitives':g.intrinsicValue('primitivecount'),
        'display_node':b.path(),'camera':{'t':list(eye),'r':rot,'orthowidth':cam.parm('orthowidth').eval()},
        'ui':hou.isUIAvailable(),'scene':str(scene),'wire':bool(job.get('wire')),'position_hash':position_hash,'hda_sha256':published_hash,
        'hda_type':b.type().name(),'definition_library':definition.libraryFilePath(),
        'window_minimized':window.isMinimized(),'window_visible':window.isVisible(),'hud_text':label.text()}
