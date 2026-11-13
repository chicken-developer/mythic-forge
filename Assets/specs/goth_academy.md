# goth_academy — Gothic academy facade

Moodboard (no building concept for this style; this preset is a style study): `img_refs/gothics_moodboard/image (3).png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/goth_academy.json`

## 1. Scale anchors
people in the plaza (1.75 m) -> door ~3.5 m, storey ~4.5 m

## 2. Massing
30 x 16 m, 3 storeys (5 + 4.5 + 4.5 m), warm cream sandstone with lighter stone trims, symmetric front: pointed windows, central portal + rose, two slender towers flanking the centre and two corner towers with blue spires
- footprint 30.0 x 16.0 m; ground 5.0 m, 2 upper x 4.5 m; roof gable 38 deg (axis along front), overhang 0.30 m

## 3. Facades (tokens left -> right, bay 4.2 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `WA WA WA DA WA WA WA` |  |
| front | upper | `WA WA WA WR WA WA WA` | rose in the middle on floor 1 and 2 |
| sides (preset) | ground / upper | `WA` / `WA` | |

## 4. Module inventory and gaps
- towers -> TOWER a (centre pair) + TOWER b (corner pair)
- white trims -> bands, cornice, pilasters (trim colour)

## 5. Trims & details
plinth 0.90 m (stone), timber off, quoins no, chimneys 0, dormers 0

## 6. Palette (linear, sampled from the moodboard image; viewport only)
wall [0.644, 0.503, 0.332], stone [0.761, 0.694, 0.578], timber [0.102, 0.054, 0.032], roof [0.05, 0.114, 0.301], glass [0.042, 0.074, 0.156], fabric [0.254, 0.023, 0.051], fabric2 [0.831, 0.784, 0.687]

## 7. Unknowns & decisions
- the far wings and the plaza are context, not part of the building
- statues, clock, tracery = not modelled
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
