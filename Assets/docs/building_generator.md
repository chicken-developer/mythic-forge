# Building generator — `bldg::building_generator::1.0`

One generator HDA for the medieval and the gothic buildings; every concept is a preset.

References (`img_refs/`, reorganised 2026-11-08):
- `medievals_refs/` — building concept sheets; each one is a preset (`NormalHouse_03` = `NormalHouse_V1`, same bytes).
- `medievals_moodboard/`, `gothics_moodboard/` — mood/style only. Gothic has no building concept sheets, so the
  three gothic presets (`goth_twin_tower_cathedral`, `goth_academy`, `goth_chapel`) are style studies built from
  moodboard images; their presets carry `"ref_kind": "moodboard"` and their compare sheets say so.
HDA: `hda/bldg_building_generator.hda` (all presets embedded: Presets tab -> Preset + Apply, also in Unreal).
Source of truth: `tools/hbuild/generators/building/` (`spec.py` = interface, `build.py` = network, `vex/*.vfl`).
Never edit the VEX only inside the HDA: port the change back into the `.vfl` and rebuild.

## Outputs
| output | content |
|---|---|
| 0 | selected by `Output 0 Quality` (`preview_quality`): LOW or HIGH |
| 1 | LOW: modular geometry, flat walls and roof planes, Cd + `unreal_material`, metric box UVs, normals, point attribute `i@piece` (one id per box / slab, used to isolate the high->low bake) |
| 2 | HIGH: LOW + PolyBevel (`bevel_w`, `bevel_div`) on the parts listed in `bevel_parts` (frames, timber, trims, posts, buttresses, chimneys ...). Walls, roof planes, glass and fabric are never bevelled — their detail comes from Substance Designer. |

## Network (one network box per module, each ends in `OUT_<MODULE>`)
| row | module | job |
|---|---|---|
| 0 | FOOTPRINT | rectangle or input curve -> prim 0 ground outline, prim 1 upper outline (jetty) |
| 0 | WING_PLAN | wing footprints from wing slots a/b (side, size, storeys, patterns, roof) |
| 0 | LAYOUT | one point per facade bay for main body + wings (`s@module`, `i@floor`, `i@edge`, `i@bay`, `i@nbays`, `i@poly`, `f@bay_width`) |
| 0 | MOD_W0 / W / D / S / WA / DA / WR | one bay each in module space, copied onto its layout points |
| 1 | TRIMS, CORE, ROOF | plinth/bands/cornice/pilasters (quoins); closed inset volumes, or with `interior` the interior shell (floor slabs, ceiling, stairs, stair holes by Boolean); gable/hip/flat roof + verge + ridge + chimneys |
| 1 | WINGS | wing volumes (or floor slabs + ceiling with `interior`) + cross gable / shed / parallel gable / flat roofs |
| 1 | DORMERS | gabled dormers on the main roof |
| 1 | CANOPY | 3 slots: porch roof slab, striped fabric awning with scalloped valance, pergola; posts or brackets |
| 1 | BALCONY | deck, railing, balusters, brackets |
| 1 | TOWER | 2 slots: square / polygon towers, turrets on every corner, flues; flat or pyramid/spire caps, corner pinnacles |
| 1 | TIMBER | half-timber overlay from the layout: posts, plates, sill rails, braces (diagonal / cross / chevron), gable trusses |
| 1 | BUTTRESS | stepped buttresses + sloped caps + pinnacles on bay boundaries |
| 2 | OUTPUT -> HIGH -> PUBLISH | roof clip (wall-mounted parts pulled under pitched roofs near eave / verge), materials, UV, normals, `i@piece` = LOW; HIGH; switch + `output` SOPs 0/1/2 |

Facade tokens per bay: `W0` wall, `W` window, `D` door, `S` shopfront, `WA` arched window, `DA` arched door/portal,
`WR` rose window. `arch_shape` 0 round / 1 pointed. Patterns repeat to fill the bays, or map 1:1 when the token count
equals the bay count.

