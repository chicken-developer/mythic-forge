import ctypes,sys,time,json
from ctypes import wintypes
from pathlib import Path
from PIL import ImageGrab
u=ctypes.windll.user32

def windows():
    found=[]
    @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def cb(h,l):
        if u.IsWindowVisible(h):
            b=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(h,b,1024)
            if b.value:found.append((h,b.value))
        return True
    u.EnumWindows(cb,0);return found

def focus(pattern):
    def area(h):
        r=wintypes.RECT();u.GetWindowRect(h,ctypes.byref(r));return (r.right-r.left)*(r.bottom-r.top)
    candidates=[h for h,t in windows() if pattern.lower() in t.lower()]
    main=[h for h in candidates if not u.GetWindow(h,4)]
    h=max(main or candidates,key=area)
    u.ShowWindow(h,9)
    u.keybd_event(0x12,0,0,0);u.SetForegroundWindow(h);u.keybd_event(0x12,0,2,0)
    time.sleep(.6);return h

def key(v):u.keybd_event(v,0,0,0);time.sleep(.05);u.keybd_event(v,0,2,0)
def click(x,y):
    u.SetCursorPos(x,y);u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0);time.sleep(.3)
def paste(s):
    k=ctypes.windll.kernel32
    k.GlobalAlloc.restype=ctypes.c_void_p;k.GlobalLock.restype=ctypes.c_void_p;k.GlobalLock.argtypes=[ctypes.c_void_p]
    k.GlobalUnlock.argtypes=[ctypes.c_void_p];u.SetClipboardData.argtypes=[wintypes.UINT,ctypes.c_void_p]
    data=(s+'\0').encode('utf-16-le');h=k.GlobalAlloc(0x42,len(data));p=k.GlobalLock(h);ctypes.memmove(p,data,len(data));k.GlobalUnlock(h)
    u.OpenClipboard(None);u.EmptyClipboard();u.SetClipboardData(13,h);u.CloseClipboard()
    u.keybd_event(0x11,0,0,0);key(0x56);u.keybd_event(0x11,0,2,0);time.sleep(.2)
if __name__=='__main__':
    if len(sys.argv)>1:focus(sys.argv[1])
    print(json.dumps(windows(),ensure_ascii=True))
    ImageGrab.grab().save('E:/Repos/mythic-forge/TDKProps/Saved/GothicReview/desktop.png')
