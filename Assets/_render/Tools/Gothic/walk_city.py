"""Brief bounded keyboard input test of the actual PIE character."""
import desktop as d
d.focus('TDKProps - Unreal')
d.click(900,500)
try:
    d.u.keybd_event(0x57,0,0,0)
    d.time.sleep(2.)
finally:
    d.u.keybd_event(0x57,0,2,0)
    d.u.keybd_event(0x10,0,0,0);d.key(0x70);d.u.keybd_event(0x10,0,2,0)