## Interior shell (`interior`, folder Interior)
The facade modules already are real walls (`wall_t` thick, openings through). With `interior = 1` the closed core
volumes are replaced by: a floor slab per storey (top face at the storey base, half-way into the walls), a ceiling
under the eave (top face = roof of a flat roof), straight stair flights between the storeys of the main body
(two lanes along the back or front wall, switchback; riser <= `stair_riser`, tread <= `stair_tread`) and the
stair holes cut into the slab above each flight. Wings get floor slabs + ceiling (no stairs). Shell only: no
furniture, exterior doors stay closed. All 17 presets have `interior = 1` (gothic: stone floors).
`renders/<asset>/cutaway.png` shows it (front half clipped away in the capture only).

## Roof clip (folder Roof)
Wall-mounted parts reach the wall top, while a pitched roof slopes down past the wall line, so the outer part of
top plates, posts, gable trusses, cornices, pilasters and shop signs used to stick out of the roof near the eave
and the verge. OUTPUT / `roof_clip` pulls every point that is above the upper face of a roof slab by less than
`roof_clip` (0.6 m) and horizontally within `roof_clip_edge` (1 m) of a low edge of that face to `roof_clip_gap`
(3 cm; 1 cm still z-fought in the viewport) under the roof. Gable triangles, whose rakes lay exactly in the roof
surface, are lowered the same way. Parts in `roof_clip_skip` (the roof, dormers, chimneys, verge, ridge, towers,
pinnacles, canopies) are never moved; a wall above a lean-to roof is far from that roof's low edges and stays. Check:
`hython tools/check_roof_poke.py <Assets>` lists what still sits 1 mm .. 0.6 m above a roof (all 17: none).

## Bake (LOW / HIGH / normal map)
`hython tools/bake_asset.py <Assets> <asset> --resolution 4096` -> `_render/SourceAssets/HBuild/<asset>/`:
`SM_<asset>_LOW.fbx`, `SM_<asset>_HIGH.fbx` (cm, triangulated; UV0 = unique packed UVs for the normal map,
UV1 = metric tile UVs for Substance; one material slot per `mat_id` = `M_<mat_id>`), `T_<asset>_N.png` (tangent
normal, DirectX / green down, MikkT, linear, Copernicus `bakegeometrytextures::2.0`, LOW and HIGH translated apart
per `i@piece`), `T_<asset>_hits.png`, `bake.json`. Unreal: `_render/Tools/HBuild/import_render.py` (see its header).

## Commands (run with hython from `Assets/`)
```
hython tools/hb.py generator --hda hda/bldg_building_generator.hda --presets-dir presets
hython tools/hb.py asset --hda hda/bldg_building_generator.hda --preset presets/<asset>.json \
       --hip hip/<asset>.hip --renders renders/<asset> --no-render
hython tools/hb.py validate --hip hip/<asset>.hip --node /obj/<asset>/generator
```
Previews are Houdini GUI viewport flipbooks, not renders: `tools/hgui/capture_batch.py` starts one fresh
Houdini GUI per asset (HOUDINI_PATH hook `tools/hgui/houdini_path/scripts/456.py`, packages skipped), flipbooks
persp/front/side (HIGH) and persp_low through the asset cameras, then exits. `tools/compare_sheet.py` puts the
concept next to the screenshots (`renders/<asset>/compare.png`). From a Houdini Python Shell:
`exec(open(r'E:/Repos/mythic-forge/Assets/tools/hgui/hbgui.py').read()); job('08_round')`.

## Parameters


### Presets

| parm | type | default | meaning |
|---|---|---|---|
| `preset` | string |  | Presets embedded in the HDA (section presets.json) |
| `apply_preset` | button |  | Apply Preset |

### Output

