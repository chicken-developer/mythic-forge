# med_food_shop — Food Shop / Bakery

Concept: `img_refs/medievals_refs/ConceptArt_04_FoodShop_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_food_shop.json`

## 1. Scale anchors
roof plan 8.0 x 6.8 m; front view 6.4 m to the ridge; door ~2.2 m

## 2. Massing
8.0 x 6.8 m, one storey (3.0 m) + attic in a 42 deg gable roof, ridge along the front; dormer on the front slope; 2 chimneys
- footprint 8.0 x 6.8 m; ground 3.0 m, 0 upper x 3.0 m; roof gable 38 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.0 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W D S S` | window + door left, open shop counter right under the striped awning |
| sides (preset) | ground / upper | `W0 W W0` / `W0` | |

## 4. Module inventory and gaps
- striped awning on posts -> CANOPY a (fabric)
- small awning over the left window -> CANOPY b
- half-timber + gable truss -> TIMBER
- dormer -> DORMERS

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 2, dormers 1

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.982, 0.807, 0.558], stone [0.687, 0.597, 0.462], timber [0.402, 0.171, 0.063], roof [0.753, 0.223, 0.091], glass [0.888, 0.527, 0.125], fabric [0.753, 0.223, 0.091], fabric2 [0.871, 0.665, 0.445]

## 7. Unknowns & decisions
- back facade: plain plaster + one window (back view)
- bread stalls, sign, lanterns, plants = props (not modelled)
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
