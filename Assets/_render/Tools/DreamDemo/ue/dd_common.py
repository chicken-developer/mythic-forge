"""Shared helpers for the DreamDemo Unreal Python scripts.

Every script is idempotent: assets are reused and their graphs rebuilt in place, and level
actors created by a step carry the tag "DreamDemo.<step>" so the step can remove them first.
"""
import json
import os

import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

ROOT = "/Game/DreamDemo"
MAP_PATH = "/Game/Kingdom/Maps/L_Kingdom_Main"
PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
RAW_DIR = os.path.join(PROJECT_DIR, "SourceAssets", "DreamDemo")
REPORT_DIR = os.path.join(PROJECT_DIR, "RenderTests", "DreamDemo")
LAYOUT_JSON = os.path.join(PROJECT_DIR, "Tools", "DreamDemo", "ue", "layout.json")

TAG_DAY = "State_Day"
TAG_NIGHT = "State_Night"
TAG_DREAM = "State_Dream"
TAG_STATE = "DreamState"      # every actor that takes part in the state switch
BP_FIELD = ROOT + "/Blueprints/BP_DreamInstancedField"
BP_WORLD_STATE = ROOT + "/Blueprints/BP_DreamWorldState"


def log(msg):
    unreal.log("DREAMDEMO " + str(msg))


def warn(msg):
    unreal.log_warning("DREAMDEMO " + str(msg))


def asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def load(path):
    asset = eal.load_asset(path) if path.startswith("/Game/") else None
    return asset or unreal.load_asset(path)


def exists(path):
    return eal.does_asset_exist(path)


def save(obj_or_path):
    path = obj_or_path if isinstance(obj_or_path, str) else obj_or_path.get_path_name().split(".")[0]
    eal.save_asset(path, only_if_is_dirty=False)


def create_asset(name, folder, asset_class, factory):
    """Return the existing asset at ROOT/folder/name or create it."""
    package_path = "%s/%s" % (ROOT, folder)
    full = "%s/%s" % (package_path, name)
    if exists(full):
        return load(full)
    eal.make_directory(package_path)
    asset = asset_tools().create_asset(name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError("could not create %s" % full)
    return asset


def linear_from_hex(hex_color, alpha=1.0):
    h = hex_color.lstrip("#")
    vals = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        vals.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return unreal.LinearColor(vals[0], vals[1], vals[2], alpha)


def write_report_json(name, data):
    os.makedirs(REPORT_DIR, exist_ok=True)
    path = os.path.join(REPORT_DIR, name)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=1)
    return path


def load_layout():
    with open(LAYOUT_JSON, encoding="utf-8") as fh:
        return json.load(fh)


# ---------------------------------------------------------------------------
# Material graphs
# ---------------------------------------------------------------------------

class Graph:
    """Small wrapper over MaterialEditingLibrary for materials and material functions."""

    def __init__(self, material=None, function=None):
        self.material = material
        self.function = function
        self._row = 0
        self.mpc = None

    # -- nodes ---------------------------------------------------------------
    def node(self, cls, **props):
        x = -1400 + (self._row % 6) * 220
        y = -600 + (self._row // 6) * 180
        self._row += 1
        if self.material is not None:
            expr = mel.create_material_expression(self.material, cls, x, y)
        else:
            expr = mel.create_material_expression_in_function(self.function, cls, x, y)
        if expr is None:
            raise RuntimeError("could not create %s" % cls)
        for key, value in props.items():
            expr.set_editor_property(key, value)
        return expr

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=float(value))

    def const3(self, color):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=color)

    def scalar(self, name, default, group="DreamDemo"):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name,
                         default_value=float(default), group=group)

    def vector(self, name, default, group="DreamDemo"):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=default, group=group)

    def collection(self, name):
        if self.mpc is None:
            self.mpc = load(ROOT + "/MPC_Dream")
        expr = self.node(unreal.MaterialExpressionCollectionParameter)
        expr.set_editor_property("collection", self.mpc)
        expr.set_editor_property("parameter_name", name)
        return expr

    def time(self):
        return self.node(unreal.MaterialExpressionTime)

    def texture_object(self, texture):
        return self.node(unreal.MaterialExpressionTextureObject, texture=texture)

    def texture_sample(self, texture, uvs=None):
        expr = self.node(unreal.MaterialExpressionTextureSample, texture=texture)
        if uvs is not None:
            self.connect(uvs, expr, "UVs")
        return expr

    def scene_texture(self, texture_id):
        return self.node(unreal.MaterialExpressionSceneTexture, scene_texture_id=texture_id)

    def function_call(self, function, argument=None):
        expr = self.node(unreal.MaterialExpressionMaterialFunctionCall)
        expr.set_material_function(function)
        if argument is not None:
            self.connect(argument, expr, "")
        return expr

    def custom(self, code, inputs, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, desc="Custom"):
        """inputs: ordered dict name -> node or (node, output_name)."""
        expr = self.node(unreal.MaterialExpressionCustom)
        expr.set_editor_property("description", desc)
        expr.set_editor_property("output_type", output_type)
        expr.set_editor_property("code", code.strip() + "\n")
        custom_inputs = []
        for name in inputs:
            item = unreal.CustomInput()
            item.set_editor_property("input_name", name)
            custom_inputs.append(item)
        expr.set_editor_property("inputs", custom_inputs)
        for name, src in inputs.items():
            self.connect(src, expr, name)
        return expr

    # -- wiring --------------------------------------------------------------
    @staticmethod
    def _split(src):
        if isinstance(src, tuple):
            return src[0], src[1]
        return src, ""

    def connect(self, src, dst, dst_input=""):
        node, output = self._split(src)
        if not mel.connect_material_expressions(node, output, dst, dst_input):
            raise RuntimeError("connect failed: %s.%s -> %s.%s" % (
                node.get_name(), output, dst.get_name(), dst_input))

    def output(self, src, prop):
        node, output = self._split(src)
        if not mel.connect_material_property(node, output, prop):
            raise RuntimeError("output connect failed: %s -> %s" % (node.get_name(), prop))

    def finish(self):
        if self.material is not None:
            mel.layout_material_expressions(self.material)
            mel.recompile_material(self.material)
            save(self.material)
        else:
            mel.layout_material_function_expressions(self.function)
            mel.update_material_function(self.function, None)
            save(self.function)


