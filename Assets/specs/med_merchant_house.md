# med_merchant_house — Merchant House

Concept: `img_refs/medievals_refs/ConceptArt_04_MerchantHouse_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_merchant_house.json`

## 1. Scale anchors
roof plan 8.4 x 7.2 m; front view 8.8 m; storey ~2.8-3.0 m from the window rows

## 2. Massing
8.4 x 7.2 m, 2 storeys (3.0 + 2.8 m), 38 deg blue gable along the front, dormer, stone quoins, timber posts/bands
- footprint 8.4 x 7.2 m; ground 3.0 m, 1 upper x 2.8 m; roof gable 38 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.8 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W D W` | centred door with a small roof |
| front | upper | `W` | 2-3 windows with flower boxes |
| sides | both | `W W` | two windows per storey |
| sides (preset) | ground / upper | `W W` / `W W` | |

## 4. Module inventory and gaps
- door canopy -> CANOPY a (roof slab, brackets)
- quoins -> pilaster_mat stone
- dormer -> DORMERS

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins stone, chimneys 1, dormers 1

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.965, 0.745, 0.503], stone [0.687, 0.597, 0.462], timber [0.287, 0.12, 0.042], roof [0.105, 0.175, 0.347], glass [0.88, 0.509, 0.117], fabric [0.105, 0.175, 0.347], fabric2 [0.965, 0.745, 0.503]

## 7. Unknowns & decisions
- flower boxes, ivy, lanterns = props
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
