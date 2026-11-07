# med_mixed_shop — Mixed Shop + Residence

Concept: `img_refs/medievals_refs/ConceptArt_04_MixedShop_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_mixed_shop.json`

## 1. Scale anchors
roof plan 9.6 x 8.4 m; front view 9.4 m

## 2. Massing
9.6 x 8.4 m, 2 storeys (3.2 + 2.8 m), upper storey jettied ~0.25 m, 37 deg terracotta gable, dormer, 2 chimneys
- footprint 9.6 x 8.4 m; ground 3.2 m, 1 upper x 2.8 m; roof gable 37 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.4 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W D S S` | door with a small round canopy, shop under the red awning |
| front | upper | `W` | 4 windows |
| sides (preset) | ground / upper | `W W0` / `W W` | |

## 4. Module inventory and gaps
- jetty -> jetty 0.25
- red/cream awning on posts -> CANOPY a
- door canopy -> CANOPY b
- timber, dormer

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 2, dormers 1

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.839, 0.591, 0.392], stone [0.672, 0.578, 0.445], timber [0.305, 0.133, 0.061], roof [0.651, 0.171, 0.08], glass [0.982, 0.491, 0.084], fabric [0.651, 0.171, 0.08], fabric2 [0.913, 0.791, 0.631]

## 7. Unknowns & decisions
- round (curved) door hood approximated by a flat slab
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