def new_material(name, folder="Materials", **props):
    material = create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(material)
    for key, value in props.items():
        material.set_editor_property(key, value)
    return Graph(material=material)


def new_function(name, folder="Materials"):
    function = create_asset(name, folder, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    mel.delete_all_material_expressions_in_function(function)
    return Graph(function=function)


def new_instance(name, parent, folder="Materials", scalars=None, vectors=None, textures=None, switches=None):
    instance = create_asset(name, folder, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    for key, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(instance, key, float(value))
    for key, value in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(instance, key, value)
    for key, value in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(instance, key, value)
    for key, value in (switches or {}).items():
        mel.set_material_instance_static_switch_parameter_value(instance, key, bool(value))
    mel.update_material_instance(instance)
    save(instance)
    return instance


# ---------------------------------------------------------------------------
# Level helpers
# ---------------------------------------------------------------------------

def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def all_actors():
    return actor_subsystem().get_all_level_actors()


def open_demo_map():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("cannot load %s" % MAP_PATH)
    return editor_world()


def save_demo_map():
    if not unreal.EditorLoadingAndSavingUtils.save_map(editor_world(), MAP_PATH):
        raise RuntimeError("cannot save %s" % MAP_PATH)


def remove_step_actors(step_tag):
    removed = 0
    for actor in list(all_actors()):
        if actor.actor_has_tag(step_tag):
            actor_subsystem().destroy_actor(actor)
            removed += 1
    return removed


def spawn(cls_or_asset, label, location, rotation=None, step_tag=None, tags=(), folder=None, scale=None):
    """rotation is (pitch, yaw, roll)."""
    rot = unreal.Rotator(roll=0, pitch=0, yaw=0)
    if rotation is not None:
        rot = unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1])
    loc = unreal.Vector(*location)
    # Actor factories are unreliable in a commandlet, so spawn the class and assign the asset.
    if cls_or_asset is None:
        raise RuntimeError("spawn failed, asset missing: %s" % label)
    if isinstance(cls_or_asset, (type, unreal.Class)):
        actor = actor_subsystem().spawn_actor_from_class(cls_or_asset, loc, rot)
    elif isinstance(cls_or_asset, unreal.StaticMesh):
        actor = actor_subsystem().spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
        actor.static_mesh_component.set_static_mesh(cls_or_asset)
    elif isinstance(cls_or_asset, unreal.SkeletalMesh):
        actor = actor_subsystem().spawn_actor_from_class(unreal.SkeletalMeshActor, loc, rot)
        actor.skeletal_mesh_component.set_skinned_asset_and_update(cls_or_asset)
    elif isinstance(cls_or_asset, unreal.Blueprint):
        actor = actor_subsystem().spawn_actor_from_class(cls_or_asset.generated_class(), loc, rot)
    else:
        actor = actor_subsystem().spawn_actor_from_object(cls_or_asset, loc, rot)
    if actor is None:
        raise RuntimeError("spawn failed: %s" % label)
    actor.set_actor_label(label)
    all_tags = list(tags)
    if any(t in all_tags for t in (TAG_DAY, TAG_NIGHT, TAG_DREAM)):
        all_tags.append(TAG_STATE)
    all_tags.append("DreamDemo")
    if step_tag:
        all_tags.append(step_tag)
    actor.tags = [unreal.Name(t) for t in all_tags]
    if folder:
        actor.set_folder_path(folder)
    if scale is not None:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def add_tags(actor, tags, folder=None):
    current = [str(t) for t in actor.tags]
    if any(t in tags for t in (TAG_DAY, TAG_NIGHT, TAG_DREAM)):
        tags = list(tags) + [TAG_STATE]
    for tag in tags:
        if tag not in current:
            current.append(tag)
    actor.tags = [unreal.Name(t) for t in current]
    if folder:
        actor.set_folder_path(folder)


