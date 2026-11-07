# med_general_store — General Store

Concept: `img_refs/medievals_refs/ConceptArt_04_GeneralStore_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_general_store.json`

## 1. Scale anchors
roof plan 9.6 x 7.2 m; front view 6.8 m; door ~2.2 m

## 2. Massing
9.6 x 7.2 m, one storey (3.2 m) + attic, 40 deg gable along the front, blue slate, dormer, 2 chimneys
- footprint 9.6 x 7.2 m; ground 3.4 m, 0 upper x 3.4 m; roof gable 36 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.4 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W D S S` | window under a blue/white awning, door, produce stalls under a big cream awning |
| sides (preset) | ground / upper | `W0 W W0` / `W W0` | |

## 4. Module inventory and gaps
- big cream awning on posts -> CANOPY a
- small striped awning -> CANOPY b
- timber frame + truss -> TIMBER
- dormer -> DORMERS

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 2, dormers 1

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.991, 0.753, 0.491], stone [0.687, 0.597, 0.462], timber [0.332, 0.13, 0.048], roof [0.091, 0.184, 0.381], glass [0.922, 0.546, 0.102], fabric [0.091, 0.184, 0.381], fabric2 [0.991, 0.753, 0.491]

## 7. Unknowns & decisions
- both awnings share the fabric colours (generator has one stripe pair per building) -> big awning is striped, concept is plain cream
- fruit crates, banner, lantern = props
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
