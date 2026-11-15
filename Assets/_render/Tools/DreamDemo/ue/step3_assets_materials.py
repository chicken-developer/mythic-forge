"""Step 3: import the Houdini bakes, create MPC_Dream, the sky materials and the caustics.

Run:  bash Tools/DreamDemo/ue_run.sh cmd Tools/DreamDemo/ue/step3_assets_materials.py dd_step3
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                "Tools", "DreamDemo", "ue"))
import dd_common as dd  # noqa: E402
import dd_layout  # noqa: E402

mel = dd.mel
REPORT = {"imports": {}, "checks": {}, "notes": []}

# Tunable defaults (also the reset values): name -> default.
MPC_SCALARS = [
    ("WorldBlend", 0.0), ("NightBlend", 0.0),
    ("CausticsIntensity", 0.6), ("CausticsSpeed", 0.15), ("CausticsScale", 350.0),
    ("ArenaCausticsFloor", 0.4), ("FloatAmount", 1.0),
    ("AuroraIntensity", 0.6), ("AuroraSpeed", 0.05), ("AuroraHueShift", 0.0),
    ("DarknessCount", 0.0), ("DarknessEdgeNoise", 0.3),
    # Additions to the prompt list (documented in STEP_3.md):
    ("CausticsBrightness", 1.2),     # light level of the caustics relative to the dream key light
    ("CausticsSharpness", 2.2),      # exponent applied to min(layer A, layer B)
    ("CausticsMaxDistance", 9000.0), # cm; caustics fade out before this distance
    ("CausticsFoliageBoost", 2.5),   # extra caustics on grass and leaves (subsurface shading)
    ("DustSpeed", 1.0),
    # Lift of the floating props in cm (TECH_DREAM_VISUALS 5.2: 15 + rand * 25).
    ("FloatLiftMin", 15.0), ("FloatLiftMax", 40.0),
    # Dream gate: the material animates itself from these, the Blueprint only sets them.
    ("GateMode", 0.0),               # 0 off, 1 covering, 2 revealing
    ("GateStartTime", 0.0),          # game time in seconds when the current half started
    ("GateDuration", 1.2),           # seconds of the current half
    ("MotionReduced", 0.0),          # 1 = reduced motion: star fish are clipped away
    ("DebugStencilView", 0.0),       # 1 = caustics material shows the custom stencil mask
]


def enum_value(enum_cls, *candidates):
    for name in candidates:
        if hasattr(enum_cls, name):
            return getattr(enum_cls, name)
    raise RuntimeError("%s has none of %s; available: %s" % (
        enum_cls.__name__, candidates, [n for n in dir(enum_cls) if n.isupper()]))


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

def enable_custom_stencil():
    """r.CustomDepth=3 (custom depth with stencil) in DefaultEngine.ini."""
    path = os.path.join(dd.PROJECT_DIR, "Config", "DefaultEngine.ini")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    if "r.CustomDepth=3" in text:
        REPORT["checks"]["r.CustomDepth"] = "already 3"
        return
    section = "[/Script/Engine.RendererSettings]"
    if section not in text:
        text += "\n%s\nr.CustomDepth=3\n" % section
    else:
        text = text.replace(section, section + "\nr.CustomDepth=3", 1)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    REPORT["checks"]["r.CustomDepth"] = "set to 3 in Config/DefaultEngine.ini (was engine default 1)"


# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------

def import_file(filename, folder, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(dd.RAW_DIR, filename))
    task.set_editor_property("destination_path", "%s/%s" % (dd.ROOT, folder))
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    dd.asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    name = os.path.splitext(filename)[0]
    asset = dd.load("%s/%s/%s" % (dd.ROOT, folder, name))
    REPORT["imports"][filename] = {
        "paths": [str(p) for p in paths],
        "class": asset.get_class().get_name() if asset else None,
    }
    if asset is None:
        raise RuntimeError("import failed: %s" % filename)
    return asset


def texture_settings(texture, **props):
    for key, value in props.items():
        texture.set_editor_property(key, value)
    dd.save(texture)


def import_textures():
    tc = unreal.TextureCompressionSettings
    out = {}
    gray = dict(compression_settings=tc.TC_GRAYSCALE, srgb=False)
    for name in ("T_Caustics", "T_AuroraStrip", "T_StarField", "T_CloudNoise"):
        out[name] = import_file(name + ".png", "Textures")
        texture_settings(out[name], **gray)
    out["T_AuroraFlow"] = import_file("T_AuroraFlow.png", "Textures")
    texture_settings(out["T_AuroraFlow"], compression_settings=tc.TC_BC7, srgb=False)
    out["T_ThinFilmRamp"] = import_file("T_ThinFilmRamp.png", "Textures")
    texture_settings(out["T_ThinFilmRamp"], srgb=True,
                     mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                     address_x=unreal.TextureAddress.TA_CLAMP,
                     address_y=unreal.TextureAddress.TA_CLAMP)

    memory = []
    for i in range(4):
        tex = import_file("T_MemoryPlaceholder_%d.png" % i, "Textures")
        texture_settings(tex, srgb=True, address_x=unreal.TextureAddress.TA_CLAMP,
                         address_y=unreal.TextureAddress.TA_CLAMP)
        memory.append(tex)
    array = dd.create_asset("T_MemoryArray", "Textures", unreal.Texture2DArray,
                            unreal.Texture2DArrayFactory())
    array.set_editor_property("source_textures", memory)
    array.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    array.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    dd.save(array)
    out["T_MemoryArray"] = array
    REPORT["checks"]["T_MemoryArray_layers"] = len(array.get_editor_property("source_textures"))

    nebula = import_file("T_DreamNebula.hdr", "Textures")
    REPORT["checks"]["T_DreamNebula_class"] = nebula.get_class().get_name()
    if isinstance(nebula, unreal.TextureCube):
        texture_settings(nebula, compression_settings=tc.TC_HDR_COMPRESSED, srgb=False)
    else:
        REPORT["notes"].append("T_DreamNebula did not import as TextureCube")
    out["T_DreamNebula"] = nebula
    return out


def import_meshes():
    out = {}
    for name, extension in (("SM_StarFish", (4000.0, 4000.0, 300.0)),
                            ("SM_DustMote", (60.0, 60.0, 700.0)),
                            ("SM_Bubble", (150.0, 150.0, 500.0))):
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("automated_import_should_detect_type", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        data = ui.static_mesh_import_data
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("auto_generate_collision", False)
        data.set_editor_property("generate_lightmap_u_vs", False)
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
        data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        mesh = import_file(name + ".fbx", "Meshes", ui)
        # The instances travel by world position offset; widen the per-instance bounds so
        # GPU instance culling does not drop them (fish orbit the whole arena).
        ext = unreal.Vector(*extension)
        mesh.set_editor_property("positive_bounds_extension", ext)
        mesh.set_editor_property("negative_bounds_extension", ext)
        dd.save(mesh)
        box = mesh.get_bounding_box()
        size = box.max - box.min
        REPORT["imports"][name + ".fbx"].update({
            "triangles": mesh.get_num_triangles(0),
            "size_cm": [round(size.x - 2 * extension[0], 1), round(size.y - 2 * extension[1], 1),
                        round(size.z - 2 * extension[2], 1)],
            "bounds_extension_cm": list(extension),
        })
        out[name] = mesh
    check_fish(out["SM_StarFish"])
    return out


def check_fish(mesh):
    """Head must be at +X with UV0.x = 0 there, tail at -X with UV0.x = 1."""
    try:
        desc = mesh.get_static_mesh_description(0)
        best_hi, best_lo = None, None
        for index in range(desc.get_vertex_count()):
            vid = unreal.VertexID(index)
            pos = desc.get_vertex_position(vid)
            instances = desc.get_vertex_vertex_instances(vid)
            if not instances:
                continue
            uv = desc.get_vertex_instance_uv(instances[0], 0)
            if best_hi is None or pos.x > best_hi[0]:
                best_hi = (pos.x, uv.x, pos.z)
            if best_lo is None or pos.x < best_lo[0]:
                best_lo = (pos.x, uv.x, pos.z)
        REPORT["checks"]["SM_StarFish_axis"] = {
            "max_x_cm": round(best_hi[0], 1), "uv_x_at_max_x": round(best_hi[1], 3),
            "min_x_cm": round(best_lo[0], 1), "uv_x_at_min_x": round(best_lo[1], 3),
            "head_at_plus_x": bool(best_hi[1] < 0.1 and best_lo[1] > 0.9),
        }
    except Exception as exc:  # noqa: BLE001
        REPORT["checks"]["SM_StarFish_axis"] = "not checked: %s" % exc


# ---------------------------------------------------------------------------
# MPC
# ---------------------------------------------------------------------------

def build_mpc(layout):
    mpc = dd.create_asset("MPC_Dream", "", unreal.MaterialParameterCollection,
                          unreal.MaterialParameterCollectionFactoryNew()) \
        if not dd.exists(dd.ROOT + "/MPC_Dream") else dd.load(dd.ROOT + "/MPC_Dream")

    # Reuse existing entries so their ids (and every material that references them) survive.
    old_scalars = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property("scalar_parameters")}
    scalars = []
    for name, default in MPC_SCALARS:
        param = old_scalars.get(name) or unreal.CollectionScalarParameter()
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", float(default))
        scalars.append(param)
    mpc.set_editor_property("scalar_parameters", scalars)

    cx, cy, cz = layout["arena_center"]
    vector_defaults = [("ArenaCenterRadius", unreal.LinearColor(cx, cy, cz, layout["arena_radius"]))]
    for i in range(4):
        vector_defaults.append(("DarknessSource%d" % i, unreal.LinearColor(0.0, 0.0, 0.0, 0.0)))
    vector_defaults.append(("CausticsTint", dd.linear_from_hex("#7FD6D0")))
    vector_defaults.append(("GateCenter", unreal.LinearColor(0.5, 0.52, 0.0, 0.0)))
    old_vectors = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property("vector_parameters")}
    vectors = []
    for name, default in vector_defaults:
        param = old_vectors.get(name) or unreal.CollectionVectorParameter()
        param.set_editor_property("parameter_name", name)
        # Darkness sources are owned by step 5; keep what it wrote.
        if not (name.startswith("DarknessSource") and name in old_vectors):
            param.set_editor_property("default_value", default)
        vectors.append(param)
    mpc.set_editor_property("vector_parameters", vectors)
    dd.save(mpc)
    REPORT["checks"]["MPC_Dream"] = {
        "scalars": [str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("scalar_parameters")],
        "vectors": [str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("vector_parameters")],
        "ArenaCenterRadius": [cx, cy, cz, layout["arena_radius"]],
    }
    return mpc


# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------

DARKNESS_CODE = r"""
float mask = 0.0;
float4 src[4] = { S0, S1, S2, S3 };
for (int i = 0; i < 4; i++)
{
    if (i < Count - 0.5 && src[i].w > 1.0)
    {
        float2 d = Pos.xy - src[i].xy;
        float dist = length(d);
        float ang = atan2(d.y, d.x);
        // slow blobby edge: three angular waves drifting at different speeds
        float n = sin(ang * 3.0 + T * 0.31 + i * 1.7) * 0.5
                + sin(ang * 7.0 - T * 0.23 + i * 4.1) * 0.3
                + sin(ang * 13.0 + T * 0.17) * 0.2;
        float r = src[i].w * (1.0 + EdgeNoise * 0.5 * n);
        mask = max(mask, 1.0 - smoothstep(r * 0.88, r, dist));
    }
}
return mask;
"""


def build_darkness_function():
    g = dd.new_function("MF_DreamDarknessMask")
    g.function.set_editor_property("description",
                                   "0..1 ink mask at a world position: union of the MPC_Dream darkness circles with a slow noisy edge. Shared by caustics, floating props, bubbles and the ink decal.")
    pos = g.node(unreal.MaterialExpressionFunctionInput, input_name="Position",
                 input_type=unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3,
                 use_preview_value_as_default=True)
    inputs = {"Pos": pos}
    for i in range(4):
        inputs["S%d" % i] = g.collection("DarknessSource%d" % i)
    inputs["Count"] = g.collection("DarknessCount")
    inputs["EdgeNoise"] = g.collection("DarknessEdgeNoise")
    inputs["T"] = g.time()
    mask = g.custom(DARKNESS_CODE, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT1, "DarknessMask")
    out = g.node(unreal.MaterialExpressionFunctionOutput, output_name="Mask")
    g.connect(mask, out, "")
    g.finish()
    return g.function


CAUSTICS_CODE = r"""
float3 scene = Scene.rgb;

