"""Import a baked hbuild asset (LOW + HIGH FBX, baked normal map) into TDKProps and render it in a level.

Run in the full Unreal editor (not a commandlet), e.g.
  UnrealEditor.exe TDKProps.uproject -ExecutePythonScript="<this file>"
Request: RenderTests/HBuild/request.json
  {"asset": "med_tavern", "map": "/Game/Levels/NewMap", "quit": true}
Reads   SourceAssets/HBuild/<asset>/{SM_<asset>_LOW.fbx, SM_<asset>_HIGH.fbx, T_<asset>_N.png, bake.json}
Creates /Game/HBuild/<asset>/: SM_<asset>_LOW, SM_<asset>_HIGH, T_<asset>_N, one material per slot
        (constant base colour from the preset palette; LOW materials add the baked normal map on UV0)
Places  LOW and HIGH side by side in the map (Outliner folder HBuild/<asset>), adds a sun / sky only if the
        map has none, cameras CAM_<asset>_*, then high-res screenshots (camera actors, no Movie Render Queue)
        into RenderTests/HBuild/<asset>/ and report.json. Saves the map, quits when "quit" is true.
"""
from pathlib import Path
import json
import time
import traceback
import unreal as u

P = Path(__file__).resolve().parents[2]                     # .../Assets/_render
REQ = json.loads((P / "RenderTests/HBuild/request.json").read_text(encoding="utf-8"))
ASSET = REQ["asset"]
MAP = REQ.get("map", "/Game/Levels/NewMap")
SRC = P / "SourceAssets/HBuild" / ASSET
OUT = P / "RenderTests/HBuild" / ASSET
OUT.mkdir(parents=True, exist_ok=True)
DEST = "/Game/HBuild/" + ASSET
AT = u.AssetToolsHelpers.get_asset_tools()
ML = u.MaterialEditingLibrary
SM = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
EA = u.get_editor_subsystem(u.EditorActorSubsystem)
LE = u.get_editor_subsystem(u.LevelEditorSubsystem)
BAKE = json.loads((SRC / "bake.json").read_text(encoding="utf-8"))
report = {"status": "running", "engine": u.SystemLibrary.get_engine_version(), "asset": ASSET, "map": MAP,
          "dest": DEST, "steps": [], "captures": []}


