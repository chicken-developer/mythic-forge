"""Run one named catalog maintenance script in the open TDKProps editor."""
from pathlib import Path
from ctypes import wintypes
import ctypes
import json
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Gothic'))
import desktop as d

allowed={'probe_editor.py','import_catalog_assets.py','update_catalogs.py','capture_catalog_detail.py','capture_joint_detail.py','finish_catalog_review.py','editable_hda_catalog.py','skytown_catalog.py'}
name=sys.argv[1]
assert name in allowed
script=Path(__file__).resolve().parent/name
assert script.is_file()
previous_foreground=d.u.GetForegroundWindow()
h=d.focus('TDKProps - Unreal')
r=wintypes.RECT();d.u.GetWindowRect(h,__import__('ctypes').byref(r))
topmost=bool(d.u.GetWindowLongW(h,-20)&8)
current_thread=ctypes.windll.kernel32.GetCurrentThreadId()
threads={d.u.GetWindowThreadProcessId(h,None),d.u.GetWindowThreadProcessId(d.u.GetForegroundWindow(),None)}-{current_thread,0}
try:
    for thread in threads:d.u.AttachThreadInput(current_thread,thread,True)
    d.u.SetWindowPos(h,-1,0,0,0,0,3)
    d.u.BringWindowToTop(h);d.u.SetForegroundWindow(h);d.u.SetActiveWindow(h)
    # Use the editor's persistent Cmd field, which is independent of viewport console state.
    x=r.left+min(450,int((r.right-r.left)*.27));y=r.bottom-24
    point=wintypes.POINT(x,y);d.u.ScreenToClient(h,ctypes.byref(point))
    coordinates=(point.y<<16)|(point.x&0xffff)
    d.u.SetCursorPos(x,y)
    d.u.PostMessageW(h,0x200,0,coordinates)
    d.u.PostMessageW(h,0x201,1,coordinates);d.u.PostMessageW(h,0x202,0,coordinates)
    time.sleep(.3)
    for _ in range(400):d.u.PostMessageW(h,0x102,8,0)
    for ch in 'py '+script.as_posix():d.u.PostMessageW(h,0x102,ord(ch),0)
    d.u.PostMessageW(h,0x100,13,0);d.u.PostMessageW(h,0x101,13,0)
    time.sleep(1)
    for thread in threads:d.u.AttachThreadInput(current_thread,thread,False)
    threads.clear()
    request_path=script.parents[2]/'Saved/SkyTownReview/request.json'
    if name=='skytown_catalog.py' and request_path.exists():
        request=json.loads(request_path.read_text(encoding='utf-8-sig'))
        if request.get('action')=='capture':
            filename=request.get('filename',request.get('reference',request.get('family','capture'))+'_'+request.get('stage','after')+'.png')
            expected=str(script.parents[2]/'RenderTests/SkyTown'/filename)
            deadline=time.monotonic()+45.
            while time.monotonic()<deadline:
                try:
                    status=json.loads(request_path.with_name('status.json').read_text())
                    if status.get('file')==expected and status.get('status') in ('passed','failed'):break
                except (FileNotFoundError,json.JSONDecodeError):pass
                time.sleep(.2)
finally:
    for thread in threads:d.u.AttachThreadInput(current_thread,thread,False)
    if not topmost:d.u.SetWindowPos(h,-2,0,0,0,0,0x13)
    if previous_foreground and previous_foreground!=h:d.u.SetForegroundWindow(previous_foreground)
if d.u.GetForegroundWindow()==h:
    d.ImageGrab.grab(bbox=(r.left,r.top,r.right,r.bottom)).save('E:/Repos/mythic-forge/Assets/_render/Saved/PropsDetail/submit_once.png')