def find_actor(label):
    for actor in all_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def is_ground(actor):
    """Landscape, or the Kingdom terrain mesh (SM_KD_Terrain)."""
    if isinstance(actor, unreal.LandscapeProxy):
        return True
    if isinstance(actor, unreal.StaticMeshActor):
        mesh = actor.static_mesh_component.static_mesh
        return mesh is not None and "Terrain" in mesh.get_name()
    return False


def controller():
    for actor in all_actors():
        if actor.get_class().get_name().startswith("BP_DreamWorldState"):
            return actor
    return None


def apply_day():
    """Leave the map in the DAY state (the state it is saved in)."""
    actor = controller()
    if actor is not None:
        actor.call_method("ApplyDay")


def place_instances(actor, mesh, material, count, seed, inner_radius, outer_radius, min_height, max_height,
                    min_scale=1.0, max_scale=1.0, arc_start=0.0, arc=360.0, face_tangent=False, random_yaw=False):
    """Fills the Instances component of a BP_DreamInstancedField actor (ring around the actor)."""
    import math
    import random
    comp = actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
    comp.clear_instances()
    comp.set_static_mesh(mesh)
    comp.set_material(0, material)
    rng = random.Random(seed)
    transforms = []
    for _ in range(count):
        angle_deg = arc_start + rng.random() * arc
        angle = math.radians(angle_deg)
        radius = math.sqrt(inner_radius ** 2 + (outer_radius ** 2 - inner_radius ** 2) * rng.random())
        height = min_height + (max_height - min_height) * rng.random()
        scale = min_scale + (max_scale - min_scale) * rng.random()
        yaw = angle_deg + 90.0 if face_tangent else (rng.random() * 360.0 if random_yaw else 0.0)
        transforms.append(unreal.Transform(
            location=unreal.Vector(math.cos(angle) * radius, math.sin(angle) * radius, height),
            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
            scale=unreal.Vector(scale, scale, scale)))
    comp.add_instances(transforms, False, False)
    return comp.get_instance_count()


_GROUND_REFS = None


def nearest_ground_reference(x, y):
    """Fallback when a trace finds no ground: the pivot height of the nearest KD tree or house.
    The Kingdom terrain answers no line trace in a commandlet, and the plaza tiers are flat."""
    global _GROUND_REFS
    if _GROUND_REFS is None:
        _GROUND_REFS = []
        for actor in all_actors():
            if isinstance(actor, unreal.StaticMeshActor) and not actor.actor_has_tag("DreamDemo"):
                mesh = actor.static_mesh_component.static_mesh
                if mesh is not None and mesh.get_name().startswith(("SM_tree", "SM_med_", "SM_goth_")):
                    loc = actor.get_actor_location()
                    _GROUND_REFS.append((loc.x, loc.y, loc.z))
    if not _GROUND_REFS:
        return None
    return min(_GROUND_REFS, key=lambda r: (r[0] - x) ** 2 + (r[1] - y) ** 2)[2]


def ground_z(world, x, y, default_z, top=20000.0, bottom=-5000.0):
    """Height of the ground under (x, y); ignores everything that is not ground."""
    fallback = nearest_ground_reference(x, y)
    if fallback is not None:
        default_z = fallback
    ignore = []
    for _ in range(8):
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, top), unreal.Vector(x, y, bottom),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
            unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return default_z
        # HitResult fields are not exposed as attributes; to_tuple() keeps the struct order
        # (..., location, impact_point, ..., hit_actor, ...).
        parts = hit.to_tuple()
        location = next(p for p in parts if isinstance(p, unreal.Vector))
        actor = next((p for p in parts if isinstance(p, unreal.Actor)), None)
        if actor is None or is_ground(actor):
            return location.z
        ignore.append(actor)
    return default_z
