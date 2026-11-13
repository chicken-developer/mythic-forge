# med_material_storage — Material Storage Building

Concept: `img_refs/medievals_refs/ConceptArt_06_MarterialStorageBuilding_v1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_material_storage.json`

## 1. Scale anchors
top 9.2 x 6.8 m; barn door ~2.8 m

## 2. Massing
9.2 x 6.8 m barn, one tall storey (4.2 m), gable end to the front (42 deg blue), big double barn door, timber frame on a stone base; lean-to shed on the left
- footprint 9.2 x 6.8 m; ground 4.2 m, 0 upper x 4.2 m; roof gable 42 deg (axis across front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 3.0 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W0 D W0` | barn door 2.6 x 3.0 m |
| sides (preset) | ground / upper | `W0 W W0` / `W W0` | |

## 4. Module inventory and gaps
- lean-to -> CANOPY a (roof slab + posts)
- timber -> TIMBER (cross braces)

## 5. Trims & details
plinth 0.70 m (stone), timber on, quoins no, chimneys 0, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.871, 0.738, 0.597], stone [0.651, 0.565, 0.434], timber [0.309, 0.133, 0.053], roof [0.098, 0.175, 0.356], glass [0.888, 0.527, 0.125], fabric [0.098, 0.175, 0.356], fabric2 [0.871, 0.738, 0.597]

## 7. Unknowns & decisions
- hoist crane + hanging crate, crates, barrels, logs = props
- gable window not modelled
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
