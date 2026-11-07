# med_artisan_house — Artisan House

Concept: `img_refs/medievals_refs/ConceptArt_06_ArtisanHouse_v1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_artisan_house.json`

## 1. Scale anchors
top 8.6 x 7.4 m; front view label 5.8 m (taken as eave height)

## 2. Massing
8.6 x 7.4 m, 2 storeys (3.0 + 2.6 m), 42 deg blue gable along the front; front cross gable with a round-arched door (wing A); lean-to workshop porches left and right
- footprint 8.6 x 7.4 m; ground 3.0 m, 1 upper x 2.6 m; roof gable 42 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.6 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W W0 W` |  |
| wing A | ground | `DA` | arched door in stone |
| front | upper | `W` |  |
| sides (preset) | ground / upper | `W W0` / `W W` | |

## 4. Module inventory and gaps
- arched door -> MOD_DA round
- cross gable -> WINGS a
- side porches -> CANOPY a, b (roof slab + posts)
- quoins

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins stone, chimneys 2, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.896, 0.716, 0.527], stone [0.672, 0.578, 0.445], timber [0.402, 0.191, 0.087], roof [0.072, 0.136, 0.309], glass [0.888, 0.527, 0.125], fabric [0.072, 0.136, 0.309], fabric2 [0.896, 0.716, 0.527]

## 7. Unknowns & decisions
- 5.8 m label read as eave height; the ridge is then ~9.5 m
- workbench, banner, barrels = props
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
