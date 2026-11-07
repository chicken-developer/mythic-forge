# Source analysis — medieval + gothic hips (step 2)

Read-only analysis, 2026-11-07, Houdini 22.0.368. Originals untouched (sha256 checked before/after,
`docs/original_hip_hashes.json`). Raw dumps: `analysis/*_inspect.json`, `analysis/*_validate_*.json`,
`analysis/*_netdump_*.json`, gothic HDA sections in `analysis/gothic_hda_sections/`.

## medieval_building.hip — `/obj/medieval_building`

- 550 nodes. A caller network with 18 `F_*` function subnets plus `CTRL_BUILDING` (null with 91 spare parms).
  Only the callers read `CTRL_BUILDING`; functions get values as their own parms.
- Cooks in 5.5 s: 20 190 pts / 15 829 prims, 10.8 × 8.75 × 9.0 m. Current values: 8.2 × 6.4 m,
  2 floors × 2.65 m, `front_bays = WDW/W.W` (rows per floor separated by `/`, `W` window, `D` door, `.` wall).
- Pipeline: `F_PLAN` (roles main/wing/tower/workshop; footprints rectangular / l_shape / round / hybrid)
  → `F_BODY_SHELL` (boolean solid) → `F_FACADE_LAYOUT` (bays from real mass faces, opening points)
  → `F_OPENING_KITS` / `F_WINDOWS` / `F_DOORS` / `F_WALL_FINISH` (boolean cut openings)
  → `F_FACADE_DETAILS` (142 nodes: timber posts/plates/curved knee braces, gable trusses)
  → `F_ROOF` (skinned sections, boolean, **individual tile geometry**) + `F_ROOF_TRIM` (ridge, chimneys)
  + `F_DORMERS`, `F_ATTACHMENTS` (bay window, cupola, workshop), `F_BALCONY`, `F_SITE` (ground, awning,
  canopy), `F_UTILITIES` (water wheel, windmill rotor, stilts), `F_INTERIOR`, `F_FURNISHING` → `F_FINISH`
  (palette Cd, metric UV, materials, scale).
- Against the new conventions (`hb.py validate`): 26 errors — no network boxes, no `OUT` node,
  `CTRL_BUILDING` values are literals; several functions read inputs from inside other functions.
  LOW/HIGH is one `mesh_quality` switch, not two outputs. Roof tiles are modelled (the new flow keeps
  roofs flat for Substance Designer).

## gothic_building.hip — `/obj/gothic_building_v2_1/generator`

- HDA `quynh::gothic_building::3.0` (embedded), preset `variant_01` (Village Chapel). 48 parms,
  29 embedded presets (baseline + variant_01..28: churches, gatehouses, towers, residences, public halls).
- Cooks in 3 s: 64 266 pts / 126 802 prims, 9.1 × 11.1 × 12.9 m.
- Logic lives in the HDA **PythonModule** (`CORE_SOURCE`, 1 100 lines): Python computes 2D profiles and
  placements, then writes them into the network as `stash` nodes (77), `edit` nodes (27) and wrangles with
  literal positions (`@P=set(-4,0,2)+...`). The SOP network is a baked result: changing a parm re-runs
  Python ("rebuild_native"); it is not readable as a procedural graph.
- Module vocabulary worth keeping: lancet panel (pointed-arch opening, archivolt, sloping sill, mullion),
  rose window (frame + 12 spokes), stepped buttress + capital + pinnacle (lantern, canopy, spire),
  towers with crowns (pyramid / canopied / openwork), stepped gables, porch, roof slabs with clip volumes.

## Proposed: one generator for both styles

`bldg::building_generator::1.0`, extended additively (existing parms keep their meaning; new parms default
to "off" so `example_townhouse` stays identical). Style = preset, not a separate generator. No split needed.

