#!/usr/bin/env python3
"""Validate the declarative HDA module DAG (not a Houdini cook/audit)."""

import argparse
import json
import re
import sys


MODULE_ID = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)*$")
PORT_ID = re.compile(r"^[a-z][a-z0-9_]*$")
STAGE_ID = re.compile(r"^[A-Z][A-Z0-9_]*$")


def _names(values, label, errors, pattern=PORT_ID):
    if not isinstance(values, list) or not all(isinstance(x, str) for x in values):
        errors.append("{} must be a list of strings".format(label))
        return []
    if len(values) != len(set(values)):
        errors.append("{} contains duplicates".format(label))
    for name in values:
        if not pattern.fullmatch(name):
            errors.append("{} contains invalid name {!r}".format(label, name))
    return values


def validate(document):
    """Return (errors, topological_order); order is empty on failure."""
    errors = []
    if not isinstance(document, dict):
        return ["root must be a JSON object"], []
    if document.get("schema_version") != 1:
        errors.append("schema_version must equal 1")
    if not isinstance(document.get("asset_name"), str) or not document["asset_name"].strip():
        errors.append("asset_name must be nonempty")

    modules = document.get("modules")
    if not isinstance(modules, list) or not modules:
        return errors + ["modules must be a nonempty list"], []

    registry = {}
    for index, module in enumerate(modules):
        label = "modules[{}]".format(index)
        if not isinstance(module, dict):
            errors.append("{} must be a JSON object".format(label))
            continue
        module_id = module.get("module_id")
        if not isinstance(module_id, str) or not MODULE_ID.fullmatch(module_id):
            errors.append("{}.module_id must be a stable lowercase dotted ID".format(label))
            continue
        if module_id in registry:
            errors.append("duplicate module_id {!r}".format(module_id))
            continue
        registry[module_id] = module

        stage = module.get("stage")
        if not isinstance(stage, str) or not STAGE_ID.fullmatch(stage):
            errors.append("{}.stage must be an uppercase stage name".format(label))
        path = module.get("path")
        if (not isinstance(path, str) or not isinstance(stage, str)
                or not path.startswith(stage + "/") or len(path.split("/")) < 2
                or any(not PORT_ID.fullmatch(part) for part in path.split("/")[1:])):
            errors.append("{}.path must be STAGE/module_subnet[/helper_subnet]".format(label))
        if not isinstance(module.get("owner"), str) or not module["owner"].strip():
            errors.append("{}.owner must be a nonempty generator name".format(label))
        if not isinstance(module.get("responsibility"), str) or not module["responsibility"].strip():
            errors.append("{}.responsibility must be nonempty".format(label))
        _names(module.get("outputs"), label + ".outputs", errors)
        _names(module.get("depends_on"), label + ".depends_on", errors, MODULE_ID)
        _names(module.get("params", []), label + ".params", errors)
        _names(module.get("attributes", []), label + ".attributes", errors)
        if not isinstance(module.get("version"), int) or isinstance(module.get("version"), bool) or module["version"] < 1:
            errors.append("{}.version must be positive integer".format(label))
        if "debug_endpoint" in module and not isinstance(module["debug_endpoint"], str):
            errors.append("{}.debug_endpoint must be string".format(label))
        if "diagnostic_only" in module and not isinstance(module["diagnostic_only"], bool):
            errors.append("{}.diagnostic_only must be boolean".format(label))

        ports = module.get("inputs")
        if not isinstance(ports, dict):
            errors.append("{}.inputs must be a map of port to module_id:output_port".format(label))
        else:
            for port, target in ports.items():
                if not isinstance(port, str) or not PORT_ID.fullmatch(port):
                    errors.append("{}.inputs has invalid port {!r}".format(label, port))
                if not isinstance(target, str) or target.count(":") != 1:
                    errors.append("{}.inputs.{} must be module_id:output_port".format(label, port))

    dependency_graph = {}
    for module_id, module in registry.items():
        deps = module.get("depends_on", [])
        deps = deps if isinstance(deps, list) else []
        deps = [x for x in deps if isinstance(x, str)]
        dependency_graph[module_id] = deps
        for dependency in deps:
            if dependency not in registry:
                errors.append("{} depends on missing module {}".format(module_id, dependency))
            if dependency == module_id:
                errors.append("{} depends on itself".format(module_id))

        ports = module.get("inputs", {})
        if not isinstance(ports, dict):
            continue
        for port, target in ports.items():
            if not isinstance(target, str) or target.count(":") != 1:
                continue
            producer, output = target.split(":", 1)
            if not MODULE_ID.fullmatch(producer) or not PORT_ID.fullmatch(output):
                errors.append("{}.inputs.{} has invalid producer/output".format(module_id, port))
                continue
            if producer not in registry:
                errors.append("{}.inputs.{} uses missing producer {}".format(module_id, port, producer))
                continue
            producer_outputs = registry[producer].get("outputs", [])
            producer_outputs = producer_outputs if isinstance(producer_outputs, list) else []
            if output not in producer_outputs:
                errors.append("{}.inputs.{} uses unexposed output {}:{}".format(module_id, port, producer, output))
            if producer not in deps:
                errors.append("{}.inputs.{} missing declared depends_on {}".format(module_id, port, producer))

    final_module = document.get("final_module")
    if not isinstance(final_module, str) or final_module not in registry:
        errors.append("final_module must name an existing module")
        final_module = None

    order = []
    visiting, visited = set(), set()

    def visit(module_id):
        if module_id in visiting:
            errors.append("dependency cycle involving {}".format(module_id))
            return
        if module_id in visited:
            return
        visiting.add(module_id)
        for dependency in dependency_graph.get(module_id, []):
            if dependency in registry:
                visit(dependency)
        visiting.remove(module_id)
        visited.add(module_id)
        order.append(module_id)

    for module_id in registry:
        visit(module_id)

    if final_module in registry:
        reachable = set()

        def traverse(module_id):
            if module_id in reachable:
                return
            reachable.add(module_id)
            for dependency in dependency_graph.get(module_id, []):
                if dependency in registry:
                    traverse(dependency)

        traverse(final_module)
        for module_id, module in registry.items():
            if module_id not in reachable and module.get("diagnostic_only") is not True:
                errors.append("{} does not reach final_module {}".format(module_id, final_module))

    return errors, (order if not errors else [])