| parm | type | default | meaning |
|---|---|---|---|
| `preview_quality` | menu | 0 (LOW) | what output 0 / the display shows; output 1 is always LOW, output 2 always HIGH |
| `bevel_w` | float | 0.015 | PolyBevel offset on hardsurface parts (group hs_bevel) |
| `bevel_div` | int | 2 | HIGH Bevel Divisions |
| `bevel_ignore_flat` | toggle | 1 | HIGH Ignore Flat Edges |
| `bevel_parts` | string | frame mullion sill header door bulkhead sign plinth band cornice pilaster coping chimney chimney_cap timber_* verge ridge_beam buttress pinnacle tower_band arch_frame rose_frame rose_spoke rose_hub beam post rail baluster bracket balcony_deck stair dormer_frame dormer_sill | s@part patterns that get bevelled; walls, roof planes, glass and fabric stay flat |

### Footprint

| parm | type | default | meaning |
|---|---|---|---|
| `footprint_mode` | menu | 0 (Rectangle) | Input curve: closed/open polyline on input 1 (e.g. a spline from Unreal) |
| `width` | float | 10.0 | rectangle width, metres |
| `depth` | float | 8.0 | rectangle depth, metres |

### Floors

| parm | type | default | meaning |
|---|---|---|---|
| `ground_h` | float | 3.6 | Ground Floor Height |
| `floor_n` | int | 2 | Upper Floors |
| `floor_h` | float | 3.0 | Upper Floor Height |
| `wall_t` | float | 0.3 | core is inset by this |
| `jetty` | float | 0.0 | upper storeys step out by this on every edge (medieval overhang); 0 = flush |
| `ground_mat` | menu | 0 (Wall) | Stone: ground-storey walls use the stone material |

### Facade

| parm | type | default | meaning |
|---|---|---|---|
| `bay_w` | float | 2.2 | target width; bays are fitted per facade |
| `front_edge` | int | 0 | footprint edge index of the front |
| `ground_front` | string | S D S | tokens per bay: W0 W D S WA DA WR; repeated to fill; exact if count matches |
| `ground_side` | string | W0 | Ground Side Pattern |
| `upper_front` | string | W | Upper Front Pattern |
| `upper_side` | string | W W0 | Upper Side Pattern |
| `center_door` | toggle | 1 | if the ground pattern has one D and fewer tokens than bays, put D on the centre bay |

### Modules

| parm | type | default | meaning |
|---|---|---|---|
| `win_w` | float | 0.55 | Window Width (of bay) |
| `win_h` | float | 0.55 | Window Height (of floor) |
| `sill_h` | float | 0.9 | Sill Height |
| `frame_w` | float | 0.08 | Frame Width |
| `frame_d` | float | 0.06 | Frame Depth (out) |
| `sill_out` | float | 0.1 | Sill Overhang |
| `win_header` | toggle | 1 | Window/Door Header |
| `door_w` | float | 1.2 | Door Width |
| `door_h` | float | 2.4 | arched door: floor to apex |
| `shop_pier` | float | 0.2 | Shop Pier Width |
| `shop_bulkhead` | float | 0.6 | Shop Bulkhead Height |
| `shop_sign_h` | float | 0.7 | Shop Sign Band |
| `arch_shape` | menu | 0 (Round) | Arch Shape (WA/DA) |
| `arch_seg` | int | 8 | Arch Segments |
| `rose_r` | float | 1.2 | Rose Window Radius |
| `rose_y` | float | 0.55 | Rose Window Centre (of floor) |
| `rose_spokes` | int | 12 | Rose Window Spokes |

### Trims

| parm | type | default | meaning |
|---|---|---|---|
| `plinth_h` | float | 0.5 | 0 disables |
| `plinth_out` | float | 0.08 | Plinth Out |
| `plinth_mat` | menu | 0 (Trim) | Plinth Material |
| `band_h` | float | 0.22 | Floor Band Height |
| `band_out` | float | 0.08 | Floor Band Out |
| `cornice_h` | float | 0.45 | Cornice Height |
| `cornice_out` | float | 0.3 | Cornice Out |
| `pilaster_w` | float | 0.45 | 0 disables; bays start after it |
| `pilaster_out` | float | 0.08 | Corner Pilaster Out |
| `pilaster_mat` | menu | 0 (Trim) | Stone = quoins |

