"""Regression checks for the native SOP construction graph (run with hython)."""
import argparse
import copy
import json
import math
from pathlib import Path
import time

import hou
import build_hda as B
import native_network as N
from assembly_validation import check, polygon_area

ROOT = Path(__file__).resolve().parent


def configure():
    from production_pipeline import configure as production_configure
    return production_configure()


def set_config(node, config):
    for name,value in config["parameters"].items():
        node.parm(name).set(int(value) if isinstance(value,bool) else value)
    for name,value in config["seeds"].items():
        node.parm("seed_"+name).set(value)


def build(config, name="native_validation"):
    obj = hou.node("/obj").createNode("geo", name, run_init_scripts=False)
    node = obj.createNode("subnet", "generator")
    node.setParmTemplateGroup(N.interface(B.normalize(config)))
    set_config(node, B.normalize(config))
    N.rebuild(node)
    node.setDisplayFlag(True)
    node.setRenderFlag(True)
    return obj, node


def validate(node, config, audit=True):
    if node.userData('native_graph_version')=='3.0':
        from production_pipeline import validate as production_validate
        return production_validate(node)
    started = time.perf_counter()
    try:
        node.cook(force=True)
    except hou.OperationFailed:
        pass
    nodes = [node] + list(node.allSubChildren(recurse_in_locked_nodes=False))
    errors = [{"node": n.path(), "errors": list(n.errors()), "warnings": list(n.warnings())}
              for n in nodes if n.errors() or n.warnings()]
    if errors:
        raise AssertionError(json.dumps(errors[:12],indent=2))
    geometry = node.geometry()
    if not geometry.prims():
        raise AssertionError("Empty native geometry")
    reference = B.generate(config)
    expected_area = sum(polygon_area([reference.vertices[i] for i in face]) for face in reference.faces)
    area = sum(p.intrinsicValue("measuredarea") for p in geometry.prims())
    relative_area_error = abs(area-expected_area)/expected_area
    bounds = geometry.boundingBox()
    expected_min = [min(p[i] for p in reference.vertices) for i in range(3)]
    expected_max = [max(p[i] for p in reference.vertices) for i in range(3)]
    bound_error = max(abs(a-b) for a,b in zip(list(bounds.minvec())+list(bounds.maxvec()),
                                             expected_min+expected_max))
    reversed_normals = sum(p.normal().dot(hou.Vector3(p.vertices()[0].attribValue("N"))) < -1e-7
                           for p in geometry.prims())
    python_sops = [n.path() for n in nodes if n.type().name() == "python"]
    finite = all(math.isfinite(v) for p in geometry.points() for v in p.position())
    # Native Clip retains narrow coplanar roof strips discarded by the legacy solver.
    # Keep the bound/normal/intersection checks strict; allow at most 0.1% total area difference.
    area_tolerance = 1e-3
    if relative_area_error > area_tolerance or (expected_area-area)/expected_area > 2e-5:
        actual, expected = {}, {}
        for poly in geometry.prims():
            name = poly.attribValue("name")
            actual[name] = actual.get(name,0) + poly.intrinsicValue("measuredarea")
        for face,name in zip(reference.faces,reference.names):
            expected[name] = expected.get(name,0) + polygon_area([reference.vertices[i] for i in face])
        differences = sorted(((name,actual.get(name,0)-expected.get(name,0),actual.get(name,0),expected.get(name,0))
                              for name in set(actual)|set(expected)),key=lambda row:abs(row[1]),reverse=True)
        print("AREA_DIFFERENCES", json.dumps(differences[:18]),flush=True)
        raise AssertionError("Surface area differs: native=%s reference=%s relative=%s" %
                             (area, expected_area, relative_area_error))
    if bound_error > 1e-4 or reversed_normals or python_sops or not finite:
        raise AssertionError(str(dict(bounds=bound_error,normals=reversed_normals,python=python_sops,finite=finite)))
    result = dict(points=len(geometry.points()), primitives=len(geometry.prims()),
                  surface_area=area, reference_surface_area=expected_area,
                  reference_primitives=len(reference.faces), surface_area_relative_tolerance=area_tolerance,
                  surface_area_loss_tolerance=2e-5,
                  relative_area_error=relative_area_error, maximum_bound_error=bound_error,
                  reversed_normals=reversed_normals, python_sops=len(python_sops),
                  native_nodes=len(nodes), finite=finite, errors=errors)
    if audit:
        proxy = copy.copy(reference)
        proxy.vertices = [tuple(p.position()) for p in geometry.points()]
        proxy.faces = [[v.point().number() for v in p.vertices()] for p in geometry.prims()]
        proxy.names = [p.attribValue("name") for p in geometry.prims()]
        audit_result = check(proxy, config)
        if audit_result["residual_intrusions"]:
            raise AssertionError(json.dumps(audit_result))
        result["assembly_audit"] = audit_result
    result["seconds"] = round(time.perf_counter()-started,3)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--preset", default="variant_01")
    args = parser.parse_args()
    records = configure()
    selected = records if args.all else [r for r in records if r["variant_id"] == args.preset]
    if not selected:
        parser.error("Unknown preset")
    results, failures = [], []
    for record in selected:
        obj = None
        started = time.perf_counter()
        try:
            obj, node = build(record)
            built = time.perf_counter()-started
            print("BUILT",record["variant_id"],"nodes",len(node.allSubChildren(recurse_in_locked_nodes=False)),
                  "seconds",round(built,3),flush=True)
            result = validate(node,record)
            result.update(variant_id=record["variant_id"],build_seconds=round(built,3))
            results.append(result)
            print(json.dumps({k:v for k,v in result.items() if k != "assembly_audit"}),flush=True)
        except Exception as error:
            failure = dict(variant_id=record["variant_id"],error=str(error))
            failures.append(failure)
            print("FAILED",json.dumps(failure),flush=True)
        finally:
            if obj is not None:
                obj.destroy()
    (ROOT/"reports").mkdir(exist_ok=True)
    report = dict(status="failed" if failures else "passed",houdini=hou.applicationVersionString(),
                  graph_version="2.2",results=results,failures=failures)
    filename = "native_validation.json" if args.all else "native_validation_"+args.preset+".json"
    (ROOT/"reports"/filename).write_text(json.dumps(report,indent=2),encoding="utf-8")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
