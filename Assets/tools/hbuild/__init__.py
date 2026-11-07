"""hbuild - small framework for building clean, maintainable Houdini networks from Python.

Layout of the package
    conventions.py   naming, colours, grid spacing  (the house style every network follows)
    net.py           NetBuilder: places nodes on a grid, network boxes, sticky notes, OUT nulls
    parms.py         ParmSpec registry -> hou.ParmTemplateGroup (one source of truth for UI + presets)
    presets.py       load / apply / save / validate presets (also embedded in the HDA PythonModule)
    hda.py           turn a built subnet into an HDA with interface, PythonModule and preset section
    scene.py         asset scene: HDA instance, verification cameras, OpenGL ROPs
    netdump.py       dump any network to JSON (for the network diagram)
    validate.py      structural lint + cook check of a network
    generators/      one sub-package per generator (building, ...), each with build(), PARMS, MODULES

All modules import `hou` lazily so the package can be dry-run with tests/mock_hou.py.
"""
VERSION = "0.1.0"  # skill copy; project additions: apicheck, inspect, selftest --no-render
