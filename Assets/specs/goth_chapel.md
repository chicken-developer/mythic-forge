# goth_chapel — Gothic chapel (blue roof)

Moodboard (no building concept for this style; this preset is a style study): `img_refs/gothics_moodboard/image (6).png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/goth_chapel.json`

## 1. Scale anchors
door anchor 2.6 m -> wall ~9 m; side bay ~4 m

## 2. Massing
9 x 20 m single nave, one tall storey (9 m), 55 deg blue roof with gold verge, stepped buttresses with pinnacles on the sides, slender corner pinnacles, tall pointed windows
- footprint 9.0 x 20.0 m; ground 9.0 m, 0 upper x 9.0 m; roof gable 55 deg (axis auto), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 3.3 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W0 DA W0` | pointed portal |
| sides | ground | `WA` | 5 tall lancets |
| sides (preset) | ground / upper | `WA` / `W W0` | |

## 4. Module inventory and gaps
- buttresses -> BUTTRESS (side walls, pinnacles)
- corner pinnacles -> TOWER b (every corner, thin)
- verge -> verge boards (gold)

## 5. Trims & details
plinth 0.80 m (stone), timber off, quoins no, chimneys 0, dormers 0

## 6. Palette (linear, sampled from the moodboard image; viewport only)
wall [0.738, 0.768, 0.807], stone [0.546, 0.578, 0.624], timber [0.584, 0.371, 0.095], roof [0.05, 0.125, 0.392], glass [0.05, 0.275, 0.434], fabric [0.352, 0.042, 0.023], fabric2 [0.584, 0.371, 0.095]

## 7. Unknowns & decisions
- flamboyant tracery and the side porch = not modelled
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
