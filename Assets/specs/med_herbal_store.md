# med_herbal_store — Potion / Herbal Store

Concept: `img_refs/medievals_refs/ConceptArt_04_HerbalStore_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_herbal_store.json`

## 1. Scale anchors
roof plan 8.0 x 6.8 m; front view 6.4 m

## 2. Massing
8.0 x 6.8 m, one storey (3.2 m), 40 deg blue gable along the front; stone wall part with a round-arched window; open shop under a green awning
- footprint 8.0 x 6.8 m; ground 3.4 m, 0 upper x 3.4 m; roof gable 36 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.6 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `WA S S` | arched window (round) left, open counter under the green awning right |
| sides (preset) | ground / upper | `W0 D W0` / `W W0` | |

## 4. Module inventory and gaps
- arched window -> MOD_WA (arch_shape round)
- green awning on posts -> CANOPY a
- timber -> TIMBER

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 1, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.956, 0.753, 0.546], stone [0.687, 0.597, 0.462], timber [0.392, 0.171, 0.068], roof [0.125, 0.168, 0.283], glass [0.888, 0.527, 0.125], fabric [0.198, 0.296, 0.05], fabric2 [0.283, 0.376, 0.153]

## 7. Unknowns & decisions
- side entrance not visible in the hero view: door placed on the left side
- potions, herbs, banner, lanterns = props
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