def save_report():
    (OUT / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


def step(msg):
    report["steps"].append(msg)
    print("HBUILD", msg)
    save_report()


def import_task(path, name, options=None):
    t = u.AssetImportTask()
    for k, v in {"filename": str(path), "destination_path": DEST, "destination_name": name, "automated": True,
                 "save": True, "replace_existing": True}.items():
        t.set_editor_property(k, v)
    if options:
        t.set_editor_property("options", options)
    AT.import_asset_tasks([t])
    a = u.load_asset(DEST + "/" + name)
    if a is None:
        raise RuntimeError("import failed: %s" % path)
    return a


def import_mesh(fbx, name):
    path = DEST + "/" + name
    if u.EditorAssetLibrary.does_asset_exist(path) and not u.EditorAssetLibrary.delete_asset(path):
        raise RuntimeError("cannot replace " + path)
    o = u.FbxImportUI()
    o.set_editor_property("import_mesh", True)
    o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("mesh_type_to_import", u.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("import_materials", False)
    o.set_editor_property("import_textures", False)
    d = o.get_editor_property("static_mesh_import_data")
    d.set_editor_property("combine_meshes", True)
    d.set_editor_property("generate_lightmap_u_vs", False)
    d.set_editor_property("auto_generate_collision", False)
    d.set_editor_property("normal_import_method", u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    mesh = import_task(fbx, name, o)
    s = SM.get_lod_build_settings(mesh, 0)
    s.set_editor_property("recompute_normals", False)
    s.set_editor_property("recompute_tangents", True)
    s.set_editor_property("use_mikk_t_space", True)
    s.set_editor_property("generate_lightmap_u_vs", False)
    s.set_editor_property("use_full_precision_u_vs", True)
    SM.set_lod_build_settings(mesh, 0, s)
    return mesh


def material(name, color, normal):
    path = DEST + "/Materials/" + name
    mat = u.load_asset(path)
    if mat is None:
        mat = AT.create_asset(name, DEST + "/Materials", u.Material, u.MaterialFactoryNew())
    else:
        ML.delete_all_material_expressions(mat)
    c = ML.create_material_expression(mat, u.MaterialExpressionConstant3Vector, -500, 0)
    c.set_editor_property("constant", u.LinearColor(color[0], color[1], color[2], 1.0))
    ML.connect_material_property(c, "", u.MaterialProperty.MP_BASE_COLOR)
    r = ML.create_material_expression(mat, u.MaterialExpressionConstant, -500, 180)
    r.set_editor_property("r", 0.75)
    ML.connect_material_property(r, "", u.MaterialProperty.MP_ROUGHNESS)
    if normal is not None:
        t = ML.create_material_expression(mat, u.MaterialExpressionTextureSample, -500, 350)
        t.set_editor_property("texture", normal)
        t.set_editor_property("sampler_type", u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        ML.connect_material_property(t, "RGB", u.MaterialProperty.MP_NORMAL)
    ML.recompile_material(mat)
    u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def assign(mesh, normal, suffix):
    used = []
    cols = BAKE["colors_linear"]
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        nm = str(slot.get_editor_property("imported_material_slot_name") or slot.get_editor_property("material_slot_name"))
        mid = nm.split("/")[-1]
        mid = mid[2:] if mid.startswith("M_") else mid
        col = cols.get(mid) or [0.5, 0.5, 0.5]
        mesh.set_material(i, material("M_%s_%s%s" % (ASSET, mid, suffix), col, normal))
        used.append(mid)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    u.EditorAssetLibrary.save_loaded_asset(mesh)
    return used


def mesh_info(mesh, key):
    ext = mesh.get_bounds().box_extent
    size = [round(2 * ext.x / 100, 3), round(2 * ext.y / 100, 3), round(2 * ext.z / 100, 3)]
    exp = BAKE[key]["bounds_m"]
    tris = mesh.get_num_triangles(0)
    return {"asset": mesh.get_path_name(), "triangles": tris, "expected_triangles": BAKE[key]["triangles"],
            "size_m": size, "expected_size_m_houdini_xyz": exp,
            "size_ok": max(abs(a - b) for a, b in zip(sorted(size), sorted(exp))) < 0.03}


def spawn(cls, label, loc, rot=None):
    a = EA.spawn_actor_from_class(cls, loc, rot or u.Rotator())
    a.set_actor_label(label)
    a.set_folder_path("HBuild/" + ASSET)
    return a


def look(eye, target):
    return u.MathLibrary.find_look_at_rotation(eye, target)


jobs = []
state = {"i": 0, "next": 0.0, "pending": None}


def tick(delta):
    try:
        now = time.monotonic()
        if state["pending"] is not None:
            job = state["pending"]
            if job["file"].exists() and job["file"].stat().st_mtime >= state["start"]:
                report["captures"].append({"id": job["id"], "file": str(job["file"]), "camera": job["cam"].get_actor_label(),
                                           "resolution": list(job["size"]), "image_provenance": "unreal_engine_capture"})
                save_report()
                state.update(pending=None, i=state["i"] + 1, next=now + 1.0)
            elif now - state["asked"] > 120:
                raise RuntimeError("screenshot timed out: %s" % job["file"])
            return
        if now < state["next"]:
            return
        if state["i"] >= len(jobs):
            LE.save_current_level()
            report["status"] = "pass"
            save_report()
            u.unregister_slate_post_tick_callback(handle)
            u.EditorPythonScripting.set_keep_python_script_alive(False)
            if REQ.get("quit", True):
                u.SystemLibrary.quit_editor()
            return
        job = jobs[state["i"]]
        u.EditorLevelLibrary.set_level_viewport_camera_info(job["cam"].get_actor_location(), job["cam"].get_actor_rotation())
        state.update(pending=job, start=time.time(), asked=now)
        u.AutomationLibrary.take_high_res_screenshot(job["size"][0], job["size"][1], str(job["file"]), camera=job["cam"], delay=2.0)
    except Exception:
        report["status"] = "fail"
        report["error"] = traceback.format_exc()
        save_report()
        u.unregister_slate_post_tick_callback(handle)
        u.EditorPythonScripting.set_keep_python_script_alive(False)
        if REQ.get("quit", True):
            u.SystemLibrary.quit_editor()


def main():
    if Path(u.Paths.project_dir()).resolve() != P.resolve():
        raise RuntimeError("wrong Unreal project: " + u.Paths.project_dir())
    normal = import_task(SRC / ("T_%s_N.png" % ASSET), "T_%s_N" % ASSET)
    normal.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_NORMALMAP)
    normal.set_editor_property("srgb", False)
    normal.set_editor_property("flip_green_channel", False)       # baked DirectX, green down = Unreal
    u.EditorAssetLibrary.save_loaded_asset(normal)
    step("normal imported")
    low = import_mesh(SRC / ("SM_%s_LOW.fbx" % ASSET), "SM_%s_LOW" % ASSET)
    high = import_mesh(SRC / ("SM_%s_HIGH.fbx" % ASSET), "SM_%s_HIGH" % ASSET)
    report["low"] = dict(mesh_info(low, "low"), materials=assign(low, normal, ""))
    report["high"] = dict(mesh_info(high, "high"), materials=assign(high, None, "_HIGH"))
    step("meshes imported")

    if not LE.load_level(MAP):
        raise RuntimeError("cannot load " + MAP)
    actors = EA.get_all_level_actors()
    report["map_actors_before"] = sorted(set(a.get_class().get_name() for a in actors))
    for a in actors:                                        # re-run: replace our own actors only
        if str(a.get_folder_path()).startswith("HBuild/" + ASSET):
            EA.destroy_actor(a)
    actors = EA.get_all_level_actors()
    # base: on top of the largest flat static mesh near the origin (a floor), else z = 0 with a ground slab
    base = u.Vector(0, 0, 0)
    floor = None
    for a in actors:
        if isinstance(a, u.StaticMeshActor):
            o, e = a.get_actor_bounds(False)
            if e.x > 1500 and e.y > 1500 and e.z < 200 and abs(o.x) < 20000 and abs(o.y) < 20000:
                if floor is None or e.x * e.y > floor[1].x * floor[1].y:
                    floor = (o, e, a)
    ext = low.get_bounds().box_extent
    gap = 600.0
    if floor:
        base = u.Vector(floor[0].x - ext.x - gap * 0.5, floor[0].y, floor[0].z + floor[1].z)
        report["placed_on"] = floor[2].get_actor_label()
    else:
        # top 3 cm under z = 0: the building's ground floor slab sits at z = 0 (no z-fight inside)
        g = spawn(u.StaticMeshActor, "%s ground" % ASSET, u.Vector(ext.x + gap * 0.5, 0, -28))
        g.static_mesh_component.set_static_mesh(u.load_asset("/Engine/BasicShapes/Cube.Cube"))
        g.set_actor_scale3d(u.Vector((4 * ext.x + 3 * gap) / 100, (4 * ext.y + 2 * gap) / 100, 0.5))
        report["placed_on"] = "new ground slab"
    lo = spawn(u.StaticMeshActor, "%s_LOW" % ASSET, base)
    lo.static_mesh_component.set_static_mesh(low)
    hi = spawn(u.StaticMeshActor, "%s_HIGH" % ASSET, base + u.Vector(2 * ext.x + gap, 0, 0))
    hi.static_mesh_component.set_static_mesh(high)
    kinds = set(a.get_class().get_name() for a in actors)
    if "DirectionalLight" not in kinds:
        s = spawn(u.DirectionalLight, "%s sun" % ASSET, base + u.Vector(0, 0, 2000), u.Rotator(-40, -35, 0))
        s.light_component.set_editor_property("intensity", 6.0)
        s.light_component.set_editor_property("atmosphere_sun_light", True)
    if "SkyLight" not in kinds:
        k = spawn(u.SkyLight, "%s sky light" % ASSET, base + u.Vector(0, 0, 1500))
        k.light_component.set_editor_property("real_time_capture", True)
    if "SkyAtmosphere" not in kinds:
        spawn(u.SkyAtmosphere, "%s sky" % ASSET, base)
    report["lights_added"] = sorted({"DirectionalLight", "SkyLight", "SkyAtmosphere"} - kinds)

    c_lo = lo.get_actor_location() + low.get_bounds().origin
    c_hi = hi.get_actor_location() + high.get_bounds().origin
    mid = (c_lo + c_hi) * 0.5
    R = max(ext.x, ext.y, ext.z)
    cams = []
    for side, sy in (("a", -1.0), ("b", 1.0)):
        eye = mid + u.Vector(-0.9 * R, sy * 4.2 * R, 1.9 * R)
        cams.append(("pair_" + side, eye, mid, (2400, 1350), 50.0))
        for nm, c in (("low", c_lo), ("high", c_hi)):
            e2 = c + u.Vector(1.6 * R, sy * 2.2 * R, 1.1 * R)
            cams.append(("%s_%s" % (nm, side), e2, c, (1920, 1080), 45.0))
    # interior (LOW, ground floor) + a point light, looking both ways along the long axis
    fl = lo.get_actor_location() + u.Vector(0, 0, 165)
    pl = spawn(u.PointLight, "%s interior light" % ASSET, lo.get_actor_location() + u.Vector(0, 0, 230))
    pl.light_component.set_editor_property("intensity", 3000.0)
    pl.light_component.set_editor_property("attenuation_radius", 1800.0)
    for side, sx in (("a", 1.0), ("b", -1.0)):
        cams.append(("interior_" + side, fl + u.Vector(-sx * 0.35 * ext.x, 0, 0), fl + u.Vector(sx * ext.x, 0, -40), (1920, 1080), 85.0))
    for cid, eye, tgt, size, fov in cams:
        cam = spawn(u.CameraActor, "CAM_%s_%s" % (ASSET, cid), eye, look(eye, tgt))
        cam.camera_component.set_editor_property("field_of_view", fov)
        jobs.append({"id": cid, "cam": cam, "file": OUT / ("%s_%s.png" % (ASSET, cid)), "size": size})
    LE.save_current_level()
    step("level ready: %d cameras" % len(jobs))
    for cmd in ("Slate.bAllowThrottling 0", "r.ScreenPercentage 100", "r.Streaming.PoolSize 3000"):
        u.SystemLibrary.execute_console_command(None, cmd)
    perf = u.get_default_object(u.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
    perf.set_editor_property("bThrottleCPUWhenNotForeground", False)
    state["next"] = time.monotonic() + 30.0                   # let shaders / lighting settle


try:
    main()
    u.EditorPythonScripting.set_keep_python_script_alive(True)
    handle = u.register_slate_post_tick_callback(tick)
except Exception:
    report["status"] = "fail"
    report["error"] = traceback.format_exc()
    save_report()
    if REQ.get("quit", True):
        u.SystemLibrary.quit_editor()
