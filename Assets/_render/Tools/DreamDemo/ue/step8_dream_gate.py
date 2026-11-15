"""Step 8 (optional): dream gate transition material and volume.

The sequence lives in BP_DreamWorldState (event Gate, keys G and H): cover the screen in
1.2 s, swap the state while covered, hold 0.1 s, reveal in 1.2 s. Waking up uses 0.6 s per
half. The material computes the cover from GateMode / GateStartTime / GateDuration, so the
Blueprint needs no Tick. WorldBlend jumps while the screen is covered.

Run:  bash Tools/DreamDemo/ue_run.sh cmd Tools/DreamDemo/ue/step8_dream_gate.py dd_step8
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                                "Tools", "DreamDemo", "ue"))
import dd_common as dd  # noqa: E402

STEP = "DreamDemo.Step8"

GATE_CODE = r"""
float3 scene = Scene.rgb;
// GateMode 1 covers the screen over GateDuration, 2 reveals it again; the timing is game time
float k = saturate((T - Start) / max(Duration, 0.01));
float p = Mode > 1.5 ? 1.0 - k : (Mode > 0.5 ? k : 0.0);
if (p <= 0.0001)
{
    return scene;
}
// ring growing from GateCenter until it covers the whole screen
float aspect = View.ViewSizeAndInvSize.x * View.ViewSizeAndInvSize.w;
float2 q = (UV - Center.xy) * float2(aspect, 1.0);
float ang = atan2(q.y, q.x);
float r = length(q) + sin(ang * 9.0 + T * 2.0) * 0.02 + sin(ang * 23.0 - T * 3.0) * 0.012;
float maxR = length(float2(aspect, 1.0)) * (0.5 + length(Center.xy - 0.5)) + 0.12;
float front = p * maxR;
float cover = 1.0 - smoothstep(front - 0.07, front, r);
float ring = exp(-pow((r - front) / 0.045, 2.0)) * (1.0 - smoothstep(0.97, 1.0, p));

// sparkling dust along the edge
float2 cell = floor(UV * View.ViewSizeAndInvSize.xy / 5.0);
float h = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float sparkle = step(0.94, h) * exp(-pow((r - front) / 0.11, 2.0)) * (0.5 + 0.5 * sin(T * 18.0 + h * 50.0))
              * (1.0 - smoothstep(0.97, 1.0, p));

float3 veil = Tint.rgb * 0.75 + 0.2;     // display-referred: scene colour here is already pre-exposed
float3 result = lerp(scene, veil, cover);
result += Tint.rgb * 2.2 * ring + sparkle * 2.5;
return result;
"""


def main():
    blend_location = getattr(unreal.BlendableLocation, "BL_SCENE_COLOR_AFTER_DOF",
                             getattr(unreal.BlendableLocation, "BL_BEFORE_TONEMAPPING", None))
    g = dd.new_material("M_PP_DreamGate", material_domain=unreal.MaterialDomain.MD_POST_PROCESS,
                        blendable_location=blend_location, blendable_priority=10)
    result = g.custom(GATE_CODE, {
        "Scene": g.scene_texture(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),
        "UV": (g.node(unreal.MaterialExpressionScreenPosition), "ViewportUV"),
        "T": g.time(),
        "Mode": g.collection("GateMode"),
        "Start": g.collection("GateStartTime"),
        "Duration": g.collection("GateDuration"),
        "Center": g.collection("GateCenter"),
        "Tint": g.collection("CausticsTint"),
    }, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "DreamGate")
    g.output(result, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()

    dd.open_demo_map()
    dd.remove_step_actors(STEP)
    volume = dd.spawn(unreal.PostProcessVolume, "PPV_DreamGate", (0, 0, 500), step_tag=STEP, folder="DreamDemo")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)
    volume.set_editor_property("enabled", False)      # the controller enables it during a transition
    settings = volume.settings
    settings.set_editor_property("weighted_blendables", unreal.WeightedBlendables(
        array=[unreal.WeightedBlendable(weight=1.0, object=g.material)]))
    volume.set_editor_property("settings", settings)

    controller = dd.controller()
    if controller is None:
        raise RuntimeError("BP_DreamWorldState missing: run step 4 first")
    controller.set_editor_property("GateVolume", volume)
    dd.apply_day()
    dd.save_demo_map()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    dd.write_report_json("step8_build.json", {
        "material": "M_PP_DreamGate", "volume": "PPV_DreamGate (unbound, disabled until a transition)",
        "controller_gate_volume": controller.get_editor_property("GateVolume").get_actor_label(),
        "gate_half_seconds": controller.get_editor_property("GateHalfSeconds"),
        "wake_half_seconds": controller.get_editor_property("WakeHalfSeconds"),
        "gate_hold_seconds": controller.get_editor_property("GateHoldSeconds"),
    })
    dd.log("step8 done")


main()
