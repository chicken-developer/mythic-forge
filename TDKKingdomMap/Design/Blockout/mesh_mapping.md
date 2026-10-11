# Thanh A mesh mapping

Bounds, pivot, slots and collision come from actual Unreal Asset Registry and loaded assets. Asset pivot is the origin; its bounds center may differ. Pack originals are not modified.

| Blockout type | Actual asset / exception | Local bounds cm (min to max) | Collision |
|---|---|---|---|
| body/foundation/wall/retaining/pier | `/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube` | [0, 0, 0] to [100, 100, 100] | 1 simple shapes; CTF_UseDefault |
| round tower/plaza | `/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder` | [-50, -50, 0] to [50, 50, 100] | 1 simple shapes; CTF_UseSimpleAndComplex |
| wedge ramp calibration | `/Game/LevelPrototyping/Meshes/SM_Ramp.SM_Ramp` | [0, 0, 0] to [100, 100, 100] | 1 simple shapes; CTF_UseDefault |
| road/ramp slab | A wedge would change the constant thickness road surface and its existing landing/route geometry. | Generated geometry / Landscape | Closed source mesh for walkable surfaces |
| stairs | No dedicated stair mesh in the audited registry; one extruded watertight step profile replaces stacked boxes. | Generated geometry / Landscape | Closed source mesh for walkable surfaces |
| roof/pyramid | No matching roof or spire mesh in audited registry; generated simple closed meshes preserve silhouette. | Generated geometry / Landscape | Closed source mesh for walkable surfaces |
| Landscape | Imported ALandscape is retained; never replaced by a cube. | Generated geometry / Landscape | Closed source mesh for walkable surfaces |

Scale = target metres * 100 / measured local bounds size in cm. Component translation = minus measured bounds center before actor scale/rotation. Native DesignOrigin keeps the intended center through level reload.

Bodies/foundations/walls/retaining/pier use SM_Cube; circular towers use SM_Cylinder. Plazas derive geometry from the actual pack Cylinder, then remove exposed coplanar overlaps at junctions. Pack Ramp is used in calibration. Thin main-route ramp slabs preserve the existing road thickness, landings and footprint; a wedge would change that geometry. Stair, roof and spire primitives are missing from the audited pack, so generated simple geometry is retained with the same metric material.

The JSON table contains full material slots, pivot and all 13 audited meshes including Door/JumpPad/Target. Those interaction shapes do not match this city massing.

Road render triangles are clipped at coplanar junctions. Their untrimmed watertight source mesh remains as ComplexCollisionMesh, CTF_UseComplexAsSimple. Render meshes may be open where another surface covers the cut; only collision sources are claimed closed.

Grid material is a project-owned variant of M_PrototypeGrid. Own MI_Metric instances replace the previous flat-color overrides. No original pack asset is edited.