| module (network box) | status | used by |
|---|---|---|
| FOOTPRINT | exists | all |
| LAYOUT | extend: per-floor jetty offset, arch-shape per token | all |
| MOD_W0 / MOD_W / MOD_D / MOD_S | exist | all |
| MOD_WA (arched window: round or pointed by `arch_shape`) | new | gothic, some medieval |
| MOD_DA (arched door/portal, round or pointed) | new | gothic, guild hall, artisan |
| MOD_WR (rose / round window) | new | gothic fronts |
| CORE | extend: jetty (upper floors step out) | medieval |
| TRIMS | exists (plinth, bands, cornice, corner pilasters/quoins) | all |
| TIMBER (half-timber overlay: posts, plates, braces from OUT_LAYOUT) | new | medieval |
| BUTTRESS (stepped buttresses + pinnacles at bay boundaries) | new | gothic |
| ROOF | extend: ridge axis override, gable verge boards, steep pitch, stepped gable | all |
| WINGS (2 annex slots: box + own gable/shed roof, side/offset/size) | new | tavern, artisan, guild, storage, gothic aisles |
| DORMERS (count, size, on front slope) | new | most medieval |
| LEANTO (shed roof on posts along one facade: porch / canopy / side shed) | new | herbal, normal B/C, storage, toolmaker, tavern |
| AWNING (fabric awning over the shop bays, stripes as Cd) | new | food, general, mixed, tavern, weapon, toolmaker |
| BALCONY (slab + railing on one floor) | new | normal C |
| TOWER (square or round tower + spire/pyramid cap; corner turrets mode) | new | gothic towers, guild hall turrets, forge flue |
| CHIMNEY | extend: count + positions | all |
| OUTPUT | extend: OUT_LOW / OUT_HIGH (HIGH = PolyBevel on hardsurface parts only; walls/roof planes untouched) | all |

## Gaps per style

### Medieval (15 concept sheets in `img_refs/medievals_refs/`; 2 NormalHouse sheets are byte-identical)
1. Half-timber framing (posts, plates, X/diagonal braces) — biggest visual gap → TIMBER.
2. Wings, cross gables and side sheds (tavern, artisan, storage, toolmaker) → WINGS + LEANTO.
3. Dormers on almost every sheet → DORMERS.
4. Fabric awnings over shop fronts → AWNING.
5. Jettied upper floor (upper floor slightly wider) → LAYOUT/CORE.
6. Stone ground floor / quoins vs plaster upper floors → material per floor + pilasters as quoins.
7. Arched door/window (herbal, normal C, artisan, guild hall portal) → MOD_WA / MOD_DA.
8. Balcony (normal C) → BALCONY. Corner turrets with pyramid caps (guild hall) → TOWER (turret mode).
9. Not planned (props / sculpt / bespoke): barrels, crates, signs, lanterns, plants, flower boxes,
   hoist cranes, anvil, banners, forge fire; forge's curved flared roofs and belvedere are approximated
   by straight roofs + a round flue tower.

### Gothic (`img_refs/gothics_moodboard/`: moodboard only, no building concept sheets)
1. Pointed-arch windows/doors, rose window → MOD_WA / MOD_DA / MOD_WR with `arch_shape = pointed`.
2. Buttresses + pinnacles along the side walls → BUTTRESS.
3. Front towers (single / pair) with spires → TOWER.
4. Steep roofs (50–65°) and aisles (lower side volumes with lean-to roofs) → ROOF + WINGS.
5. Not planned: flying buttresses, tracery fill patterns, statues, crockets, domes and baroque palace forms
   (image (4)), whole-city panoramas — no single-building procedural target.
6. Scale: no dimensions in the gothic images; sizes come from the gothic HDA's own presets
   (nave 8 m, wall 5.5–12 m, bay 4 m) and the 1.75 m person / 2.4 m door anchors.

## Repo notes
- `AGENTS.md` asks Medieval work to follow `.agents/skills/design-hda-building`; this rebuild follows the
  user's explicit request (houdini-procedural-builder flow) in new folders and leaves the old HIP/HDA
  and their contracts untouched.
- At analysis time the `master` working tree already had ~500 uncommitted tracked deletions
  (`Assets/hda_medieval_building/*`, `Assets/hda_samples/*`, `Assets/texture_gothic/*`, `Assets/skills/*`).
  They carried over to branch `hbuild/building-generator` as unstaged changes and are NOT committed by this work.