### Timber

| parm | type | default | meaning |
|---|---|---|---|
| `timber` | toggle | 0 | Half-Timber Overlay |
| `timber_w` | float | 0.18 | Member Width |
| `timber_d` | float | 0.06 | Member Depth (out) |
| `timber_from` | int | 0 | 0 = ground storey, 1 = upper storeys only |
| `timber_brace` | menu | 1 (Diagonal) | Braces In Plain Bays |
| `timber_rail` | toggle | 1 | Sill Rail On Window Bays |
| `timber_gable` | toggle | 1 | Gable Trusses |
| `timber_collar` | float | 0.5 | Collar Height (of gable) |

### Interior

| parm | type | default | meaning |
|---|---|---|---|
| `interior` | toggle | 0 | hollow storeys: floor slabs, ceiling, stairs (replaces the closed core volumes) |
| `floor_t` | float | 0.15 | keep <= the window lintel (0.15 m) so a slab never shows in an opening |
| `ceiling_t` | float | 0.12 | Top Ceiling Thickness |
| `interior_floor_mat` | menu | 0 (Timber) | Floor Material |
| `stairs` | toggle | 1 | Stairs |
| `stair_side` | menu | 0 (Back wall) | relative to the front edge |
| `stair_w` | float | 0.9 | Stair Width |
| `stair_riser` | float | 0.19 | Max Riser |
| `stair_tread` | float | 0.28 | Max Tread |
| `stair_u` | float | 0.5 | Position Along Wall |
| `stair_landing` | float | 0.9 | Min Landing |
| `stair_hole_margin` | float | 0.02 | Stair Hole Margin |

### Buttress

| parm | type | default | meaning |
|---|---|---|---|
| `buttress` | toggle | 0 | Buttresses |
| `buttress_edges` | menu | 0 (Side walls) | On Edges |
| `buttress_wings` | toggle | 0 | Also On Wings |
| `buttress_w` | float | 0.6 | Width |
| `buttress_d` | float | 0.9 | Depth |
| `buttress_hr` | float | 0.85 | Height (of wall) |
| `buttress_steps` | int | 2 | Steps |
| `buttress_taper` | float | 0.4 | Step Taper |
| `buttress_mat` | menu | 1 (Stone) | Material |
| `pinnacle` | toggle | 0 | Pinnacles |
| `pinnacle_h` | float | 1.5 | Pinnacle Height |

### Roof

| parm | type | default | meaning |
|---|---|---|---|
| `roof_type` | menu | 1 (Gable) | gable/hip use the oriented bounding rectangle of the footprint (edge 0) |
| `roof_axis` | menu | 0 (Auto (longer side)) | Ridge Axis |
| `roof_pitch` | float | 38.0 | Pitch (deg) |
| `roof_overhang` | float | 0.4 | Overhang |
| `roof_t` | float | 0.0 | 0 = thin double-sided plane |
| `verge_w` | float | 0.0 | timber boards on the gable rakes; 0 = none |
| `verge_t` | float | 0.06 | Verge Board Thickness |
| `ridge_w` | float | 0.0 | 0 = none |
| `roof_clip` | float | 0.6 | points of wall-mounted parts (timber, cornice, pilaster, sign ...) above a pitched roof by less than this, near its eave / verge, are pulled under the roof; 0 = off |
| `roof_clip_gap` | float | 0.03 | keep it a few cm: closer than that the viewport depth buffer shows the part through the roof |
| `roof_clip_edge` | float | 1.0 | only points horizontally this close to an edge of the roof face are clipped |
| `roof_clip_skip` | string | slope hip soffit verge ridge_beam chimney chimney_cap dormer_* tower_* pinnacle* canopy awning* core_top parapet coping | parts that belong to the roof or stand on it on purpose |
| `parapet_h` | float | 0.8 | Parapet Height (flat) |
| `chimney` | toggle | 1 | Chimney |
| `chimney_n` | int | 1 | 2 = second one mirrored along the ridge |
| `chimney_u` | float | 0.75 | Chimney Along Ridge |
| `chimney_v` | float | 0.18 | Chimney Across (of span) |
| `chimney_size` | float | 0.8 | Chimney Size |
| `chimney_h` | float | 0.8 | Chimney Above Ridge |
| `chimney_mat` | menu | 0 (Trim) | Chimney Material |
| `chimney_cap` | toggle | 0 | Chimney Cap |
| `chimney_cap_h` | float | 0.15 | Chimney Cap Height |

