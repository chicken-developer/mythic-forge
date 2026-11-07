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
