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
| 1 | LOW: modular geometry, flat walls and roof planes, Cd + `unreal_material`, metric box UVs, normals |
| 2 | HIGH: LOW + PolyBevel (`bevel_w`, `bevel_div`) on the parts listed in `bevel_parts` (frames, timber, trims, posts, buttresses, chimneys ...). Walls, roof planes, glass and fabric are never bevelled — their detail comes from Substance Designer. |

## Network (one network box per module, each ends in `OUT_<MODULE>`)
| row | module | job |
|---|---|---|
| 0 | FOOTPRINT | rectangle or input curve -> prim 0 ground outline, prim 1 upper outline (jetty) |
| 0 | WING_PLAN | wing footprints from wing slots a/b (side, size, storeys, patterns, roof) |
| 0 | LAYOUT | one point per facade bay for main body + wings (`s@module`, `i@floor`, `i@edge`, `i@bay`, `i@nbays`, `i@poly`, `f@bay_width`) |
| 0 | MOD_W0 / W / D / S / WA / DA / WR | one bay each in module space, copied onto its layout points |
| 1 | TRIMS, CORE, ROOF | plinth/bands/cornice/pilasters (quoins); inset volumes; gable/hip/flat roof + verge + ridge + chimneys |
| 1 | WINGS | wing volumes + cross gable / shed / parallel gable / flat roofs |
| 1 | DORMERS | gabled dormers on the main roof |
| 1 | CANOPY | 3 slots: porch roof slab, striped fabric awning with scalloped valance, pergola; posts or brackets |
| 1 | BALCONY | deck, railing, balusters, brackets |
| 1 | TOWER | 2 slots: square / polygon towers, turrets on every corner, flues; flat or pyramid/spire caps, corner pinnacles |
| 1 | TIMBER | half-timber overlay from the layout: posts, plates, sill rails, braces (diagonal / cross / chevron), gable trusses |
| 1 | BUTTRESS | stepped buttresses + sloped caps + pinnacles on bay boundaries |
| 2 | OUTPUT -> HIGH -> PUBLISH | LOW, HIGH, switch + `output` SOPs 0/1/2 |

Facade tokens per bay: `W0` wall, `W` window, `D` door, `S` shopfront, `WA` arched window, `DA` arched door/portal,
`WR` rose window. `arch_shape` 0 round / 1 pointed. Patterns repeat to fill the bays, or map 1:1 when the token count
equals the bay count.

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
| `apply_preset` | button | None |  |

### Output

| parm | type | default | meaning |
|---|---|---|---|
| `preview_quality` | menu | 0 (LOW) | what output 0 / the display shows; output 1 is always LOW, output 2 always HIGH |
| `bevel_w` | float | 0.015 | PolyBevel offset on hardsurface parts (group hs_bevel) |
| `bevel_div` | int | 2 |  |
| `bevel_ignore_flat` | toggle | 1 |  |
| `bevel_parts` | string | frame mullion sill header door bulkhead sign plinth band cornice pilaster coping chimney chimney_cap timber_* verge ridge_beam buttress pinnacle tower_band arch_frame rose_frame rose_spoke rose_hub beam post rail baluster bracket balcony_deck | s@part patterns that get bevelled; walls, roof planes, glass and fabric stay flat |

### Footprint

| parm | type | default | meaning |
|---|---|---|---|
| `footprint_mode` | menu | 0 (Rectangle) | Input curve: closed/open polyline on input 1 (e.g. a spline from Unreal) |
| `width` | float | 10.0 | rectangle width, metres |
| `depth` | float | 8.0 | rectangle depth, metres |

### Floors

| parm | type | default | meaning |
|---|---|---|---|
| `ground_h` | float | 3.6 |  |
| `floor_n` | int | 2 |  |
| `floor_h` | float | 3.0 |  |
| `wall_t` | float | 0.3 | core is inset by this |
| `jetty` | float | 0.0 | upper storeys step out by this on every edge (medieval overhang); 0 = flush |
| `ground_mat` | menu | 0 (Wall) | Stone: ground-storey walls use the stone material |

### Facade

