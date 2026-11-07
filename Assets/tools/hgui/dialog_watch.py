"""Close the third-party startup dialog "SHFS not properly setup" (and nothing else) while capture
sessions run: it is modal and blocks the unattended Houdini GUI processes of capture_batch.py.
Windows only (user32 via ctypes). watch(seconds, log) polls top-level windows by exact title prefix."""
import ctypes
import time
from ctypes import wintypes

TITLES = ("SHFS not properly setup",)
WM_CLOSE = 0x0010


def _close_matching():
    user32 = ctypes.windll.user32
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        n = user32.GetWindowTextLengthW(hwnd)
        if n:
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            if user32.IsWindowVisible(hwnd) and buf.value.startswith(TITLES):
                user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
                found.append(buf.value)
        return True

    user32.EnumWindows(cb, 0)
    return found


def watch(seconds, log=print, every=1.0):
    t_end = time.time() + seconds
    closed = 0
    while time.time() < t_end:
        for t in _close_matching():
            closed += 1
            log("closed dialog: %s" % t)
        time.sleep(every)
    return closed
