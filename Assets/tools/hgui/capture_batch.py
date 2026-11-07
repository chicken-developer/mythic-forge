"""Viewport captures, one fresh Houdini GUI process per asset (a long GUI session that loads many
assets crashed the viewport driver on this machine). Each child process gets HOUDINI_PATH with
tools/hgui/houdini_path (456.py hook), loads the asset hip, flipbooks persp/front/side/persp_low
through the asset's cameras (capture.py) and exits. No ROP / Karma / Mantra.

    run(root, assets, houdini_exe, log)  -> {asset: status}
"""
import json
import os
import subprocess
import time

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")


def _start_dialog_watch(log):
    """Background thread closing only the third-party 'SHFS not properly setup' startup dialog (dialog_watch.py)."""
    import threading
    try:
        import dialog_watch
    except Exception as e:                      # non-Windows or ctypes missing: captures may then need a click
        log("dialog watcher unavailable: %s" % e)
        return None
    stop = threading.Event()

    def loop():
        while not stop.is_set():
            for t in dialog_watch._close_matching():
                log("  closed dialog: %s" % t)
            stop.wait(1.0)
    th = threading.Thread(target=loop, daemon=True)
    th.start()
    return stop


def run(root, assets, houdini_exe, log=print, timeout=240, retries=1):
    stop = _start_dialog_watch(log)
    try:
        res = _run(root, assets, houdini_exe, log, timeout)
        for _ in range(retries):
            failed = [a for a, r in res.items() if not r.get("ok")]
            if not failed:
                break
            log("retry: %s" % failed)
            res.update(_run(root, failed, houdini_exe, log, timeout))
    finally:
        if stop is not None:
            stop.set()
    return res


def _run(root, assets, houdini_exe, log, timeout):
    res = {}
    os.makedirs(root + "/temp/capture_jobs", exist_ok=True)
    for a in assets:
        job = {"hip": "%s/hip/%s.hip" % (root, a), "asset": a, "out": "%s/renders/%s" % (root, a),
               "hda": root + "/hda/bldg_building_generator.hda",
               "status": "%s/temp/capture_jobs/%s.status.json" % (root, a), "hgui": HERE}
        jf = "%s/temp/capture_jobs/%s.json" % (root, a)
        with open(jf, "w", encoding="utf-8") as f:
            json.dump(job, f)
        if os.path.exists(job["status"]):
            os.remove(job["status"])
        env = dict(os.environ)
        env["HB_CAPTURE_JOB"] = jf
        env["HOUDINI_PATH"] = HERE + "/houdini_path;&"
        env["HOUDINI_PACKAGE_SKIP"] = "1"               # no third-party packages (their startup dialogs block)
        env["HOUDINI_NO_START_PAGE_SPLASH"] = "1"
        t0 = time.time()
        log("capture %s" % a)
        p = subprocess.Popen([houdini_exe, "-foreground", job["hip"]], env=env)
        try:
            p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            p.kill()
            log("  timeout, killed")
        st = {}
        if os.path.exists(job["status"]):
            st = json.load(open(job["status"], encoding="utf-8"))
        res[a] = {"rc": p.returncode, "seconds": round(time.time() - t0, 1), "ok": st.get("ok"),
                  "views": (st.get("report") or {}).get("views"), "error": (st.get("error") or "")[-800:]}
        log("  %s" % res[a])
    return res
