"""hbgui - drive the hbuild toolkit from a Houdini GUI Python Shell.

Load once in Windows > Python Shell:
    exec(open(r"E:/Repos/mythic-forge/Assets/tools/hgui/hbgui.py").read())

Then:
    job("01_bootstrap")    run Assets/temp/hb_jobs/01_bootstrap.py in a background thread
                           (subprocess work: hython, git) - the GUI stays responsive
    gjob("05_capture")     run a job on the main thread (needs the live GUI: viewport captures)

Every job writes Assets/temp/hb_logs/<name>.log and <name>.status.json
({"state": "running"|"done"|"failed", ...}). temp/ is git-ignored.
Jobs get these globals: ROOT, REPO, HYTHON, sh(args, timeout=None), log(msg), result(dict).
"""
import json
import os
import subprocess
import threading
import time
import traceback

REPO = "E:/Repos/mythic-forge"
ROOT = REPO + "/Assets"
HYTHON = "E:/SideFX/Houdini 22.0.368/bin/hython.exe"
JOBS = ROOT + "/temp/hb_jobs"
LOGS = ROOT + "/temp/hb_logs"
os.makedirs(JOBS, exist_ok=True)
os.makedirs(LOGS, exist_ok=True)


def _env():
    try:
        with open(ROOT + "/houdini_env.json", "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def _status(name, **kw):
    with open("%s/%s.status.json" % (LOGS, name), "w", encoding="utf-8") as f:
        json.dump(dict(kw, name=name, time=time.strftime("%Y-%m-%d %H:%M:%S")), f, indent=1)


def _make_globals(name):
    logf = "%s/%s.log" % (LOGS, name)
    open(logf, "w", encoding="utf-8").close()
    res = {}

    def log(msg):
        with open(logf, "a", encoding="utf-8") as f:
            f.write(str(msg).rstrip() + "\n")

    def sh(args, timeout=None, cwd=REPO):
        """Run a command (list), log stdout+stderr, return (returncode, stdout_text)."""
        log("$ " + " ".join('"%s"' % a if " " in str(a) else str(a) for a in args))
        t0 = time.time()
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        p = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=timeout, creationflags=flags)
        out = (p.stdout or "") + (("\n[stderr]\n" + p.stderr) if p.stderr else "")
        log(out)
        log("[rc=%s, %.1fs]" % (p.returncode, time.time() - t0))
        return p.returncode, p.stdout or ""

    def result(d):
        res.update(d)

    g = {"ROOT": ROOT, "REPO": REPO, "HYTHON": _env().get("hython", HYTHON), "sh": sh, "log": log,
         "result": result, "__name__": "hb_job_" + name}
    return g, res


def _run(name):
    path = "%s/%s.py" % (JOBS, name)
    g, res = _make_globals(name)
    _status(name, state="running")
    t0 = time.time()
    try:
        with open(path, "r", encoding="utf-8") as f:
            code = compile(f.read(), path, "exec")
        exec(code, g)
        _status(name, state="done", seconds=round(time.time() - t0, 1), result=res)
    except Exception:
        tb = traceback.format_exc()
        g["log"](tb)
        _status(name, state="failed", seconds=round(time.time() - t0, 1), error=tb[-3000:], result=res)


def job(name):
    """Background thread: for subprocess jobs (hython, git). Returns immediately."""
    t = threading.Thread(target=_run, args=(name,), daemon=True)
    t.start()
    print("[hbgui] started job %s -> %s/%s.status.json" % (name, LOGS, name))
    return t


def gjob(name):
    """Main thread: for jobs that use the live GUI (hou.ui, SceneViewer)."""
    _run(name)
    print("[hbgui] job %s finished -> %s/%s.status.json" % (name, LOGS, name))


print("[hbgui] ready. job('<name>') / gjob('<name>'), jobs in %s" % JOBS)
