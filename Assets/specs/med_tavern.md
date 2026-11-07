# med_tavern — Tavern / Inn

Concept: `img_refs/medievals_refs/ConceptArt_04_Tarven_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_tavern.json`

## 1. Scale anchors
roof plan 12.0 x 9.6 m; front view 9.8 m

## 2. Massing
12.0 x 8.0 m main block, 2 storeys (3.2 + 2.9 m), 40 deg terracotta gable; shallow front cross gable (wing A); lower side wing with a shed roof (wing B); big stone chimney
- footprint 12.0 x 8.0 m; ground 3.2 m, 1 upper x 2.9 m; roof gable 40 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.4 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W W D W W` | porch with tables under the orange awning |
| front | upper | `W` |  |
| sides (preset) | ground / upper | `W W0 W` / `W W` | |

## 4. Module inventory and gaps
- cross gable -> WINGS a (cross gable, 0.8 m deep)
- side wing -> WINGS b (shed)
- porch awning on posts -> CANOPY a
- side door roof -> CANOPY b
- stone ground storey -> ground_mat stone

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 1, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.913, 0.753, 0.552], stone [0.651, 0.558, 0.423], timber [0.238, 0.102, 0.042], roof [0.591, 0.138, 0.058], glass [0.888, 0.423, 0.068], fabric [0.888, 0.423, 0.068], fabric2 [0.913, 0.815, 0.672]

## 7. Unknowns & decisions
- roof plan depth 9.6 m includes the porch; main block taken as 8.0 m
- tables, barrels, sign, ivy = props
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