// Two layers drifting in opposite directions; min() of the two soft networks gives sharp veins.
// The ground uses the top-down XY projection of TECH_DREAM_VISUALS 4.1. Steep faces blend in
// the two side projections, otherwise a top-down texture smears into vertical streaks there.
float s = 1.0 / max(Scale, 1.0);
float t = T * Speed * 0.25;
float2 o1 = float2(t, t * 0.37);
float2 o2 = float2(0.37, 0.11) - float2(t * 0.83, -t * 0.29);
#define DD_CAUSTIC(P2) min(Texture2DSample(Tex, TexSampler, (P2) * s + o1).r, Texture2DSample(Tex, TexSampler, (P2) * s * 1.31 + o2).r)
float3 w = pow(abs(N.xyz), 4.0);
w /= max(w.x + w.y + w.z, 1e-4);
// Grass and leaves use the subsurface / two-sided foliage shading models. Only the part that
// faces up (the grass field, whose normals point up in this pack) gets the foliage boost;
// tree crowns keep the three-way projection like any other steep surface.
float foliage = (abs(ShadingModel.r - 2.0) < 0.5 || abs(ShadingModel.r - 6.0) < 0.5) ? 1.0 : 0.0;
foliage *= w.z * step(0.0, N.z);
float caustic = DD_CAUSTIC(Pos.xy) * w.z + DD_CAUSTIC(Pos.yz) * w.x + DD_CAUSTIC(Pos.xz) * w.y;
#undef DD_CAUSTIC
caustic = pow(saturate(caustic), Sharp);

