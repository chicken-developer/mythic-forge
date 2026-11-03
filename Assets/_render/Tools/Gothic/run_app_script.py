"""Submit one explicit workspace script to the current application's UI."""
import sys
from pathlib import Path
import desktop as d

application, script = sys.argv[1:3]
path = Path(script).resolve()
assert path.is_relative_to(Path(__file__).resolve().parents[4]) and path.suffix == '.py'
if application == 'designer':
    d.focus('Substance 3D Designer')
    d.click(157,36); d.click(192,132); d.time.sleep(.4)
    d.click(300,210)
    d.u.keybd_event(0x11,0,0,0); d.key(0x41); d.u.keybd_event(0x11,0,2,0)
    namespace = {'__file__': path.as_posix(), '__name__': '__main__'}
    d.paste("exec(compile(open("+repr(path.as_posix())+", encoding='utf-8-sig').read(), "+repr(path.as_posix())+", 'exec'), "+repr(namespace)+")")
    d.key(0x74)
elif application == 'unreal':
    h=d.focus('TDKProps - Unreal')
    was_topmost=bool(d.u.GetWindowLongW(h,-20)&8)
    try:
        # Keep the selected editor above overlays only while focusing its Cmd
        # widget. Restore the previous window state after this one submission.
        d.u.SetWindowPos(h,-1,0,0,0,0,3);d.click(451,1080);d.time.sleep(.5)
        d.u.PostMessageW(h,0x100,0x23,0); d.u.PostMessageW(h,0x101,0x23,0)
        for _ in range(300):d.u.PostMessageW(h,0x102,8,0)
        for ch in 'py '+path.as_posix():d.u.PostMessageW(h,0x102,ord(ch),0)
        d.u.PostMessageW(h,0x100,13,0); d.u.PostMessageW(h,0x101,13,0)
        d.time.sleep(.6)
    finally:
        if not was_topmost:d.u.SetWindowPos(h,-2,0,0,0,0,0x13)
else:
    raise ValueError(application)
d.time.sleep(1)
d.ImageGrab.grab(bbox=(0,0,1800,1100)).save('E:/Repos/mythic-forge/Assets/_render/Saved/GothicReview/'+application+'_last.png')