| parm | type | default | meaning |
|---|---|---|---|
| `bay_w` | float | 2.2 | target width; bays are fitted per facade |
| `front_edge` | int | 0 | footprint edge index of the front |
| `ground_front` | string | S D S | tokens per bay: W0 W D S WA DA WR; repeated to fill; exact if count matches |
| `ground_side` | string | W0 |  |
| `upper_front` | string | W |  |
| `upper_side` | string | W W0 |  |
| `center_door` | toggle | 1 | if the ground pattern has one D and fewer tokens than bays, put D on the centre bay |

### Modules

| parm | type | default | meaning |
|---|---|---|---|
| `win_w` | float | 0.55 |  |
| `win_h` | float | 0.55 |  |
| `sill_h` | float | 0.9 |  |
| `frame_w` | float | 0.08 |  |
| `frame_d` | float | 0.06 |  |
| `sill_out` | float | 0.1 |  |
| `win_header` | toggle | 1 |  |
| `door_w` | float | 1.2 |  |
| `door_h` | float | 2.4 | arched door: floor to apex |
| `shop_pier` | float | 0.2 |  |
| `shop_bulkhead` | float | 0.6 |  |
| `shop_sign_h` | float | 0.7 |  |
| `arch_shape` | menu | 0 (Round) |  |
| `arch_seg` | int | 8 |  |
| `rose_r` | float | 1.2 |  |
| `rose_y` | float | 0.55 |  |
| `rose_spokes` | int | 12 |  |

### Trims

| parm | type | default | meaning |
|---|---|---|---|
| `plinth_h` | float | 0.5 | 0 disables |
| `plinth_out` | float | 0.08 |  |
| `plinth_mat` | menu | 0 (Trim) |  |
| `band_h` | float | 0.22 |  |
| `band_out` | float | 0.08 |  |
| `cornice_h` | float | 0.45 |  |
| `cornice_out` | float | 0.3 |  |
| `pilaster_w` | float | 0.45 | 0 disables; bays start after it |
| `pilaster_out` | float | 0.08 |  |
| `pilaster_mat` | menu | 0 (Trim) | Stone = quoins |

### Timber

| parm | type | default | meaning |
|---|---|---|---|
| `timber` | toggle | 0 |  |
| `timber_w` | float | 0.18 |  |
| `timber_d` | float | 0.06 |  |
| `timber_from` | int | 0 | 0 = ground storey, 1 = upper storeys only |
| `timber_brace` | menu | 1 (Diagonal) |  |
| `timber_rail` | toggle | 1 |  |
| `timber_gable` | toggle | 1 |  |
| `timber_collar` | float | 0.5 |  |

### Buttress

| parm | type | default | meaning |
|---|---|---|---|
| `buttress` | toggle | 0 |  |
| `buttress_edges` | menu | 0 (Side walls) |  |
| `buttress_wings` | toggle | 0 |  |
| `buttress_w` | float | 0.6 |  |
| `buttress_d` | float | 0.9 |  |
| `buttress_hr` | float | 0.85 |  |
| `buttress_steps` | int | 2 |  |
| `buttress_taper` | float | 0.4 |  |
| `buttress_mat` | menu | 1 (Stone) |  |
| `pinnacle` | toggle | 0 |  |
| `pinnacle_h` | float | 1.5 |  |

### Roof

| parm | type | default | meaning |
|---|---|---|---|
| `roof_type` | menu | 1 (Gable) | gable/hip use the oriented bounding rectangle of the footprint (edge 0) |
| `roof_axis` | menu | 0 (Auto (longer side)) |  |
| `roof_pitch` | float | 38.0 |  |
| `roof_overhang` | float | 0.4 |  |
| `roof_t` | float | 0.0 | 0 = thin double-sided plane |
| `verge_w` | float | 0.0 | timber boards on the gable rakes; 0 = none |
| `verge_t` | float | 0.06 |  |
| `ridge_w` | float | 0.0 | 0 = none |
| `parapet_h` | float | 0.8 |  |
| `chimney` | toggle | 1 |  |
| `chimney_n` | int | 1 | 2 = second one mirrored along the ridge |
| `chimney_u` | float | 0.75 |  |
| `chimney_v` | float | 0.18 |  |
| `chimney_size` | float | 0.8 |  |
| `chimney_h` | float | 0.8 |  |
| `chimney_mat` | menu | 0 (Trim) |  |
| `chimney_cap` | toggle | 0 |  |
| `chimney_cap_h` | float | 0.15 |  |

