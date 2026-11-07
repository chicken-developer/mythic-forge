# med_normal_house_c — Nha Dan (Loai C)

Concept: `img_refs/medievals_refs/ConceptArt_05_NormalHouse_V1.png`  
Generator: `bldg::building_generator::1.0` (shared by medieval + gothic); preset `presets/med_normal_house_c.json`

## 1. Scale anchors
top 8.8 x 7.6 m; front 8.8 m wide, 9.8 m high (incl. chimney)

## 2. Massing
8.8 x 7.6 m, 2 storeys (3.2 + 2.9 m), gable end to the front (ridge across the front edge), 37 deg terracotta; front balcony; pergola over the door; side canvas canopy on posts; stone chimney
- footprint 8.8 x 7.6 m; ground 3.2 m, 1 upper x 2.9 m; roof gable 37 deg (axis across front), overhang 0.45 m

## 3. Facades (tokens left -> right, bay 2.9 m)
| facade | storey | tokens | notes |
|---|---|---|---|
| front | ground | `W0 D WA` | door under the pergola, big round-arched window right |
| front | upper | `W0 W W` | balcony window + small window |
| sides (preset) | ground / upper | `W W0` / `W W` | |

## 4. Module inventory and gaps
- balcony -> BALCONY
- pergola -> CANOPY a (pergola + posts)
- side canopy -> CANOPY b (fabric)
- arched window -> MOD_WA
- ridge across the front -> roof_axis 2

## 5. Trims & details
plinth 0.45 m (stone), timber on, quoins stone, chimneys 1, dormers 0

## 6. Palette (linear, from the concept's colour chips; viewport only)
wall [0.991, 0.73, 0.497], stone [0.716, 0.61, 0.456], timber [0.275, 0.1, 0.032], roof [0.723, 0.209, 0.048], glass [0.122, 0.162, 0.381], fabric [0.896, 0.768, 0.552], fabric2 [0.896, 0.768, 0.552]

## 7. Unknowns & decisions
- NormalHouse_03 has identical bytes: same preset
- vines on the pergola, flower boxes = props
- roofs and flat walls stay planar (detail from Substance Designer); props are not modelled
