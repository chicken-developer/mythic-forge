# med_weapon_guild — Weapons Guild

Concept: `img_refs/medievals_refs/ConceptArt_04_WeaponGuild_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_weapon_guild.json`

## 1. Scale anchors
roof plan 10.6 x 8.8 m; front view 9.2 m

## 2. Massing
10.6 x 8.8 m, stone ground storey (3.4 m) + timber upper (2.6 m), 35 deg blue gable, front cross gable with banner (wing A), pointed-arch door, cream awning over the weapon stalls
- footprint 10.6 x 8.8 m; ground 3.4 m, 1 upper x 2.6 m; roof gable 35 deg (axis along front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.6 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `DA W S S` | pointed door in a stone surround left, stalls right |
| front | upper | `W` |  |
| sides (preset) | ground / upper | `WA W0 W` / `W W0` | |

## 4. Module inventory and gaps
- pointed door -> MOD_DA (arch_shape pointed)
- cross gable -> WINGS a
- awning -> CANOPY a
- stone ground -> ground_mat

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins no, chimneys 1, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.888, 0.716, 0.552], stone [0.644, 0.552, 0.418], timber [0.386, 0.153, 0.056], roof [0.076, 0.165, 0.366], glass [0.88, 0.434, 0.04], fabric [0.888, 0.768, 0.584], fabric2 [0.776, 0.604, 0.462]

## 7. Unknowns & decisions
- gold ridge ornaments, banners, shields, weapon racks = props / texture
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