### Dormers

| parm | type | default | meaning |
|---|---|---|---|
| `dormer_n` | int | 0 | Dormers |
| `dormer_side` | menu | 0 (Front) | On Slope |
| `dormer_w` | float | 1.4 | Width |
| `dormer_h` | float | 1.2 | Front Wall Height |
| `dormer_pitch` | float | 45.0 | Pitch |
| `dormer_pos` | float | 0.6 | fraction of the half span, ridge (0) -> eave line (1) |
| `dormer_off` | float | 0.0 | Offset Along Ridge |
| `dormer_ov` | float | 0.15 | Dormer Overhang |

### Wings

| parm | type | default | meaning |
|---|---|---|---|
| `wing_a` | toggle | 0 | Wing A |
| `wing_a_side` | menu | 0 (Front) | main-body edge it stands against (relative to the front edge) |
| `wing_a_w` | float | 4.0 | along that wall |
| `wing_a_d` | float | 3.0 | out of that wall |
| `wing_a_off` | float | 0.0 | shift of the wing centre along the wall |
| `wing_a_floors` | int | 1 | incl. the ground storey |
| `wing_a_ground` | string | W D W | outer face, ground storey; side faces use Ground/Upper Side Pattern |
| `wing_a_upper` | string | W | Wing A Upper Pattern |
| `wing_a_roof` | menu | 0 (Cross gable) | Wing A Roof |
| `wing_a_pitch` | float | 40.0 | Wing A Pitch |
| `wing_a_back` | float | 1.5 | cross gable: how far the wing roof runs into the main roof |
| `wing_b` | toggle | 0 | Wing B |
| `wing_b_side` | menu | 0 (Front) | main-body edge it stands against (relative to the front edge) |
| `wing_b_w` | float | 4.0 | along that wall |
| `wing_b_d` | float | 3.0 | out of that wall |
| `wing_b_off` | float | 0.0 | shift of the wing centre along the wall |
| `wing_b_floors` | int | 1 | incl. the ground storey |
| `wing_b_ground` | string | W D W | outer face, ground storey; side faces use Ground/Upper Side Pattern |
| `wing_b_upper` | string | W | Wing B Upper Pattern |
| `wing_b_roof` | menu | 0 (Cross gable) | Wing B Roof |
| `wing_b_pitch` | float | 40.0 | Wing B Pitch |
| `wing_b_back` | float | 1.5 | cross gable: how far the wing roof runs into the main roof |

### Canopies