def self_test():
    good = {
        "schema_version": 1,
        "asset_name": "smoke_building",
        "final_module": "output.game_export",
        "modules": [
            {"module_id": "layout.semantic", "stage": "LAYOUT", "path": "LAYOUT/semantic",
             "owner": "smoke_builder", "responsibility": "Publish wall guides",
             "inputs": {}, "outputs": ["wall_guides"], "depends_on": [], "version": 1},
            {"module_id": "shell.wall", "stage": "SHELL", "path": "SHELL/wall",
             "owner": "smoke_builder", "responsibility": "Create shell",
             "inputs": {"guide": "layout.semantic:wall_guides"}, "outputs": ["shell_geo"],
             "depends_on": ["layout.semantic"], "version": 1},
            {"module_id": "output.game_export", "stage": "OUTPUT", "path": "OUTPUT/game_export",
             "owner": "smoke_builder", "responsibility": "Assemble output",
             "inputs": {"shell": "shell.wall:shell_geo"}, "outputs": ["geometry"],
             "depends_on": ["shell.wall"], "version": 1}
        ]
    }
    errors, order = validate(good)
    assert not errors, errors
    assert order == ["layout.semantic", "shell.wall", "output.game_export"]

    import copy
    duplicate = copy.deepcopy(good)
    duplicate["modules"].append(copy.deepcopy(duplicate["modules"][0]))
    assert any("duplicate" in msg for msg in validate(duplicate)[0])

    missing_port = copy.deepcopy(good)
    missing_port["modules"][1]["inputs"]["guide"] = "layout.semantic:missing"
    assert any("unexposed output" in msg for msg in validate(missing_port)[0])

    cycle = copy.deepcopy(good)
    cycle["modules"][0]["depends_on"].append("output.game_export")
    assert any("cycle" in msg for msg in validate(cycle)[0])

    orphan = copy.deepcopy(good)
    orphan["modules"].append({"module_id": "details.orphan", "stage": "DETAILS", "path": "DETAILS/orphan",
                              "owner": "smoke_builder", "responsibility": "Unused",
                              "inputs": {}, "outputs": ["details"], "depends_on": [], "version": 1})
    assert any("does not reach final_module" in msg for msg in validate(orphan)[0])
    print("PASS: module graph validator self-test (valid, duplicate, missing port, cycle, orphan)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("graph", nargs="?", help="module_graph.json to validate")
    parser.add_argument("--self-test", action="store_true", help="run built-in tests")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.graph:
        parser.error("provide module_graph.json or --self-test")
    try:
        with open(args.graph, "r", encoding="utf-8") as file:
            graph = json.load(file)
    except (OSError, ValueError) as exc:
        print("ERROR: cannot read module graph: {}".format(exc), file=sys.stderr)
        return 2
    errors, order = validate(graph)
    for error in errors:
        print("ERROR: {}".format(error), file=sys.stderr)
    if errors:
        print("FAIL: {} module graph error(s)".format(len(errors)), file=sys.stderr)
        return 1
    print("PASS: {} modules, build order: {}".format(len(order), " -> ".join(order)))
    print("NOTE: This validates JSON contracts only; a Houdini graph/cook audit is still required.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
