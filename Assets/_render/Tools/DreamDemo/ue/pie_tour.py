"""Step 7 capture: plays L_Kingdom_Main in the editor (PIE) and drives BP_DreamWorldState
through its own events, so the screenshots also prove that the Blueprint logic works.

Started by Tools/DreamDemo/capture_all.py through autorun/init_unreal.py (full editor, GPU).
Config comes from the JSON file named in DREAMDEMO_TOUR: {"out_dir": ..., "quick": bool, "gate": bool}.

Sequence: warm-up of the three states (shader compilation), then Day/Night/Dream x
CAM_Wide/CAM_Wall/CAM_Sky as 1920x1080 high-res screenshots, the stencil check
(caustics only, WorldBlend 0 against 1, then the stencil mask) and the dream gate frames.
"""
import json
import os
import time

import unreal

CFG = json.load(open(os.environ["DREAMDEMO_TOUR"], encoding="utf-8"))
OUT = CFG["out_dir"]
MAP = "/Game/Kingdom/Maps/L_Kingdom_Main"
MPC = "/Game/DreamDemo/MPC_Dream"
STATES = [("Day", 0), ("Night", 1), ("Dream", 2)]
CAMERAS = [("CAM_Wide", "ViewWide"), ("CAM_Wall", "ViewWall"), ("CAM_Sky", "ViewSky")]
FX_TAGS = ["FX_Fish", "FX_Bubbles", "FX_Dust", "FX_Float", "FX_Ink"]
SETTLE = 4.0          # seconds between a change and its screenshot

S = {"phase": "boot", "ticks": 0, "steps": [], "index": 0, "next": 0.0, "handle": None, "log": []}


def ues():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def les():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def game_world():
    return ues().get_game_world()


def controller():
    world = game_world()
    if world is None:
        return None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name().startswith("BP_DreamWorldState"):
            return actor
    return None


def call(event, *args):
    controller().call_method(event, args=tuple(args))
    S["log"].append("call %s%s" % (event, args))


def shot(name):
    path = os.path.join(OUT, name)
    if os.path.exists(path):
        os.remove(path)
    ok = unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, path)
    # the capture lands a few frames later: the tour waits for the file before it moves on
    S["pending"] = (path, time.time() + 30.0)
    S["log"].append("shot %s %s" % (name, ok))
    unreal.log("DREAMDEMO shot %s %s" % (path, ok))


def set_scalar(name, value):
    unreal.MaterialLibrary.set_scalar_parameter_value(game_world(), unreal.load_asset(MPC), name, value)


def set_fx_hidden(hidden):
    for tag in FX_TAGS:
        for actor in unreal.GameplayStatics.get_all_actors_with_tag(game_world(), tag):
            actor.set_actor_hidden_in_game(hidden)


def pause_animations(paused):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.SkeletalMeshActor):
        actor.skeletal_mesh_component.set_editor_property("pause_anims", paused)


def add(wait, fn):
    S["steps"].append((wait, fn))


def build_steps():
    for _, index in STATES:                      # warm-up: let every state compile its shaders
        add(1.0, lambda i=index: call("ApplyState", i))
        add(12.0, lambda: None)
    for name, index in STATES:
        add(0.5, lambda i=index: call("ApplyState", i))
        for cam, event in CAMERAS:
            add(0.5, lambda e=event: call(e))
            add(SETTLE, lambda n="%s_%s.png" % (cam, name): shot(n))
            add(1.0, lambda: None)
    if not CFG.get("quick"):
        # stencil check, state is Dream
        add(0.5, lambda: (call("ViewWide"), set_fx_hidden(True), pause_animations(True), set_scalar("WorldBlend", 0.0)))
        add(SETTLE, lambda: shot("check_caustics_blend0.png"))
        add(1.0, lambda: set_scalar("WorldBlend", 1.0))
        add(SETTLE, lambda: shot("check_caustics_blend1.png"))
        add(1.0, lambda: set_scalar("DebugStencilView", 1.0))
        add(2.0, lambda: shot("check_stencil_mask.png"))
        add(1.0, lambda: (set_scalar("DebugStencilView", 0.0), set_fx_hidden(False), pause_animations(False)))
        # reduced motion (event of the M key)
        add(0.5, lambda: call("ToggleReducedMotion"))
        add(SETTLE, lambda: shot("check_reduced_motion.png"))
        add(1.0, lambda: call("ToggleReducedMotion"))
    if CFG.get("gate", True):
        add(0.5, lambda: (call("ApplyState", 0), call("ViewWide")))
        add(SETTLE, lambda: call("Gate", 2))
        for frame in range(12):
            add(0.3, lambda f=frame: shot("gate_%02d.png" % f))
        add(2.0, lambda: call("Gate", 0))
        for frame in range(8):
            add(0.25, lambda f=frame: shot("wake_%02d.png" % f))
        add(2.0, lambda: None)


