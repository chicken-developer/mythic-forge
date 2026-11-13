# Review log (concept vs Houdini viewport screenshots)

Screenshots: `renders/<asset>/{persp,front,side,persp_low}.png` (one fresh Houdini 22 GUI per asset, SceneViewer
flipbook through the asset cameras, no ROP). Side-by-side sheets: `renders/<asset>/compare_rN.png` (latest = `compare.png`).
Max 3 rounds per asset. Order of checks: silhouette / massing -> rhythm -> roof -> trims -> palette.

## Round 1 (2026-11-07)
All 17 assets cook with 0 errors / 0 validate warnings (only `med_normal_house_c` had PolyBevel non-manifold
warnings from degenerate balcony side rails -> fixed in `balcony.vfl`).

Generic findings
- Fabric awnings were a zero-thickness double-sided sheet: the viewport z-fought and showed the underside (grey).
  -> `canopy.vfl`: awnings are a thin closed sheet, valance back face 1 cm behind.
- Medieval stone chips sampled from the sheets read too dark/brown under the viewport light -> lighter stone colours.
- One-storey shops: awnings sat at/above the eave line and hid under the roof -> canopy heights 2.6-2.8 m,
  walls 3.4 m and 36 deg roofs for the general / herbal store.

Per asset
- med_food_shop: massing, dormer, 2 chimneys, awnings OK -> pitch 42->38, awnings lower.
- med_general_store / med_herbal_store: roof too dominant -> taller walls, 36 deg.
- med_merchant_house, med_mixed_shop, med_tavern, med_toolmaker_workshop: silhouette OK; lighter stone.
- med_weapon_guild: OK (pointed door, cross gable); stone ground storey too brown -> lighter.
- med_normal_house_b / _c: concept ground storey is plaster with stone quoins, not full stone -> ground_mat Wall.
- med_artisan_house: front cross gable too small -> 3.4 m wide, 52 deg.
- med_craft_guild_hall: gable wall pale against brown stone -> the whole hall uses one stone colour.
- med_material_storage: OK.
- med_forgehouse: two forge arches (wing 2 bays) and a too heavy flue -> wing 1 bay, flue 2.0 x 9.5 m.
- goth_twin_tower_cathedral: silhouette good; rose window too small -> 15 m front, 5 m bays, rose r 2.4 m.
- goth_academy: OK (brick, white trims, 4 spired towers, rose on both upper storeys).
- goth_chapel: portal off-centre (2 front bays for 3 tokens) -> bay 3.3 m (3 front bays, 6 side bays).

## Round 2 (2026-11-07)
All 17 assets rebuilt and recaptured (0 errors, 0 validate warnings). Sheets: `renders/<asset>/compare_r2.png`.

- Canopy fix confirmed: awnings show their stripes / colour from above, no z-fighting.
- med_*: silhouettes, floor counts, jetties, dormers, chimneys and awning colours read like the concepts. The
  remaining differences are props and hand-modelled details (see "Not matching" in `building_generator.md`).
- med_general_store / med_herbal_store: walls 3.4 m + 36 deg now give the concept's roof-to-wall ratio. OK.
- med_artisan_house: front cross gable reads right. OK.
- med_forgehouse: one forge arch, slimmer flue. Curved/stepped roofs of the concept are still straight gables (gap).
- goth_twin_tower_cathedral: rose window and portal now in proportion. OK.
- goth_chapel: portal centred. OK.
- goth_academy: wrong palette. Looking again at the concept, the walls are warm cream sandstone (only the
  corner towers have a light pink-tan brick); round 1 read it as red brick -> round 3: wall #d2bc9c,
  stone #e2d9c8, trim #f0eadf (spec updated).

## Round 3 (2026-11-08) — goth_academy only
`img_refs/` was reorganised: gothic images are now `gothics_moodboard/` (no building concept sheets), so the three
gothic presets are style studies; their sheets label the reference "moodboard (style study)".
- goth_academy: walls cream sandstone -> the towers and the central block now read like the moodboard. Closer look
  at the full image: the lower wing walls between the towers ARE red brick, so the image is mixed (brick wings,
  cream towers / centre / trims). Neither round 2 (all brick) nor round 3 (all cream) is exact. Round limit reached;
  suggested preset change for the user: `col_wall` back to brick (#b2553f) with `tower_a_mat` / `tower_b_mat` and
  `pilaster_mat` = stone (cream #e2d9c8). A cream centre block between the brick wings would need a per-wing wall
  colour (generator gap).
- All other assets: no round 3 needed (round 2 accepted; remaining differences are props / modelling work).

## Fixes 2026-11-08 (user feedback: timber showing through roofs, no interiors)

- Roof: wall-mounted parts stuck out of pitched roofs near the eave / verge (top plates and posts, gable-truss
  collars / king posts / tie beams, cornices, corner pilasters, shop sign bands). Generic fix in the generator
  (OUTPUT / roof_clip, see building_generator.md). `tools/check_roof_poke.py`: before 11 assets had pokes
  (timber_plate, timber_gable, cornice, pilaster, sign ...), after: none on all 17.
- med_tavern: the left lean-to (wing b) reached 4.24 m and cut the upper-storey windows -> pitch 22 -> 15 deg.
- Interior shell on all 17 presets (`interior = 1`): floor slabs, ceilings, stairs with holes; see `cutaway.png`.
- Dormer window parts renamed `dormer_frame / dormer_glass / dormer_sill` (bevel list updated) so the roof clip
  never touches them.
- roof_clip_gap 1 cm still showed through the roof in the viewport (depth precision) -> 3 cm; gable triangles
  (rakes exactly in the roof plane) are lowered too.
- Bake med_tavern: xyzdist piece transfer gave ~10% wrong texels; the generator's point i@piece fixed it (0.4%).
  1008 HIGH vertices had non-finite UV/N after PolyBevel -> Unreal 5.8 Interchange asserted and the editor
  crashed on import; bake_asset.py now zeroes them (count in bake.json).
- Unreal (TDKProps 5.8.4, /Game/Levels/NewMap): LOW 7879 tris (= Houdini), HIGH 48133 of 48286 (Unreal drops
  degenerate bevel slivers), sizes match. 8 captures in _render/RenderTests/HBuild/med_tavern/.
  Still visible: one small timber stub at the valley where the tavern's cross gable meets the main roof.

## Round 5 (user feedback: small defects, then Unreal export)
- Lean-to (med_tavern wing b): sills / rail above its high edge were pulled down onto the roof (black slivers).
  The lean-to rule counted the slab's own thickness face (it points partly up on a 15 deg slab) as a ridge
  neighbour. Fix: a neighbour counts only when it reaches further than `roof_clip_shared` (0.3 m) from the high
  edge, and thickness faces are never clip targets.
- Valley sliver (cross gable / main roof): top plates are now built in segments (`timber_seg`), so the roof
  clip can follow the slope; no sliver left in the viewport.
- Canopy through a window header: headers touching a canopy / awning / beam are removed (`canopy_clear_*`).
- Unreal fans through the interior: 72 NaN vertices in SM_med_tavern_HIGH.fbx (PolyBevel on parts whose edges
  the roof clip collapsed). Fixed in HIGH (no bevel on collapsed parts + finite repair); the bake now refuses to
  export non-finite positions and `tools/check_fbx.py` checks the FBX files.
- Window cut by a wing (med_tavern front, next to the cross-gable block): main-body openings that reach into a
  wing on a storey the wing has become plain wall bays (`wing_clear` 0.12 m).
