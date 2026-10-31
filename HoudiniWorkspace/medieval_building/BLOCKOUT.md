# Live blockout input contract

The HDA is a SOP asset with one optional input. First package/cook with layout_input_enable=false. For live layout: connect un-packed polygon boxes to input 0, then enable layout_input_enable. Give every polygon of each box a primitive string attribute `layout_role`.

Roles: main (required), wing (L footprint), tower (hybrid), workshop (when enabled). Exactly one axis-aligned eight-corner box per role; multiple boxes of the same role and rotated boxes are rejected. Round/tower proxies require square XZ dimensions. Dimensions must remain within parameter_schema.json constraints.

Y-up, metres. Main height must be floor_count * floor_height_m; bottom sets foundation_height_m. All secondary boxes share that bottom. Wing height follows the same floor grid. Workshop must be 2.5m high and attached immediately to the right of main. scale must be 1. Input controls layout dimensions; facade/style/interior parameters remain editable.

Use build_hda.proxy_boxes(config) for valid sample role/center/size JSON. Use resolve_layout(config, boxes) for offline constraint checks, and boxes_from_geometry for native input parsing. Test real input recook and parameter changes in Houdini before claiming engine support. This is a constrained grammar, not arbitrary box massing/boolean architecture.

The legacy build_hda.py CLI mentions create_scene.py for layout packaging; that older scene script is not part of this handoff. Use create_hda.py for initial parameter-mode export, then connect the described live input to the exported asset. Do not invoke a missing legacy script.
