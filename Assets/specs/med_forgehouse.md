# med_forgehouse — Main Forge (Area 07)

Concept: `img_refs/medievals_refs/ConceptArt_07_Forgehouse_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_forgehouse.json`

## 1. Scale anchors
roof plan 13.6 x 11.2 m incl. overhangs and annexes

## 2. Massing
~12 x 9 m, 2 storeys (3.6 + 3.0 m), steep 52 deg blue roof; front cross gable with a big round forge arch (wing A); round stone flue tower (TOWER b); belvedere on the ridge (TOWER a); red awning; side lean-to
- footprint 12.0 x 9.0 m; ground 3.6 m, 1 upper x 3.0 m; roof gable 52 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.4 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W W0 W0 W` |  |
| wing A | ground | `DA` | forge arch |
| sides (preset) | ground / upper | `W W0 W` / `W W0` | |

## 4. Module inventory and gaps
- cross gable -> WINGS a
- flue -> TOWER b (polygon, flat cap)
- belvedere -> TOWER a (square + pyramid)
- awning -> CANOPY a

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 0, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.973, 0.768, 0.558], stone [0.651, 0.565, 0.434], timber [0.347, 0.117, 0.041], roof [0.065, 0.136, 0.314], glass [0.991, 0.552, 0.15], fabric [0.665, 0.12, 0.084], fabric2 [0.665, 0.12, 0.084]

## 7. Unknowns & decisions
- curved/flared eaves, bell, weather vane, fire, smoke = not modelled (straight roofs)
- annex plan simplified to a rectangle
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