// ground gets the full pattern, walls 35 %, undersides less
float normalFactor = lerp(0.35, 1.0, w.z * step(0.0, N.z)) * (N.z < -0.5 ? 0.3 : 1.0);

// calmer in the middle of the arena than at its rim
float dist = length(Pos.xy - Arena.xy);
float arenaFactor = lerp(Floor, 1.0, smoothstep(0.4 * Arena.w, Arena.w, dist));

float stencilMask = Stencil.r < 0.5 ? 1.0 : 0.0;          // characters, Xenon, weak spots: none
float fade = 1.0 - smoothstep(MaxDist * 0.6, MaxDist, Depth.r);   // also removes the sky

float amount = caustic * Intensity * Brightness * normalFactor * arenaFactor
             * (1.0 - Dark) * stencilMask * fade * Blend;
// subsurface foliage is lit far brighter than its diffuse colour suggests
amount *= lerp(1.0, FoliageBoost, foliage);
// scene colour is stored pre-exposed, so light added here must be pre-exposed too
float3 result = scene + amount * Diffuse.rgb * Tint.rgb * View.PreExposure;

// DebugStencilView: 1 = stencil mask, 2 = shading model id, 3 = diffuse, 4 = base colour, 5 = normal
if (Debug > 1.5)
{
    float3 view = Debug < 2.5 ? ShadingModel.rrr / 8.0
                : (Debug < 3.5 ? Diffuse.rgb : (Debug < 4.5 ? Base.rgb : N.xyz * 0.5 + 0.5));
    return view * 0.5;
}
float3 debugColor = lerp(scene * 0.15, float3(1.0, 1.0, 1.0), 1.0 - stencilMask);
return lerp(result, debugColor, saturate(Debug));
"""


def build_caustics(textures, darkness_fn):
    blend_location = enum_value(unreal.BlendableLocation,
                                "BL_SCENE_COLOR_AFTER_DOF", "BL_BEFORE_TONEMAPPING")
    g = dd.new_material("M_PP_DreamCaustics",
                        material_domain=unreal.MaterialDomain.MD_POST_PROCESS,
                        blendable_location=blend_location)
    sid = unreal.SceneTextureId
    pos = g.node(unreal.MaterialExpressionWorldPosition)
    inputs = {
        "Scene": g.scene_texture(enum_value(sid, "PPI_POST_PROCESS_INPUT0")),
        "N": g.scene_texture(enum_value(sid, "PPI_WORLD_NORMAL")),
        "Diffuse": g.scene_texture(enum_value(sid, "PPI_DIFFUSE_COLOR")),
        "Stencil": g.scene_texture(enum_value(sid, "PPI_CUSTOM_STENCIL")),
        "Depth": g.scene_texture(enum_value(sid, "PPI_SCENE_DEPTH")),
        "ShadingModel": g.scene_texture(enum_value(sid, "PPI_SHADING_MODEL_ID")),
        "Base": g.scene_texture(enum_value(sid, "PPI_BASE_COLOR")),
        "Pos": pos,
        "Tex": g.texture_object(textures["T_Caustics"]),
        "T": g.time(),
        "Blend": g.collection("WorldBlend"),
        "Intensity": g.collection("CausticsIntensity"),
        "Speed": g.collection("CausticsSpeed"),
        "Scale": g.collection("CausticsScale"),
        "Arena": g.collection("ArenaCenterRadius"),
        "Floor": g.collection("ArenaCausticsFloor"),
        "Dark": g.function_call(darkness_fn, pos),
        "Brightness": g.collection("CausticsBrightness"),
        "Sharp": g.collection("CausticsSharpness"),
        "MaxDist": g.collection("CausticsMaxDistance"),
        "Tint": g.collection("CausticsTint"),
        "Debug": g.collection("DebugStencilView"),
        "FoliageBoost": g.collection("CausticsFoliageBoost"),
    }
    result = g.custom(CAUSTICS_CODE, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "DreamCaustics")
    g.output(result, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()
    REPORT["checks"]["M_PP_DreamCaustics_location"] = str(blend_location)
    return g.material


SKY_DIR_CODE = r"""
float3 d = normalize(-CamVec);
float s = sin(Yaw * 6.2831853); float c = cos(Yaw * 6.2831853);
return float3(d.x * c - d.y * s, d.x * s + d.y * c, d.z);
"""

DREAM_SKY_COMMON = r"""
float3 d = normalize(-CamVec);
float3 col = Neb.rgb * NebulaIntensity;
float3 ad = abs(d);
float2 suv = (ad.z >= ad.x && ad.z >= ad.y) ? d.xy / ad.z : (ad.x >= ad.y ? d.yz / ad.x + 3.7 : d.xz / ad.y + 7.3);
suv *= StarTiling;
float star = Texture2DSampleLevel(StarTex, StarTexSampler, suv, 0).r;
star = star * star * star;
float up = smoothstep(-0.05, 0.25, d.z);
"""

DREAM_SKY_HIGH = DREAM_SKY_COMMON + r"""
// stars twinkle per cell
float2 cell = floor(suv * 48.0);
float h = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float tw = 0.65 + 0.35 * sin(T * (1.5 + 3.0 * h) + h * 40.0);
col += star * tw * StarIntensity * up * float3(0.8, 0.95, 1.0);