### Dormers

| parm | type | default | meaning |
|---|---|---|---|
| `dormer_n` | int | 0 |  |
| `dormer_side` | menu | 0 (Front) |  |
| `dormer_w` | float | 1.4 |  |
| `dormer_h` | float | 1.2 |  |
| `dormer_pitch` | float | 45.0 |  |
| `dormer_pos` | float | 0.6 | fraction of the half span, ridge (0) -> eave line (1) |
| `dormer_off` | float | 0.0 |  |
| `dormer_ov` | float | 0.15 |  |

### Wings

| parm | type | default | meaning |
|---|---|---|---|
| `wing_a` | toggle | 0 |  |
| `wing_a_side` | menu | 0 (Front) | main-body edge it stands against (relative to the front edge) |
| `wing_a_w` | float | 4.0 | along that wall |
| `wing_a_d` | float | 3.0 | out of that wall |
| `wing_a_off` | float | 0.0 | shift of the wing centre along the wall |
| `wing_a_floors` | int | 1 | incl. the ground storey |
| `wing_a_ground` | string | W D W | outer face, ground storey; side faces use Ground/Upper Side Pattern |
| `wing_a_upper` | string | W |  |
| `wing_a_roof` | menu | 0 (Cross gable) |  |
| `wing_a_pitch` | float | 40.0 |  |
| `wing_a_back` | float | 1.5 | cross gable: how far the wing roof runs into the main roof |
| `wing_b` | toggle | 0 |  |
| `wing_b_side` | menu | 0 (Front) | main-body edge it stands against (relative to the front edge) |
| `wing_b_w` | float | 4.0 | along that wall |
| `wing_b_d` | float | 3.0 | out of that wall |
| `wing_b_off` | float | 0.0 | shift of the wing centre along the wall |
| `wing_b_floors` | int | 1 | incl. the ground storey |
| `wing_b_ground` | string | W D W | outer face, ground storey; side faces use Ground/Upper Side Pattern |
| `wing_b_upper` | string | W |  |
| `wing_b_roof` | menu | 0 (Cross gable) |  |
| `wing_b_pitch` | float | 40.0 |  |
| `wing_b_back` | float | 1.5 | cross gable: how far the wing roof runs into the main roof |

### Canopies

| parm | type | default | meaning |
|---|---|---|---|
| `canopy_t` | float | 0.08 |  |
| `stripe_w` | float | 0.5 | a stripe wider than the awning = plain fabric |
| `valance_h` | float | 0.25 |  |
| `post_w` | float | 0.18 |  |
| `post_gap` | float | 2.5 |  |
| `canopy_a` | toggle | 0 |  |
| `canopy_a_side` | menu | 0 (Front) |  |
| `canopy_a_kind` | menu | 0 (Roof slab) |  |
| `canopy_a_w` | float | 3.0 |  |
| `canopy_a_off` | float | 0.0 |  |
| `canopy_a_d` | float | 1.5 |  |
| `canopy_a_h` | float | 3.0 |  |
| `canopy_a_drop` | float | 0.6 |  |
| `canopy_a_posts` | toggle | 0 |  |
| `canopy_b` | toggle | 0 |  |
| `canopy_b_side` | menu | 0 (Front) |  |
| `canopy_b_kind` | menu | 0 (Roof slab) |  |
| `canopy_b_w` | float | 3.0 |  |
| `canopy_b_off` | float | 0.0 |  |
| `canopy_b_d` | float | 1.5 |  |
| `canopy_b_h` | float | 3.0 |  |
| `canopy_b_drop` | float | 0.6 |  |
| `canopy_b_posts` | toggle | 0 |  |
| `canopy_c` | toggle | 0 |  |
| `canopy_c_side` | menu | 0 (Front) |  |
| `canopy_c_kind` | menu | 0 (Roof slab) |  |
| `canopy_c_w` | float | 3.0 |  |
| `canopy_c_off` | float | 0.0 |  |
| `canopy_c_d` | float | 1.5 |  |
| `canopy_c_h` | float | 3.0 |  |
| `canopy_c_drop` | float | 0.6 |  |
| `canopy_c_posts` | toggle | 0 |  |

