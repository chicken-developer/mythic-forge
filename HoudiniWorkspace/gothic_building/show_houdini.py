"""Focus the native graph after the canonical HIP has loaded and the UI is ready."""
import hou

asset = hou.node("/obj/gothic_building_v2_1/generator")
if asset is None:
    raise RuntimeError("Gothic Building generator is missing")
asset.cook(force=True)
asset.setCurrent(True,clear_all_selected=True)
for pane in hou.ui.paneTabs():
    if pane.type()==hou.paneTabType.SceneViewer:
        pane.setPwd(asset.parent())
        viewport=pane.curViewport()
        camera=viewport.defaultCamera()
        camera.setRotation(hou.Matrix3(hou.hmath.buildRotateLookAt(
            hou.Vector3(-.72,.40,-.65),hou.Vector3(0,0,0),hou.Vector3(0,1,0))))
        viewport.setDefaultCamera(camera)
        viewport.frameBoundingBox(asset.geometry().boundingBox())
        viewport.settings().setLighting(hou.viewportLighting.Headlight)
        for display_type in (hou.displaySetType.DisplayModel,hou.displaySetType.CurrentModel):
            display=viewport.settings().displaySet(display_type)
            display.setShadedMode(hou.glShadingType.Smooth)
            display.useGhostedLook(False)
        pane.enterViewState()
    elif pane.type()==hou.paneTabType.NetworkEditor:
        target = asset.node("construction/generated") or asset
        pane.setPwd(target)
        if hasattr(pane,"homeToItems"):
            pane.homeToItems(target.children())
        else:
            pane.homeToSelection()
    elif pane.type()==hou.paneTabType.Parm:
        pane.setCurrentNode(asset)
hou.ui.setStatusMessage("Gothic Building ready. Dive into construction/generated to debug native SOP modules.",
                        severity=hou.severityType.Message)