// aurora curtain between two elevations, pushed along a flow map
float az = atan2(d.y, d.x) / 6.2831853 + 0.5;
float el = asin(clamp(d.z, -1.0, 1.0));
float v = 1.0 - (el - AuroraLow) / max(AuroraHigh - AuroraLow, 0.01);
float inband = step(0.0, v) * step(v, 1.0);
float t = T * AuroraSpeed;
float2 auv = float2(az * AuroraRepeat + t * 0.2, v);
float2 flow = Texture2DSample(FlowTex, FlowTexSampler, float2(az * AuroraRepeat * 0.5, v * 0.5 + 0.1)).rg * 2.0 - 1.0;
float p0 = frac(t); float p1 = frac(t + 0.5);
float a0 = Texture2DSample(StripTex, StripTexSampler, auv - flow * p0 * 0.12).r;
float a1 = Texture2DSample(StripTex, StripTexSampler, auv - flow * p1 * 0.12 + float2(0.37, 0.0)).r;
float aur = lerp(a0, a1, abs(1.0 - 2.0 * p0)) * inband;
float3 ca = lerp(float3(0.10, 0.45, 0.80), float3(0.212, 0.672, 0.631), saturate(v));
float ang = HueShift * 6.2831853;
float3 k = float3(0.57735, 0.57735, 0.57735);
ca = ca * cos(ang) + cross(k, ca) * sin(ang) + k * dot(k, ca) * (1.0 - cos(ang));
col += max(ca, 0.0) * aur * AuroraIntensity;
return col * SkyIntensity;
"""

DREAM_SKY_LOW = DREAM_SKY_COMMON + r"""
col += star * 0.8 * StarIntensity * up * float3(0.8, 0.95, 1.0);
return col * SkyIntensity;
"""


def build_dream_sky(textures):
    g = dd.new_material("M_DreamSky", shading_model=unreal.MaterialShadingModel.MSM_UNLIT,
                        two_sided=True, is_sky=True)
    cam = g.node(unreal.MaterialExpressionCameraVectorWS)
    direction = g.custom(SKY_DIR_CODE, {"CamVec": cam, "Yaw": g.scalar("NebulaYaw", 0.0)},
                         unreal.CustomMaterialOutputType.CMOT_FLOAT3, "SkyDirection")
    nebula = g.texture_sample(textures["T_DreamNebula"], direction)
    common = {
        "CamVec": cam,
        "Neb": nebula,
        "NebulaIntensity": g.scalar("NebulaIntensity", 1.0),
        "StarTex": g.texture_object(textures["T_StarField"]),
        "StarTiling": g.scalar("StarTiling", 3.0),
        "StarIntensity": g.scalar("StarIntensity", 0.35),
        "SkyIntensity": g.scalar("SkyIntensity", 0.5),
    }
    high_inputs = dict(common)
    high_inputs.update({
        "T": g.time(),
        "FlowTex": g.texture_object(textures["T_AuroraFlow"]),
        "StripTex": g.texture_object(textures["T_AuroraStrip"]),
        "AuroraLow": g.scalar("AuroraLowElevation", 0.30),
        "AuroraHigh": g.scalar("AuroraHighElevation", 0.95),
        "AuroraRepeat": g.scalar("AuroraRepeat", 2.0),
        "AuroraSpeed": g.collection("AuroraSpeed"),
        "AuroraIntensity": g.collection("AuroraIntensity"),
        "HueShift": g.collection("AuroraHueShift"),
    })
    high = g.custom(DREAM_SKY_HIGH, high_inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "DreamSkyHigh")
    low = g.custom(DREAM_SKY_LOW, common, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "DreamSkyLow")
    switch = g.node(unreal.MaterialExpressionStaticSwitchParameter, parameter_name="HighQuality",
                    default_value=True, group="DreamDemo")
    g.connect(high, switch, "True")
    g.connect(low, switch, "False")
    g.output(switch, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()
    dd.new_instance("MI_DreamSky", g.material)
    dd.new_instance("MI_DreamSky_Low", g.material, switches={"HighQuality": False})
    return g.material


NIGHT_SKY_CODE = r"""
float3 d = normalize(-CamVec);
float h = saturate(d.z);
float3 col = lerp(Horizon.rgb, Zenith.rgb, pow(h, 0.5));
float3 m = normalize(MoonDir.xyz);
float md = dot(d, m);
float up = smoothstep(0.0, 0.2, d.z);