| parm | type | default | meaning |
|---|---|---|---|
| `canopy_t` | float | 0.08 | Slab Thickness |
| `stripe_w` | float | 0.5 | a stripe wider than the awning = plain fabric |
| `valance_h` | float | 0.25 | Awning Valance |
| `post_w` | float | 0.18 | Post / Beam Size |
| `post_gap` | float | 2.5 | Max Post Spacing |
| `canopy_a` | toggle | 0 | Canopy A |
| `canopy_a_side` | menu | 0 (Front) | Canopy A Side |
| `canopy_a_kind` | menu | 0 (Roof slab) | Canopy A Kind |
| `canopy_a_w` | float | 3.0 | Canopy A Width |
| `canopy_a_off` | float | 0.0 | Canopy A Offset |
| `canopy_a_d` | float | 1.5 | Canopy A Depth |
| `canopy_a_h` | float | 3.0 | Canopy A Wall Height |
| `canopy_a_drop` | float | 0.6 | Canopy A Drop |
| `canopy_a_posts` | toggle | 0 | Canopy A Posts |
| `canopy_b` | toggle | 0 | Canopy B |
| `canopy_b_side` | menu | 0 (Front) | Canopy B Side |
| `canopy_b_kind` | menu | 0 (Roof slab) | Canopy B Kind |
| `canopy_b_w` | float | 3.0 | Canopy B Width |
| `canopy_b_off` | float | 0.0 | Canopy B Offset |
| `canopy_b_d` | float | 1.5 | Canopy B Depth |
| `canopy_b_h` | float | 3.0 | Canopy B Wall Height |
| `canopy_b_drop` | float | 0.6 | Canopy B Drop |
| `canopy_b_posts` | toggle | 0 | Canopy B Posts |
| `canopy_c` | toggle | 0 | Canopy C |
| `canopy_c_side` | menu | 0 (Front) | Canopy C Side |
| `canopy_c_kind` | menu | 0 (Roof slab) | Canopy C Kind |
| `canopy_c_w` | float | 3.0 | Canopy C Width |
| `canopy_c_off` | float | 0.0 | Canopy C Offset |
| `canopy_c_d` | float | 1.5 | Canopy C Depth |
| `canopy_c_h` | float | 3.0 | Canopy C Wall Height |
| `canopy_c_drop` | float | 0.6 | Canopy C Drop |
| `canopy_c_posts` | toggle | 0 | Canopy C Posts |

### Balcony

| parm | type | default | meaning |
|---|---|---|---|
| `balcony` | toggle | 0 | Balcony |
| `balcony_side` | menu | 0 (Front) | Side |
| `balcony_floor` | int | 1 | Storey |
| `balcony_w` | float | 3.0 | Width |
| `balcony_off` | float | 0.0 | Offset |
| `balcony_d` | float | 0.9 | Depth |
| `balcony_t` | float | 0.15 | Deck Thickness |
| `balcony_rail_h` | float | 1.0 | Railing Height |
| `balcony_rail_w` | float | 0.08 | Rail Size |
| `balcony_gap` | float | 0.18 | Baluster Spacing |

### Towers

