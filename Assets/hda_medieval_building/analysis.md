# Medieval layout review — 2026-11-02

The supplied scene is the native function graph at `/obj/medieval_building`.
At the start of this task this directory contained only `medieval_building.hip`;
the former `house_rebuild/function_refactor` sources and their review guide were
absent. The edits update the existing graph rather than replacing it with a
different mesh generator. The untouched starting HIP is in
`backup/before_layout_20261102/medieval_building.hip`.

All 22 reference images were inspected. The 15 individual ConceptArt sheets map
to the requested 15 presets. The Mark Lin tower and Thomas Kraak kit images are
additional style references, not extra baked variants.

| Reference / dataset | Complexity | Massing / floors | Roof | Opening and support strategy |
| --- | --- | --- | --- | --- |
| 01 Food shop | simple | rectangular, 1 + attic | transverse gable, dormer | centered entrance, two shop windows, broad supported awning |
| 02 General store | simple | broad rectangular, 1 + attic | blue gable | aligned three-bay facade, full-width shop canopy |
| 03 Herbal store | simple | rectangular, 1 + attic | blue gable | arched casements, explicit planters and trellis slots |
| 04 Merchant house | medium | rectangular, 2 | blue gable | aligned upper windows, braced entrance canopy |
| 05 Mixed shop | medium | broad rectangular, 2 | red gable | commercial ground floor, three upper windows |
| 06 Tavern | complex | intersecting main + wing, 2 | joined gables | supported balcony and a real upper access door |
| 07 Weapon guild | complex | main + wing, 2 | blue joined gables | arched openings and a designated banner bay |
| 08 Normal house B | medium | rectangular, 2 | red gable | rectangular casements and clearance for open shutters |
| 09 Normal house C | medium | rectangular, 2 | red gable | front balcony aligned with upper door |
| 10 Normal house C canopy | medium | rectangular, 2 | red gable | wrap balcony and broader canopy variation |
| 11 Artisan house | complex | main + low wing, 2 / 1 | perpendicular gables | entrance canopy, supported balcony |
| 12 Craft guild hall | complex | broad rectangular, 2; four course-built square turrets | longitudinal gable + four pyramids | wide arch entrance and explicit crest/banner bays |
| 13 Material storage | simple | rectangular, tall 1 + attic | longitudinal gable | broad loading entrance, cross bracing, attic opening and a braced hoist with hanging crate |
| 14 Toolmaker workshop | complex | main + service wing, 2 / 1 | perpendicular gables | workshop furnishing, side awning and a braced jib with hanging ring |
| 15 Forgehouse | complex | main + low wing, 2 / 1 | curved gables + cupola | real furnace arch, hollow industrial flue and metal flue bands |

The two NormalHouse C files have identical bytes. Presets 09 and 10 intentionally
vary balcony/canopy coverage and record this assumption. Dimensions are model
targets; image dimensions are not treated as surveyed measurements. All models
use meters and Houdini Y-up; FBX export converts explicitly to centimeters.

The main fixes remove primitive-order randomness from facade modules, align bay
counts across floors, reserve the whole frame/shutter envelope, remove duplicate
bay-end posts, add gable trusses, and derive interior wall framing from the same
visible facade-bay stream. Interior Copy SOPs explicitly transfer their part
metadata. Extruded floor-edge faces retain wood material and semantic names.
Numeric ranges in the controller and typed function arguments agree, preventing
large presets from silently clamping at eight meters.

The existing module vocabulary is extended with native Boolean/Tube/Box/Copy
kits for the guild turrets, furnace, hollow flue and hoists. They are selected by
explicit `facade_overrides` values (`guild_pier`, `forge`, `hoist`, `hoist_hook`)
and share the same bay metadata. Bespoke emblems, vegetation, fire simulation,
the forge bell and unique shop merchandise remain simplified or omitted.
These limitations are stored per dataset; the images show the actual result.

Normal baking uses a separate native branch: per-face local projection → native
UV Layout → triangulated low mesh → semantic high-detail bevel → cleanup/normals.
Matching connected components are temporarily translated into separate grid
cells for projection. This prevents neighboring parts from contaminating the
bake; original export positions are unchanged. Copernicus produces real 4K
DirectX tangent-space normals with MikkT tangents. Base colors are material
constants; no albedo textures or SkyTown geometry are required.

Mantra could not obtain a render license. Houdini OpenGL review renders run in
separate processes because mixing baking and viewport rendering triggered an
AMD driver crash. Each render has a configuration/hash sidecar. Unreal import,
material, mesh and screenshot results are recorded separately from Houdini checks.