def tick(_dt):
    if S.get("busy"):
        return
    S["ticks"] += 1
    phase = S["phase"]
    if phase == "boot":
        if S["ticks"] < 60 or ues() is None or ues().get_editor_world() is None:
            return
        S["phase"] = "loading"          # load_map pumps Slate and re-enters this callback
        unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        os.makedirs(OUT, exist_ok=True)
        for cmd in ("Slate.bAllowThrottling 0", "r.HighResScreenshotDelay 8", "t.IdleWhenNotForeground 0"):
            unreal.SystemLibrary.execute_console_command(None, cmd)
        try:    # the editor is not the foreground window during a capture run
            perf = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
            perf.set_editor_property("throttle_cpu_when_not_foreground", False)
        except Exception as exc:  # noqa: BLE001
            S["log"].append("could not disable background throttling: %s" % exc)
        S.update(phase="warm", ticks=0)
    elif phase == "warm":
        if S["ticks"] > 200:
            les().editor_request_begin_play()
            S.update(phase="starting", ticks=0)
    elif phase == "starting":
        if controller() is not None and S["ticks"] > 120:
            build_steps()
            S.update(phase="run", index=0, next=time.time() + S["steps"][0][0])
        elif S["ticks"] > 6000:
            S["log"].append("PIE did not start or BP_DreamWorldState not found")
            S.update(phase="finish", ticks=0)
    elif phase == "run":
        pending = S.get("pending")
        if pending is not None:
            if not os.path.exists(pending[0]) and time.time() < pending[1]:
                return
            if not os.path.exists(pending[0]):
                S["log"].append("ERROR screenshot never written: %s" % pending[0])
            S["pending"] = None
            S["next"] = time.time() + S["steps"][S["index"]][0] if S["index"] < len(S["steps"]) else 0.0
            return
        if time.time() < S["next"]:
            return
        # A step may pump Slate (screenshots do) and re-enter this callback: move on first.
        index = S["index"]
        _, fn = S["steps"][index]
        S["index"] += 1
        S["busy"] = True
        try:
            fn()
        except Exception as exc:  # noqa: BLE001
            S["log"].append("ERROR step %d: %s" % (index, exc))
            unreal.log_error("DREAMDEMO tour step %d failed: %s" % (index, exc))
        finally:
            S["busy"] = False
        if S["index"] >= len(S["steps"]):
            S.update(phase="finish", ticks=0)
        else:
            S["next"] = time.time() + S["steps"][S["index"]][0]
    elif phase == "finish":
        if S["ticks"] == 1:
            with open(os.path.join(OUT, "tour_log.json"), "w", encoding="utf-8") as fh:
                json.dump({"log": S["log"], "errors": [l for l in S["log"] if l.startswith("ERROR")]}, fh, indent=1)
            les().editor_request_end_play()
        if S["ticks"] > 120:
            unreal.unregister_slate_post_tick_callback(S["handle"])
            unreal.SystemLibrary.quit_editor()


S["handle"] = unreal.register_slate_post_tick_callback(tick)
