"""Capture-session hook (only active when HB_CAPTURE_JOB is set; added to HOUDINI_PATH by
tools/hgui/capture_batch.py for its own child Houdini processes, never for the artist's session).

Runs once after the GUI is ready: viewport captures of the hip given on the command line, then quits.
"""
import os

_job = os.environ.pop("HB_CAPTURE_JOB", "")      # pop: a later hip load in this session must not re-trigger
if _job:
    import hou

    def _run():
        import json, sys, traceback
        job = json.loads(open(_job, encoding="utf-8").read())
        sys.path.insert(0, job["hgui"])                 # tools/hgui (capture.py)
        status = {"asset": job["asset"]}
        try:
            import capture
            status["report"] = capture.capture_asset(job["hip"], job["asset"], job["out"], hda=job.get("hda"))
            status["ok"] = True
        except Exception:
            status["ok"] = False
            status["error"] = traceback.format_exc()
        with open(job["status"], "w", encoding="utf-8") as f:
            json.dump(status, f, indent=1)
        # hard exit: hou.exit() from a deferred callback segfaults on this machine (crash dialog blocks the batch);
        # nothing needs saving in a capture session.
        os._exit(0 if status["ok"] else 3)

    if hou.isUIAvailable():
        import hdefereval
        hdefereval.executeDeferredAfterWaiting(_run, 25)    # let the viewport finish its first draws
