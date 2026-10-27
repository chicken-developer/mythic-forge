"""Submit one explicit workspace script to the current application's UI."""
import sys
from pathlib import Path
import desktop as d

application, script = sys.argv[1:3]
path = Path(script).resolve()
assert path.is_relative_to(Path('E:/Repos/mythic-forge')) and path.suffix == '.py'
if application == 'designer':
    d.focus('Substance 3D Designer')
    d.click(157,36); d.click(192,132); d.time.sleep(.4)
    d.click(300,210)
    d.u.keybd_event(0x11,0,0,0); d.key(0x41); d.u.keybd_event(0x11,0,2,0)
    d.paste("exec(compile(open("+repr(path.as_posix())+", encoding='utf-8-sig').read(), "+repr(path.as_posix())+", 'exec'))")
    d.key(0x74)
elif application == 'unreal':
    d.focus('TDKProps - Unreal')
    d.click(451,1080)
    d.u.keybd_event(0x11,0,0,0); d.key(0x41); d.u.keybd_event(0x11,0,2,0)
    d.paste('py '+path.as_posix()); d.key(13)
else:
    raise ValueError(application)
d.time.sleep(1)
d.ImageGrab.grab(bbox=(0,0,1800,1100)).save('E:/Repos/mythic-forge/TDKProps/Saved/GothicReview/'+application+'_last.png')
