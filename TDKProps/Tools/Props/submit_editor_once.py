"""Run one named catalog maintenance script in the open TDKProps editor."""
from pathlib import Path
from ctypes import wintypes
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Gothic'))
import desktop as d

allowed={'probe_editor.py','import_catalog_assets.py','update_catalogs.py','capture_catalog_detail.py','capture_joint_detail.py','finish_catalog_review.py','editable_hda_catalog.py'}
name=sys.argv[1]
assert name in allowed
script=Path(__file__).resolve().parent/name
assert script.is_file()
h=d.focus('TDKProps - Unreal')
r=wintypes.RECT();d.u.GetWindowRect(h,__import__('ctypes').byref(r))
topmost=bool(d.u.GetWindowLongW(h,-20)&8)
try:
    d.u.SetWindowPos(h,-1,0,0,0,0,3)
    # Use the editor's persistent Cmd field, which is independent of viewport console state.
    d.click(r.left+min(450,int((r.right-r.left)*.27)),r.bottom-24)
    time.sleep(.4)
    d.u.PostMessageW(h,0x100,0x23,0);d.u.PostMessageW(h,0x101,0x23,0)
    for _ in range(400):d.u.PostMessageW(h,0x102,8,0)
    for ch in 'py '+script.as_posix():d.u.PostMessageW(h,0x102,ord(ch),0)
    d.u.PostMessageW(h,0x100,13,0);d.u.PostMessageW(h,0x101,13,0)
    time.sleep(1)
finally:
    if not topmost:d.u.SetWindowPos(h,-2,0,0,0,0,0x13)
d.ImageGrab.grab(bbox=(r.left,r.top,r.right,r.bottom)).save('E:/Repos/mythic-forge/TDKProps/Saved/PropsDetail/submit_once.png')