### Balcony

| parm | type | default | meaning |
|---|---|---|---|
| `balcony` | toggle | 0 |  |
| `balcony_side` | menu | 0 (Front) |  |
| `balcony_floor` | int | 1 |  |
| `balcony_w` | float | 3.0 |  |
| `balcony_off` | float | 0.0 |  |
| `balcony_d` | float | 0.9 |  |
| `balcony_t` | float | 0.15 |  |
| `balcony_rail_h` | float | 1.0 |  |
| `balcony_rail_w` | float | 0.08 |  |
| `balcony_gap` | float | 0.18 |  |

### Towers

| parm | type | default | meaning |
|---|---|---|---|
| `tower_a` | toggle | 0 |  |
| `tower_a_mode` | menu | 0 (Single (u, v)) |  |
| `tower_a_u` | float | 0.0 | along the front edge, fraction of its length from the middle |
| `tower_a_v` | float | 0.0 | metres out of the front wall (corner mode: out along the corner diagonal) |
| `tower_a_shape` | menu | 0 (Square) |  |
| `tower_a_sides` | int | 8 |  |
| `tower_a_w` | float | 3.0 |  |
| `tower_a_h` | float | 12.0 |  |
| `tower_a_levels` | int | 3 |  |
| `tower_a_win_w` | float | 0.3 | of the face |
| `tower_a_win_h` | float | 0.5 | of a level |
| `tower_a_cap` | menu | 1 (Pyramid / spire) |  |
| `tower_a_cap_h` | float | 4.0 |  |
| `tower_a_cap_over` | float | 0.3 |  |
| `tower_a_pinnacles` | toggle | 0 |  |
| `tower_a_mat` | menu | 1 (Stone) |  |
| `tower_b` | toggle | 0 |  |
| `tower_b_mode` | menu | 0 (Single (u, v)) |  |
| `tower_b_u` | float | 0.0 | along the front edge, fraction of its length from the middle |
| `tower_b_v` | float | 0.0 | metres out of the front wall (corner mode: out along the corner diagonal) |
| `tower_b_shape` | menu | 0 (Square) |  |
| `tower_b_sides` | int | 8 |  |
| `tower_b_w` | float | 3.0 |  |
| `tower_b_h` | float | 12.0 |  |
| `tower_b_levels` | int | 3 |  |
| `tower_b_win_w` | float | 0.3 | of the face |
| `tower_b_win_h` | float | 0.5 | of a level |
| `tower_b_cap` | menu | 1 (Pyramid / spire) |  |
| `tower_b_cap_h` | float | 4.0 |  |
| `tower_b_cap_over` | float | 0.3 |  |
| `tower_b_pinnacles` | toggle | 0 |  |
| `tower_b_mat` | menu | 1 (Stone) |  |

### Look

| parm | type | default | meaning |
|---|---|---|---|
| `col_wall` | color | (0.78, 0.68, 0.55) |  |
| `col_trim` | color | (0.88, 0.85, 0.78) |  |
| `col_frame` | color | (0.3, 0.22, 0.16) |  |
| `col_glass` | color | (0.35, 0.5, 0.6) |  |
| `col_door` | color | (0.45, 0.18, 0.12) |  |
| `col_roof` | color | (0.45, 0.2, 0.16) |  |
| `col_stone` | color | (0.55, 0.52, 0.47) |  |
| `col_timber` | color | (0.25, 0.15, 0.08) |  |
| `col_fabric` | color | (0.75, 0.25, 0.18) |  |
| `col_fabric2` | color | (0.85, 0.8, 0.68) |  |
| `col_metal` | color | (0.2, 0.2, 0.22) |  |
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