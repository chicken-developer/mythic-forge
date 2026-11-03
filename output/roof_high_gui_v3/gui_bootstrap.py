"""File-queue automation inside a real visible Houdini GUI, never a ROP renderer."""
from pathlib import Path
import json
import sys
import traceback
import time
import os
import ctypes
from ctypes import wintypes
import hou
from PySide6 import QtCore,QtWidgets

ROOT=Path(os.environ['TDK_REVIEW_ROOT']).resolve()
(ROOT/'gui_commands').mkdir(exist_ok=True)
(ROOT/'gui_results').mkdir(exist_ok=True)
ctypes.windll.user32.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
ctypes.windll.user32.SetForegroundWindow.argtypes=[wintypes.HWND]
if str(ROOT) not in sys.path:sys.path.insert(0,str(ROOT))

def write_result(ident,data):
    (ROOT/'gui_results'/f'{ident}.json').write_text(json.dumps(data,indent=2,default=str),encoding='utf-8')

def tick():
    if getattr(hou.session,'_tdk_gui_busy',False):return
    jobs=sorted((ROOT/'gui_commands').glob('*.json'))
    if not jobs:return
    jobfile=jobs[0]
    job=json.loads(jobfile.read_text(encoding='utf-8'))
    jobfile.rename(jobfile.with_suffix('.running'))
    hou.session._tdk_gui_busy=True
    try:
        if job.get('action')=='screenshot':
            window=hou.qt.mainWindow()
            if window.isMinimized():window.showNormal()
            window.raise_();window.activateWindow()
            if QtWidgets.QApplication.activeModalWidget():raise RuntimeError('Modal dialog obscures Houdini; capture refused')
            ctypes.windll.user32.SetWindowPos(int(window.winId()),-1,0,0,0,0,0x0043)
            ctypes.windll.user32.SetForegroundWindow(int(window.winId()))
            QtWidgets.QApplication.processEvents()
            target=ROOT/job['output'];target.parent.mkdir(parents=True,exist_ok=True)
            pixmap=window.screen().grabWindow(int(window.winId()))
            if pixmap.isNull():raise RuntimeError('Native GUI screenshot is empty')
            pixmap.save(str(target))
            if job.get('crop_viewport'):
                viewer=hou.ui.paneTabOfType(hou.paneTabType.SceneViewer);pane=viewer.qtWindow()
                origin=pane.mapToGlobal(QtCore.QPoint(0,0))-window.mapToGlobal(QtCore.QPoint(0,0));size=pane.size()
                rect=QtCore.QRect(origin,size).intersected(QtCore.QRect(0,0,pixmap.width(),pixmap.height()))
                if rect.isEmpty():raise RuntimeError('Viewport rectangle outside captured Houdini window')
                crop=pixmap.copy(rect)
                crop.save(str(target.with_name(target.stem+'_viewport.png')))
            ctypes.windll.user32.SetWindowPos(int(window.winId()),-2,0,0,0,0,0x0043)
            result={'path':str(target),'dimensions':[pixmap.width(),pixmap.height()],
                    'provenance':'houdini_gui_window_screenshot','window_title':window.windowTitle(),
                    'houdini':hou.applicationVersionString(),'ui_available':hou.isUIAvailable(),
                    'window_minimized':window.isMinimized(),'window_visible':window.isVisible(),
                    'hud_text':getattr(hou.session,'_tdk_quality_label',None).text() if getattr(hou.session,'_tdk_quality_label',None) else ''}
        else:
            script=(ROOT/job['script']).resolve()
            if not script.is_relative_to(ROOT):raise ValueError('Script outside owned package')
            scope={'__file__':str(script),'__name__':'__gui_job__','job':job}
            exec(compile(script.read_text(encoding='utf-8'),str(script),'exec'),scope)
            result=scope.get('result',{})
        write_result(job['id'],{'status':'pass','result':result})
    except Exception as exc:
        write_result(job['id'],{'status':'fail','error':str(exc),'traceback':traceback.format_exc()})
    finally:hou.session._tdk_gui_busy=False

window=hou.qt.mainWindow()
old_timer=getattr(hou.session,'_tdk_gui_timer',None)
if old_timer:old_timer.stop()
timer=QtCore.QTimer(window);timer.setInterval(250);timer.timeout.connect(tick);timer.start()
hou.session._tdk_gui_timer=timer
write_result('gui_ready',{'ui_available':hou.isUIAvailable(),'houdini':hou.applicationVersionString(),
                         'title':window.windowTitle(),'screens':[{'name':s.name(),'size':[s.size().width(),s.size().height()]} for s in QtWidgets.QApplication.screens()]})
