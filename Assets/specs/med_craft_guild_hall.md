# med_craft_guild_hall — Craft Guild Hall (small)

Concept: `img_refs/medievals_refs/ConceptArt_06_CraftGuildHall_v1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_craft_guild_hall.json`

## 1. Scale anchors
top 11.4 x 9.6 m; front view 7.8 m

## 2. Massing
11.4 x 9.6 m stone hall, one tall storey (5.0 m), gable end to the front (28 deg blue roof), 4 square corner turrets with pyramid caps, big round-arched portal, round-arched side windows
- footprint 11.4 x 9.6 m; ground 5.0 m, 0 upper x 5.0 m; roof gable 28 deg (axis across front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 3.8 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W0 DA W0` | portal 2.6 x 4.2 m |
| sides | ground | `WA` |  |
| sides (preset) | ground / upper | `WA` / `W W0` | |

## 4. Module inventory and gaps
- corner turrets -> TOWER a (every corner)
- portal -> MOD_DA round
- all stone -> ground_mat + col_wall = stone

## 5. Trims & details
plinth 0.60 m (stone), timber off, quoins no, chimneys 1, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.791, 0.658, 0.491], stone [0.791, 0.658, 0.491], timber [0.262, 0.109, 0.05], roof [0.089, 0.178, 0.509], glass [0.888, 0.527, 0.125], fabric [0.053, 0.102, 0.328], fabric2 [0.847, 0.694, 0.597]

## 7. Unknowns & decisions
- gold finials, banners, emblems, capitals = props / texture
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