float2 cuv = d.xy / (abs(d.z) + 0.25) * CloudScale + float2(T * CloudSpeed, T * CloudSpeed * 0.3);
float cn = Texture2DSample(CloudTex, CloudTexSampler, cuv).r;
float cn2 = Texture2DSample(CloudTex, CloudTexSampler, cuv * 2.3 + 0.5).r;
float cloud = smoothstep(CloudCoverage, CloudCoverage + 0.25, cn * 0.7 + cn2 * 0.3) * up;

float3 ad = abs(d);
float2 suv = (ad.z >= ad.x && ad.z >= ad.y) ? d.xy / ad.z : (ad.x >= ad.y ? d.yz / ad.x + 3.7 : d.xz / ad.y + 7.3);
float star = Texture2DSampleLevel(StarTex, StarTexSampler, suv * StarTiling, 0).r;
col += star * star * star * StarIntensity * up * (1.0 - cloud);

float disc = smoothstep(cos(MoonSize), cos(MoonSize * 0.9), md);
float halo = pow(saturate(md), 600.0) * 0.6 + pow(saturate(md), 40.0) * 0.12;
float3 moonCol = float3(1.0, 0.97, 0.88);
col += moonCol * MoonIntensity * (disc * (1.0 - cloud * 0.85) + halo * 0.08);

