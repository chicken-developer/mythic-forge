# goth_twin_tower_cathedral — Twin-tower cathedral front

Moodboard (no building concept for this style; this preset is a style study): `img_refs/gothics_moodboard/07809a044fb95b388186282dc88ca05c.jpg`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/goth_twin_tower_cathedral.json`

## 1. Scale anchors
no dimensions in the image; door anchor 2.4 m -> portal ~6 m; storey anchor from the gothic HDA baseline (nave 8-12 m, bay 4 m)

## 2. Massing
12 x 24 m nave, 2 tiers (7 + 6 m), 55 deg slate roof, twin square towers 4.5 m wide with tall spires on the front corners
- footprint 15.0 x 25.0 m; ground 7.0 m, 1 upper x 6.0 m; roof gable 55 deg (axis auto), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 5.0 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W0 DA W0` | pointed portal |
| front | upper | `W0 WR W0` | rose window |
| sides | both | `WA` | tall lancets between buttresses |
| sides (preset) | ground / upper | `WA` / `WA` | |

## 4. Module inventory and gaps
- towers + spires -> TOWER a (pair)
- rose -> MOD_WR
- buttresses + pinnacles -> BUTTRESS

## 5. Trims & details
plinth 0.80 m (stone), timber off, quoins no, chimneys 0, dormers 0

## 6. Palette (linear, sampled from the moodboard image; viewport only)
wall [0.738, 0.768, 0.823], stone [0.485, 0.533, 0.604], timber [0.102, 0.054, 0.032], roof [0.068, 0.144, 0.323], glass [0.03, 0.08, 0.254], fabric [0.254, 0.023, 0.051], fabric2 [0.644, 0.434, 0.144]

## 7. Unknowns & decisions
- tracery, statues, banners, crockets, flying buttresses = not modelled
- back of the nave not shown: plain apse-less gable
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