| parm | type | default | meaning |
|---|---|---|---|
| `tower_a` | toggle | 0 | Tower A |
| `tower_a_mode` | menu | 0 (Single (u, v)) | Tower A Placement |
| `tower_a_u` | float | 0.0 | along the front edge, fraction of its length from the middle |
| `tower_a_v` | float | 0.0 | metres out of the front wall (corner mode: out along the corner diagonal) |
| `tower_a_shape` | menu | 0 (Square) | Tower A Shape |
| `tower_a_sides` | int | 8 | Tower A Sides |
| `tower_a_w` | float | 3.0 | Tower A Width |
| `tower_a_h` | float | 12.0 | Tower A Shaft Height |
| `tower_a_levels` | int | 3 | Tower A Levels |
| `tower_a_win_w` | float | 0.3 | of the face |
| `tower_a_win_h` | float | 0.5 | of a level |
| `tower_a_cap` | menu | 1 (Pyramid / spire) | Tower A Cap |
| `tower_a_cap_h` | float | 4.0 | Tower A Cap Height |
| `tower_a_cap_over` | float | 0.3 | Tower A Cap Overhang |
| `tower_a_pinnacles` | toggle | 0 | Tower A Corner Pinnacles |
| `tower_a_mat` | menu | 1 (Stone) | Tower A Material |
| `tower_b` | toggle | 0 | Tower B |
| `tower_b_mode` | menu | 0 (Single (u, v)) | Tower B Placement |
| `tower_b_u` | float | 0.0 | along the front edge, fraction of its length from the middle |
| `tower_b_v` | float | 0.0 | metres out of the front wall (corner mode: out along the corner diagonal) |
| `tower_b_shape` | menu | 0 (Square) | Tower B Shape |
| `tower_b_sides` | int | 8 | Tower B Sides |
| `tower_b_w` | float | 3.0 | Tower B Width |
| `tower_b_h` | float | 12.0 | Tower B Shaft Height |
| `tower_b_levels` | int | 3 | Tower B Levels |
| `tower_b_win_w` | float | 0.3 | of the face |
| `tower_b_win_h` | float | 0.5 | of a level |
| `tower_b_cap` | menu | 1 (Pyramid / spire) | Tower B Cap |
| `tower_b_cap_h` | float | 4.0 | Tower B Cap Height |
| `tower_b_cap_over` | float | 0.3 | Tower B Cap Overhang |
| `tower_b_pinnacles` | toggle | 0 | Tower B Corner Pinnacles |
| `tower_b_mat` | menu | 1 (Stone) | Tower B Material |

### Look

| parm | type | default | meaning |
|---|---|---|---|
| `col_wall` | color | 0.78, 0.68, 0.55 | Wall Colour |
| `col_trim` | color | 0.88, 0.85, 0.78 | Trim Colour |
| `col_frame` | color | 0.3, 0.22, 0.16 | Frame Colour |
| `col_glass` | color | 0.35, 0.5, 0.6 | Glass Colour |
| `col_door` | color | 0.45, 0.18, 0.12 | Door Colour |
| `col_roof` | color | 0.45, 0.2, 0.16 | Roof Colour |
| `col_stone` | color | 0.55, 0.52, 0.47 | Stone Colour |
| `col_timber` | color | 0.25, 0.15, 0.08 | Timber Colour |
| `col_fabric` | color | 0.75, 0.25, 0.18 | Fabric Colour |
| `col_fabric2` | color | 0.85, 0.8, 0.68 | Fabric2 Colour |
| `col_metal` | color | 0.2, 0.2, 0.22 | Metal Colour |
| `uv_tile` | float | 2.0 | world-space box UVs: metres per 0-1 UV |

### Unreal

| parm | type | default | meaning |
|---|---|---|---|
| `ue_wall` | string | /Game/Env/Materials/MI_Wall.MI_Wall | written to prim attribute unreal_material |
| `ue_trim` | string | /Game/Env/Materials/MI_Trim.MI_Trim | written to prim attribute unreal_material |
| `ue_frame` | string | /Game/Env/Materials/MI_Frame.MI_Frame | written to prim attribute unreal_material |
| `ue_glass` | string | /Game/Env/Materials/MI_Glass.MI_Glass | written to prim attribute unreal_material |
| `ue_door` | string | /Game/Env/Materials/MI_Door.MI_Door | written to prim attribute unreal_material |
| `ue_roof` | string | /Game/Env/Materials/MI_Roof.MI_Roof | written to prim attribute unreal_material |
| `ue_stone` | string | /Game/Env/Materials/MI_Stone.MI_Stone | written to prim attribute unreal_material |
| `ue_timber` | string | /Game/Env/Materials/MI_Timber.MI_Timber | written to prim attribute unreal_material |
| `ue_fabric` | string | /Game/Env/Materials/MI_Fabric.MI_Fabric | written to prim attribute unreal_material |
| `ue_fabric2` | string | /Game/Env/Materials/MI_Fabric2.MI_Fabric2 | written to prim attribute unreal_material |
| `ue_metal` | string | /Game/Env/Materials/MI_Metal.MI_Metal | written to prim attribute unreal_material |