float3 cloudCol = lerp(CloudDark.rgb, CloudLit.rgb, pow(saturate(md * 0.5 + 0.5), 6.0));
col = lerp(col, cloudCol + moonCol * halo * MoonIntensity * 0.02, cloud * 0.85);
return col * SkyIntensity;
"""


def build_night_sky(textures):
    g = dd.new_material("M_NightSky", shading_model=unreal.MaterialShadingModel.MSM_UNLIT,
                        two_sided=True, is_sky=True)
    inputs = {
        "CamVec": g.node(unreal.MaterialExpressionCameraVectorWS),
        "T": g.time(),
        "Horizon": g.vector("HorizonColor", unreal.LinearColor(0.018, 0.032, 0.075, 1.0)),
        "Zenith": g.vector("ZenithColor", unreal.LinearColor(0.004, 0.008, 0.028, 1.0)),
        "MoonDir": g.vector("MoonDirection", unreal.LinearColor(0.6, 0.3, 0.74, 0.0)),
        "MoonSize": g.scalar("MoonSize", 0.055),
        "MoonIntensity": g.scalar("MoonIntensity", 4.0),
        "StarTex": g.texture_object(textures["T_StarField"]),
        "StarTiling": g.scalar("StarTiling", 3.0),
        "StarIntensity": g.scalar("StarIntensity", 0.5),
        "CloudTex": g.texture_object(textures["T_CloudNoise"]),
        "CloudScale": g.scalar("CloudScale", 0.35),
        "CloudSpeed": g.scalar("CloudSpeed", 0.004),
        "CloudCoverage": g.scalar("CloudCoverage", 0.52),
        "CloudDark": g.vector("CloudDarkColor", unreal.LinearColor(0.010, 0.016, 0.035, 1.0)),
        "CloudLit": g.vector("CloudLitColor", unreal.LinearColor(0.10, 0.12, 0.16, 1.0)),
        "SkyIntensity": g.scalar("SkyIntensity", 1.0),
    }
    result = g.custom(NIGHT_SKY_CODE, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "NightSky")
    g.output(result, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()
    dd.new_instance("MI_NightSky", g.material)
    return g.material


def main():
    enable_custom_stencil()
    layout = dd_layout.ensure()
    REPORT["layout"] = layout
    textures = import_textures()
    import_meshes()
    build_mpc(layout)
    darkness = build_darkness_function()
    build_caustics(textures, darkness)
    build_dream_sky(textures)
    build_night_sky(textures)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    dd.write_report_json("step3_build.json", REPORT)
    dd.log("step3 done")


main()
